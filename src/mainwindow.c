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
#include <gdk/gdkkeysyms.h>
//#include <gdk/gdkx.h>
//#include <cairo.h>
//#include <cairo-xlib.h>

#include "main.h"
#include "search_road.h"
#include "gui.h"
#include "util.h"
#include "gotowindow.h"
#include "db.h"
#include "map.h"
#include "map_math.h"
#include "map_hittest.h"
#include "map_style.h"
#include "importwindow.h"
#include "locationset.h"
#include "gpsclient.h"
#include "mainwindow.h"
#include "glyph.h"
#include "animator.h"
#include "map_history.h"
#include "mapinfowindow.h"
#include "tooltipwindow.h"
#include "locationeditwindow.h"

#define PROGRAM_NAME			"Roadster"
#define PROGRAM_COPYRIGHT		"Copyright (c) 2005 Ian McIntosh"
#define PROGRAM_DESCRIPTION		"Mapping for everyone!"
#define WEBSITE_URL				"http://linuxadvocate.org/projects/roadster"

//#define ENABLE_MAP_TOOLTIP

#define MAP_STYLE_FILENAME 		("layers.xml")

// how long after stopping various movements should we redraw in high-quality mode
#define DRAW_PRETTY_SCROLL_TIMEOUT_MS	(200)	// NOTE: should be longer than the SCROLL_TIMEOUT_MS below!!
#define DRAW_PRETTY_ZOOM_TIMEOUT_MS		(180)
#define DRAW_PRETTY_DRAG_TIMEOUT_MS		(250)
#define DRAW_PRETTY_RESIZE_TIMEOUT_MS	(500)	// nice and long so keep window resizing smooth

#define SCROLL_TIMEOUT_MS				(130)	// SCROLL_TIMEOUT_MS how often (in MS) to move.
												// A shorter time is good, because it looks smoother. 
												// A longer time is good:
												//  - it gives the user longer to release to do a 'click jump', and
												//  - it reduces the number of frames we try to draw (good for slow computer, and reduces CPU usage on fast computers)  
#define SCROLL_DISTANCE_IN_PIXELS		(110)	// how far to move every (above) MS

#define BORDER_SCROLL_CLICK_TARGET_SIZE	(16)	// the size of the click target (distance from edge of map view) to begin scrolling
#define BORDER_JUMP_PERCENT_OF_SCREEN	(0.65)	// a %. when user *clicks* on eg. left border, jump (width * this) pixels to the left
												// as big as possible while still showing SOME of the old content, so it's not so jarring

#define SLIDE_TIMEOUT_MS				(50)	// time between frames (in MS) for smooth-sliding on double click
#define	SLIDE_TIME_IN_SECONDS			(0.4)	// how long the whole slide should take, in seconds
#define	SLIDE_TIME_IN_SECONDS_AUTO		(0.8)	// time for sliding to search results, etc.

// Layerlist columns
#define LAYERLIST_COLUMN_ENABLED		(0)
#define LAYERLIST_COLUMN_NAME			(1)

// Locationset columns
#define	LOCATIONSETLIST_COLUMN_ENABLED  (0)
#define LOCATIONSETLIST_COLUMN_NAME		(1)
#define LOCATIONSETLIST_COLUMN_ID		(2)
#define LOCATIONSETLIST_COLUMN_COUNT	(3)
#define LOCATIONSETLIST_COLUMN_PIXBUF	(4)

#define ZOOM_TOOL_THRESHOLD (5) // in pixels.  a box less than AxA will be ignored

// Limits
#define MAX_SEARCH_TEXT_LENGTH			(100)
#define SPEED_LABEL_FORMAT				("<span font_desc='32'>%.0f</span>")

#define TOOLTIP_FORMAT					("%s")

#define	ROAD_TOOLTIP_BG_COLOR			65535, 65535, 39000
#define	LOCATION_TOOLTIP_BG_COLOR		52800, 60395, 65535

// Settings
#define TIMER_GPS_REDRAW_INTERVAL_MS	(2500)		// lower this (to 1000?) when it's faster to redraw track

#define	TOOLTIP_OFFSET_X 				(20)	// to the right of the mouse cursor
#define	TOOLTIP_OFFSET_Y 				(0)

#define MAX_DISTANCE_FOR_AUTO_SLIDE_IN_PIXELS	(3500.0)	// when selecting search results, we slide to them instead of jumping if they are within this distance

// Types
// typedef struct {
//     GdkCursorType CursorType;
//     GdkCursor* pGdkCursor;
// } cursor_t;

// typedef struct {
//     char* szName;
//     cursor_t Cursor;
// } toolsettings_t;

// typedef enum {
//     MOUSE_TOOL_POINTER = 0,
//     MOUSE_TOOL_ZOOM = 1,
// } EMouseToolType;

// Prototypes
static void mainwindow_setup_selected_tool(void);
static void mainwindow_map_center_on_windowpoint(gint nX, gint nY);
static void mainwindow_on_web_url_clicked(GtkWidget *_unused, gchar* pszURLPattern);

static gboolean mainwindow_on_window_state_change(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

void mainwindow_update_zoom_buttons();

void mainwindow_on_sidebarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);

static gboolean mainwindow_on_mouse_button_click(GtkWidget* w, GdkEventButton *event);
static gboolean mainwindow_on_mouse_motion(GtkWidget* w, GdkEventMotion *event);
static gboolean mainwindow_on_mouse_scroll(GtkWidget* w, GdkEventScroll *event);
static gboolean mainwindow_on_expose_event(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data);
static gint 	mainwindow_on_configure_event(GtkWidget *pDrawingArea, GdkEventConfigure *event);
static gboolean mainwindow_on_gps_redraw_timeout(gpointer pData);
static gboolean mainwindow_on_slide_timeout(gpointer pData);
static gboolean mainwindow_on_enter_notify(GtkWidget* w, GdkEventCrossing *event);
static gboolean mainwindow_on_leave_notify(GtkWidget* w, GdkEventCrossing *event);
static void 	mainwindow_on_locationset_visible_checkbox_clicked(GtkCellRendererToggle *cell, gchar *path_str, gpointer data);
static gboolean mainwindow_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static gboolean mainwindow_on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

void mainwindow_on_fullscreenmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);

static void mainwindow_configure_locationset_list();
void mainwindow_refresh_locationset_list();

void mainwindow_on_zoomscale_value_changed(GtkRange *range, gpointer user_data);

struct {
	gint nX;
	gint nY;
} g_aDirectionMultipliers[] = {{0,0}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}};

struct {
	gint nCursor;
} g_aDirectionCursors[] = {{GDK_LEFT_PTR}, 
{GDK_TOP_SIDE}, 
{GDK_TOP_RIGHT_CORNER}, 
{GDK_RIGHT_SIDE}, {GDK_BOTTOM_RIGHT_CORNER}, {GDK_BOTTOM_SIDE}, {GDK_BOTTOM_LEFT_CORNER}, {GDK_LEFT_SIDE}, {GDK_TOP_LEFT_CORNER}};

struct {
	GtkWindow* pWindow;
	GtkTooltips* pTooltips;
	GtkMenu* pMapPopupMenu;

	// Toolbar
	GtkHBox* pToolbar;
	GtkRadioButton* pPointerToolRadioButton;
	GtkRadioButton* pZoomToolRadioButton;
	GtkHScale* pZoomScale;
	GtkEntry* pSearchBox;
	GtkImage* pStatusbarGPSIcon;
	GtkWidget* pSidebox;

	GtkButton* pZoomInButton;
	GtkMenuItem* pZoomInMenuItem;
	GtkButton* pZoomOutButton;
	GtkMenuItem* pZoomOutMenuItem;

	// Sidebar

	// "Draw" Sidebar (currently POI)
	GtkTreeView* pLayersListTreeView;
	GtkTreeView* pLocationSetsTreeView;
	GtkListStore* pLocationSetsListStore;
	GtkNotebook* pSidebarNotebook;

	// "GPS" sidebar
	GtkLabel* pSpeedLabel;	// these belong in GPS
	GtkProgressBar* pGPSSignalStrengthProgressBar;

	struct {
		GtkCheckButton* pShowPositionCheckButton;
		GtkCheckButton* pKeepPositionCenteredCheckButton;
		GtkCheckButton* pShowTrailCheckButton;
		GtkCheckButton* pStickToRoadsCheckButton;
	} GPS;

	// Statusbar
	GtkVBox*  pStatusbar;
 	GtkLabel* pPositionLabel;
	GtkLabel* pZoomScaleLabel;
	GtkProgressBar* pProgressBar;

	// Boxes
	GtkVBox* pContentBox;

	// Drawing area
	GtkDrawingArea* pDrawingArea;
	tooltip_t* pTooltip;
	map_t* pMap;

//     EToolType eSelectedTool;

	gboolean bScrolling;
	EDirection eScrollDirection;
	gboolean bScrollMovement;

	gboolean bMouseDragging;
	gboolean bMouseDragMovement;

	screenpoint_t ptClickLocation;	

	gint nCurrentGPSPath;
	gint nGPSLocationGlyph;
	gint nDrawPrettyTimeoutID;
	gint nScrollTimeoutID;

	// Sliding
	gboolean bSliding;
	mappoint_t ptSlideStartLocation;
	mappoint_t ptSlideEndLocation;
	animator_t* pAnimator;

	// Zoom Tool
	gboolean bDrawingZoomRect;
	screenrect_t rcZoomRect;

	// History (forward / back)
	maphistory_t* pMapHistory;
	GtkButton* pForwardButton;
	GtkButton* pBackButton;
	GtkMenuItem* pForwardMenuItem;
	GtkMenuItem* pBackMenuItem;
	
	GtkMenuItem* pWebMapsMenuItem;

	GtkCheckMenuItem* pViewSidebarMenuItem;
	GtkCheckMenuItem* pViewFullscreenMenuItem;
} g_MainWindow = {0};

// XXX: Use GDK_HAND1 for the map

