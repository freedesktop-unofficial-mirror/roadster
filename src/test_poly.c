#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include "gui.h"
#include "map.h"
#include "util.h"

struct {
	GtkWindow* pWindow;

	GtkHScale* pScale;
	GtkVBox* pContentBox;
	GtkButton* pClearButton;
	GtkDrawingArea* pDrawingArea;
	GtkLabel* pLabel;
	
	GArray* pPointsArray;
} g_Test_Poly;

static gboolean on_time_to_update(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data);
static void test_poly_draw();
static gboolean on_mouse_button_click(GtkWidget* w, GdkEventButton *event);
static gboolean on_clear_clicked(GtkWidget* w, GdkEventButton *event);

void test_poly_init(GladeXML* pGladeXML)
{
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pWindow, GTK_WINDOW, "test_polywindow");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pScale, GTK_HSCALE, "test_poly_scale");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pContentBox, GTK_VBOX, "test_poly_contentbox");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pClearButton, GTK_BUTTON, "test_poly_clear_button");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pLabel, GTK_LABEL, "test_polylabel");
	GLADE_LINK_WIDGET(pGladeXML, g_Test_Poly.pDrawingArea, GTK_DRAWING_AREA, "test_polydrawingarea");

	g_Test_Poly.pPointsArray = g_array_new(FALSE, FALSE, sizeof(mappoint_t));

	// Drawing area
	gtk_widget_set_double_buffered(GTK_WIDGET(g_Test_Poly.pDrawingArea), FALSE);
	gtk_widget_add_events(GTK_WIDGET(g_Test_Poly.pDrawingArea), GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(g_Test_Poly.pDrawingArea), "expose-event", G_CALLBACK(on_time_to_update), NULL);
	g_signal_connect(G_OBJECT(g_Test_Poly.pDrawingArea), "button_press_event", G_CALLBACK(on_mouse_button_click), NULL);

	// Scale
	g_signal_connect(G_OBJECT(g_Test_Poly.pScale), "value-changed", G_CALLBACK(on_time_to_update), NULL);
	// make it instant-change using our hacky callback
	//g_signal_connect(G_OBJECT(g_Test_Poly.pScale), "change-value", G_CALLBACK(util_gtk_range_instant_set_on_value_changing_callback), NULL);

	// "Clear" button
	g_signal_connect(G_OBJECT(g_Test_Poly.pClearButton), "clicked", G_CALLBACK(on_clear_clicked), NULL);

	// don't delete window on X, just hide it
	g_signal_connect(G_OBJECT(g_Test_Poly.pWindow), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
}

void test_poly_show(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_widget_show(GTK_WIDGET(g_Test_Poly.pWindow));
}

static gboolean on_clear_clicked(GtkWidget* w, GdkEventButton *event)
{
	g_array_remove_range(g_Test_Poly.pPointsArray, 0, g_Test_Poly.pPointsArray->len);
	gtk_widget_queue_draw(GTK_WIDGET(g_Test_Poly.pDrawingArea));
}

static gboolean on_mouse_button_click(GtkWidget* w, GdkEventButton *event)
{
	gint nX, nY;
	gdk_window_get_pointer(w->window, &nX, &nY, NULL);

	gint nWidth = GTK_WIDGET(g_Test_Poly.pDrawingArea)->allocation.width;
	gint nHeight = GTK_WIDGET(g_Test_Poly.pDrawingArea)->allocation.height;

	mappoint_t point;
	point.fLongitude = (gdouble)nX / (gdouble)nWidth;
	point.fLatitude = (gdouble)nY / (gdouble)nHeight;
	g_array_append_val(g_Test_Poly.pPointsArray, point);

	gtk_widget_queue_draw(GTK_WIDGET(g_Test_Poly.pDrawingArea));
}

void test_poly_draw_array(cairo_t* pCairo, GArray* pArray)
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

static gboolean on_time_to_update(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data)
{
	Display* dpy;
	Drawable drawable;
	dpy = gdk_x11_drawable_get_xdisplay(GTK_WIDGET(g_Test_Poly.pDrawingArea)->window);
	Visual *visual = DefaultVisual (dpy, DefaultScreen (dpy));
	gint width, height;
	drawable = gdk_x11_drawable_get_xid(GTK_WIDGET(g_Test_Poly.pDrawingArea)->window);
	gdk_drawable_get_size (GTK_WIDGET(g_Test_Poly.pDrawingArea)->window, &width, &height);
	cairo_surface_t *pSurface = cairo_xlib_surface_create (dpy, drawable, visual, width, height);

	gdouble fValue = gtk_range_get_value(g_Test_Poly.pScale);

	cairo_t* pCairo = cairo_create (pSurface);

	cairo_scale(pCairo, width, height);

	cairo_set_source_rgba(pCairo, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(pCairo, 0.0, 0.0, 1.0, 1.0);
	cairo_fill(pCairo);

	// Draw lines
	cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_ROUND);
	cairo_save(pCairo);
	cairo_set_line_width(pCairo, 0.02);
	cairo_set_source_rgba(pCairo, 1.0, 0.0, 0.0, 1.0);
	test_poly_draw_array(pCairo, g_Test_Poly.pPointsArray);
	cairo_stroke(pCairo);
	cairo_restore(pCairo);

	cairo_save(pCairo);
	GArray* pSimplified = g_array_new(FALSE, FALSE, sizeof(mappoint_t));
	map_math_simplify_pointstring(g_Test_Poly.pPointsArray, fValue, pSimplified);
	cairo_set_line_width(pCairo, 0.01);
	cairo_set_source_rgba(pCairo, 0.0, 1.0, 0.0, 0.5);
	test_poly_draw_array(pCairo, pSimplified);
	cairo_fill(pCairo);
	cairo_restore(pCairo);

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

// static void paint (GtkWidget      *widget,GdkEventExpose *eev,gpointer        data)
// {
//   gint width, height;
//   gint i;
//   cairo_t *cr;
//
//   width  = widget->allocation.width;
//   height = widget->allocation.height;
//
//   cr = gdk_cairo_create (widget->window);
//
//     /* clear background */
//     cairo_set_source_rgb (cr, 1,1,1);
//     cairo_paint (cr);
//
//
//     cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
//                                         CAIRO_FONT_WEIGHT_BOLD);
//
//     /* enclosing in a save/restore pair since we alter the
//      * font size
//      */
//     cairo_save (cr);
//       cairo_set_font_size (cr, 40);
//       cairo_move_to (cr, 40, 60);
//       cairo_set_source_rgb (cr, 0,0,0);
//       cairo_show_text (cr, "Hello World");
//     cairo_restore (cr);
//
//     cairo_set_source_rgb (cr, 1,0,0);
//     cairo_set_font_size (cr, 20);
//     cairo_move_to (cr, 50, 100);
//     cairo_show_text (cr, "greetings from gtk and cairo");
//
//     cairo_set_source_rgb (cr, 0,0,1);
//
//     cairo_move_to (cr, 0, 150);
//     for (i=0; i< width/10; i++)
//       {
//         cairo_rel_line_to (cr, 5,  10);
//         cairo_rel_line_to (cr, 5, -10);
//       }
//     cairo_stroke (cr);
//
//   cairo_destroy (cr);
// }
//
// static gboolean on_expose_event(GtkWidget *pDrawingArea, GdkEventExpose *event, gpointer data)
// {
// }
