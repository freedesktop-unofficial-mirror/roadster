/***************************************************************************
 *            mainwindow.c
 *
 *  Copyright  2005  Ian McIntosh
 *  ian_mcintosh@linuxadvocate.org
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gtk/gtksignal.h>

#include "search_road.h"
#include "gui.h"
#include "util.h"
#include "gotowindow.h"
#include "db.h"
#include "map.h"
#include "layers.h"
#include "importwindow.h"
#include "datasetwindow.h"
#include "welcomewindow.h"
#include "locationset.h"
#include "gpsclient.h"
#include "databasewindow.h"
#include "mainwindow.h"
#include "glyph.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <cairo.h>

#ifdef HAVE_CAIRO_0_2_0
#  include <cairo-xlib.h>
#endif

#define PROGRAM_NAME			"Roadster"
#define PROGRAM_COPYRIGHT		"Copyright (c) 2005 Ian McIntosh"
#define PROGRAM_DESCRIPTION		"Mapping for everyone!"

#define DRAW_PRETTY_TIMEOUT_MS		(180)	// how long after stopping various movements should we redraw in high-quality mode
#define SCROLL_TIMEOUT_MS		(100)	// how often (in MS) to move (SHORTER THAN ABOVE TIME)
#define SCROLL_DISTANCE_IN_PIXELS	(60)	// how far to move every (above) MS
#define BORDER_SCROLL_CLICK_TARGET_SIZE	(20)	// the size of the click target (distance from edge of map view) to begin scrolling

// Layerlist columns
#define LAYERLIST_COLUMN_ENABLED	(0)
#define LAYERLIST_COLUMN_NAME		(1)

// Limits
#define MAX_SEARCH_TEXT_LENGTH		(100)
#define SPEED_LABEL_FORMAT		("<span font_desc='32'>%.0f</span>")

// Settings
#define TIMER_GPS_REDRAW_INTERVAL_MS	(2500)		// lower this (to 1?) when it's faster to redraw track

// Types
typedef struct {
	GdkCursorType m_CursorType;
	GdkCursor* m_pGdkCursor;
} cursor_t;

typedef struct {
	char* m_szName;
	cursor_t m_Cursor;
} toolsettings_t;

typedef enum {
	kToolPointer = 0,
	kToolZoom = 1,
} EToolType;

// Prototypes
static gboolean mainwindow_on_mouse_button_click(GtkWidget* w, GdkEventButton *event);
static gboolean mainwindow_on_mouse_motion(GtkWidget* w, GdkEventMotion *event);
static gboolean mainwindow_on_mouse_scroll(GtkWidget* w, GdkEventScroll *event);
static gboolean mainwindow_on_expose_event(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data);
static gint mainwindow_on_configure_event(GtkWidget *pDrawingArea, GdkEventConfigure *event);
static gboolean mainwindow_callback_on_gps_redraw_timeout(gpointer pData);
static void mainwindow_setup_selected_tool(void);


void mainwindow_map_center_on_windowpoint(gint nX, gint nY);

struct {
	gint m_nX;
	gint m_nY;
} g_aDirectionMultipliers[] = {{0,0}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}};

struct {
	gint m_nCursor;
} g_aDirectionCursors[] = {GDK_LEFT_PTR, GDK_TOP_SIDE, GDK_TOP_RIGHT_CORNER, GDK_RIGHT_SIDE, GDK_BOTTOM_RIGHT_CORNER, GDK_BOTTOM_SIDE, GDK_BOTTOM_LEFT_CORNER, GDK_LEFT_SIDE, GDK_TOP_LEFT_CORNER};

struct {
	GtkWindow* m_pWindow;
	GtkTooltips* m_pTooltips;
	GtkMenu* m_pMapPopupMenu;

	// Toolbar
	GtkToolbar* m_pToolbar;
	GtkToolButton* m_pPointerToolButton;
	GtkToolButton* m_pZoomToolButton;
	GtkHScale* m_pZoomScale;
	GtkEntry* m_pSearchBox;
	GtkImage* m_pStatusbarGPSIcon;
	GtkWidget *m_pSidebox;

	// Sidebar

	// "Draw" Sidebar
	GtkTreeView* m_pLayersListTreeView;
	GtkTreeView* m_pLocationSetsTreeView;
	GtkNotebook* m_pSidebarNotebook;

	// "GPS" sidebar
	GtkLabel* m_pSpeedLabel;
	GtkProgressBar* m_pGPSSignalStrengthProgressBar;

	// Statusbar
	GtkVBox*  m_pStatusbar;
 	GtkLabel* m_pPositionLabel;
	GtkLabel* m_pZoomScaleLabel;
	GtkProgressBar* m_pProgressBar;

	// Boxes
	GtkHBox* m_pContentBox;

	// Drawing area
	GtkDrawingArea* m_pDrawingArea;

	map_t* m_pMap;

	EToolType m_eSelectedTool;

	gboolean m_bScrolling;
	EDirection m_eScrollDirection;
	
	gboolean m_bMouseDragging;
	gboolean m_bMouseDragMovement;
	screenpoint_t m_ptClickLocation;

	gint m_nCurrentGPSPath;
	gint m_nGPSLocationGlyph;
	gint m_nDrawPrettyTimeoutID;
	gint m_nScrollTimeoutID;
} g_MainWindow = {0};


// Data
toolsettings_t g_Tools[] = {
	{"Pointer Tool", {GDK_LEFT_PTR, NULL}},
	{"Zoom Tool", {GDK_CIRCLE, NULL}},
};
void cursor_init()
{
	int i;
	for(i=0 ; i<NUM_ELEMS(g_Tools) ; i++) {
		g_Tools[i].m_Cursor.m_pGdkCursor = gdk_cursor_new(g_Tools[i].m_Cursor.m_CursorType);
	}
}

static void util_set_image_to_stock(GtkImage* pImage, gchar* pszStockIconID, GtkIconSize nSize)
{
	GdkPixbuf* pPixbuf = gtk_widget_render_icon(GTK_WIDGET(g_MainWindow.m_pStatusbarGPSIcon),pszStockIconID, nSize, "name");
	gtk_image_set_from_pixbuf(pImage, pPixbuf);
	g_object_unref(pPixbuf);
}

static void mainwindow_set_statusbar_position(gchar* pMessage)
{
	gtk_label_set_text(GTK_LABEL(g_MainWindow.m_pPositionLabel), pMessage);
}

static void mainwindow_set_statusbar_zoomscale(gchar* pMessage)
{
	gtk_label_set_text(GTK_LABEL(g_MainWindow.m_pZoomScaleLabel), pMessage);
}

void* mainwindow_set_busy(void)
{
	GdkCursor* pCursor = gdk_cursor_new(GDK_WATCH);
	// set it for the whole window
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pWindow)->window, pCursor);

	// HACK: children with their own cursors set need to be set manually and restored later :/
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);

	gdk_flush();
	return pCursor;
}

void mainwindow_set_not_busy(void** ppCursor)
{
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pWindow)->window, NULL);
	gdk_cursor_unref(*ppCursor);
	gdk_flush();

	// HACK: manually restore the cursor for selected tool :/
	mainwindow_setup_selected_tool();
}

/*
** Status bar
*/


