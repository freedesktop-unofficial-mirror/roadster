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
#include <glade/glade.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "search_road.h"
#include "search_location.h"
#include "mainwindow.h"
#include "searchwindow.h"

#define GTK_PROCESS_MAINLOOP  while (gtk_events_pending ()) { gtk_main_iteration (); }

#define RESULTLIST_COLUMN_NAME 	0
#define RESULTLIST_LATITUDE	1
#define RESULTLIST_LONGITUDE	2


struct {
	// window
	GtkComboBox* m_pSearchTypeComboBox;		// choose between types of searches

	// search text box (on the toolbar)
	GtkEntry* m_pSearchEntry;

	// search button (on the toolbar)
	GtkButton* m_pSearchButton;

	// results list (on the sidebar)
	GtkTreeView* m_pResultsTreeView;
	GtkListStore* m_pResultsListStore;
	
	// go to selected result button (on the sidebar)
	GtkButton* m_pGoButton;
} g_SearchWindow = {0};

static void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data);

void searchwindow_init(GladeXML* pGladeXML)
{
	g_SearchWindow.m_pSearchEntry 		= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "searchentry"));			g_return_if_fail(g_SearchWindow.m_pSearchEntry != NULL);	
	g_SearchWindow.m_pSearchButton		= GTK_BUTTON(glade_xml_get_widget(pGladeXML, "searchbutton"));			g_return_if_fail(g_SearchWindow.m_pSearchButton != NULL);	
	g_SearchWindow.m_pResultsTreeView	= GTK_TREE_VIEW(glade_xml_get_widget(pGladeXML, "searchresultstreeview"));	g_return_if_fail(g_SearchWindow.m_pResultsTreeView != NULL);	
	g_SearchWindow.m_pGoButton		= GTK_BUTTON(glade_xml_get_widget(pGladeXML, "searchgobutton"));		g_return_if_fail(g_SearchWindow.m_pGoButton != NULL);	

	// create results tree view
	g_SearchWindow.m_pResultsListStore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
	gtk_tree_view_set_model(g_SearchWindow.m_pResultsTreeView, GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore));

	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

	// add Name column (with a text renderer)
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Road", pCellRenderer, "markup", RESULTLIST_COLUMN_NAME, NULL);
	gtk_tree_view_append_column(g_SearchWindow.m_pResultsTreeView, pColumn);	

	/* attach handler for selection-changed signal */
	GtkTreeSelection *pTreeSelection = gtk_tree_view_get_selection(g_SearchWindow.m_pResultsTreeView);
	g_signal_connect(G_OBJECT(pTreeSelection), "changed", (GtkSignalFunc)searchwindow_on_resultslist_selection_changed, NULL);
}

void searchwindow_clear_results(void)
{
	// remove all search results from the list
	if(g_SearchWindow.m_pResultsListStore != NULL) {
		gtk_list_store_clear(g_SearchWindow.m_pResultsListStore);
	}
}

// begin a search
void searchwindow_on_findbutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	g_print("begin search\n");

	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.m_pSearchButton), FALSE);

	searchwindow_clear_results();

	const gchar* pszSearch = gtk_entry_get_text(g_SearchWindow.m_pSearchEntry);

	void* pBusy = mainwindow_set_busy();
	search_road_execute(pszSearch);
	mainwindow_set_not_busy(&pBusy);

	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.m_pSearchButton), TRUE);
}

// add a result row to the list
void searchwindow_add_result(const gchar* pszText, mappoint_t* pPoint)
{
	GtkTreeIter iter;

	gchar* pszBuffer = g_strdup_printf("<span size='small'>%s</span>", pszText);
	gtk_list_store_append(g_SearchWindow.m_pResultsListStore, &iter);
	gtk_list_store_set(g_SearchWindow.m_pResultsListStore, &iter,
		RESULTLIST_COLUMN_NAME, pszBuffer,
		RESULTLIST_LATITUDE, pPoint->m_fLatitude,
		RESULTLIST_LONGITUDE, pPoint->m_fLongitude,		
		-1);
	g_free(pszBuffer);
}

static void searchwindow_go_to_selected_result(void)
{
	GtkTreeSelection* pSelection = gtk_tree_view_get_selection(
		g_SearchWindow.m_pResultsTreeView);

	GtkTreeIter iter;
	GtkTreeModel* pModel = GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore);
	if(gtk_tree_selection_get_selected(pSelection, &pModel, &iter)) {
		mappoint_t pt;
		gtk_tree_model_get(GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore), &iter,
			RESULTLIST_LATITUDE, &pt.m_fLatitude,
			RESULTLIST_LONGITUDE, &pt.m_fLongitude,
			-1);

		mainwindow_set_centerpoint(&pt);
		mainwindow_draw_map();
		mainwindow_statusbar_update_position();
	}
}

void searchwindow_on_addressresultstreeview_row_activated(GtkWidget *pWidget, gpointer* p)
{
	searchwindow_go_to_selected_result();
}

void searchwindow_on_gobutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	searchwindow_go_to_selected_result();
}

static void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
	/* set "Remove" button sensitive if >0 items selected */
	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.m_pGoButton), gtk_tree_selection_count_selected_rows(treeselection) > 0);	
}
