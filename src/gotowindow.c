/***************************************************************************
 *            gotowindow.c
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
 
#include <glade/glade.h>
#include <gnome.h>
#include "../include/util.h"
#include "../include/map.h"
#include "../include/mainwindow.h"

#define LOCATIONLIST_COLUMN_NAME 		(0)
#define LOCATIONLIST_COLUMN_LOCATION 	(1)
#define LOCATIONLIST_COLUMN_LATITUDE	(2)
#define	LOCATIONLIST_COLUMN_LONGITUDE	(3)
#define	LOCATIONLIST_NUM_COLUMNS		(4)	

void gotowindow_on_locationlist_row_activated(GtkTreeView *treeview, GtkTreePath *arg1, GtkTreeViewColumn *arg2, gpointer user_data);
void gotowindow_on_location_list_selection_changed(GtkTreeSelection *treeselection, gpointer user_data);

struct {
	GtkWindow* m_pWindow;
//	GtkRadioButton* m_pNewLocationRadioButton;
//	GtkRadioButton* m_pKnownLocationRadioButton;
	GtkTreeView* m_pLocationListTreeView;
	GtkListStore* m_pLocationListModel;
	GtkEntry* m_pLongitudeEntry;
	GtkEntry* m_pLatitudeEntry;
} g_GotoWindow;

void gotowindow_init(GladeXML* pGladeXML)
{
	g_GotoWindow.m_pWindow						= GTK_WINDOW(glade_xml_get_widget(pGladeXML, "gotowindow")); g_return_if_fail(g_GotoWindow.m_pWindow != NULL);
	//g_GotoWindow.m_pNewLocationRadioButton 		= GTK_RADIO_BUTTON(glade_xml_get_widget(pGladeXML, "newlocationradiobutton")); g_return_if_fail(g_GotoWindow.m_pNewLocationRadioButton != NULL);
	//g_GotoWindow.m_pKnownLocationRadioButton 	= GTK_RADIO_BUTTON(glade_xml_get_widget(pGladeXML, "knownlocationradiobutton")); g_return_if_fail(g_GotoWindow.m_pKnownLocationRadioButton != NULL);
	g_GotoWindow.m_pLocationListTreeView		= GTK_TREE_VIEW(glade_xml_get_widget(pGladeXML, "knownlocationtreeview")); g_return_if_fail(g_GotoWindow.m_pLocationListTreeView != NULL);
	g_GotoWindow.m_pLongitudeEntry				= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "longitudeentry")); g_return_if_fail(g_GotoWindow.m_pLongitudeEntry != NULL);
	g_GotoWindow.m_pLatitudeEntry				= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "latitudeentry")); g_return_if_fail(g_GotoWindow.m_pLatitudeEntry != NULL);

	// don't delete window on X, just hide it
    g_signal_connect(G_OBJECT(g_GotoWindow.m_pWindow), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);

	g_GotoWindow.m_pLocationListModel = gtk_list_store_new(LOCATIONLIST_NUM_COLUMNS, 
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_DOUBLE,
		G_TYPE_DOUBLE);
	gtk_tree_view_set_model(g_GotoWindow.m_pLocationListTreeView, GTK_TREE_MODEL(g_GotoWindow.m_pLocationListModel));

	GtkTreeSelection* pTreeSelection = gtk_tree_view_get_selection(g_GotoWindow.m_pLocationListTreeView);
	g_signal_connect(G_OBJECT(pTreeSelection), "changed", (GtkSignalFunc)gotowindow_on_location_list_selection_changed, NULL);
	
	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

	/* add Layer Name column (with a text renderer) */
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Name", pCellRenderer, "text", LOCATIONLIST_COLUMN_NAME, NULL);	
	gtk_tree_view_append_column(g_GotoWindow.m_pLocationListTreeView, pColumn);	

	/* add Location column */
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Location", pCellRenderer, "text", LOCATIONLIST_COLUMN_LOCATION, NULL);	
	gtk_tree_view_append_column(g_GotoWindow.m_pLocationListTreeView, pColumn);	

	//
	// add some fake data (FIXME)
	//
	struct { gchar* name; double lat; double lon; } list[] = {
		{"Home", 42.3718, -71.0906},
		{"Concord, NH", 43.2062, -71.5363},
		{"Cambridge Common, MA", 42.3768, -71.1212},
		{"Central Park, NY", 40.78410, -73.96429},
	};
	GtkTreeIter iter;
	int i;
	for(i=0 ; i<NUM_ELEMS(list) ;i++) {
		gtk_list_store_append(g_GotoWindow.m_pLocationListModel, &iter);

		gtk_list_store_set(g_GotoWindow.m_pLocationListModel, &iter, 
		LOCATIONLIST_COLUMN_NAME, list[i].name,
		LOCATIONLIST_COLUMN_LOCATION, g_strdup_printf("(%8.5f, %8.5f)", list[i].lat, list[i].lon),
		LOCATIONLIST_COLUMN_LATITUDE, list[i].lat,
		LOCATIONLIST_COLUMN_LONGITUDE, list[i].lon,
		-1);
	}
}

