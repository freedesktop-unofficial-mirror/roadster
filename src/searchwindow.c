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

#include "main.h"
#include "search_road.h"
#include "search_location.h"
#include "mainwindow.h"
#include "searchwindow.h"
#include "util.h"
#include "gui.h"

#define RESULTLIST_COLUMN_NAME 	0	// visible data
#define RESULTLIST_LATITUDE		1
#define RESULTLIST_LONGITUDE	2
#define RESULTLIST_DISTANCE		3
#define RESULTLIST_ZOOMLEVEL	4
#define RESULTLIST_CLICKABLE	5

#define MAGIC_GTK_NO_SORT_COLUMN (-2)	// why -2?  dunno.  is there a real define for this?  dunno.

#define SEARCHWINDOW_RESULT_FORMAT	("<span size='small'>%s</span>")
#define SEARCHWINDOW_INFO_FORMAT	("<span size='small'><i>%s</i></span>")

struct {
	GtkEntry* m_pSearchEntry;		// search text box (on the toolbar)
	GtkButton* m_pSearchButton;		// search button (on the toolbar)

	// results list (on the sidebar)
	GtkTreeView* m_pResultsTreeView;
	GtkListStore* m_pResultsListStore;

	gint m_nNumResults;
} g_SearchWindow = {0};

static void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data);

void searchwindow_init(GladeXML* pGladeXML)
{
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.m_pSearchEntry, GTK_ENTRY, "searchentry");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.m_pSearchButton, GTK_BUTTON, "searchbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_SearchWindow.m_pResultsTreeView, GTK_TREE_VIEW, "searchresultstreeview");

	// create results tree view
	g_SearchWindow.m_pResultsListStore = gtk_list_store_new(6, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_INT, G_TYPE_BOOLEAN);
	gtk_tree_view_set_model(g_SearchWindow.m_pResultsTreeView, GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore));

	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

	// add Name column (with a text renderer)
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Road", pCellRenderer, "markup", RESULTLIST_COLUMN_NAME, NULL);
	gtk_tree_view_append_column(g_SearchWindow.m_pResultsTreeView, pColumn);	

	// attach handler for selection-changed signal
	GtkTreeSelection *pTreeSelection = gtk_tree_view_get_selection(g_SearchWindow.m_pResultsTreeView);
	g_signal_connect(G_OBJECT(pTreeSelection), "changed", (GtkSignalFunc)searchwindow_on_resultslist_selection_changed, NULL);
}

void searchwindow_clear_results(void)
{
	// remove all search results from the list
	if(g_SearchWindow.m_pResultsListStore != NULL) {
		gtk_list_store_clear(g_SearchWindow.m_pResultsListStore);
	}
	g_SearchWindow.m_nNumResults = 0;
}

void searchwindow_add_message(gchar* pszMessage)
{
	// Add a basic text message to the list, instead of a search result (eg. "no results")
	GtkTreeIter iter;
	gtk_list_store_append(g_SearchWindow.m_pResultsListStore, &iter);
	gtk_list_store_set(g_SearchWindow.m_pResultsListStore, &iter, 
					   RESULTLIST_COLUMN_NAME, pszMessage, 
					   RESULTLIST_CLICKABLE, FALSE,
					   -1);
}

