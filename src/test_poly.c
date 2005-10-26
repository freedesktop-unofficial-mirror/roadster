/***************************************************************************
 *            test_poly.c
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

/*
Purpose of test_poly.c:
 - This is the debug window for testing polygon related utility functions.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include "gui.h"
#include "map.h"
#include "map_math.h"
#include "util.h"

struct {
	GtkWindow* pWindow;

	GtkHScale* pScale;
	GtkVBox* pContentBox;
	GtkButton* pClearButton;
	GtkDrawingArea* pDrawingArea;
	GtkLabel* pLabel;
	GtkCheckButton* pHideDrawingCheckButton;
	GtkCheckButton* pClipCheckButton;
	
	GArray* pPointsArray;
} g_Test_Poly;

static gboolean test_poly_on_time_to_update(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data);
static gboolean test_poly_on_mouse_button_click(GtkWidget* w, GdkEventButton *event);
static gboolean test_poly_on_clearbutton_clicked(GtkWidget* w, GdkEventButton *event);

void test_poly_init(GladeXML* pGladeXML)
{
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pWindow, GTK_WINDOW, "test_polywindow");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pScale, GTK_HSCALE, "test_poly_scale");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pContentBox, GTK_VBOX, "test_poly_contentbox");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pClearButton, GTK_BUTTON, "test_poly_clear_button");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pLabel, GTK_LABEL, "test_polylabel");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pDrawingArea, GTK_DRAWING_AREA, "test_polydrawingarea");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pHideDrawingCheckButton, GTK_CHECK_BUTTON, "test_polyhidecheck");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pClipCheckButton, GTK_CHECK_BUTTON, "test_poly_clip");

	g_Test_Poly.pPointsArray = g_array_new(FALSE, FALSE, sizeof(mappoint_t));

	// Drawing area
	gtk_widget_set_double_buffered(GTK_WIDGET(g_Test_Poly.pDrawingArea), FALSE);
	gtk_widget_add_events(GTK_WIDGET(g_Test_Poly.pDrawingArea), GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(g_Test_Poly.pDrawingArea), "expose-event", G_CALLBACK(test_poly_on_time_to_update), NULL);
	g_signal_connect(G_OBJECT(g_Test_Poly.pDrawingArea), "button_press_event", G_CALLBACK(test_poly_on_mouse_button_click), NULL);

	// Scale
	g_signal_connect(G_OBJECT(g_Test_Poly.pScale), "value-changed", G_CALLBACK(test_poly_on_time_to_update), NULL);
	// make it instant-change using our hacky callback
	//g_signal_connect(G_OBJECT(g_Test_Poly.pScale), "change-value", G_CALLBACK(util_gtk_range_instant_set_on_value_changing_callback), NULL);

	// "Clear" button
	g_signal_connect(G_OBJECT(g_Test_Poly.pClearButton), "clicked", G_CALLBACK(test_poly_on_clearbutton_clicked), NULL);

	// don't delete window on X, just hide it
	g_signal_connect(G_OBJECT(g_Test_Poly.pWindow), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
}

void test_poly_show(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_widget_show(GTK_WIDGET(g_Test_Poly.pWindow));
}

//
// callbacks etc.
//
static gboolean test_poly_on_clearbutton_clicked(GtkWidget* w, GdkEventButton *event)
{
	g_array_remove_range(g_Test_Poly.pPointsArray, 0, g_Test_Poly.pPointsArray->len);
	gtk_widget_queue_draw(GTK_WIDGET(g_Test_Poly.pDrawingArea));
	return TRUE;
}

gboolean test_poly_on_time_to_queue_draw(GtkWidget* w, GdkEventButton *event)
{
	gtk_widget_queue_draw(GTK_WIDGET(g_Test_Poly.pDrawingArea));
	return FALSE;
}

static gboolean test_poly_on_mouse_button_click(GtkWidget* w, GdkEventButton *event)
{
	gint nX, nY;
	gdk_window_get_pointer(w->window, &nX, &nY, NULL);

	gint nWidth = GTK_WIDGET(g_Test_Poly.pDrawingArea)->allocation.width;
	gint nHeight = GTK_WIDGET(g_Test_Poly.pDrawingArea)->allocation.height;

	mappoint_t point;
	point.fLongitude = (gdouble)nX / (gdouble)nWidth;
	point.fLatitude = (gdouble)nY / (gdouble)nHeight;
	g_array_append_val(g_Test_Poly.pPointsArray, point);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_Test_Poly.pHideDrawingCheckButton), FALSE);
	gtk_widget_queue_draw(GTK_WIDGET(g_Test_Poly.pDrawingArea));
	return FALSE;
}

static void test_poly_draw_array(cairo_t* pCairo, GArray* pArray)
{
	if(pArray->len >= 1) {
		mappoint_t* pPoint;
		pPoint = &g_array_index(pArray, mappoint_t, 0);
		cairo_move_to(pCairo, pPoint->fLongitude, pPoint->fLatitude);
		cairo_arc(pCairo, pPoint->fLongitude, pPoint->fLatitude, 0.02, 0, 2*M_PI);
		cairo_fill(pCairo);
		cairo_move_to(pCairo, pPoint->fLongitude, pPoint->fLatitude);
		gint i;
		for(i=1 ; i<pArray->len ;i++) {
			pPoint = &g_array_index(pArray, mappoint_t, i);
			cairo_line_to(pCairo, pPoint->fLongitude, pPoint->fLatitude);
		}
		pPoint = &g_array_index(pArray, mappoint_t, 0);
		cairo_line_to(pCairo, pPoint->fLongitude, pPoint->fLatitude);
	}
}

static gboolean test_poly_on_time_to_update(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data)
{
	Display* dpy = gdk_x11_drawable_get_xdisplay(GTK_WIDGET(g_Test_Poly.pDrawingArea)->window);
	Visual *visual = DefaultVisual (dpy, DefaultScreen (dpy));
	Drawable drawable = gdk_x11_drawable_get_xid(GTK_WIDGET(g_Test_Poly.pDrawingArea)->window);
	gint width, height;
	gdk_drawable_get_size (GTK_WIDGET(g_Test_Poly.pDrawingArea)->window, &width, &height);
	cairo_surface_t *pSurface = cairo_xlib_surface_create (dpy, drawable, visual, width, height);

	gdouble fValue = gtk_range_get_value(GTK_RANGE(g_Test_Poly.pScale));

	cairo_t* pCairo = cairo_create (pSurface);

	cairo_scale(pCairo, width, height);

	cairo_set_source_rgba(pCairo, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(pCairo, 0.0, 0.0, 1.0, 1.0);
	cairo_fill(pCairo);

	GArray* pSimplified = g_array_new(FALSE, FALSE, sizeof(mappoint_t));

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_Test_Poly.pClipCheckButton)) == TRUE) {
		maprect_t rcClipper;
		rcClipper.A.fLatitude = rcClipper.A.fLongitude = 0.25;
		rcClipper.B.fLatitude = rcClipper.B.fLongitude = 0.75;


		if(g_Test_Poly.pPointsArray->len > 0) {
			GArray* pClipped = g_array_new(FALSE, FALSE, sizeof(mappoint_t));
			mappoint_t ptFirst = g_array_index(g_Test_Poly.pPointsArray, mappoint_t, 0);
			g_array_append_val(g_Test_Poly.pPointsArray, ptFirst);
				map_math_clip_pointstring_to_worldrect(g_Test_Poly.pPointsArray, &rcClipper, pClipped);
			g_array_remove_index(g_Test_Poly.pPointsArray, g_Test_Poly.pPointsArray->len-1);

			// Simplify
			map_math_simplify_pointstring(pClipped, fValue, pSimplified);

			g_array_free(pClipped, TRUE);
		}

		// Draw clip rectangle
		cairo_save(pCairo);
		cairo_set_source_rgba(pCairo, 0.0, 0.0, 0.0, 1.0);
		cairo_set_line_width(pCairo, 0.005);
		cairo_rectangle(pCairo, rcClipper.A.fLongitude, rcClipper.A.fLatitude, (rcClipper.B.fLongitude - rcClipper.A.fLongitude), (rcClipper.B.fLatitude - rcClipper.A.fLatitude));
		cairo_stroke(pCairo);
		cairo_restore(pCairo);
	}
	else {
		// Simplify
		map_math_simplify_pointstring(g_Test_Poly.pPointsArray, fValue, pSimplified);
	}

	// Draw pSimplified filled
	cairo_save(pCairo);
	cairo_set_source_rgba(pCairo, 0.0, 1.0, 0.0, 1.0);
	test_poly_draw_array(pCairo, pSimplified);
	cairo_fill(pCairo);
	cairo_restore(pCairo);

	// Draw lines
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_Test_Poly.pHideDrawingCheckButton)) == FALSE) {
		cairo_save(pCairo);
		cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_ROUND);
		cairo_set_line_width(pCairo, 0.02);
		cairo_set_source_rgba(pCairo, 1.0, 0.0, 0.0, 1.0);
		test_poly_draw_array(pCairo, g_Test_Poly.pPointsArray);
		cairo_stroke(pCairo);
		cairo_restore(pCairo);
	}

	cairo_destroy(pCairo);

	if(g_Test_Poly.pPointsArray->len == 0) {
		gtk_label_set_markup(g_Test_Poly.pLabel, "<b>Click to create points</b>");
	}
	else {
		gchar* pszMarkup = g_strdup_printf("%d points, %d simplified", g_Test_Poly.pPointsArray->len, pSimplified->len);
		gtk_label_set_markup(g_Test_Poly.pLabel, pszMarkup);
		g_free(pszMarkup);
	}

	g_array_free(pSimplified, TRUE);
	return TRUE;
}
