/***************************************************************************
 *            searchwindow.c
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

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glade/glade.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

//#define ENABLE_SLEEP_DURING_SEARCH_HACK		// just for testing GUI behavior

#include "main.h"
#include "search.h"
//#include "search_road.h"
//#include "search_location.h"
//#include "search_city.h"
#include "mainwindow.h"
#include "searchwindow.h"
#include "util.h"
#include "gui.h"

#define RESULTLIST_COLUMN_NAME 			0	// visible data
#define RESULTLIST_COLUMN_LATITUDE		1
#define RESULTLIST_COLUMN_LONGITUDE		2
#define RESULTLIST_COLUMN_DISTANCE		3
#define RESULTLIST_COLUMN_ZOOMLEVEL		4
#define RESULTLIST_COLUMN_CLICKABLE		5
#define RESULTLIST_COLUMN_PIXBUF		6
#define RESULTLIST_COLUMN_SORT			7

#define MAGIC_GTK_NO_SORT_COLUMN (-2)	// why -2?  dunno.  is there a real define for this?  dunno.

#define SEARCHWINDOW_RESULT_FORMAT	("<span size='small'>%s</span>")
#define SEARCHWINDOW_INFO_FORMAT	("<span size='small'><i>%s</i></span>")

struct {
	GtkEntry* pSearchEntry;		// search text box (on the toolbar)
	GtkComboBoxEntry* pSearchComboBoxEntry;

	GtkEntry* pSearchLocationEntry;		// search text box (on the toolbar)

	GtkButton* pSearchButton;		// search button (on the toolbar)

	// results list (on the sidebar)
	GtkTreeView* pResultsTreeView;
	GtkTreeViewColumn* pResultsTreeViewGlyphColumn;
	GtkListStore* pResultsListStore;

	GHashTable* pResultsHashTable;

	GtkMenuItem* pNextSearchResultMenuItem;
	GtkMenuItem* pPreviousSearchResultMenuItem;

	gint nNumResults;
} g_SearchWindow = {0};

static void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data);
static void searchwindow_set_message(gchar* pszMessage);
static void searchwindow_update_next_and_prev_buttons(void);
static void searchwindow_go_to_selected_result(void);

void searchwindow_init(GladeXML* pGladeXML)
{
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pSearchEntry, GTK_ENTRY, "searchentry");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pSearchComboBoxEntry, GTK_COMBO_BOX_ENTRY, "searchcomboboxentry");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pSearchLocationEntry, GTK_ENTRY, "searchlocationentry");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pSearchButton, GTK_BUTTON, "searchbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pResultsTreeView, GTK_TREE_VIEW, "searchresultstreeview");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pNextSearchResultMenuItem, GTK_MENU_ITEM, "nextresultmenuitem");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.pPreviousSearchResultMenuItem, GTK_MENU_ITEM, "previousresultmenuitem");

	// create results tree view
	g_SearchWindow.pResultsListStore = gtk_list_store_new(8, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_INT, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_DOUBLE);
	gtk_tree_view_set_model(g_SearchWindow.pResultsTreeView, GTK_TREE_MODEL(g_SearchWindow.pResultsListStore));
//	gtk_tree_view_set_search_equal_func(g_SearchWindow.pResultsTreeView, util_treeview_match_all_words_callback, NULL, NULL);

	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

		// NEW COLUMN: "Icon"
		pCellRenderer = gtk_cell_renderer_pixbuf_new();
		g_object_set(G_OBJECT(pCellRenderer), "xpad", 2, NULL);
		pColumn = gtk_tree_view_column_new_with_attributes("", pCellRenderer, "pixbuf", RESULTLIST_COLUMN_PIXBUF, NULL);
		gtk_tree_view_append_column(g_SearchWindow.pResultsTreeView, pColumn);

		g_SearchWindow.pResultsTreeViewGlyphColumn = pColumn;

		// NEW COLUMN: "Name" (with a text renderer)
		pCellRenderer = gtk_cell_renderer_text_new();
			g_object_set(G_OBJECT(pCellRenderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
		pColumn = gtk_tree_view_column_new_with_attributes("Road", pCellRenderer, "markup", RESULTLIST_COLUMN_NAME, NULL);
		gtk_tree_view_append_column(g_SearchWindow.pResultsTreeView, pColumn);

	// Attach handler for selection-changed signal
	GtkTreeSelection *pTreeSelection = gtk_tree_view_get_selection(g_SearchWindow.pResultsTreeView);
	g_signal_connect(G_OBJECT(pTreeSelection), "changed", (GtkSignalFunc)searchwindow_on_resultslist_selection_changed, NULL);

	searchwindow_update_next_and_prev_buttons();

//	util_gtk_entry_add_hint_text(g_SearchWindow.pSearchEntry, "type place or address");
	util_gtk_entry_add_hint_text(g_SearchWindow.pSearchLocationEntry, "near here");

	g_SearchWindow.pSearchComboBoxEntry;
	GtkComboBoxEntry a;
}

void searchwindow_clear_results(void)
{
	// remove all search results from the list
	if(g_SearchWindow.pResultsListStore != NULL) {
		gtk_list_store_clear(g_SearchWindow.pResultsListStore);
	}
	g_SearchWindow.nNumResults = 0;

	// Scroll window all the way left.  (Vertical scroll is reset by emptying the list.)
    gtk_adjustment_set_value(gtk_tree_view_get_hadjustment(g_SearchWindow.pResultsTreeView), 0.0);
	
	searchwindow_update_next_and_prev_buttons();
}

static void searchwindow_update_next_and_prev_buttons()
{
	gtk_widget_set_sensitive(g_SearchWindow.pNextSearchResultMenuItem, g_SearchWindow.nNumResults > 0);
	gtk_widget_set_sensitive(g_SearchWindow.pPreviousSearchResultMenuItem, g_SearchWindow.nNumResults > 0);
}

void searchwindow_set_message(gchar* pszMessage)
{
	// Add a basic text message to the list, instead of a search result (eg. "no results")
	GtkTreeIter iter;
	gtk_list_store_append(g_SearchWindow.pResultsListStore, &iter);
	gtk_list_store_set(g_SearchWindow.pResultsListStore, &iter, 
					   RESULTLIST_COLUMN_NAME, pszMessage, 
					   RESULTLIST_COLUMN_CLICKABLE, FALSE,
					   -1);

	g_object_set(G_OBJECT(g_SearchWindow.pResultsTreeViewGlyphColumn), "visible", FALSE, NULL);
}

// Begin a search
void searchwindow_on_findbutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	g_print("Searching...\n");

	// NOTE: By setting the SEARCH button inactive, we prevent this code from becoming reentrant when
	// we call GTK_PROCESS_MAINLOOP below.
	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.pSearchButton), FALSE);

	// XXX: make list unsorted (sorting once at the end is much faster than for each insert)
	//gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(g_SearchWindow.pResultsListStore), MAGIC_GTK_NO_SORT_COLUMN, GTK_SORT_ASCENDING);

	searchwindow_clear_results();

	g_SearchWindow.pResultsHashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	const gchar* pszSearch = gtk_entry_get_text(g_SearchWindow.pSearchEntry);	// NOTE: do NOT free this pointer

	// ensure the search results are visible
	mainwindow_sidebar_set_tab(SIDEBAR_TAB_SEARCH_RESULTS);
	mainwindow_set_sidebox_visible(TRUE);

	gchar* pszBuffer = g_strdup_printf(SEARCHWINDOW_INFO_FORMAT, "Searching...");
	searchwindow_set_message(pszBuffer);
	g_free(pszBuffer);

	void* pBusy = mainwindow_set_busy();

	// XXX: Set search entry to use its parent's busy cursor (why do we need to do this?)
	gdk_window_set_cursor(g_SearchWindow.pSearchEntry->text_area, NULL);

	// Let GTK update the treeview to show the message
	GTK_PROCESS_MAINLOOP;	// see note above

	// Start the search!
	search_all(pszSearch);

#ifdef ENABLE_SLEEP_DURING_SEARCH_HACK
	sleep(5);	// good for seeing how the UI behaves during long searches
#endif
	mainwindow_set_not_busy(&pBusy);

	// HACK: Set a cursor for the private 'text_area' member of the search GtkEntry (otherwise it won't show busy cursor)
	GdkCursor* pCursor = gdk_cursor_new(GDK_XTERM);
	gdk_window_set_cursor(g_SearchWindow.pSearchEntry->text_area, pCursor);

	if(g_SearchWindow.nNumResults == 0) {
		searchwindow_clear_results();

		// insert a "no results" message
		gchar* pszBuffer = g_strdup_printf(SEARCHWINDOW_INFO_FORMAT, "No search results.");
		searchwindow_set_message(pszBuffer);
		g_free(pszBuffer);
	}
	else {
		// Show icon column
		g_object_set(G_OBJECT(g_SearchWindow.pResultsTreeViewGlyphColumn), "visible", TRUE, NULL);

		// Sort the list
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(g_SearchWindow.pResultsListStore), RESULTLIST_COLUMN_SORT, GTK_SORT_ASCENDING);
	}

	g_hash_table_destroy(g_SearchWindow.pResultsHashTable);
	
	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.pSearchButton), TRUE);
	searchwindow_update_next_and_prev_buttons();
}

// add a result row to the list
void searchwindow_add_result(ESearchResultType eResultType, const gchar* pszText, glyph_t* pGlyph, mappoint_t* pPoint, gint nZoomLevel)
{
	GtkTreeIter iter;

	if(g_SearchWindow.nNumResults == 0) {
		// Clear any messages
		searchwindow_clear_results();
	}

	if(!g_utf8_validate(pszText, -1, NULL)) {
		g_warning("Search result not UTF-8: '%s'\n", pszText);
		return;
	}

	if(g_hash_table_lookup(g_SearchWindow.pResultsHashTable, pszText)) {
		// duplicate found
		return;
	}
	g_hash_table_insert(g_SearchWindow.pResultsHashTable, g_strdup(pszText), (gpointer)TRUE);

	mappoint_t ptCenter;
	mainwindow_get_centerpoint(&ptCenter);

	gdouble fDistance = map_get_straight_line_distance_in_degrees(&ptCenter, pPoint);

	gchar* pszTmp = g_strdup_printf("%d.%08d", eResultType, (gint)(fDistance * 100000.0));	// move decimal over 5 places
	gdouble fSort = g_ascii_strtod(pszTmp, NULL);
//	g_print("distance=%f, string='%s', sort=%f\n", fDistance, pszTmp, fSort);
	g_free(pszTmp);

	gchar* pszBuffer = g_strdup_printf(SEARCHWINDOW_RESULT_FORMAT, pszText);

//	glyph_t* pGlyph = search_glyph_for_search_result_type(eResultType);
//	g_assert(pGlyph != NULL);
	GdkPixbuf* pRowPixbuf = (pGlyph != NULL) ? glyph_get_pixbuf(pGlyph) : NULL; 
//	g_assert(pRowPixbuf != NULL);

	gtk_list_store_append(g_SearchWindow.pResultsListStore, &iter);
	gtk_list_store_set(g_SearchWindow.pResultsListStore, &iter,
		RESULTLIST_COLUMN_NAME, pszBuffer,
		RESULTLIST_COLUMN_LATITUDE, pPoint->fLatitude,
		RESULTLIST_COLUMN_LONGITUDE, pPoint->fLongitude,
		RESULTLIST_COLUMN_DISTANCE, fDistance,
		RESULTLIST_COLUMN_ZOOMLEVEL, nZoomLevel,
		RESULTLIST_COLUMN_CLICKABLE, TRUE,
		RESULTLIST_COLUMN_PIXBUF, pRowPixbuf,
		RESULTLIST_COLUMN_SORT, fSort,
		-1);

	g_free(pszBuffer);

	g_SearchWindow.nNumResults++;
}

static void searchwindow_go_to_selected_result(void)
{
	GtkTreeSelection* pSelection = gtk_tree_view_get_selection(g_SearchWindow.pResultsTreeView);

	GtkTreeIter iter;
	GtkTreeModel* pModel = GTK_TREE_MODEL(g_SearchWindow.pResultsListStore);
	if(gtk_tree_selection_get_selected(pSelection, &pModel, &iter)) {
		mappoint_t pt;
		gint nZoomLevel;
		gboolean bClickable;
		gtk_tree_model_get(GTK_TREE_MODEL(g_SearchWindow.pResultsListStore), &iter,
			RESULTLIST_COLUMN_LATITUDE, &pt.fLatitude,
			RESULTLIST_COLUMN_LONGITUDE, &pt.fLongitude,
			RESULTLIST_COLUMN_ZOOMLEVEL, &nZoomLevel,
			RESULTLIST_COLUMN_CLICKABLE, &bClickable,
			-1);

		if(!bClickable) return;	// XXX: is this the right way to make a treeview item not clickable?

		// XXX: Slide or jump?  Should this be a setting?
		//mainwindow_map_slide_to_mappoint(&pt);
		mainwindow_set_zoomlevel(nZoomLevel);
		mainwindow_map_center_on_mappoint(&pt);

		void* pBusy = mainwindow_set_busy();
		mainwindow_draw_map(DRAWFLAG_ALL);
		mainwindow_set_not_busy(&pBusy);
	}
}

void searchwindow_on_resultslist_row_activated(GtkWidget *pWidget, gpointer* p)
{
	// XXX: Double-click on a search result.  Do we want to support this?
	// We should prevent a double click on an unselected row from firing both this and selection_changed
	//searchwindow_go_to_selected_result();
}

static void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
	searchwindow_go_to_selected_result();
	searchwindow_update_next_and_prev_buttons();
}

void searchwindow_on_nextresultbutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	util_gtk_tree_view_select_next(g_SearchWindow.pResultsTreeView);
	searchwindow_update_next_and_prev_buttons();
}

void searchwindow_on_previousresultbutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	util_gtk_tree_view_select_previous(g_SearchWindow.pResultsTreeView);
	searchwindow_update_next_and_prev_buttons();
}

void searchwindow_on_searchentry_changed(GtkWidget *pWidget, gpointer* p)
{
	// Clear results when search entry changes(?)
	//searchwindow_clear_results();
//     const gchar* pszSearch = gtk_entry_get_text(g_SearchWindow.pSearchEntry);   // NOTE: do NOT free this pointer
//     if(pszSearch[0] != '\0') {
//         gchar* pszBuffer = g_strdup_printf(SEARCHWINDOW_INFO_FORMAT, "Hit <b>Enter</b> to search.");
//         searchwindow_set_message(pszBuffer);
//         g_free(pszBuffer);
//     }

	//
	//gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.pSearchButton), TRUE);
}