// Data
// toolsettings_t g_Tools[] = {
//     {"Pointer Tool", {GDK_LEFT_PTR, NULL}},
//     {"Zoom Tool", {GDK_CIRCLE, NULL}},
// };
// void cursor_init()
// {
//     int i;
//     for(i=0 ; i<G_N_ELEMENTS(g_Tools) ; i++) {
//         g_Tools[i].Cursor.pGdkCursor = gdk_cursor_new(g_Tools[i].Cursor.CursorType);
//     }
// }

static void util_set_image_to_stock(GtkImage* pImage, gchar* pszStockIconID, GtkIconSize nSize)
{
	GdkPixbuf* pPixbuf = gtk_widget_render_icon(GTK_WIDGET(g_MainWindow.pStatusbarGPSIcon),pszStockIconID, nSize, "name");
	gtk_image_set_from_pixbuf(pImage, pPixbuf);
	g_object_unref(pPixbuf);
}

static void mainwindow_set_statusbar_position(gchar* pMessage)
{
	gtk_label_set_text(GTK_LABEL(g_MainWindow.pPositionLabel), pMessage);
}

static void mainwindow_set_statusbar_zoomscale(gchar* pMessage)
{
	gtk_label_set_text(GTK_LABEL(g_MainWindow.pZoomScaleLabel), pMessage);
}

void* mainwindow_set_busy(void)
{
	GdkCursor* pCursor = gdk_cursor_new(GDK_WATCH);
	// set it for the whole window
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pWindow)->window, pCursor);

	// HACK: children with their own cursors set need to be set manually and restored later :/
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, pCursor);

	gdk_flush();
	return pCursor;
}

void mainwindow_draw_selection_rect(screenrect_t* pRect)
{
	map_draw_xor_rect(g_MainWindow.pMap, GTK_WIDGET(g_MainWindow.pDrawingArea)->window, pRect);
}

void mainwindow_set_not_busy(void** ppCursor)
{
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pWindow)->window, NULL);
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, NULL);	// HACK: see mainwindow_set_busy

	gdk_cursor_unref(*ppCursor);
	gdk_flush();

	// XXX: do we need to restore the mouse cursor (based on selected tool and location in window)?
	//mainwindow_setup_selected_tool();
}

/*
** Status bar
*/

void mainwindow_init_add_web_maps_menu_items()
{
	typedef struct { gchar* pszName; gchar* pszURL; } web_map_url_t;

	web_map_url_t aWebMapURLs[] = {
		{"Google Maps Roads", "http://maps.google.com/?ll={LAT}%2C{LON}&spn={LAT_SPAN}%2C{LON_SPAN}"},
		{"Google Maps Satellite", "http://maps.google.com/?ll={LAT}%2C{LON}&spn={LAT_SPAN}%2C{LON_SPAN}&t=k"},	// t=k means sat
		{"Google Maps Hybrid", "http://maps.google.com/?ll={LAT}%2C{LON}&spn={LAT_SPAN}%2C{LON_SPAN}&t=h"},		// t=h means hybrid

		// XXX: how do we specify zoom level in YMaps URL?
		{"Yahoo! Maps", "http://maps.yahoo.com/maps_result?lat={LAT}&lon={LON}"},

		{"MSN Maps", "http://maps.msn.com/map.aspx?C={LAT}%2C{LON}&S=800%2C740&alts1={ALTITUDE_MILES}"},

		{"MSN Virtual Earth Roads", "http://virtualearth.msn.com/default.aspx?v=1&cp={LAT}|{LON}&lvl={ZOOM_1_TO_19}&style=r"},
		{"MSN Virtual Earth Satellite", "http://virtualearth.msn.com/default.aspx?v=1&cp={LAT}|{LON}&lvl={ZOOM_1_TO_19}&style=a"},
		{"MSN Virtual Earth Hybrid", "http://virtualearth.msn.com/default.aspx?v=1&cp={LAT}|{LON}&lvl={ZOOM_1_TO_19}&style=h"},

		// Mapquest doesn't seem to accept LAT/LON params

		// NOTE: multimap zoomscales may not match ours, so we're just hardcoding one for now
		{"Multimap", "http://www.multimap.com/p/browse.cgi?lon={LON}&lat={LAT}&scale=10000"},
	};
	// Google Maps Directions:
	// DIRS: http://maps.google.com/maps?saddr=42.358333+-71.060278+(boston)&daddr=40.714167+-74.006389+(new+york)

	// Build a new submenu
	GtkWidget* pSubMenu = gtk_menu_new();
	gint i;
	for(i=0 ; i<G_N_ELEMENTS(aWebMapURLs) ; i++) {
		gchar* pszName;
		// Set name
		if((i+1) <= 9) {
			pszName = g_strdup_printf("_%d %s", i+1, aWebMapURLs[i].pszName);	// "1 Supermaps" with 1 underlined
		}
		else {
			pszName = g_strdup_printf("%s", aWebMapURLs[i].pszName);
		}
		GtkMenuItem* pNewMenuItem = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(pszName));
		g_free(pszName);
		
		// Add a click handler which gets passed the URL with {TAGS}
		g_signal_connect(G_OBJECT(pNewMenuItem), "activate", (GCallback)mainwindow_on_web_url_clicked, aWebMapURLs[i].pszURL);

		gtk_menu_shell_append(GTK_MENU_SHELL(pSubMenu), GTK_WIDGET(pNewMenuItem));
	}
	gtk_widget_show_all(pSubMenu);
	gtk_menu_item_set_submenu(g_MainWindow.pWebMapsMenuItem, pSubMenu);
}

void mainwindow_init(GladeXML* pGladeXML)
{
	// Window / Container Widgets
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pWindow, GTK_WINDOW, "mainwindow");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pToolbar, GTK_HBOX, "maintoolbar");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pStatusbar, GTK_VBOX, "statusbar");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pSidebox, GTK_WIDGET, "mainwindowsidebox");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pSidebarNotebook, GTK_NOTEBOOK, "sidebarnotebook");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pContentBox, GTK_VBOX, "mainwindowcontentsbox");

//     g_object_set(G_OBJECT(g_MainWindow.pSidebarNotebook), "show-border", FALSE, NULL);
	g_object_set(G_OBJECT(g_MainWindow.pSidebarNotebook), "tab-border", 1, NULL);
//     g_object_set(G_OBJECT(g_MainWindow.pSidebarNotebook), "tab-hborder", 0, NULL);

	// View menu
   	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pViewSidebarMenuItem, GTK_CHECK_MENU_ITEM, "viewsidebarmenuitem");
   	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pViewFullscreenMenuItem, GTK_CHECK_MENU_ITEM, "viewfullscreenmenuitem");

	// Zoom controls
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomInButton, GTK_BUTTON, "zoominbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomInMenuItem, GTK_MENU_ITEM, "zoominmenuitem");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomOutButton, GTK_BUTTON, "zoomoutbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomOutMenuItem, GTK_MENU_ITEM, "zoomoutmenuitem");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomScale, GTK_HSCALE, "zoomscale");

	// make it instant-change using our hacky callback
	g_signal_connect(G_OBJECT(g_MainWindow.pZoomScale), "change-value", G_CALLBACK(util_gtk_range_instant_set_on_value_changing_callback), NULL);

	// Labels
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pPositionLabel, GTK_LABEL, "positionlabel");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomScaleLabel, GTK_LABEL, "zoomscalelabel");

	// Popup menus
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pMapPopupMenu, GTK_MENU, "mappopupmenu");

	// Tools
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pPointerToolRadioButton, GTK_RADIO_BUTTON, "pointertoolradiobutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pZoomToolRadioButton, GTK_RADIO_BUTTON, "zoomtoolradiobutton");

	// GPS Widgets
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.GPS.pShowPositionCheckButton, GTK_CHECK_BUTTON, "gpsshowpositioncheckbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.GPS.pKeepPositionCenteredCheckButton, GTK_CHECK_BUTTON, "gpskeeppositioncenteredcheckbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.GPS.pShowTrailCheckButton, GTK_CHECK_BUTTON, "gpsshowtrailcheckbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.GPS.pStickToRoadsCheckButton, GTK_CHECK_BUTTON, "gpssticktoroadscheckbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pGPSSignalStrengthProgressBar, GTK_PROGRESS_BAR, "gpssignalprogressbar");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pStatusbarGPSIcon, GTK_IMAGE, "statusbargpsicon");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pSpeedLabel, GTK_LABEL, "speedlabel");

	// History Widgets
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pForwardButton, GTK_BUTTON, "forwardbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pBackButton, GTK_BUTTON, "backbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pForwardMenuItem, GTK_MENU_ITEM, "forwardmenuitem");
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pBackMenuItem, GTK_MENU_ITEM, "backmenuitem");
	g_MainWindow.pMapHistory = map_history_new();

	// LocationSet Widgets
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pLocationSetsTreeView, GTK_TREE_VIEW, "locationsetstreeview");

	// Web Map menu
	GLADE_LINK_WIDGET(pGladeXML, g_MainWindow.pWebMapsMenuItem, GTK_MENU_ITEM, "webmapsmenuitem");
	mainwindow_init_add_web_maps_menu_items();

	// Normal widget tooltips and our super-map-tooltip (tooltip.c)
	g_MainWindow.pTooltips		= gtk_tooltips_new();
	g_MainWindow.pTooltip 		= tooltip_new();


	// Signal handlers for main window
	gtk_widget_add_events(GTK_WIDGET(g_MainWindow.pWindow), GDK_KEY_PRESS_MASK);
	g_signal_connect(G_OBJECT(g_MainWindow.pWindow), "window_state_event", G_CALLBACK(mainwindow_on_window_state_change), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pWindow), "key_press_event", G_CALLBACK(mainwindow_on_key_press), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pWindow), "key_release_event", G_CALLBACK(mainwindow_on_key_release), NULL);

	// Drawing area
	g_MainWindow.pDrawingArea = GTK_DRAWING_AREA(gtk_drawing_area_new());	
	gtk_widget_set_double_buffered(GTK_WIDGET(g_MainWindow.pDrawingArea), FALSE);

	// Pack drawing area into application window
	gtk_box_pack_end(GTK_BOX(g_MainWindow.pContentBox), GTK_WIDGET(g_MainWindow.pDrawingArea),
					TRUE, // expand
					TRUE, // fill
					 0);
	gtk_widget_show(GTK_WIDGET(g_MainWindow.pDrawingArea));
	
	// Signal handlers for drawing area
	gtk_widget_add_events(GTK_WIDGET(g_MainWindow.pDrawingArea), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "expose_event", G_CALLBACK(mainwindow_on_expose_event), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "configure_event", G_CALLBACK(mainwindow_on_configure_event), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "button_press_event", G_CALLBACK(mainwindow_on_mouse_button_click), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "button_release_event", G_CALLBACK(mainwindow_on_mouse_button_click), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "motion_notify_event", G_CALLBACK(mainwindow_on_mouse_motion), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "scroll_event", G_CALLBACK(mainwindow_on_mouse_scroll), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "enter_notify_event", G_CALLBACK(mainwindow_on_enter_notify), NULL);
	g_signal_connect(G_OBJECT(g_MainWindow.pDrawingArea), "leave_notify_event", G_CALLBACK(mainwindow_on_leave_notify), NULL);

	// create map and load style
	map_new(&g_MainWindow.pMap, GTK_WIDGET(g_MainWindow.pDrawingArea));
	map_style_load(g_MainWindow.pMap, MAP_STYLE_FILENAME);
	