void mainwindow_init(GladeXML* pGladeXML)
{
	g_MainWindow.m_pWindow				= GTK_WINDOW(glade_xml_get_widget(pGladeXML, "mainwindow"));			g_return_if_fail(g_MainWindow.m_pWindow != NULL);
	g_MainWindow.m_pMapPopupMenu		= GTK_MENU(glade_xml_get_widget(pGladeXML, "mappopupmenu"));			g_return_if_fail(g_MainWindow.m_pMapPopupMenu != NULL);
	g_MainWindow.m_pPointerToolButton 	= GTK_TOOL_BUTTON(glade_xml_get_widget(pGladeXML, "pointertoolbutton"));g_return_if_fail(g_MainWindow.m_pPointerToolButton != NULL);
	g_MainWindow.m_pZoomToolButton 		= GTK_TOOL_BUTTON(glade_xml_get_widget(pGladeXML, "zoomtoolbutton")); 	g_return_if_fail(g_MainWindow.m_pZoomToolButton != NULL);
	g_MainWindow.m_pContentBox 			= GTK_HBOX(glade_xml_get_widget(pGladeXML, "mainwindowcontentsbox"));	g_return_if_fail(g_MainWindow.m_pContentBox != NULL);
	g_MainWindow.m_pLocationSetsTreeView= GTK_TREE_VIEW(glade_xml_get_widget(pGladeXML, "locationsetstreeview"));	g_return_if_fail(g_MainWindow.m_pLocationSetsTreeView != NULL);
	g_MainWindow.m_pLayersListTreeView	= GTK_TREE_VIEW(glade_xml_get_widget(pGladeXML, "layerstreeview"));		g_return_if_fail(g_MainWindow.m_pLayersListTreeView != NULL);
	g_MainWindow.m_pZoomScale			= GTK_HSCALE(glade_xml_get_widget(pGladeXML, "zoomscale"));				g_return_if_fail(g_MainWindow.m_pZoomScale != NULL);
	g_MainWindow.m_pPositionLabel 		= GTK_LABEL(glade_xml_get_widget(pGladeXML, "positionlabel"));			g_return_if_fail(g_MainWindow.m_pPositionLabel != NULL);
	g_MainWindow.m_pStatusbarGPSIcon 	= GTK_IMAGE(glade_xml_get_widget(pGladeXML, "statusbargpsicon"));		g_return_if_fail(g_MainWindow.m_pStatusbarGPSIcon != NULL);
	g_MainWindow.m_pZoomScaleLabel 		= GTK_LABEL(glade_xml_get_widget(pGladeXML, "zoomscalelabel"));			g_return_if_fail(g_MainWindow.m_pZoomScaleLabel != NULL);
	g_MainWindow.m_pToolbar				= GTK_TOOLBAR(glade_xml_get_widget(pGladeXML, "maintoolbar"));			g_return_if_fail(g_MainWindow.m_pToolbar != NULL);
	g_MainWindow.m_pStatusbar			= GTK_VBOX(glade_xml_get_widget(pGladeXML, "statusbar"));				g_return_if_fail(g_MainWindow.m_pStatusbar != NULL);
	g_MainWindow.m_pSidebox				= GTK_WIDGET(glade_xml_get_widget(pGladeXML, "mainwindowsidebox"));		g_return_if_fail(g_MainWindow.m_pSidebox != NULL);
	g_MainWindow.m_pSidebarNotebook			= GTK_NOTEBOOK(glade_xml_get_widget(pGladeXML, "sidebarnotebook"));		g_return_if_fail(g_MainWindow.m_pSidebarNotebook != NULL);
//	g_MainWindow.m_pSearchBox			= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "searchbox"));		g_return_if_fail(g_MainWindow.m_pSearchBox != NULL);
//	g_MainWindow.m_pProgressBar			= GTK_PROGRESS_BAR(glade_xml_get_widget(pGladeXML, "mainwindowprogressbar"));		g_return_if_fail(g_MainWindow.m_pProgressBar != NULL);
	g_MainWindow.m_pTooltips			= gtk_tooltips_new();
	g_MainWindow.m_pSpeedLabel			= GTK_LABEL(glade_xml_get_widget(pGladeXML, "speedlabel"));		g_return_if_fail(g_MainWindow.m_pSpeedLabel != NULL);
	g_MainWindow.m_pGPSSignalStrengthProgressBar = GTK_PROGRESS_BAR(glade_xml_get_widget(pGladeXML, "gpssignalprogressbar"));		g_return_if_fail(g_MainWindow.m_pGPSSignalStrengthProgressBar != NULL);
	
	// create drawing area
	g_MainWindow.m_pDrawingArea = GTK_DRAWING_AREA(gtk_drawing_area_new());
	// create map
	map_new(&g_MainWindow.m_pMap, GTK_WIDGET(g_MainWindow.m_pDrawingArea));

	// add signal handlers to drawing area
	gtk_widget_add_events(GTK_WIDGET(g_MainWindow.m_pDrawingArea), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	g_signal_connect(G_OBJECT(g_MainWindow.m_pDrawingArea), "expose_event", G_CALLBACK(mainwindow_on_expose_event), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.m_pDrawingArea), "configure_event", G_CALLBACK(mainwindow_on_configure_event), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.m_pDrawingArea), "button_press_event", G_CALLBACK(mainwindow_on_mouse_button_click), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.m_pDrawingArea), "button_release_event", G_CALLBACK(mainwindow_on_mouse_button_click), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.m_pDrawingArea), "motion_notify_event", G_CALLBACK(mainwindow_on_mouse_motion), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.m_pDrawingArea), "scroll_event", G_CALLBACK(mainwindow_on_mouse_scroll), NULL);

	// Pack canvas into application window
	gtk_box_pack_end(GTK_BOX(g_MainWindow.m_pContentBox), GTK_WIDGET(g_MainWindow.m_pDrawingArea),
					TRUE, // expand
					TRUE, // fill
					0);
	gtk_widget_show(GTK_WIDGET(g_MainWindow.m_pDrawingArea));

	cursor_init();

	g_MainWindow.m_nGPSLocationGlyph = glyph_load(PACKAGE_DATA_DIR"/car.svg");

	/*
	**
	*/
	// create layers tree view
	GtkListStore* pLocationSetsListStore = gtk_list_store_new(2, G_TYPE_BOOLEAN, G_TYPE_STRING);
	gtk_tree_view_set_model(g_MainWindow.m_pLocationSetsTreeView, GTK_TREE_MODEL(pLocationSetsListStore));

	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

	// add a checkbox column for layer enabled/disabled
	pCellRenderer = gtk_cell_renderer_toggle_new();
  	g_object_set_data(G_OBJECT(pCellRenderer), "column", (gint *)LAYERLIST_COLUMN_ENABLED);