void searchwindow_on_findbutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	// Begin a search

	// XXX: make list unsorted (sorting once at the end is much faster than for each insert)
	//gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(g_SearchWindow.m_pResultsListStore), MAGIC_GTK_NO_SORT_COLUMN, GTK_SORT_ASCENDING);

	searchwindow_clear_results();

	const gchar* pszSearch = gtk_entry_get_text(g_SearchWindow.m_pSearchEntry);

	if(pszSearch[0] == '\0') {
		gchar* pszBuffer = g_strdup_printf(SEARCHWINDOW_INFO_FORMAT, "Type search words above.");
		searchwindow_add_message(pszBuffer);
		g_free(pszBuffer);
	}
	else {
		void* pBusy = mainwindow_set_busy();
		search_road_execute(pszSearch);
		search_location_execute(pszSearch);
		mainwindow_set_not_busy(&pBusy);

		if(g_SearchWindow.m_nNumResults == 0) {
			// insert a "no results" message
			gchar* pszBuffer = g_strdup_printf(SEARCHWINDOW_INFO_FORMAT, "No results.");
			searchwindow_add_message(pszBuffer);
			g_free(pszBuffer);
		}
		// Sort the list by distance from viewer
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(g_SearchWindow.m_pResultsListStore), RESULTLIST_DISTANCE, GTK_SORT_ASCENDING);
	}
	// ensure the search results are visible
	mainwindow_sidebar_set_tab(SIDEBAR_TAB_SEARCH_RESULTS);
	mainwindow_set_sidebox_visible(TRUE);
}

// add a result row to the list
void searchwindow_add_result(const gchar* pszText, mappoint_t* pPoint, gint nZoomLevel)
{
	GtkTreeIter iter;

	mappoint_t ptCenter;
	mainwindow_get_centerpoint(&ptCenter);

	gdouble fDistance = map_get_distance_in_meters(&ptCenter, pPoint);

	gchar* pszBuffer = NULL;
	if(g_utf8_validate(pszText, -1, NULL)) {
		pszBuffer = g_markup_printf_escaped(SEARCHWINDOW_RESULT_FORMAT, pszText);
	}
	else {
		g_warning("Search result not UTF-8: '%s'\n", pszText);
		pszBuffer = g_strdup_printf(SEARCHWINDOW_RESULT_FORMAT, "<i>Invalid Name</i>");
	}

//	g_print("Adding: (%f,%f) (%f) %s\n", pPoint->m_fLatitude, pPoint->m_fLongitude, fDistance, pszBuffer);

	gtk_list_store_append(g_SearchWindow.m_pResultsListStore, &iter);
	gtk_list_store_set(g_SearchWindow.m_pResultsListStore, &iter,
		RESULTLIST_COLUMN_NAME, pszBuffer,
		RESULTLIST_LATITUDE, pPoint->m_fLatitude,
		RESULTLIST_LONGITUDE, pPoint->m_fLongitude,
		RESULTLIST_DISTANCE, fDistance,
		RESULTLIST_ZOOMLEVEL, nZoomLevel,
		RESULTLIST_CLICKABLE, TRUE,
		-1);

	g_free(pszBuffer);

	g_SearchWindow.m_nNumResults++;
}

static void searchwindow_go_to_selected_result(void)
{
	GtkTreeSelection* pSelection = gtk_tree_view_get_selection(g_SearchWindow.m_pResultsTreeView);

	GtkTreeIter iter;
	GtkTreeModel* pModel = GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore);
	if(gtk_tree_selection_get_selected(pSelection, &pModel, &iter)) {
		mappoint_t pt;
		gint nZoomLevel;
		gboolean bClickable;
		gtk_tree_model_get(GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore), &iter,
			RESULTLIST_LATITUDE, &pt.m_fLatitude,
			RESULTLIST_LONGITUDE, &pt.m_fLongitude,
			RESULTLIST_ZOOMLEVEL, &nZoomLevel,
			RESULTLIST_CLICKABLE, &bClickable,
			-1);

		if(!bClickable) return;	// XXX: is this the right way to make a treeview item not clickable?

		// XXX: Slide or jump?  Should this be a setting?
//		mainwindow_map_slide_to_mappoint(&pt);
		mainwindow_set_zoomlevel(nZoomLevel);
		mainwindow_map_center_on_mappoint(&pt);
		mainwindow_draw_map(DRAWFLAG_ALL);
	}
}

void searchwindow_on_addressresultstreeview_row_activated(GtkWidget *pWidget, gpointer* p)
{
	searchwindow_go_to_selected_result();
}

static void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
//	GTK_PROCESS_MAINLOOP;	// make sure GUI updates before we start our cpu-intensive move to the result
	searchwindow_go_to_selected_result();
}