//     cursor_init();

	mainwindow_configure_locationset_list();
	mainwindow_refresh_locationset_list();

	mainwindow_statusbar_update_zoomscale();
	mainwindow_statusbar_update_position();

	// Slide timeout
	g_timeout_add(SLIDE_TIMEOUT_MS, (GSourceFunc)mainwindow_on_slide_timeout, (gpointer)NULL);

	// GPS check timeout
	g_timeout_add(TIMER_GPS_REDRAW_INTERVAL_MS, (GSourceFunc)mainwindow_on_gps_redraw_timeout, (gpointer)NULL);
	mainwindow_on_gps_redraw_timeout(NULL);     // give it a call to set all labels, etc.

	// When main window closes, quit.
	g_signal_connect(G_OBJECT(g_MainWindow.pWindow), "delete_event", G_CALLBACK(gtk_main_quit), NULL);

	// XXX: move map to starting location... for now it's (0,0)
	mainwindow_add_history();
	mainwindow_update_zoom_buttons();	// make sure buttons are grayed out

	mapinfowindow_update(g_MainWindow.pMap);
}

gboolean mainwindow_locationset_list_is_separator_callback(GtkTreeModel *_unused, GtkTreeIter *pIter, gpointer __unused)
{
	gint nLocationSetID;
	gtk_tree_model_get(GTK_TREE_MODEL(g_MainWindow.pLocationSetsListStore), pIter, LOCATIONSETLIST_COLUMN_ID, &nLocationSetID, -1);
	return (nLocationSetID == 0);
}

static void mainwindow_configure_locationset_list()
{
	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

	// Create location sets tree view
	g_MainWindow.pLocationSetsListStore = gtk_list_store_new(5, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF);
	gtk_tree_view_set_model(g_MainWindow.pLocationSetsTreeView, GTK_TREE_MODEL(g_MainWindow.pLocationSetsListStore));
	gtk_tree_view_set_search_column(g_MainWindow.pLocationSetsTreeView, LOCATIONSETLIST_COLUMN_NAME);
	gtk_tree_view_set_search_equal_func(g_MainWindow.pLocationSetsTreeView, util_treeview_match_all_words_callback, NULL, NULL);
	gtk_tree_view_set_row_separator_func(g_MainWindow.pLocationSetsTreeView, mainwindow_locationset_list_is_separator_callback, NULL, NULL);

		// NEW COLUMN: "Visibility" checkbox column
		pCellRenderer = gtk_cell_renderer_toggle_new();
			g_signal_connect(pCellRenderer, "toggled", G_CALLBACK(mainwindow_on_locationset_visible_checkbox_clicked), NULL);
		pColumn = gtk_tree_view_column_new_with_attributes("", pCellRenderer, "active", LOCATIONSETLIST_COLUMN_ENABLED, NULL);
		gtk_tree_view_append_column(g_MainWindow.pLocationSetsTreeView, pColumn);
	
		// NEW COLUMN: "Icon"
		pCellRenderer = gtk_cell_renderer_pixbuf_new();
		//g_object_set(G_OBJECT(pCellRenderer), "visible", FALSE, NULL);	// HIDE THIS COLUMN for now/ever
	//	g_object_set(G_OBJECT(pCellRenderer), "width", 24, NULL);
		g_object_set(G_OBJECT(pCellRenderer), "xpad", 2, NULL);
		g_object_set(G_OBJECT(pCellRenderer), "ypad", 1, NULL);
		pColumn = gtk_tree_view_column_new_with_attributes("", pCellRenderer, "pixbuf", LOCATIONSETLIST_COLUMN_PIXBUF, NULL);
		gtk_tree_view_append_column(g_MainWindow.pLocationSetsTreeView, pColumn);
	
		// NEW COLUMN: "Name" column
		pCellRenderer = gtk_cell_renderer_text_new();
			g_object_set(G_OBJECT(pCellRenderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
		pColumn = gtk_tree_view_column_new_with_attributes("Name", pCellRenderer, "markup", LOCATIONSETLIST_COLUMN_NAME, NULL);
			gtk_tree_view_column_set_expand(pColumn, TRUE);
		gtk_tree_view_append_column(g_MainWindow.pLocationSetsTreeView, pColumn);
	
		// NEW COLUMN: "#" column
		pCellRenderer = gtk_cell_renderer_text_new();
		g_object_set(G_OBJECT(pCellRenderer), "visible", FALSE, NULL);	// HIDE THIS COLUMN for now/ever
		g_object_set(G_OBJECT(pCellRenderer), "xalign", 1.0, NULL); // right align column contents
		pColumn = gtk_tree_view_column_new_with_attributes("#", pCellRenderer, "markup", LOCATIONSETLIST_COLUMN_COUNT, NULL);
			gtk_tree_view_column_set_alignment(pColumn, 1.0);   // right align column header
		gtk_tree_view_append_column(g_MainWindow.pLocationSetsTreeView, pColumn);
}

void mainwindow_refresh_locationset_list()
{
	GtkTreeIter iter;

	// Fill locationset list with live data
	const GPtrArray* pLocationSetArray = locationset_get_array();
	g_debug("%d locationsets", pLocationSetArray->len);

	gtk_list_store_clear(g_MainWindow.pLocationSetsListStore);

	gint i;
	for(i=0 ; i<pLocationSetArray->len ; i++) {
		locationset_t* pLocationSet = g_ptr_array_index(pLocationSetArray, i);

		gchar* pszName = g_strdup_printf("<small>%s</small>", pLocationSet->pszName);
		gchar* pszCount = g_strdup_printf("<small><small>(%d)</small></small>", pLocationSet->nLocationCount);

		GdkPixbuf* pPixbuf = glyph_get_pixbuf(pLocationSet->pGlyph);

		gtk_list_store_append(g_MainWindow.pLocationSetsListStore, &iter);
		gtk_list_store_set(g_MainWindow.pLocationSetsListStore, &iter, 
						   LOCATIONSETLIST_COLUMN_NAME, pszName, 
						   LOCATIONSETLIST_COLUMN_ENABLED, TRUE,
						   LOCATIONSETLIST_COLUMN_ID, pLocationSet->nID,
						   LOCATIONSETLIST_COLUMN_COUNT, pszCount,
						   LOCATIONSETLIST_COLUMN_PIXBUF, pPixbuf,
						   -1);
		g_free(pszName);
		g_free(pszCount);
	}
}

void mainwindow_show(void)
{
	gtk_widget_show(GTK_WIDGET(g_MainWindow.pWindow));
	gtk_window_present(g_MainWindow.pWindow);
}

void mainwindow_hide(void)
{
	gtk_widget_hide(GTK_WIDGET(g_MainWindow.pWindow));
}

void mainwindow_set_sensitive(gboolean bSensitive)
{
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pWindow), bSensitive);
}

//
// the "draw pretty" timeout lets us draw ugly/fast graphics while moving, then redraw pretty after we stop
//
gboolean mainwindow_on_draw_pretty_timeout(gpointer _unused)
{
	mainwindow_draw_map(DRAWFLAG_ALL);

	// We've just drawn a complete frame, so we can consider this a new drag with no movement yet
	g_MainWindow.bMouseDragMovement = FALSE;	// NOTE: may have already been FALSE, for example if we weren't dragging... :)

	g_MainWindow.nDrawPrettyTimeoutID = 0;
	return FALSE;		// NOTE: Returning FALSE from the timeout callback kills the timer.
}

void mainwindow_cancel_draw_pretty_timeout()
{
	if(g_MainWindow.nDrawPrettyTimeoutID != 0) {
		g_source_remove(g_MainWindow.nDrawPrettyTimeoutID);
	}
}

void mainwindow_set_draw_pretty_timeout(gint nTimeoutInMilliseconds)
{
	// cancel existing one, if one exists
	mainwindow_cancel_draw_pretty_timeout();

	g_MainWindow.nDrawPrettyTimeoutID = g_timeout_add(nTimeoutInMilliseconds, mainwindow_on_draw_pretty_timeout, NULL);
	g_assert(g_MainWindow.nDrawPrettyTimeoutID != 0);
}

//
// the scroll timeout
//
void mainwindow_scroll_direction(EDirection eScrollDirection, gint nPixels)
{
	if(eScrollDirection != DIRECTION_NONE) {
		gint nWidth = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width;
		gint nHeight = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height;

		gint nDeltaX = nPixels * g_aDirectionMultipliers[eScrollDirection].nX;
		gint nDeltaY = nPixels * g_aDirectionMultipliers[eScrollDirection].nY;

		mainwindow_map_center_on_windowpoint((nWidth / 2) + nDeltaX, (nHeight / 2) + nDeltaY);
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_SCROLL_TIMEOUT_MS);
	}
}

gboolean mainwindow_on_scroll_timeout(gpointer _unused)
{
	if(g_MainWindow.bScrolling) {
		mainwindow_scroll_direction(g_MainWindow.eScrollDirection, SCROLL_DISTANCE_IN_PIXELS);
		g_MainWindow.bScrollMovement = TRUE;
	}
	return TRUE;	// more events, please
}

