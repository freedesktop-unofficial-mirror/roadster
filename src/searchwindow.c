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

#include <gdk/gdkx.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "../include/search_road.h"
#include "../include/search_location.h"
#include "../include/locationset.h"
#include "../include/mainwindow.h"

void fill_combobox_with_distance_unit_names(GtkComboBox* pComboBox)
{
	gint i;
	for(i=UNIT_FIRST ; i<=UNIT_LAST ; i++) {
		gtk_combo_box_append_text(pComboBox, g_aDistanceUnitNames[i]);
	}
	gtk_combo_box_set_active(pComboBox, DEFAULT_UNIT);
}

struct {
	// window
	GtkComboBox* m_pSearchTypeComboBox;		// choose between types of searches

	// address search
	GtkVBox* m_pAddressSearchBox;
	GtkEntry* m_pAddressStreetEntry;
	GtkEntry* m_pAddressCityEntry;
	GtkEntry* m_pAddressStateEntry;
	GtkEntry* m_pAddressZIPCodeEntry;

	// location search
	GtkVBox* m_pLocationSearchBox;
	GtkEntry* m_pLocationNameEntry;
	GtkComboBox* m_pLocationSetComboBox;
	GtkListStore* m_pLocationSetComboBoxModel;
	GtkComboBox* m_pLocationDistanceUnitComboBox;
	GtkSpinButton* m_pLocationDistanceSpinButton;

	// common to all searches	
	GtkButton* m_pFindButton;

	// results list
	GtkTreeView* m_pResultsTreeView;
	GtkListStore* m_pResultsListStore;
	GtkButton* m_pGoButton;
} g_SearchWindow = {0};

#define SEARCH_TYPE_ADDRESS 	0
#define SEARCH_TYPE_LOCATIONS	1

#define RESULTLIST_COLUMN_NAME 	0
#define RESULTLIST_LATITUDE		1
#define RESULTLIST_LONGITUDE	2

void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data);

void searchwindow_init(GladeXML* pGladeXML)
{
	g_SearchWindow.m_pSearchTypeComboBox 	= GTK_COMBO_BOX(glade_xml_get_widget(pGladeXML, "searchtypecombo"));		g_return_if_fail(g_SearchWindow.m_pSearchTypeComboBox != NULL);	
	g_SearchWindow.m_pAddressStreetEntry 	= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "addressstreetentry"));			g_return_if_fail(g_SearchWindow.m_pAddressStreetEntry != NULL);	
	g_SearchWindow.m_pAddressCityEntry 		= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "addresscityentry"));			g_return_if_fail(g_SearchWindow.m_pAddressCityEntry != NULL);	
	g_SearchWindow.m_pAddressStateEntry 	= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "addressstateentry"));			g_return_if_fail(g_SearchWindow.m_pAddressStateEntry != NULL);	
	g_SearchWindow.m_pAddressZIPCodeEntry 	= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "addresszipcodeentry"));		g_return_if_fail(g_SearchWindow.m_pAddressZIPCodeEntry != NULL);	
	g_SearchWindow.m_pResultsTreeView		= GTK_TREE_VIEW(glade_xml_get_widget(pGladeXML, "addressresultstreeview"));	g_return_if_fail(g_SearchWindow.m_pResultsTreeView != NULL);	
	g_SearchWindow.m_pLocationSearchBox		= GTK_VBOX(glade_xml_get_widget(pGladeXML, "locationsearchbox"));			g_return_if_fail(g_SearchWindow.m_pLocationSearchBox != NULL);	
	g_SearchWindow.m_pAddressSearchBox		= GTK_VBOX(glade_xml_get_widget(pGladeXML, "addresssearchbox"));			g_return_if_fail(g_SearchWindow.m_pAddressSearchBox != NULL);	
	g_SearchWindow.m_pFindButton			= GTK_BUTTON(glade_xml_get_widget(pGladeXML, "searchfindbutton"));				g_return_if_fail(g_SearchWindow.m_pFindButton!= NULL);	
	g_SearchWindow.m_pGoButton				= GTK_BUTTON(glade_xml_get_widget(pGladeXML, "searchgobutton"));					g_return_if_fail(g_SearchWindow.m_pGoButton != NULL);	
	g_SearchWindow.m_pLocationNameEntry		= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "locationnameentry"));			g_return_if_fail(g_SearchWindow.m_pLocationNameEntry != NULL);	
	g_SearchWindow.m_pLocationSetComboBox 	= GTK_COMBO_BOX(glade_xml_get_widget(pGladeXML, "locationsetcombobox"));		g_return_if_fail(g_SearchWindow.m_pLocationSetComboBox != NULL);	
	g_SearchWindow.m_pLocationDistanceSpinButton	= GTK_SPIN_BUTTON(glade_xml_get_widget(pGladeXML, "locationdistancespinbutton"));		g_return_if_fail(g_SearchWindow.m_pLocationDistanceSpinButton != NULL);	
	g_SearchWindow.m_pLocationDistanceUnitComboBox 	= GTK_COMBO_BOX(glade_xml_get_widget(pGladeXML, "locationdistanceunitcombobox"));		g_return_if_fail(g_SearchWindow.m_pLocationDistanceUnitComboBox != NULL);	

	// 
	fill_combobox_with_distance_unit_names(g_SearchWindow.m_pLocationDistanceUnitComboBox);

	gtk_combo_box_set_active(g_SearchWindow.m_pSearchTypeComboBox, SEARCH_TYPE_ADDRESS);
	gtk_widget_hide(GTK_WIDGET(g_SearchWindow.m_pLocationSearchBox));
	gtk_widget_show(GTK_WIDGET(g_SearchWindow.m_pAddressSearchBox));

	//
	//
	//
	g_SearchWindow.m_pLocationSetComboBoxModel = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	gtk_combo_box_set_model(g_SearchWindow.m_pLocationSetComboBox, GTK_TREE_MODEL(g_SearchWindow.m_pLocationSetComboBoxModel));