//	g_signal_connect(pCellRenderer, "toggled", G_CALLBACK(on_layervisible_checkbox_clicked), pLocationSetsListStore);
	pColumn = gtk_tree_view_column_new_with_attributes("Visible", pCellRenderer, "active", LAYERLIST_COLUMN_ENABLED, NULL);	
	gtk_tree_view_append_column(g_MainWindow.m_pLocationSetsTreeView, pColumn);

	// add Name column (with a text renderer)
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Layers", pCellRenderer, "text", LAYERLIST_COLUMN_NAME, NULL);	
	gtk_tree_view_append_column(g_MainWindow.m_pLocationSetsTreeView, pColumn);

	/*
	**
	*/
	// create layers tree view
	GtkListStore* pLayersListStore = gtk_list_store_new(2, G_TYPE_BOOLEAN, G_TYPE_STRING);
	gtk_tree_view_set_model(g_MainWindow.m_pLayersListTreeView, GTK_TREE_MODEL(pLayersListStore));

//	GtkCellRenderer* pCellRenderer;
//  	GtkTreeViewColumn* pColumn;

	// add a checkbox column for layer enabled/disabled
	pCellRenderer = gtk_cell_renderer_toggle_new();
  	g_object_set_data (G_OBJECT (pCellRenderer ), "column", (gint *)LAYERLIST_COLUMN_ENABLED);
//	g_signal_connect (pCellRenderer, "toggled", G_CALLBACK (on_layervisible_checkbox_clicked), pLayersListStore);
	pColumn = gtk_tree_view_column_new_with_attributes("Visible", pCellRenderer, "active", LAYERLIST_COLUMN_ENABLED, NULL);	
	gtk_tree_view_append_column(g_MainWindow.m_pLayersListTreeView, pColumn);

	// add Layer Name column (with a text renderer)
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Layers", pCellRenderer, "text", LAYERLIST_COLUMN_NAME, NULL);	
	gtk_tree_view_append_column(g_MainWindow.m_pLayersListTreeView, pColumn);

	/*
	**
	*/
	mainwindow_statusbar_update_zoomscale();
	mainwindow_statusbar_update_position();	

//	locationset_load_locationsets();
//	mainwindow_load_locationset_list();
	
	/* add some data to the layers list */
//         GtkTreeIter iter;
//
//         int i;
//         for(i=LAYER_FIRST ; i<=LAYER_LAST ; i++) {
//                 gboolean bEnabled = TRUE;
//
//                 gtk_list_store_append(GTK_LIST_STORE(pLayersListStore), &iter);
//                 gtk_list_store_set(GTK_LIST_STORE(pLayersListStore), &iter,
//                         LAYERLIST_COLUMN_ENABLED, bEnabled,
//                         LAYERLIST_COLUMN_NAME, g_aLayers[i].m_pszName,
//                         -1);
//         }

	g_timeout_add(TIMER_GPS_REDRAW_INTERVAL_MS,
			  (GSourceFunc)mainwindow_callback_on_gps_redraw_timeout,
			  (gpointer)NULL);

	// give it a call to init everything
	mainwindow_callback_on_gps_redraw_timeout(NULL);
}


void mainwindow_show(void)
{
	gtk_widget_show(GTK_WIDGET(g_MainWindow.m_pWindow));
	gtk_window_present(g_MainWindow.m_pWindow);
}