void mainwindow_cancel_scroll_timeout()
{
	if(g_MainWindow.nScrollTimeoutID != 0) {
		g_source_remove(g_MainWindow.nScrollTimeoutID);
	}
}

void mainwindow_set_scroll_timeout()
{
	// cancel existing one, if one exists
	mainwindow_cancel_scroll_timeout();

	g_MainWindow.nScrollTimeoutID = g_timeout_add(SCROLL_TIMEOUT_MS, mainwindow_on_scroll_timeout, NULL);
	g_assert(g_MainWindow.nScrollTimeoutID != 0);
}

/*
** Toolbar
*/
void mainwindow_set_toolbar_visible(gboolean bVisible)
{
	util_gtk_widget_set_visible(GTK_WIDGET(g_MainWindow.pToolbar), bVisible);
}

gboolean mainwindow_get_toolbar_visible(void)
{
	return GTK_WIDGET_VISIBLE(g_MainWindow.pToolbar);
}

/*
** Statusbar
*/
void mainwindow_set_statusbar_visible(gboolean bVisible)
{
	if(bVisible) {
		gtk_widget_show(GTK_WIDGET(g_MainWindow.pStatusbar));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(g_MainWindow.pStatusbar));
	}
}

gboolean mainwindow_get_statusbar_visible(void)
{
	return GTK_WIDGET_VISIBLE(g_MainWindow.pStatusbar);
}

void mainwindow_statusbar_update_zoomscale(void)
{
	gchar* pszNew = g_strdup_printf("1:%d", map_get_scale(g_MainWindow.pMap));
	mainwindow_set_statusbar_zoomscale(pszNew);
	g_free(pszNew);
}

void mainwindow_statusbar_update_position(void)
{
	mappoint_t pt;
	map_get_centerpoint(g_MainWindow.pMap, &pt);

	gchar* pszNew = g_strdup_printf("Lat: %.5f, Lon: %.5f", pt.fLatitude, pt.fLongitude);
	mainwindow_set_statusbar_position(pszNew);
	g_free(pszNew);
}

/*
** Sidebox (Layers, etc.)
*/
void mainwindow_set_sidebox_visible(gboolean bVisible)
{
	// Set the menu's check without calling the signal handler
	g_signal_handlers_block_by_func(g_MainWindow.pViewSidebarMenuItem, mainwindow_on_sidebarmenuitem_activate, NULL);
	gtk_check_menu_item_set_active(g_MainWindow.pViewSidebarMenuItem, bVisible);
	g_signal_handlers_unblock_by_func(g_MainWindow.pViewSidebarMenuItem, mainwindow_on_sidebarmenuitem_activate, NULL);

	util_gtk_widget_set_visible(GTK_WIDGET(g_MainWindow.pSidebox), bVisible);
}

gboolean mainwindow_get_sidebox_visible(void)
{
	return GTK_WIDGET_VISIBLE(g_MainWindow.pSidebox);
}

// GtkWidget* mainwindow_get_window(void)
// {
//     return GTK_WIDGET(g_MainWindow.pWindow);
// }

void mainwindow_toggle_fullscreen(void)
{
	util_gtk_window_set_fullscreen(g_MainWindow.pWindow, !util_gtk_window_is_fullscreen(g_MainWindow.pWindow));
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

//
// Zoom
//
void mainwindow_update_zoom_buttons()
{
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pZoomInButton), map_can_zoom_in(g_MainWindow.pMap));
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pZoomInMenuItem), map_can_zoom_in(g_MainWindow.pMap));
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pZoomOutButton), map_can_zoom_out(g_MainWindow.pMap));
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pZoomOutMenuItem), map_can_zoom_out(g_MainWindow.pMap));

	// set zoomlevel scale but prevent it from calling handler (mainwindow_on_zoomscale_value_changed)
    g_signal_handlers_block_by_func(g_MainWindow.pZoomScale, mainwindow_on_zoomscale_value_changed, NULL);
	gtk_range_set_value(GTK_RANGE(g_MainWindow.pZoomScale), map_get_zoomlevel(g_MainWindow.pMap));
	g_signal_handlers_unblock_by_func(g_MainWindow.pZoomScale, mainwindow_on_zoomscale_value_changed, NULL);

	mainwindow_statusbar_update_zoomscale();
}

void mainwindow_set_zoomlevel(gint nZoomLevel)
{
	map_set_zoomlevel(g_MainWindow.pMap, nZoomLevel);

	// set zoomlevel scale but prevent it from calling handler (mainwindow_on_zoomscale_value_changed)
//     g_signal_handlers_block_by_func(g_MainWindow.pZoomScale, mainwindow_on_zoomscale_value_changed, NULL);
//     gtk_range_set_value(GTK_RANGE(g_MainWindow.pZoomScale), nZoomLevel);
//     g_signal_handlers_unblock_by_func(g_MainWindow.pZoomScale, mainwindow_on_zoomscale_value_changed, NULL);

	mainwindow_update_zoom_buttons();
//	g_print("nZoomLevel = %d, height = %f miles\n", nZoomLevel, map_get_altitude(g_MainWindow.pMap, UNIT_MILES));
}

// the range slider changed value
void mainwindow_on_zoomscale_value_changed(GtkRange *range, gpointer user_data)
{
	gdouble fValue = gtk_range_get_value(range);
	gint16 nValue = (gint16)fValue;

	if(map_get_zoomlevel(g_MainWindow.pMap) != nValue) {
		// update GUI
		mainwindow_set_zoomlevel(nValue);

		// also redraw immediately and add history item
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_ZOOM_TIMEOUT_MS);

		mainwindow_add_history();
	}
}

//
//
//
// static void gui_set_tool(EToolType eTool)
// {
//     g_MainWindow.eSelectedTool = eTool;
//     gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, g_Tools[eTool].Cursor.pGdkCursor);
// }

//
// Callbacks for About box
//
#if(GLIB_CHECK_VERSION(2,6,0))
void callback_url_clicked(GtkAboutDialog *about, const gchar *link, gpointer data)
{
	util_open_uri(link);
}

void callback_email_clicked(GtkAboutDialog *about, const gchar *link, gpointer data)
{
	gchar* pszURI = g_strdup_printf("mailto:%s", link);
	util_open_uri(pszURI);
	g_free(pszURI);
}

void mainwindow_on_aboutmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
         const gchar *ppAuthors[] = {
                 "Ian McIntosh <ian_mcintosh@linuxadvocate.org>",
                 "Nathan Fredrickson <nathan@silverorange.com>",
                 NULL
         };

         const gchar *ppArtists[] = {
				"Stephen DesRoches <stephen@silverorange.com>",
                 NULL
         };

		 GdkPixbuf* pIconPixbuf;
		 gchar* pszPath;

		 pszPath = g_strdup_printf(PACKAGE_SOURCE_DIR"/data/%s", "roadster-logo.png");
		 pIconPixbuf = gdk_pixbuf_new_from_file(pszPath, NULL);
		 g_free(pszPath);

		 if(pIconPixbuf == NULL) {
			 pszPath = g_strdup_printf(PACKAGE_DATA_DIR"/data/%s", "roadster-logo.png");
			 pIconPixbuf = gdk_pixbuf_new_from_file(pszPath, NULL);
			 g_free(pszPath);
		 }

         gtk_about_dialog_set_url_hook(callback_url_clicked, NULL, NULL);
         gtk_about_dialog_set_email_hook(callback_email_clicked, NULL, NULL);
		 gtk_show_about_dialog(g_MainWindow.pWindow,
				   "authors", ppAuthors,
			       "artists", ppArtists,
			       "comments", PROGRAM_DESCRIPTION,
			       "copyright", PROGRAM_COPYRIGHT,
			       "name", PROGRAM_NAME,
				    "website", WEBSITE_URL,
				   "website-label", "Visit the Roadster Website",
			       "version", VERSION,
				   "logo", pIconPixbuf,
			       NULL);

		 if(pIconPixbuf) g_object_unref(pIconPixbuf);
}
#else
void mainwindow_on_aboutmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	g_warning("Compiled with glib < 2.6.0. About Box not available.");
}
#endif	// if(GLIB_CHECK_VERSION(2,6,0))

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

#define ZOOM_MAJOR_TICK_SIZE	(4)

// Zoom buttons / menu items (shared callbacks)
void mainwindow_on_zoomin_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gint nNewZoomLevel = map_get_zoomlevel(g_MainWindow.pMap) - 1; 	// XXX: make zoomlevel 0-based and the -1 will go away
	nNewZoomLevel -= (nNewZoomLevel % ZOOM_MAJOR_TICK_SIZE);
	nNewZoomLevel += ZOOM_MAJOR_TICK_SIZE;
	nNewZoomLevel += 1;	// XXX: make zoomlevel 0-based and the +1 will go away

	// tell the map
	map_set_zoomlevel(g_MainWindow.pMap, nNewZoomLevel);
	
	// update the gui
	mainwindow_set_zoomlevel(map_get_zoomlevel(g_MainWindow.pMap));
	mainwindow_draw_map(DRAWFLAG_GEOMETRY);
	mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_ZOOM_TIMEOUT_MS);
	mainwindow_add_history();
}

void mainwindow_on_zoomout_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gint nNewZoomLevel = map_get_zoomlevel(g_MainWindow.pMap) - 1;
	if((nNewZoomLevel % ZOOM_MAJOR_TICK_SIZE) == 0) nNewZoomLevel -= ZOOM_MAJOR_TICK_SIZE;
	else nNewZoomLevel -= (nNewZoomLevel % ZOOM_MAJOR_TICK_SIZE);
	nNewZoomLevel += 1;	// XXX: make zoomlevel 0-based and the +1 will go away

	map_set_zoomlevel(g_MainWindow.pMap, nNewZoomLevel);
	mainwindow_set_zoomlevel(map_get_zoomlevel(g_MainWindow.pMap));
	mainwindow_draw_map(DRAWFLAG_GEOMETRY);
	mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_ZOOM_TIMEOUT_MS);
	mainwindow_add_history();
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
	map_style_load(g_MainWindow.pMap, MAP_STYLE_FILENAME);
	mainwindow_draw_map(DRAWFLAG_ALL);
}