void gotowindow_show()
{
	if(!GTK_WIDGET_VISIBLE(g_GotoWindow.m_pWindow)) {
//		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_GotoWindow.m_pKnownLocationRadioButton), TRUE);
		gtk_widget_grab_focus(GTK_WIDGET(g_GotoWindow.m_pLocationListTreeView));

		//~ gtk_entry_set_text(g_GotoWindow.m_pLongitudeEntry, "");
		//~ gtk_entry_set_text(g_GotoWindow.m_pLatitudeEntry, "");

		gtk_widget_show(GTK_WIDGET(g_GotoWindow.m_pWindow));
	}
	gtk_window_present(g_GotoWindow.m_pWindow);
}

void gotowindow_hide()
{
	gtk_widget_hide(GTK_WIDGET(g_GotoWindow.m_pWindow));
}

gboolean util_string_to_double(const gchar* psz, gdouble* pReturn)
{
	gdouble d = g_strtod(psz, NULL);
	if(d == 0.0) {
		const gchar* p = psz;

		// check for bad characters
		while(*p != '\0') {
			if(*p != '0' && *p != '.' && *p != '-' && *p != '+') {
				return FALSE;
			}
			p++;
		}
	}
	*pReturn = d;
	return TRUE;
}

gboolean gotowindow_go()
{
	const gchar* pszLatitude = gtk_entry_get_text(g_GotoWindow.m_pLatitudeEntry);
	const gchar* pszLongitude = gtk_entry_get_text(g_GotoWindow.m_pLongitudeEntry);
	double fLatitude;
	if(!util_string_to_double(pszLatitude, &fLatitude)) return FALSE;
	double fLongitude;
	if(!util_string_to_double(pszLongitude, &fLongitude)) return FALSE;

	// TODO: error checking for 0 (meaning either bad text "3a21" or "000" etc.

	map_center_on_worldpoint(fLatitude, fLongitude);
	mainwindow_draw_map();
	mainwindow_statusbar_update_position();
	return TRUE;
}

//~ gboolean gotowindow_goto_known_location()
//~ {
	//~ double fLatitude = 0, fLongitude = 0;
	//~ GtkTreeIter iter;
	//~ GtkTreeSelection *pKnownLocationSelection;
	//~ GtkTreeModel* pModel = GTK_TREE_MODEL(g_GotoWindow.m_pLocationListModel);
	//~ pKnownLocationSelection = gtk_tree_view_get_selection(g_GotoWindow.m_pLocationListTreeView);
	//~ if(gtk_tree_selection_get_selected(pKnownLocationSelection, &pModel, &iter)) {
		//~ gtk_tree_model_get(pModel,
						 //~ &iter,
						 //~ LOCATIONLIST_COLUMN_LATITUDE, &fLatitude,		
						 //~ LOCATIONLIST_COLUMN_LONGITUDE, &fLongitude,
						 //~ -1);

		//~ map_center_on_worldpoint(fLatitude, fLongitude);
		//~ mainwindow_draw_map();
		//~ return TRUE;
	//~ }
	//~ return FALSE;
//~ }

//
// Callbacks
//
void gotowindow_on_knownlocationtreeview_row_activated(GtkTreeView *treeview, GtkTreePath *arg1, GtkTreeViewColumn *arg2, gpointer user_data)
{
	if(gotowindow_go()) {
		gotowindow_hide();
	}
}

void gotowindow_on_gobutton_clicked(GtkButton *button, gpointer user_data)
{
	// hide window when "Go" button clicked (successfully)
	if(gotowindow_go()) {
		gotowindow_hide();
	}
}

void gotowindow_on_location_list_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
	double fLatitude = 0, fLongitude = 0;
	GtkTreeIter iter;
	GtkTreeSelection *pLocationSelection;
	GtkTreeModel* pModel = GTK_TREE_MODEL(g_GotoWindow.m_pLocationListModel);
	pLocationSelection = gtk_tree_view_get_selection(g_GotoWindow.m_pLocationListTreeView);

	if(gtk_tree_selection_get_selected(pLocationSelection, &pModel, &iter)) {
		gtk_tree_model_get(pModel,
						 &iter,
						 LOCATIONLIST_COLUMN_LATITUDE, &fLatitude,		
						 LOCATIONLIST_COLUMN_LONGITUDE, &fLongitude,
						 -1);
		gchar azValue[50];
		g_snprintf(azValue, 50, "%.5f", fLatitude);
		gtk_entry_set_text(g_GotoWindow.m_pLatitudeEntry, azValue);
		g_snprintf(azValue, 50, "%.5f", fLongitude);
		gtk_entry_set_text(g_GotoWindow.m_pLongitudeEntry, azValue);
	}
}