void mainwindow_hide(void)
{
	gtk_widget_hide(GTK_WIDGET(g_MainWindow.m_pWindow));
}

void mainwindow_set_sensitive(gboolean bSensitive)
{
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.m_pWindow), bSensitive);
}

//
// the "draw pretty" timeout lets us draw ugly/fast graphics while moving, then redraw pretty after we stop
//
gboolean mainwindow_on_draw_pretty_timeout(gpointer _unused)
{
	g_MainWindow.m_nDrawPrettyTimeoutID = 0;
	mainwindow_draw_map(DRAWFLAG_ALL);
	return FALSE;
}

void mainwindow_cancel_draw_pretty_timeout()
{
	if(g_MainWindow.m_nDrawPrettyTimeoutID != 0) {
		g_source_remove(g_MainWindow.m_nDrawPrettyTimeoutID);
	}
}

void mainwindow_set_draw_pretty_timeout()
{
	// cancel existing one, if one exists
	mainwindow_cancel_draw_pretty_timeout();

	g_MainWindow.m_nDrawPrettyTimeoutID = g_timeout_add(DRAW_PRETTY_TIMEOUT_MS, mainwindow_on_draw_pretty_timeout, NULL);
	g_assert(g_MainWindow.m_nDrawPrettyTimeoutID != 0);
}


//
// the "scroll" timeout
//
void mainwindow_scroll_direction(EDirection eScrollDirection, gint nPixels)
{
	if(eScrollDirection != DIRECTION_NONE) {
		gint nWidth = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.width;
		gint nHeight = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.height;

		gint nDeltaX = nPixels * g_aDirectionMultipliers[eScrollDirection].m_nX;
		gint nDeltaY = nPixels * g_aDirectionMultipliers[eScrollDirection].m_nY;

		mainwindow_map_center_on_windowpoint((nWidth / 2) + nDeltaX, (nHeight / 2) + nDeltaY);
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout();
	}
}

gboolean mainwindow_on_scroll_timeout(gpointer _unused)
{
	mainwindow_scroll_direction(g_MainWindow.m_eScrollDirection, SCROLL_DISTANCE_IN_PIXELS);
	return TRUE;	// more events, please
}
void mainwindow_cancel_scroll_timeout()
{
	if(g_MainWindow.m_nScrollTimeoutID != 0) {
		g_source_remove(g_MainWindow.m_nScrollTimeoutID);
	}
}
void mainwindow_set_scroll_timeout()
{
	// cancel existing one, if one exists
	mainwindow_cancel_scroll_timeout();

	g_MainWindow.m_nScrollTimeoutID = g_timeout_add(SCROLL_TIMEOUT_MS, mainwindow_on_scroll_timeout, NULL);
	g_assert(g_MainWindow.m_nScrollTimeoutID != 0);
}


/*
** Toolbar
*/
void mainwindow_set_toolbar_visible(gboolean bVisible)
{
	if(bVisible) {
		gtk_widget_show(GTK_WIDGET(g_MainWindow.m_pToolbar));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(g_MainWindow.m_pToolbar));
	}
}

gboolean mainwindow_get_toolbar_visible(void)
{
	return GTK_WIDGET_VISIBLE(g_MainWindow.m_pToolbar);
}

/*
** Statusbar
*/
void mainwindow_set_statusbar_visible(gboolean bVisible)
{
	if(bVisible) {
		gtk_widget_show(GTK_WIDGET(g_MainWindow.m_pStatusbar));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(g_MainWindow.m_pStatusbar));
	}
}

gboolean mainwindow_get_statusbar_visible(void)
{
	return GTK_WIDGET_VISIBLE(g_MainWindow.m_pStatusbar);
}

void mainwindow_statusbar_update_zoomscale(void)
{
	char buf[200];
	guint32 uZoomLevelScale = map_get_zoomlevel_scale(g_MainWindow.m_pMap);

	snprintf(buf, 199, "1:%d", uZoomLevelScale);
	mainwindow_set_statusbar_zoomscale(buf);
	GTK_PROCESS_MAINLOOP;
}

void mainwindow_statusbar_update_position(void)
{
	char buf[200];
	mappoint_t pt;
	map_get_centerpoint(g_MainWindow.m_pMap, &pt);
	g_snprintf(buf, 200, "Lat: %.5f, Lon: %.5f", pt.m_fLatitude, pt.m_fLongitude);
	mainwindow_set_statusbar_position(buf);
	GTK_PROCESS_MAINLOOP;
}

/*
** Sidebox (Layers, etc.)
*/
void mainwindow_set_sidebox_visible(gboolean bVisible)
{
	if(bVisible) {
		gtk_widget_show(GTK_WIDGET(g_MainWindow.m_pSidebox));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(g_MainWindow.m_pSidebox));
	}
}

gboolean mainwindow_get_sidebox_visible(void)
{
	return GTK_WIDGET_VISIBLE(g_MainWindow.m_pSidebox);
}

GtkWidget* mainwindow_get_window(void)
{
	return GTK_WIDGET(g_MainWindow.m_pWindow);
}

void mainwindow_toggle_fullscreen(void)
{
    GdkWindow* toplevelgdk = gdk_window_get_toplevel(GTK_WIDGET(g_MainWindow.m_pWindow)->window);
	if(toplevelgdk) {
		GdkWindowState windowstate = gdk_window_get_state(toplevelgdk);
		if(windowstate & GDK_WINDOW_STATE_FULLSCREEN) {
			gdk_window_unfullscreen(toplevelgdk);
		}
		else {
			gdk_window_fullscreen(toplevelgdk);
		}
	}
}

// User clicked Quit window
void mainwindow_on_quitmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gui_exit();
}

// User closed main window
gboolean mainwindow_on_application_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gui_exit();
	return FALSE; // satisfy strick compiler
}