void mainwindow_on_reloadglyphsmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	glyph_reload_all();
	mainwindow_draw_map(DRAWFLAG_ALL);
}

static gboolean mainwindow_on_mouse_button_click(GtkWidget* w, GdkEventButton *event)
{
	gint nX, nY;
	gdk_window_get_pointer(w->window, &nX, &nY, NULL);

	gint nWidth = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width;
	gint nHeight = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height;

	EDirection eScrollDirection = DIRECTION_NONE;

	// get mouse position on screen
	screenpoint_t screenpoint = {nX,nY};
	mappoint_t mappoint;
	map_windowpoint_to_mappoint(g_MainWindow.pMap, &screenpoint, &mappoint);
	
	maphit_t* pHitStruct = NULL;
	map_hittest(g_MainWindow.pMap, &mappoint, &pHitStruct);
	// hitstruct free'd far below

	if(event->button == MOUSE_BUTTON_LEFT) {
		// Left mouse button down?
		if(event->type == GDK_BUTTON_PRESS) {
			tooltip_hide(g_MainWindow.pTooltip);

			// Is it at a border?
			eScrollDirection = util_match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);
			if(eScrollDirection != DIRECTION_NONE) {
				// begin a scroll
				GdkCursor* pCursor = gdk_cursor_new(g_aDirectionCursors[eScrollDirection].nCursor);
				gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, pCursor);
				gdk_cursor_unref(pCursor);

				g_MainWindow.bScrolling = TRUE;
				g_MainWindow.eScrollDirection = eScrollDirection;
				g_MainWindow.bScrollMovement = FALSE;	// no movement yet

				mainwindow_set_scroll_timeout();
			}
			else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_MainWindow.pZoomToolRadioButton))) {
				//g_print("begin rect draw\n");
				g_MainWindow.bDrawingZoomRect = TRUE;

				// set both rect points to click point
				g_MainWindow.rcZoomRect.A.nX = g_MainWindow.rcZoomRect.B.nX = nX;
				g_MainWindow.rcZoomRect.A.nY = g_MainWindow.rcZoomRect.B.nY = nY;
			}
			else {
				g_MainWindow.bMouseDragging = TRUE;
				g_MainWindow.bMouseDragMovement = FALSE;
				g_MainWindow.ptClickLocation.nX = nX;
				g_MainWindow.ptClickLocation.nY = nY;
			}
		}
		// Left mouse button up?
		else if(event->type == GDK_BUTTON_RELEASE) {
			// end mouse dragging, if active
			if(g_MainWindow.bMouseDragging == TRUE) {
				// restore cursor
				GdkCursor* pCursor = gdk_cursor_new(GDK_LEFT_PTR);
				gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, pCursor);
				gdk_cursor_unref(pCursor);

				g_MainWindow.bMouseDragging = FALSE;
				if(g_MainWindow.bMouseDragMovement) {
					mainwindow_cancel_draw_pretty_timeout();
					mainwindow_draw_map(DRAWFLAG_ALL);

					mainwindow_add_history();
				}
				else if(pHitStruct != NULL) {
					if(pHitStruct->eHitType == MAP_HITTYPE_LOCATION) {
						if(map_location_selection_add(g_MainWindow.pMap, pHitStruct->LocationHit.nLocationID)) {
							mainwindow_draw_map(DRAWFLAG_ALL);
						}
					}
					else if(pHitStruct->eHitType == MAP_HITTYPE_LOCATIONSELECTION_CLOSE) {
						if(map_location_selection_remove(g_MainWindow.pMap, pHitStruct->LocationHit.nLocationID)) {
							mainwindow_draw_map(DRAWFLAG_ALL);
						}
					}
					else if(pHitStruct->eHitType == MAP_HITTYPE_LOCATIONSELECTION_EDIT) {
						// edit POI
						//g_MainWindow.pMap, pHitStruct->LocationHit.nLocationID
					}
					else if(pHitStruct->eHitType == MAP_HITTYPE_URL) {
						util_open_uri(pHitStruct->URLHit.pszURL);
					}
				}
			}

			if(g_MainWindow.bDrawingZoomRect == TRUE) {
				// Finished drawing a zoom-rect.  Zoom in (if the rect was past the minimum threshold).
				
				if((map_screenrect_width(&(g_MainWindow.rcZoomRect)) > ZOOM_TOOL_THRESHOLD) && (map_screenrect_height(&(g_MainWindow.rcZoomRect)) > ZOOM_TOOL_THRESHOLD)) {
					map_zoom_to_screenrect(g_MainWindow.pMap, &(g_MainWindow.rcZoomRect));

					// update GUI
					mainwindow_update_zoom_buttons();
					mainwindow_statusbar_update_position();

					mainwindow_draw_map(DRAWFLAG_GEOMETRY);
					mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_ZOOM_TIMEOUT_MS);
					mainwindow_add_history();
				}
				else {
					// Since we're not redrawing the map, we need to erase the selection rectangle
					mainwindow_draw_selection_rect(&(g_MainWindow.rcZoomRect));
				}
				// all done
				g_MainWindow.bDrawingZoomRect = FALSE;
			}

			if(g_MainWindow.bScrolling == TRUE) {
				// End scrolling

				// NOTE: don't restore cursor (mouse could *still* be over screen edge)

				g_MainWindow.bScrolling = FALSE;
				mainwindow_cancel_draw_pretty_timeout();

				// has there been any movement?
				if(g_MainWindow.bScrollMovement) {
					g_MainWindow.bScrollMovement = FALSE;
					mainwindow_draw_map(DRAWFLAG_ALL);
				}
				else {
					// user clicked the edge of the screen, but so far we haven't moved at all (they released the button too fast)
					// so consider this a 'click' and just jump a bit in that direction
					gint nHeight = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height;
					gint nWidth = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width;

					gint nDistanceInPixels;
					if(g_MainWindow.eScrollDirection == DIRECTION_N || g_MainWindow.eScrollDirection == DIRECTION_S) {
						// scroll half the height of the screen
						nDistanceInPixels = (gint)((gdouble)nHeight * BORDER_JUMP_PERCENT_OF_SCREEN);
					}
					else if(g_MainWindow.eScrollDirection == DIRECTION_E || g_MainWindow.eScrollDirection == DIRECTION_W) {
						nDistanceInPixels = (gint)((gdouble)nWidth * BORDER_JUMP_PERCENT_OF_SCREEN);
					}
					else {
						// half the distance from corner to opposite corner
						nDistanceInPixels = (gint)(sqrt(nHeight*nHeight + nWidth*nWidth) * BORDER_JUMP_PERCENT_OF_SCREEN);
					}

					mainwindow_scroll_direction(g_MainWindow.eScrollDirection, nDistanceInPixels);
				}
				g_MainWindow.eScrollDirection = DIRECTION_NONE;
				mainwindow_add_history();
			}
		}
		else if(event->type == GDK_2BUTTON_PRESS) {
			// can only double-click in the middle (not on a scroll border)
			eScrollDirection = util_match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);
			if(eScrollDirection == DIRECTION_NONE) {
				animator_destroy(g_MainWindow.pAnimator);

				g_MainWindow.bSliding = TRUE;
				g_MainWindow.pAnimator = animator_new(ANIMATIONTYPE_FAST_THEN_SLIDE, SLIDE_TIME_IN_SECONDS);

				// set startpoint
				map_get_centerpoint(g_MainWindow.pMap, &g_MainWindow.ptSlideStartLocation);

				// set endpoint
				screenpoint_t ptScreenPoint = {nX, nY};
				map_windowpoint_to_mappoint(g_MainWindow.pMap, &ptScreenPoint, &(g_MainWindow.ptSlideEndLocation));
			}
		}
	}
	else if (event->button == MOUSE_BUTTON_RIGHT) {
		// single right-click?
		if(event->type == GDK_BUTTON_PRESS) {
			//GtkMenu* pMenu = g_MainWindow.pMapPopupMenu;	// default to generic map popup

			if(pHitStruct != NULL) {
				if(pHitStruct->eHitType == MAP_HITTYPE_LOCATION) {
					// Use POI specific popup menu
					g_print("right-clicked on a POI: %d\n", pHitStruct->LocationHit.nLocationID);
					// pHitStruct->LocationHit.nLocationID

				}
				else if(pHitStruct->eHitType == MAP_HITTYPE_URL) {
					// Use URL specific popup menu (open/copy)
					g_print("right-clicked on a URL: %s\n", pHitStruct->URLHit.pszURL);
					//;
				}
			}

			// Save click location for use by menu item callback
			g_MainWindow.ptClickLocation.nX = nX;
			g_MainWindow.ptClickLocation.nY = nY;

			locationeditwindow_show_for_new(0);

			// Show popup!
			//g_print("showing\n");
			//gtk_menu_popup(pMenu, NULL, NULL, NULL, NULL, event->button, event->time);
		}
	}
	else if(event->button == MOUSE_BUTTON_MIDDLE) {
		if(event->type == GDK_BUTTON_PRESS) {
			g_MainWindow.bMouseDragging = TRUE;
			g_MainWindow.bMouseDragMovement = FALSE;
			g_MainWindow.ptClickLocation.nX = nX;
			g_MainWindow.ptClickLocation.nY = nY;
			tooltip_hide(g_MainWindow.pTooltip);
		}
		else if(event->type == GDK_BUTTON_RELEASE) {
			if(g_MainWindow.bMouseDragging == TRUE) {
				// restore cursor
				GdkCursor* pCursor = gdk_cursor_new(GDK_LEFT_PTR);
				gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, pCursor);
				gdk_cursor_unref(pCursor);

				g_MainWindow.bMouseDragging = FALSE;
				if(g_MainWindow.bMouseDragMovement) {
					mainwindow_cancel_draw_pretty_timeout();
					mainwindow_draw_map(DRAWFLAG_ALL);
					mainwindow_add_history();
				}
			}
		}
	}
	map_hittest_maphit_free(g_MainWindow.pMap, pHitStruct);
	return TRUE;
}