#define LOCATIONSETLIST_COLUMN_NAME 	0
#define LOCATIONSETLIST_COLUMN_ID		1

	gint i;
	const GPtrArray* pLocationSets = locationset_get_set_array();
	//~ gtk_combo_box_append_text(g_SearchWindow.m_pLocationSetComboBox, "All");
	
	GtkTreeIter iter;

	gtk_list_store_append(g_SearchWindow.m_pLocationSetComboBoxModel, &iter);
	gtk_list_store_set(g_SearchWindow.m_pLocationSetComboBoxModel, &iter, 
		LOCATIONSETLIST_COLUMN_NAME, "All",
		LOCATIONSETLIST_COLUMN_ID, 0,
		-1);
	gtk_combo_box_set_active_iter(g_SearchWindow.m_pLocationSetComboBox, &iter);

	for(i=0 ; i<pLocationSets->len ;i++) {
		locationset_t* pLocationSet = g_ptr_array_index(pLocationSets, i);

		gtk_list_store_append(g_SearchWindow.m_pLocationSetComboBoxModel, &iter);
		gtk_list_store_set(g_SearchWindow.m_pLocationSetComboBoxModel, &iter, 
			LOCATIONSETLIST_COLUMN_NAME, pLocationSet->m_pszName,
			LOCATIONSETLIST_COLUMN_ID, pLocationSet->m_nID,
			-1);
	}

	// create results tree view
	g_SearchWindow.m_pResultsListStore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_FLOAT, G_TYPE_FLOAT);
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

gint searchwindow_get_selected_locationset()
{
	gint nID;
	GtkTreeIter iter;
	
	if(gtk_combo_box_get_active_iter(g_SearchWindow.m_pLocationSetComboBox, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(g_SearchWindow.m_pLocationSetComboBoxModel), &iter, 
			LOCATIONSETLIST_COLUMN_ID, &nID,
			-1);
		return nID;
	}
	return 0;
}

void searchwindow_clear_results()
{
	// remove all search results from the list
	if(g_SearchWindow.m_pResultsListStore != NULL) {
		gtk_list_store_clear(g_SearchWindow.m_pResultsListStore);
	}
}