// the range slider changed value
void mainwindow_on_zoomscale_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble fValue = gtk_range_get_value(range);
	gint16 nValue = (gint16)fValue;
	gtk_range_set_value(range, (gdouble)nValue);

	map_set_zoomlevel(g_MainWindow.m_pMap, nValue);
	mainwindow_statusbar_update_zoomscale();

	mainwindow_draw_map(DRAWFLAG_GEOMETRY);
	mainwindow_set_draw_pretty_timeout();
}

//
// Zoom
//
void mainwindow_set_zoomlevel(gint nZoomLevel)
{
	// set zoomlevel scale but prevent it from calling handler (mainwindow_on_zoomscale_value_changed)
        g_signal_handlers_block_by_func(g_MainWindow.m_pZoomScale, mainwindow_on_zoomscale_value_changed, NULL);
	gtk_range_set_value(GTK_RANGE(g_MainWindow.m_pZoomScale), nZoomLevel);
	g_signal_handlers_unblock_by_func(g_MainWindow.m_pZoomScale, mainwindow_on_zoomscale_value_changed, NULL);
}

static void zoom_in_one(void)
{
	// tell the map
	map_set_zoomlevel(g_MainWindow.m_pMap, map_get_zoomlevel(g_MainWindow.m_pMap) + 1);
	// tell the GUI
	mainwindow_set_zoomlevel(map_get_zoomlevel(g_MainWindow.m_pMap));

	// NOTE: doesn't trigger an actual redraw
}

static void zoom_out_one(void)
{
	map_set_zoomlevel(g_MainWindow.m_pMap, map_get_zoomlevel(g_MainWindow.m_pMap) - 1 );
	mainwindow_set_zoomlevel(map_get_zoomlevel(g_MainWindow.m_pMap));

	// NOTE: doesn't trigger an actual redraw
}

//
//
//
static void gui_set_tool(EToolType eTool)
{
	g_MainWindow.m_eSelectedTool = eTool;
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, g_Tools[eTool].m_Cursor.m_pGdkCursor);
}

void mainwindow_on_aboutmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	const gchar *ppAuthors[] = {
		"Ian McIntosh <ian_mcintosh@linuxadvocate.org>",
		"Nathan Fredrickson <nathan@silverorange.com>",
		NULL
	};

  	GtkWidget *pAboutWindow = gnome_about_new(
	  				PROGRAM_NAME,
	  				VERSION,
					PROGRAM_COPYRIGHT,
					PROGRAM_DESCRIPTION,
	  				(const gchar **) ppAuthors,
					NULL,
	 				NULL,
					NULL);
	gtk_widget_show(pAboutWindow);
}

// Toggle toolbar visibility
void mainwindow_on_toolbarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	mainwindow_set_toolbar_visible( !mainwindow_get_toolbar_visible() );
}

// Toggle statusbar visibility
void mainwindow_on_statusbarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	mainwindow_set_statusbar_visible( !mainwindow_get_statusbar_visible() );
}

void mainwindow_on_sidebarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	mainwindow_set_sidebox_visible(!mainwindow_get_sidebox_visible());
}

// Zoom buttons / menu items (shared callbacks)
void mainwindow_on_zoomin_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	zoom_in_one();
	mainwindow_draw_map(DRAWFLAG_ALL);
}

void mainwindow_on_zoomout_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	zoom_out_one();
	mainwindow_draw_map(DRAWFLAG_ALL);
}

void mainwindow_on_fullscreenmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	mainwindow_toggle_fullscreen();
}

void mainwindow_on_gotomenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gotowindow_show();
}

void mainwindow_on_reloadstylesmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	layers_reload();
	mainwindow_draw_map(DRAWFLAG_ALL);
}

EDirection match_border(gint nX, gint nY, gint nWidth, gint nHeight, gint nBorderSize)
{
	EDirection eDirection;

	// LEFT EDGE?
	if(nX <= nBorderSize) {
		if(nY <= nBorderSize) {
			eDirection = DIRECTION_NW;
		}
		else if((nY+nBorderSize) >= nHeight) {
			eDirection = DIRECTION_SW;
		}
		else {
			eDirection = DIRECTION_W;
		}
	}
	// RIGHT EDGE?
	else if((nX+nBorderSize) >= nWidth) {
		if(nY <= BORDER_SCROLL_CLICK_TARGET_SIZE) {
			eDirection = DIRECTION_NE;
		}
		else if((nY+nBorderSize) >= nHeight) {
			eDirection = DIRECTION_SE;
		}
		else {
			eDirection = DIRECTION_E;
		}
	}
	// TOP?
	else if(nY <= nBorderSize) {
		eDirection = DIRECTION_N;
	}
	// BOTTOM?
	else if((nY+nBorderSize) >= nHeight) {
		eDirection = DIRECTION_S;
	}
	// center.
	else {
		eDirection = DIRECTION_NONE;
	}
	return eDirection;
}