static gboolean mainwindow_on_mouse_motion(GtkWidget* w, GdkEventMotion *event)
{
    gint nX,nY;

	// XXX: why do we do this?
	if (event->is_hint) {
		gdk_window_get_pointer(w->window, &nX, &nY, NULL);
	}
	else {
		nX = event->x;
		nY = event->y;
	}

	gint nWidth = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width;
	gint nHeight = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height;

	// nX and nY clipped to screen
	gint nClippedX = (nX < 0) ? 0 : ((nX > nWidth-1) ? nWidth-1 : nX);
	gint nClippedY = (nY < 0) ? 0 : ((nY > nHeight-1) ? nHeight-1 : nY);

	gint nCursor;
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_MainWindow.pZoomToolRadioButton))) {
		nCursor = GDK_SIZING;
	}
	else {
		nCursor = GDK_LEFT_PTR;
	}

	if(g_MainWindow.bMouseDragging) {
		g_MainWindow.bMouseDragMovement = TRUE;

		// Set cursor here and not when first clicking because now we know it's a drag (on click it could be a double-click)
		nCursor = GDK_FLEUR;

		gint nDeltaX = g_MainWindow.ptClickLocation.nX - nX;
		gint nDeltaY = g_MainWindow.ptClickLocation.nY - nY;

		if(nDeltaX == 0 && nDeltaY == 0) return TRUE;

		mainwindow_map_center_on_windowpoint((nWidth / 2) + nDeltaX, (nHeight / 2) + nDeltaY);
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_DRAG_TIMEOUT_MS);

		g_MainWindow.ptClickLocation.nX = nX;
		g_MainWindow.ptClickLocation.nY = nY;
	}
	else if(g_MainWindow.bScrolling) {
		EDirection eScrollDirection = util_match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);

		// update cursor
		nCursor = g_aDirectionCursors[eScrollDirection].nCursor;

		// update direction if actively scrolling
		g_MainWindow.eScrollDirection = eScrollDirection;
	}
	else if(g_MainWindow.bDrawingZoomRect) {
		//g_print("updating rect\n");
		mainwindow_draw_selection_rect(&g_MainWindow.rcZoomRect);	// erase old rect (XOR operator rocks!)

		g_MainWindow.rcZoomRect.B.nX = nClippedX;
		g_MainWindow.rcZoomRect.B.nY = nClippedY;

		mainwindow_draw_selection_rect(&g_MainWindow.rcZoomRect);	// draw new rect
	}
	else {
		// If not dragging or scrolling, user is just moving mouse around.
		// Update tooltip and mouse cursor based on what we're pointing at.

		EDirection eScrollDirection = util_match_border(nX, nY, nWidth, nHeight, BORDER_SCROLL_CLICK_TARGET_SIZE);

		if(eScrollDirection == DIRECTION_NONE) {
#ifdef ENABLE_MAP_TOOLTIP
			// get mouse position on screen
			screenpoint_t screenpoint;
			screenpoint.nX = nX;
			screenpoint.nY = nY;
			mappoint_t mappoint;
			map_windowpoint_to_mappoint(g_MainWindow.pMap, &screenpoint, &mappoint);
	
			// try to "hit" something on the map. a road, a location, whatever!
			maphit_t* pHitStruct = NULL;
			if(map_hittest(g_MainWindow.pMap, &mappoint, &pHitStruct)) {
				// A hit!  Move the tooltip here, format the text, and show it.
				tooltip_set_upper_left_corner(g_MainWindow.pTooltip, (gint)(event->x_root) + TOOLTIP_OFFSET_X, (gint)(event->y_root) + TOOLTIP_OFFSET_Y);

				gchar* pszMarkup = g_strdup_printf(TOOLTIP_FORMAT, pHitStruct->pszText);
				tooltip_set_markup(g_MainWindow.pTooltip, pszMarkup);
				g_free(pszMarkup);

				// choose color based on type of hit
				if(pHitStruct->eHitType == MAP_HITTYPE_LOCATION) {
					GdkColor clr = {0, LOCATION_TOOLTIP_BG_COLOR};
					tooltip_set_bg_color(g_MainWindow.pTooltip, &clr);

					// also set mouse cursor to hand
					nCursor = GDK_HAND2;
					tooltip_show(g_MainWindow.pTooltip);
				}
				else if(pHitStruct->eHitType == MAP_HITTYPE_ROAD) {
					GdkColor clr = {0, ROAD_TOOLTIP_BG_COLOR};
					tooltip_set_bg_color(g_MainWindow.pTooltip, &clr);
					tooltip_show(g_MainWindow.pTooltip);
				}
				else if(pHitStruct->eHitType == MAP_HITTYPE_LOCATIONSELECTION) {
					tooltip_hide(g_MainWindow.pTooltip);
				}
				else if(pHitStruct->eHitType == MAP_HITTYPE_LOCATIONSELECTION_CLOSE) {
					nCursor = GDK_HAND2;
					tooltip_hide(g_MainWindow.pTooltip);
				}
				else if(pHitStruct->eHitType == MAP_HITTYPE_LOCATIONSELECTION_EDIT) {
					nCursor = GDK_HAND2;
					tooltip_hide(g_MainWindow.pTooltip);
				}
				else if(pHitStruct->eHitType == MAP_HITTYPE_URL) {
					nCursor = GDK_HAND2;
					tooltip_hide(g_MainWindow.pTooltip);

					// XXX: add url (pHitStruct->URLHit.pszURL) to statusbar
					g_print("url: %s\n", pHitStruct->URLHit.pszURL);
				}

				map_hittest_maphit_free(g_MainWindow.pMap, pHitStruct);
			}
			else {
				// no hit. hide the tooltip
				tooltip_hide(g_MainWindow.pTooltip);
			}
#endif
		}
		else {
			nCursor = g_aDirectionCursors[eScrollDirection].nCursor;

			// using a funky (non-pointer) cursor so hide the tooltip
			tooltip_hide(g_MainWindow.pTooltip);
		}
	}
	// apply cursor that was chosen above
	GdkCursor* pCursor = gdk_cursor_new(nCursor);
	gdk_window_set_cursor(GTK_WIDGET(g_MainWindow.pDrawingArea)->window, pCursor);
	gdk_cursor_unref(pCursor);

	return FALSE;
}

static gboolean mainwindow_on_enter_notify(GtkWidget* w, GdkEventCrossing *event)
{
	// mouse entered our window.  nothing to do (we'll respond to mouse motion)
	return FALSE; 	// propagate further
}

static gboolean mainwindow_on_leave_notify(GtkWidget* w, GdkEventCrossing *event)
{
	// mouse left our window
	tooltip_hide(g_MainWindow.pTooltip);
	return FALSE; 	// propagate further
}

// Respond to mouse scroll wheel
static gboolean mainwindow_on_mouse_scroll(GtkWidget* w, GdkEventScroll *event)
{
	// respond to scroll wheel events by zooming in and out
	if(event->direction == GDK_SCROLL_UP) {
		map_set_zoomlevel(g_MainWindow.pMap, map_get_zoomlevel(g_MainWindow.pMap) + 1);
		mainwindow_set_zoomlevel(map_get_zoomlevel(g_MainWindow.pMap));

		// Draw map quickly, then slowly after a short timeout.  This lets the user zoom many levels quickly.
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_ZOOM_TIMEOUT_MS);
	}
	else if(event->direction == GDK_SCROLL_DOWN) {
		map_set_zoomlevel(g_MainWindow.pMap, map_get_zoomlevel(g_MainWindow.pMap) - 1);
		mainwindow_set_zoomlevel(map_get_zoomlevel(g_MainWindow.pMap));
		mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_ZOOM_TIMEOUT_MS);
	}
	return FALSE; 	// propagate further
}

static gboolean mainwindow_on_key_press(GtkWidget *widget, GdkEventKey *pEvent, gpointer user_data)
{
	//g_print("key_press\n");
	if(pEvent->keyval == GDK_F1) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_MainWindow.pPointerToolRadioButton), TRUE);
	}
	else if(pEvent->keyval == GDK_F2) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_MainWindow.pZoomToolRadioButton), TRUE);
	}
	else if(pEvent->keyval == GDK_Escape) {
		if(g_MainWindow.bDrawingZoomRect == TRUE) {
			// cancel zoom-rect
			mainwindow_draw_selection_rect(&(g_MainWindow.rcZoomRect));
			g_MainWindow.bDrawingZoomRect = FALSE;
		}
	}
	return FALSE; 	// propagate further
}
static gboolean mainwindow_on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	//g_print("key_release\n");
	return FALSE; 	// propagate further
}

static gboolean mainwindow_on_window_state_change(GtkWidget *_unused, GdkEventKey *pEvent, gpointer __unused)
{
	// Set the menu's check without calling the signal handler
	g_signal_handlers_block_by_func(g_MainWindow.pViewFullscreenMenuItem, mainwindow_on_fullscreenmenuitem_activate, NULL);
	gtk_check_menu_item_set_active(g_MainWindow.pViewFullscreenMenuItem, util_gtk_window_is_fullscreen(g_MainWindow.pWindow));
	g_signal_handlers_unblock_by_func(g_MainWindow.pViewFullscreenMenuItem, mainwindow_on_fullscreenmenuitem_activate, NULL);
	return FALSE; 	// propagate further
}

static void mainwindow_begin_import_geography_data(void)
{
g_print("starting..\n");

	GtkWidget* pDialog = gtk_file_chooser_dialog_new(
						"Select TIGER .ZIP files for Import",
                		g_MainWindow.pWindow,
		                GTK_FILE_CHOOSER_ACTION_OPEN,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						"Import", GTK_RESPONSE_ACCEPT,
						NULL);

g_print("setting..\n");
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(pDialog), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pWindow), FALSE);

g_print("running..\n");
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
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pWindow), TRUE);
	gtk_widget_destroy(pDialog);
}

void mainwindow_on_import_maps_activate(GtkWidget *widget, gpointer user_data)
{
	mainwindow_begin_import_geography_data();
}