// begin a search
void searchwindow_on_findbutton_clicked(GtkWidget *pWidget, gpointer* p)
{
	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.m_pFindButton), FALSE);

	searchwindow_clear_results();

	gint nSearchType = gtk_combo_box_get_active(g_SearchWindow.m_pSearchTypeComboBox);
	if(nSearchType == SEARCH_TYPE_ADDRESS) {
		const gchar* pStreet = gtk_entry_get_text(g_SearchWindow.m_pAddressStreetEntry);

		void* pBusy = mainwindow_set_busy();
		search_road_execute(pStreet);
		mainwindow_set_not_busy(&pBusy);
	}
	else {
		const gchar* pszSearch = gtk_entry_get_text(g_SearchWindow.m_pLocationNameEntry);
		gint nLocationSetID = searchwindow_get_selected_locationset();
		gfloat fDistance = gtk_spin_button_get_value(g_SearchWindow.m_pLocationDistanceSpinButton);
		gint nDistanceUnit = gtk_combo_box_get_active(g_SearchWindow.m_pLocationDistanceUnitComboBox);
		
	//~ const gchar* pSearch = gtk_entry_get_text(g_SearchWindow.m_pLocationNameEntry);
	//~ GtkComboBox* m_pLocationSetComboBox;
	//~ GtkComboBox* m_pLocationDistanceUnitComboBox;
		g_print("pszSearch = %s, nLocationSetID = %d, fDistance = %f, nDistanceUnit=%d\n", pszSearch, nLocationSetID, fDistance, nDistanceUnit);

		void* pBusy = mainwindow_set_busy();
		search_location_execute(pszSearch, nLocationSetID, fDistance, nDistanceUnit);
		mainwindow_set_not_busy(&pBusy);
	}

	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.m_pFindButton), TRUE);
}

#define GTK_PROCESS_MAINLOOP  while (gtk_events_pending ()) { gtk_main_iteration (); }

// add a result row to the list
void searchwindow_add_result(gint nRoadID, const gchar* pszText, mappoint_t* pPoint)
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
	//GTK_PROCESS_MAINLOOP;
}

// when the "Address"/"Points of Interest" combo chances, make the proper 'box' visible
void searchwindow_on_searchtypecombo_changed(GtkWidget *pWidget, gpointer* p)
{
	gint nSearchType = gtk_combo_box_get_active(g_SearchWindow.m_pSearchTypeComboBox);
	if(nSearchType == SEARCH_TYPE_ADDRESS) {
		gtk_widget_hide(GTK_WIDGET(g_SearchWindow.m_pLocationSearchBox));
		gtk_widget_show(GTK_WIDGET(g_SearchWindow.m_pAddressSearchBox));		
		gtk_widget_grab_focus(GTK_WIDGET(g_SearchWindow.m_pAddressStreetEntry));
	}
	else {
		gtk_widget_hide(GTK_WIDGET(g_SearchWindow.m_pAddressSearchBox));
		gtk_widget_show(GTK_WIDGET(g_SearchWindow.m_pLocationSearchBox));
		gtk_widget_grab_focus(GTK_WIDGET(g_SearchWindow.m_pLocationNameEntry));
	}
	searchwindow_clear_results();
}

void searchwindow_go_to_selected_result()
{
	GtkTreeSelection* pSelection = gtk_tree_view_get_selection(
		g_SearchWindow.m_pResultsTreeView);

	GtkTreeIter iter;
	GtkTreeModel* pModel = GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore);
	if(gtk_tree_selection_get_selected(pSelection, &pModel, &iter)) {
//		gchar* pszText;
		gfloat fLatitude;
		gfloat fLongitude;
		gtk_tree_model_get(GTK_TREE_MODEL(g_SearchWindow.m_pResultsListStore), &iter, 
//			RESULTLIST_COLUMN_NAME, &pszText,
			RESULTLIST_LATITUDE, &fLatitude,
			RESULTLIST_LONGITUDE, &fLongitude,
			-1);
		map_center_on_worldpoint(fLatitude, fLongitude);
		mainwindow_draw_map();
		mainwindow_statusbar_update_position();
//		g_print("yay: %s\n", pszText);
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

void searchwindow_on_resultslist_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
	/* set "Remove" button sensitive if >0 items selected */
	gtk_widget_set_sensitive(GTK_WIDGET(g_SearchWindow.m_pGoButton), gtk_tree_selection_count_selected_rows(treeselection) > 0);	
}