static gboolean mainwindow_on_mouse_button_click(GtkWidget* w, GdkEventButton *event)
{
	gint nX;
	gint nY;

	gdk_window_get_pointer(w->window, &nX, &nY, NULL);

	gint nWidth = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.width;
	gint nHeight = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.height;
	EDirection eScrollDirection = DIRECTION_NONE;

	if(event->button == 1) {
		// Left mouse button down?
		if(event->type == GDK_BUTTON_PRESS) {

			// Is it at a border?
			eScrollDirection = match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);
			if(eScrollDirection != DIRECTION_NONE) {
				// begin a scroll
				
				//GdkCursor* pCursor = gdk_cursor_new(g_aDirectionCursors[eScrollDirection].m_nCursor);
				//if(GDK_GRAB_SUCCESS == gdk_pointer_grab(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, FALSE, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_RELEASE_MASK, NULL, pCursor, GDK_CURRENT_TIME)) {
				GdkCursor* pCursor = gdk_cursor_new(g_aDirectionCursors[eScrollDirection].m_nCursor);
				gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);
				gdk_cursor_unref(pCursor);

				g_MainWindow.m_bScrolling = TRUE;
				g_MainWindow.m_eScrollDirection = eScrollDirection;

				mainwindow_scroll_direction(g_MainWindow.m_eScrollDirection, SCROLL_DISTANCE_IN_PIXELS);
				mainwindow_set_scroll_timeout();
				//}
				//gdk_cursor_unref(pCursor);

			}
			else {
				// else begin a drag
//                                 GdkCursor* pCursor = gdk_cursor_new(GDK_HAND2);
//                                 if(GDK_GRAB_SUCCESS == gdk_pointer_grab(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, FALSE, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_RELEASE_MASK, NULL, pCursor, GDK_CURRENT_TIME)) {
//                                 GdkCursor* pCursor = gdk_cursor_new(GDK_FLEUR);
//                                 gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);
//                                 gdk_cursor_unref(pCursor);

				g_MainWindow.m_bMouseDragging = TRUE;
				g_MainWindow.m_bMouseDragMovement = FALSE;
				g_MainWindow.m_ptClickLocation.m_nX = nX;
				g_MainWindow.m_ptClickLocation.m_nY = nY;
//                                 }
//                                 gdk_cursor_unref(pCursor);
			}
		}
		// Left mouse button up?
		else if(event->type == GDK_BUTTON_RELEASE) {
			// restore cursor
			GdkCursor* pCursor = gdk_cursor_new(GDK_LEFT_PTR);
			gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);
			gdk_cursor_unref(pCursor);

			// end mouse dragging, if active
			if(g_MainWindow.m_bMouseDragging == TRUE) {
				g_MainWindow.m_bMouseDragging = FALSE;
				if(g_MainWindow.m_bMouseDragMovement) {
					mainwindow_cancel_draw_pretty_timeout();
					mainwindow_draw_map(DRAWFLAG_ALL);
				}
			}

			// end scrolling, if active
			if(g_MainWindow.m_bScrolling == TRUE) {
				g_MainWindow.m_bScrolling = FALSE;
				g_MainWindow.m_eScrollDirection = DIRECTION_NONE;
//                                 gdk_pointer_ungrab(GDK_CURRENT_TIME);

				mainwindow_cancel_draw_pretty_timeout();
				mainwindow_draw_map(DRAWFLAG_ALL);
			}
		}
		else if(event->type == GDK_2BUTTON_PRESS) {
			mainwindow_map_center_on_windowpoint(nX, nY);
			mainwindow_draw_map(DRAWFLAG_ALL);
		}
	}
	// Right-click?
//         else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
//         {
//                 // Save click location for use by callback
//                 g_MainWindow.m_ptClickLocation.m_nX = nX;
//                 g_MainWindow.m_ptClickLocation.m_nY = nY;
//
//                 // Show popup!
//                 gtk_menu_popup(g_MainWindow.m_pMapPopupMenu, NULL, NULL, NULL, NULL, event->button, event->time);
//                 return TRUE;
//         }
	//	map_redraw_if_needed();

	return TRUE;
}

static gboolean mainwindow_on_mouse_motion(GtkWidget* w, GdkEventMotion *event)
{
        gint nX,nY;
	gdk_window_get_pointer(w->window, &nX, &nY, NULL);

	gint nWidth = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.width;
	gint nHeight = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.height;

	if(g_MainWindow.m_bMouseDragging) {
		g_MainWindow.m_bMouseDragMovement = TRUE;

		// Set it here and no when first clicking because now we know it's a drag (on click it could be a double-click)
		GdkCursor* pCursor = gdk_cursor_new(GDK_FLEUR);
		gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);
		gdk_cursor_unref(pCursor);

		gint nDeltaX = g_MainWindow.m_ptClickLocation.m_nX - nX;
                gint nDeltaY = g_MainWindow.m_ptClickLocation.m_nY - nY;

		if(nDeltaX == 0 && nDeltaY == 0) return TRUE;

		mainwindow_map_center_on_windowpoint((nWidth / 2) + nDeltaX, (nHeight / 2) + nDeltaY);
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout();

		g_MainWindow.m_ptClickLocation.m_nX = nX;
		g_MainWindow.m_ptClickLocation.m_nY = nY;
	}
	// set appropriate mouse cursor whether scrolling or not
	else if(g_MainWindow.m_bScrolling) {
		// just set the cursor the window

		EDirection eScrollDirection = match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);

		if(g_MainWindow.m_eScrollDirection != eScrollDirection) {
			// update cursor
			GdkCursor* pCursor = gdk_cursor_new(g_aDirectionCursors[eScrollDirection].m_nCursor);
			gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);
			gdk_cursor_unref(pCursor);

			// update direction if actively scrolling
			g_MainWindow.m_eScrollDirection = eScrollDirection;

			//GdkCursor* pCursor = gdk_cursor_new(g_aDirectionCursors[eScrollDirection].m_nCursor);
			//gdk_pointer_ungrab(GDK_CURRENT_TIME);
			//gdk_pointer_grab(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, FALSE, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_RELEASE_MASK, NULL, pCursor, GDK_CURRENT_TIME);
			//gdk_cursor_unref(pCursor);
		}
	}
	else {
		EDirection eScrollDirection = match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);

		// just set cursor based on what we're hovering over
		GdkCursor* pCursor = gdk_cursor_new(g_aDirectionCursors[eScrollDirection].m_nCursor);
		gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window, pCursor);
		gdk_cursor_unref(pCursor);
	}
	return FALSE;
}