static void mainwindow_setup_selected_tool(void)
{
//     if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(g_MainWindow.pPointerToolButton))) {
//         gui_set_tool(kToolPointer);
//     }
//     else if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(g_MainWindow.pZoomToolButton))) {
//         gui_set_tool(kToolZoom);
//     }
}

// One handler for all 'tools' buttons
void mainwindow_on_toolbutton_clicked(GtkToolButton *toolbutton, gpointer user_data)
{
	// XXX: current there are no tools!
	mainwindow_setup_selected_tool();
}

//#define ENABLE_DRAW_LIVE_LABELS

void mainwindow_draw_map(gint nDrawFlags)
{
#ifdef ENABLE_DRAW_LIVE_LABELS
	map_draw(g_MainWindow.pMap, g_MainWindow.pMap->pPixmap, (nDrawFlags & ~DRAWFLAG_LABELS));

	GdkPixmap* pMapPixmap = map_get_pixmap(g_MainWindow.pMap);
	gdk_draw_drawable(GTK_WIDGET(g_MainWindow.pDrawingArea)->window,
		      GTK_WIDGET(g_MainWindow.pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.pDrawingArea)],
		      pMapPixmap, 0,0, 0,0,
		      GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width, GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height);
	map_release_pixmap(g_MainWindow.pMap);

	if((nDrawFlags & DRAWFLAG_LABELS) > 0) {
		map_draw(g_MainWindow.pMap, GTK_WIDGET(g_MainWindow.pDrawingArea)->window, (nDrawFlags & DRAWFLAG_LABELS));

		GdkPixmap* pMapPixmap = map_get_pixmap(g_MainWindow.pMap);
		gdk_draw_drawable(pMapPixmap,
				  GTK_WIDGET(g_MainWindow.pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.pDrawingArea)],
				  GTK_WIDGET(g_MainWindow.pDrawingArea)->window, 0,0, 0,0,
				  GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width, GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height);
		map_release_pixmap(g_MainWindow.pMap);
	}
#else
	map_draw(g_MainWindow.pMap, g_MainWindow.pMap->pPixmap, nDrawFlags);

	// push it to screen
	GdkPixmap* pMapPixmap = map_get_pixmap(g_MainWindow.pMap);
	gdk_draw_drawable(GTK_WIDGET(g_MainWindow.pDrawingArea)->window,
		      GTK_WIDGET(g_MainWindow.pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.pDrawingArea)],
		      pMapPixmap,
		      0,0,
		      0,0,
		      GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width, GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height);
	map_release_pixmap(g_MainWindow.pMap);
#endif
}

// A 'configure' event is sent by GTK when our window changes size
static gint mainwindow_on_configure_event(GtkWidget *pDrawingArea, GdkEventConfigure *event)
{
	// tell the map how big to draw
	dimensions_t dim;
	dim.uWidth = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width;
	dim.uHeight = GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height;
	map_set_dimensions(g_MainWindow.pMap, &dim);

	mainwindow_draw_map(DRAWFLAG_GEOMETRY);
	mainwindow_set_draw_pretty_timeout(DRAW_PRETTY_RESIZE_TIMEOUT_MS);
	return TRUE;
}

static gboolean mainwindow_on_expose_event(GtkWidget *_unused, GdkEventExpose *pEvent, gpointer __unused)
{
    if(pEvent->count > 0) return FALSE;

    GdkPixmap* pMapPixmap = map_get_pixmap(g_MainWindow.pMap);

    // Copy relevant portion of off-screen bitmap to window
    gdk_draw_drawable(GTK_WIDGET(g_MainWindow.pDrawingArea)->window,
			   GTK_WIDGET(g_MainWindow.pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.pDrawingArea)],
			   pMapPixmap,
			   pEvent->area.x, pEvent->area.y,
			   pEvent->area.x, pEvent->area.y,
			   pEvent->area.width, pEvent->area.height);

    map_release_pixmap(g_MainWindow.pMap);
    return FALSE;
}

/*
** GPS Functions
*/
void mainwindow_on_gps_show_position_toggled(GtkWidget* _unused, gpointer* __unused)
{
	gboolean bShowPosition = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_MainWindow.GPS.pShowPositionCheckButton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.GPS.pKeepPositionCenteredCheckButton), bShowPosition == TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.GPS.pShowTrailCheckButton), bShowPosition == TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.GPS.pStickToRoadsCheckButton), FALSE);	// XXX: for now.
}

void mainwindow_on_gps_keep_position_centered_toggled(GtkWidget* _unused, gpointer* __unused)
{

}

void mainwindow_on_gps_show_trail_toggled(GtkWidget* _unused, gpointer* __unused)
{

}

void mainwindow_on_gps_stick_to_roads_toggled(GtkWidget* _unused, gpointer* __unused)
{

}

static gboolean mainwindow_on_gps_redraw_timeout(gpointer __unused)
{
	// NOTE: we're setting tooltips on the image's
	GtkWidget* pWidget = gtk_widget_get_parent(GTK_WIDGET(g_MainWindow.pStatusbarGPSIcon));

	const gpsdata_t* pData = gpsclient_getdata();
	if(pData->eStatus == GPS_STATUS_LIVE) {

//         if(g_MainWindow.nCurrentGPSPath == 0) {
//             // create a new track for GPS trail
//             g_MainWindow.nCurrentGPSPath = track_new();
//             map_add_track(g_MainWindow.pMap, g_MainWindow.nCurrentGPSPath);
//         }
//
//         track_add_point(g_MainWindow.nCurrentGPSPath, &pData->ptPosition);

		// Show position?
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_MainWindow.GPS.pShowPositionCheckButton))) {
			// Keep it centered?
			if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_MainWindow.GPS.pKeepPositionCenteredCheckButton))) {
				map_set_centerpoint(g_MainWindow.pMap, &pData->ptPosition);
				mainwindow_statusbar_update_position();
			}

			// redraw because GPS icon/trail may be different
			mainwindow_draw_map(DRAWFLAG_ALL);
		}

		// update image and tooltip for GPS icon
		util_set_image_to_stock(g_MainWindow.pStatusbarGPSIcon, GTK_STOCK_OK, GTK_ICON_SIZE_MENU);
		gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.pTooltips), pWidget,
				 "A GPS device is present and active", "");

		// update speed
		gchar* pszSpeed = g_strdup_printf(SPEED_LABEL_FORMAT, pData->fSpeedInMilesPerHour);
		gtk_label_set_markup(g_MainWindow.pSpeedLabel, pszSpeed);
		g_free(pszSpeed);

		gtk_progress_bar_set_fraction(g_MainWindow.pGPSSignalStrengthProgressBar, pData->fSignalQuality * 20.0/100.0);
	}
	else {
		// unless we're "LIVE", set signal strength to 0
		gtk_progress_bar_set_fraction(g_MainWindow.pGPSSignalStrengthProgressBar, 0.0);

		if(pData->eStatus == GPS_STATUS_NO_SIGNAL) {
			// do NOT set speed to 0 if we drop the signal temporarily...
			util_set_image_to_stock(g_MainWindow.pStatusbarGPSIcon, GTK_STOCK_CANCEL, GTK_ICON_SIZE_MENU);

			gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.pTooltips), pWidget,
					 "A GPS device is present but unable to hear satellites", "");
		}
		else {
			// update speed, set to 0 (NOTE: we don't do this for "no signal" so we don't show 0 when a user temp. loses the signal)
			gchar* pszSpeed = g_strdup_printf(SPEED_LABEL_FORMAT, 0.0);
			gtk_label_set_markup(g_MainWindow.pSpeedLabel, pszSpeed);
			g_free(pszSpeed);

			if(pData->eStatus == GPS_STATUS_NO_DEVICE) {
				util_set_image_to_stock(g_MainWindow.pStatusbarGPSIcon, GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
				gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.pTooltips), pWidget,
						 "No GPS device is present", "");
			}
			else if(pData->eStatus == GPS_STATUS_NO_GPSD) {
				util_set_image_to_stock(g_MainWindow.pStatusbarGPSIcon, GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
				gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.pTooltips), pWidget,
						 "Install package 'gpsd' to use a GPS device with Roadster", "");
			}
			else if(pData->eStatus == GPS_STATUS_NO_GPS_COMPILED_IN) {
				util_set_image_to_stock(g_MainWindow.pStatusbarGPSIcon, GTK_STOCK_CANCEL, GTK_ICON_SIZE_MENU);

				gtk_tooltips_set_tip(GTK_TOOLTIPS(g_MainWindow.pTooltips), pWidget,
						 "GPS support was not included in this version of Roadster", "");
			}
			else {
				g_assert_not_reached();
			}
		}
	}
	return TRUE;
}

static gboolean mainwindow_on_slide_timeout(gpointer pData)
{
	if(g_MainWindow.bSliding) {
		g_assert(g_MainWindow.pAnimator != NULL);

		// Figure out the progress along the path and interpolate 
		// between startpoint and endpoint

		gdouble fPercent = animator_get_progress(g_MainWindow.pAnimator);
		gboolean bDone = animator_is_done(g_MainWindow.pAnimator);
		if(bDone) {
			animator_destroy(g_MainWindow.pAnimator);
			g_MainWindow.pAnimator = NULL;
			g_MainWindow.bSliding = FALSE;

			fPercent = 1.0;

			mainwindow_add_history();
			// should we delete the timer?
		}

		gdouble fDeltaLat = g_MainWindow.ptSlideEndLocation.fLatitude - g_MainWindow.ptSlideStartLocation.fLatitude;
		gdouble fDeltaLon = g_MainWindow.ptSlideEndLocation.fLongitude - g_MainWindow.ptSlideStartLocation.fLongitude;

		mappoint_t ptNew;
		ptNew.fLatitude = g_MainWindow.ptSlideStartLocation.fLatitude + (fPercent * fDeltaLat);
		ptNew.fLongitude = g_MainWindow.ptSlideStartLocation.fLongitude + (fPercent * fDeltaLon);

		mainwindow_map_center_on_mappoint(&ptNew);
		if(bDone) {
			// when done, draw a full frame
			mainwindow_draw_map(DRAWFLAG_ALL);
		}
		else {
			mainwindow_draw_map(DRAWFLAG_GEOMETRY);
		}
	}
	return TRUE;
}