static gboolean mainwindow_on_mouse_scroll(GtkWidget* w, GdkEventScroll *event)
{
	// respond to scroll wheel events by zooming in and out
	if(event->direction == GDK_SCROLL_UP) {
		zoom_in_one();
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout();
	}
	else if(event->direction == GDK_SCROLL_DOWN) {
		zoom_out_one();
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout();
	}
}

static void mainwindow_begin_import_geography_data(void)
{
	GtkWidget* pDialog = gtk_file_chooser_dialog_new(
				"Select Map Data for Import",
                g_MainWindow.m_pWindow,
                GTK_FILE_CHOOSER_ACTION_OPEN,
    			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				"Import", GTK_RESPONSE_ACCEPT,
				NULL);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(pDialog), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.m_pWindow), FALSE);
	
	gint nResponse = gtk_dialog_run(GTK_DIALOG(pDialog));
	gtk_widget_hide(pDialog);

	if(nResponse == GTK_RESPONSE_ACCEPT) {
		GSList* pSelectedFilesList = gtk_file_chooser_get_uris(GTK_FILE_CHOOSER(pDialog));

		importwindow_begin(pSelectedFilesList);

		// free each URI item with g_free
		GSList* pFile = pSelectedFilesList;
		while(pFile != NULL) {
			g_free(pFile->data); pFile->data = NULL;
			pFile = pFile->next;	// Move to next file
		}
		g_slist_free(pSelectedFilesList);
	}
	// re-enable main window and destroy dialog
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.m_pWindow), TRUE);
	gtk_widget_destroy(pDialog);
}

void mainwindow_on_import_maps_activate(GtkWidget *widget, gpointer user_data)
{
	mainwindow_begin_import_geography_data();
}

static void mainwindow_setup_selected_tool(void)
{
	if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(g_MainWindow.m_pPointerToolButton))) {
		gui_set_tool(kToolPointer);
	}
	else if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(g_MainWindow.m_pZoomToolButton))) {
		gui_set_tool(kToolZoom);
	}
}

// Handler for ALL tool buttons
void mainwindow_on_toolbutton_clicked(GtkToolButton *toolbutton, gpointer user_data)
{
	mainwindow_setup_selected_tool();
}

void mainwindow_draw_map(gint nDrawFlags)
{
	map_draw(g_MainWindow.m_pMap, nDrawFlags);

	// push it to screen
	GdkPixmap* pMapPixmap = map_get_pixmap(g_MainWindow.m_pMap);
	gdk_draw_drawable(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window,
		      GTK_WIDGET(g_MainWindow.m_pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.m_pDrawingArea)],
		      pMapPixmap,
		      0,0,
		      0,0,
		      GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.width, GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.height);
	map_release_pixmap(g_MainWindow.m_pMap);
}

static gint mainwindow_on_configure_event(GtkWidget *pDrawingArea, GdkEventConfigure *event)
{
	// tell the map how big to draw
	dimensions_t dim;
	dim.m_uWidth = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.width;
	dim.m_uHeight = GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.height;
	map_set_dimensions(g_MainWindow.m_pMap, &dim);

	mainwindow_draw_map(DRAWFLAG_GEOMETRY);
	mainwindow_set_draw_pretty_timeout();
	return TRUE;
}

static gboolean mainwindow_on_expose_event(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data)
{
	GdkPixmap* pMapPixmap = map_get_pixmap(g_MainWindow.m_pMap);

	// Copy relevant portion of off-screen bitmap to window
	gdk_draw_drawable(GTK_WIDGET(g_MainWindow.m_pDrawingArea)->window,
                      GTK_WIDGET(g_MainWindow.m_pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.m_pDrawingArea)],
                      pMapPixmap,
                      event->area.x, event->area.y,
                      event->area.x, event->area.y,
                      event->area.width, event->area.height);

	map_release_pixmap(g_MainWindow.m_pMap);
	return FALSE;
}


static gboolean mainwindow_callback_on_gps_redraw_timeout(gpointer __unused)
{
	// NOTE: we're setting tooltips on the image's
	GtkWidget* pWidget = gtk_widget_get_parent(GTK_WIDGET(g_MainWindow.m_pStatusbarGPSIcon));

	const gpsdata_t* pData = gpsclient_getdata();
	if(pData->m_eStatus == GPS_STATUS_LIVE) {

		if(g_MainWindow.m_nCurrentGPSPath == 0) {
			// create a new track for GPS trail
			g_MainWindow.m_nCurrentGPSPath = track_new();
		}

		track_add_point(g_MainWindow.m_nCurrentGPSPath, &pData->m_ptPosition);

		// if(keep position centered) {
//         map_center_on_worldpoint(pData->m_ptPosition.m_fLatitude, pData->m_ptPosition.m_fLongitude);
//         mainwindow_statusbar_update_position();
		// }

		// redraw because GPS icon/trail may be different
		mainwindow_draw_map(DRAWFLAG_ALL);

		// update image and tooltip for GPS icon
		util_set_image_to_stock(g_MainWindow.m_pStatusbarGPSIcon, GTK_STOCK_OK, GTK_ICON_SIZE_MENU);
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.m_pTooltips), pWidget,
				 "A GPS device is present and active", "");

		// update speed
		gchar* pszSpeed = g_strdup_printf(SPEED_LABEL_FORMAT, pData->m_fSpeedInMilesPerHour);
		gtk_label_set_markup(g_MainWindow.m_pSpeedLabel, pszSpeed);
		g_free(pszSpeed);

		gtk_progress_bar_set_fraction(g_MainWindow.m_pGPSSignalStrengthProgressBar, pData->m_fSignalQuality * 20.0/100.0);
	}
	else {
		// unless we're "LIVE", set signal strength to 0
		gtk_progress_bar_set_fraction(g_MainWindow.m_pGPSSignalStrengthProgressBar, 0.0);

		if(pData->m_eStatus == GPS_STATUS_NO_SIGNAL) {
			// do NOT set speed to 0 if we drop the signal temporarily...

			util_set_image_to_stock(g_MainWindow.m_pStatusbarGPSIcon, GTK_STOCK_CANCEL, GTK_ICON_SIZE_MENU);

			gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.m_pTooltips), pWidget,
					 "A GPS device is present but unable to hear satellites", "");
		}
		else {
			// update speed, set to 0
			gchar* pszSpeed = g_strdup_printf(SPEED_LABEL_FORMAT, 0.0);
			gtk_label_set_markup(g_MainWindow.m_pSpeedLabel, pszSpeed);
			g_free(pszSpeed);

			if(pData->m_eStatus == GPS_STATUS_NO_DEVICE) {
				util_set_image_to_stock(g_MainWindow.m_pStatusbarGPSIcon, GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
				gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.m_pTooltips), pWidget,
						 "No GPS device is present", "");
			}
			else if(pData->m_eStatus == GPS_STATUS_NO_GPSD) {
				util_set_image_to_stock(g_MainWindow.m_pStatusbarGPSIcon, GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
				gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.m_pTooltips), pWidget,
						 "Install package 'gpsd' to use a GPS device with Roadster", "");
			}
			else {
				g_assert_not_reached();
			}
		}
	}
	return TRUE;
}

void mainwindow_map_center_on_windowpoint(gint nX, gint nY)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = nX - (GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.width / 2);
	gint16 nPixelDeltaY = nY - (GTK_WIDGET(g_MainWindow.m_pDrawingArea)->allocation.height / 2);

	// Convert pixels to world coordinates
	gint nZoomLevel = map_get_zoomlevel(g_MainWindow.m_pMap);
	double fWorldDeltaX = map_pixels_to_degrees(g_MainWindow.m_pMap, nPixelDeltaX, nZoomLevel);
	// reverse the X, clicking above
	double fWorldDeltaY = -map_pixels_to_degrees(g_MainWindow.m_pMap, nPixelDeltaY, nZoomLevel);

	mappoint_t pt;
	map_get_centerpoint(g_MainWindow.m_pMap, &pt);

	pt.m_fLatitude += fWorldDeltaY;
	pt.m_fLongitude += fWorldDeltaX;
	map_set_centerpoint(g_MainWindow.m_pMap, &pt);

	mainwindow_statusbar_update_position();
}

void mainwindow_map_center_on_mappoint(mappoint_t* pPoint)
{
	g_assert(pPoint != NULL);

	map_set_centerpoint(g_MainWindow.m_pMap, pPoint);

	mainwindow_statusbar_update_position();
}

void mainwindow_get_centerpoint(mappoint_t* pPoint)
{
	g_assert(pPoint != NULL);

	map_get_centerpoint(g_MainWindow.m_pMap, pPoint);
}

void mainwindow_on_addpointmenuitem_activate(GtkWidget *_unused, gpointer* __unused)
{
	mappoint_t point;
	map_windowpoint_to_mappoint(g_MainWindow.m_pMap, &g_MainWindow.m_ptClickLocation, &point);

	gint nLocationSetID = 1;
	gint nNewLocationID;

	if(locationset_add_location(nLocationSetID, &point, &nNewLocationID)) {
		g_print("new location ID = %d\n", nNewLocationID);
		mainwindow_draw_map(DRAWFLAG_ALL);
	}
	else {
		g_print("insert failed\n");
	}
}

void mainwindow_sidebar_set_tab(gint nTab)
{
	gtk_notebook_set_current_page(g_MainWindow.m_pSidebarNotebook, nTab);
}


#ifdef ROADSTER_DEAD_CODE
/*

void on_importmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	g_print("on_importmenuitem_activate\n");
	importwindow_show();
}

static void on_layervisible_checkbox_clicked(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	gboolean toggle_item;
	gint *column;

	column = g_object_get_data (G_OBJECT (cell), "column");

	// get toggled iter
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, column, &toggle_item, -1);

	// do something with the value
	toggle_item ^= 1;

	// set new value
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, column,
			  toggle_item, -1);

	// clean up
	gtk_tree_path_free (path);
}

void mainwindow_on_datasetmenuitem_activate(GtkWidget *pWidget, gpointer* p)
{
	datasetwindow_show();
}

static gboolean on_searchbox_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	// Enter key?
	if(event->keyval == 65293) {
		// Grab the first MAX_SEARCH_TEXT_LENGTH characters from search box
		gchar* pchSearchString = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, MAX_SEARCH_TEXT_LENGTH);
		search_road_execute(pchSearchString);
		g_free(pchSearchString);
	}
	return FALSE;
}

// Show preferences dialog
static void on_preferencesmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
//	gui_show_preferences_window();
}

void mainwindow_load_locationset_list(void)
{
	const GPtrArray* pLocationSetArray = locationset_get_set_array();

	// add some data to the layers list
	GtkTreeIter iter;

	GtkListStore* pListStore = (GtkListStore*)gtk_tree_view_get_model(g_MainWindow.m_pLocationSetsTreeView);
	g_assert(pListStore != NULL);

	// Add each locationset to treeview
	int i;
	for(i=0 ; i<pLocationSetArray->len ; i++) {
		locationset_t* pLocationSet = g_ptr_array_index(pLocationSetArray, i);

		gboolean bEnabled = TRUE;

		gtk_list_store_append(pListStore, &iter);
		gtk_list_store_set(pListStore, &iter,
			LAYERLIST_COLUMN_ENABLED, bEnabled,
			LAYERLIST_COLUMN_NAME, pLocationSet->m_pszName,
			-1);
	}
}
*/
#endif /* ROADSTER_DEAD_CODE */