// used only locally in response to clicks
static void mainwindow_map_center_on_windowpoint(gint nX, gint nY)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = nX - (GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.width / 2);
	gint16 nPixelDeltaY = nY - (GTK_WIDGET(g_MainWindow.pDrawingArea)->allocation.height / 2);

	// Convert pixels to world coordinates
	double fWorldDeltaX = map_math_pixels_to_degrees_at_scale(nPixelDeltaX, map_get_scale(g_MainWindow.pMap));
	double fWorldDeltaY = -map_math_pixels_to_degrees_at_scale(nPixelDeltaY, map_get_scale(g_MainWindow.pMap));

	mappoint_t pt;
	map_get_centerpoint(g_MainWindow.pMap, &pt);

	pt.fLatitude += fWorldDeltaY;
	pt.fLongitude += fWorldDeltaX;
	map_set_centerpoint(g_MainWindow.pMap, &pt);

	mainwindow_statusbar_update_position();
}

// used by goto window, search window, etc.
void mainwindow_map_center_on_mappoint(mappoint_t* pPoint)
{
	g_assert(pPoint != NULL);

	map_set_centerpoint(g_MainWindow.pMap, pPoint);

	mainwindow_statusbar_update_position();
}

void mainwindow_map_slide_to_mappoint(mappoint_t* pPoint)
{
	mappoint_t centerPoint;
	map_get_centerpoint(g_MainWindow.pMap, &centerPoint);

	if(map_points_equal(pPoint, &centerPoint)) return;

//     if(map_get_distance_in_pixels(g_MainWindow.pMap, pPoint, &centerPoint) < MAX_DISTANCE_FOR_AUTO_SLIDE_IN_PIXELS) {
		g_MainWindow.bSliding = TRUE;
		g_MainWindow.pAnimator = animator_new(ANIMATIONTYPE_FAST_THEN_SLIDE, SLIDE_TIME_IN_SECONDS_AUTO);

		// set startpoint
		g_MainWindow.ptSlideStartLocation.fLatitude = centerPoint.fLatitude;
		g_MainWindow.ptSlideStartLocation.fLongitude = centerPoint.fLongitude;

		// set endpoint
		g_MainWindow.ptSlideEndLocation.fLatitude = pPoint->fLatitude;
		g_MainWindow.ptSlideEndLocation.fLongitude = pPoint->fLongitude;
//     }
//     else {
//         mainwindow_map_center_on_mappoint(pPoint);  // Too far-- don't slide.  Jump instead.
//         mainwindow_add_history();                   // The slide timeout would have done this when the slide was over
//     }
}

// used by searchwindow and this window
void mainwindow_get_centerpoint(mappoint_t* pPoint)
{
	g_assert(pPoint != NULL);
	map_get_centerpoint(g_MainWindow.pMap, pPoint);
}

void mainwindow_sidebar_set_tab(gint nTab)
{
	gtk_notebook_set_current_page(g_MainWindow.pSidebarNotebook, nTab);
}

//
// History (forward / back buttons)
//
void mainwindow_update_forward_back_buttons()
{
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pForwardButton), map_history_can_go_forward(g_MainWindow.pMapHistory));
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pBackButton), map_history_can_go_back(g_MainWindow.pMapHistory));

	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pForwardMenuItem), map_history_can_go_forward(g_MainWindow.pMapHistory));
	gtk_widget_set_sensitive(GTK_WIDGET(g_MainWindow.pBackMenuItem), map_history_can_go_back(g_MainWindow.pMapHistory));
}

void mainwindow_go_to_current_history_item()
{
	mappoint_t point;
	gint nZoomLevel;
	map_history_get_current(g_MainWindow.pMapHistory, &point, &nZoomLevel);

	mainwindow_set_zoomlevel(nZoomLevel);
	mainwindow_map_center_on_mappoint(&point);
	mainwindow_draw_map(DRAWFLAG_ALL);
}

void mainwindow_on_backbutton_clicked(GtkWidget* _unused, gpointer* __unused)
{
	map_history_go_back(g_MainWindow.pMapHistory);
	mainwindow_go_to_current_history_item();
	mainwindow_update_forward_back_buttons();
}

void mainwindow_on_forwardbutton_clicked(GtkWidget* _unused, gpointer* __unused)
{
	map_history_go_forward(g_MainWindow.pMapHistory);
	mainwindow_go_to_current_history_item();
	mainwindow_update_forward_back_buttons();
}

void mainwindow_add_history()
{
	// add the current spot to the history
	mappoint_t point;
	map_get_centerpoint(g_MainWindow.pMap, &point);

	map_history_add(g_MainWindow.pMapHistory, &point, map_get_zoomlevel(g_MainWindow.pMap));

	mainwindow_update_forward_back_buttons();
}

void mainwindow_toggle_locationset_visible_checkbox(GtkTreeIter* pIter)
{
	// get locationset ID and whether it's set or not
	gboolean bEnabled;
	gint nLocationSetID;
	gtk_tree_model_get(GTK_TREE_MODEL(g_MainWindow.pLocationSetsListStore), pIter, LOCATIONSETLIST_COLUMN_ENABLED, &bEnabled, -1);
	gtk_tree_model_get(GTK_TREE_MODEL(g_MainWindow.pLocationSetsListStore), pIter, LOCATIONSETLIST_COLUMN_ID, &nLocationSetID, -1);

	// toggle selection and set GUI to reflect new value
	bEnabled = !bEnabled;
	gtk_list_store_set(g_MainWindow.pLocationSetsListStore, pIter, LOCATIONSETLIST_COLUMN_ENABLED, bEnabled, -1);

	locationset_t* pLocationSet = NULL;
	if(locationset_find_by_id(nLocationSetID, &pLocationSet)) {
		locationset_set_visible(pLocationSet, bEnabled);
	}
	else {
		g_warning("POI Set not found in list, not doing anything");
//		g_assert_not_reached();
	}

	GTK_PROCESS_MAINLOOP;		// for UI to respond before drawing
	GTK_PROCESS_MAINLOOP;		// for UI to respond before drawing
	GTK_PROCESS_MAINLOOP;		// for UI to respond before drawing

	mainwindow_draw_map(DRAWFLAG_ALL);
}

static void mainwindow_on_locationset_visible_checkbox_clicked(GtkCellRendererToggle *pCell, gchar *pszPath, gpointer pData)
{
	// get an iterator for this item
	GtkTreePath *pPath = gtk_tree_path_new_from_string(pszPath);
	GtkTreeIter iter;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(g_MainWindow.pLocationSetsListStore), &iter, pPath);
	gtk_tree_path_free (pPath);

	mainwindow_toggle_locationset_visible_checkbox(&iter);
}


void mainwindow_on_addpointmenuitem_activate(GtkWidget *_unused, gpointer* __unused)
{
	mappoint_t point;
	map_windowpoint_to_mappoint(g_MainWindow.pMap, &g_MainWindow.ptClickLocation, &point);

//     gint nLocationSetID = 1;
//     gint nNewLocationID;
//
//     if(locationset_add_location(nLocationSetID, &point, &nNewLocationID)) {
//         g_print("new location ID = %d\n", nNewLocationID);
//         mainwindow_draw_map(DRAWFLAG_ALL);
//     }
//     else {
//         g_print("insert failed\n");
//     }
}

static void mainwindow_on_web_url_clicked(GtkWidget *_unused, gchar* pszURLPattern)
{
	static util_str_replace_t apszReplacements[] = {
		{"{LAT}", NULL},
		{"{LON}", NULL},
		{"{LAT_SPAN}", NULL},
		{"{LON_SPAN}", NULL},
		{"{ZOOM_SCALE}", NULL},
		{"{ZOOM_1_TO_10}", NULL},
		{"{ZOOM_1_TO_19}", NULL},
		{"{ALTITUDE_MILES}", NULL},
	};

	mappoint_t ptCenter;
	map_get_centerpoint(g_MainWindow.pMap, &ptCenter);
	maprect_t rcVisible;
	map_get_visible_maprect(g_MainWindow.pMap, &rcVisible);

//	nAltitudeMiles = map_get_altitude(

	gdouble fZoomPercent = util_get_percent_of_range(map_get_zoomlevel(g_MainWindow.pMap), MIN_ZOOM_LEVEL, MAX_ZOOM_LEVEL);

	apszReplacements[0].pszReplace = util_format_gdouble(ptCenter.fLatitude);
	apszReplacements[1].pszReplace = util_format_gdouble(ptCenter.fLongitude);
	apszReplacements[2].pszReplace = g_strdup_printf("%f", rcVisible.B.fLatitude - rcVisible.A.fLatitude);
	apszReplacements[3].pszReplace = g_strdup_printf("%f", rcVisible.B.fLongitude - rcVisible.A.fLongitude);
	apszReplacements[4].pszReplace = g_strdup_printf("%d", map_get_scale(g_MainWindow.pMap));
	apszReplacements[5].pszReplace = g_strdup_printf("%d", util_get_int_at_percent_of_range(fZoomPercent, 1, 10));
	apszReplacements[6].pszReplace = g_strdup_printf("%d", util_get_int_at_percent_of_range(fZoomPercent, 1, 19));
//	apszReplacements[7].pszReplace = g_strdup_printf("%d", nAltitudeMiles);

	// 
	gchar* pszURL = util_str_replace_many(pszURLPattern, apszReplacements, G_N_ELEMENTS(apszReplacements));
	util_open_uri(pszURL);
	g_free(pszURL);

	// cleanup
	gint i;
	for(i=0 ; i<G_N_ELEMENTS(apszReplacements) ; i++) {
		g_free(apszReplacements[i].pszReplace);
		apszReplacements[i].pszReplace = NULL;
	}
}

void mainwindow_on_use_aa_rendering_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	map_set_antialiased(g_MainWindow.pMap, !map_get_antialiased(g_MainWindow.pMap));
	mainwindow_draw_map(DRAWFLAG_ALL);
}

#ifdef ROADSTER_DEAD_CODE
/*
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
*/
#endif /* ROADSTER_DEAD_CODE */
