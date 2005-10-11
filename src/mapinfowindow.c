/***************************************************************************
 *            mapinfowindow.c
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
	This is the bar containing current [Country] [State] [City] buttons
*/

#include <gtk/gtk.h>
#include <glade/glade.h>
#include "map.h"
#include "gui.h"
#include "util.h"
#include "db.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

struct {
	GtkMenuToolButton* pCountryButton;
	GtkMenuToolButton* pStateButton;
	GtkMenuToolButton* pCityButton;
	
	GtkLabel* pCountryLabel;
	GtkLabel* pStateLabel;
	GtkLabel* pCityLabel;

	GtkMenu* pCountryMenu;
	GtkMenu* pStateMenu;
	GtkMenu* pCityMenu;

	gint nCurrentCountryID;	//
	gint nCurrentStateID;
	gint nCurrentCityID;
} g_MapInfoWindow = {0};

static void mapinfowindow_load_cities(gint nStateID);
static void mapinfowindow_load_states(gint nCountryID);

static void mapinfowindow_on_country_button_clicked(GtkWidget* _unused, gpointer __unused)
{
	if(g_MapInfoWindow.nCurrentCountryID != 0) {
		g_debug("recentering on countryID %d", g_MapInfoWindow.nCurrentCountryID);
	}
}

static void mapinfowindow_on_state_button_clicked(GtkWidget* _unused, gpointer __unused)
{
	if(g_MapInfoWindow.nCurrentStateID != 0) {
		g_debug("recentering on StateID %d", g_MapInfoWindow.nCurrentStateID);
	}
}

static void mapinfowindow_on_city_button_clicked(GtkWidget* _unused, gpointer __unused)
{
	if(g_MapInfoWindow.nCurrentCityID != 0) {
		g_debug("recentering on cityID %d", g_MapInfoWindow.nCurrentCityID);
	}
}

void mapinfowindow_init(GladeXML* pGladeXML)
{
	GLADE_LINK_WIDGET(pGladeXML, g_MapInfoWindow.pCountryButton, GTK_MENU_TOOL_BUTTON, "countrybutton");
	GLADE_LINK_WIDGET(pGladeXML, g_MapInfoWindow.pStateButton, GTK_MENU_TOOL_BUTTON, "statebutton");
    GLADE_LINK_WIDGET(pGladeXML, g_MapInfoWindow.pCityButton, GTK_MENU_TOOL_BUTTON, "citybutton");

	g_MapInfoWindow.pCountryLabel = GTK_LABEL(gtk_label_new(""));	gtk_widget_show(GTK_WIDGET(g_MapInfoWindow.pCountryLabel));
	g_MapInfoWindow.pStateLabel = GTK_LABEL(gtk_label_new(""));	gtk_widget_show(GTK_WIDGET(g_MapInfoWindow.pStateLabel));
	g_MapInfoWindow.pCityLabel = GTK_LABEL(gtk_label_new(""));	gtk_widget_show(GTK_WIDGET(g_MapInfoWindow.pCityLabel));

	gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(g_MapInfoWindow.pCountryButton), GTK_WIDGET(g_MapInfoWindow.pCountryLabel));
	gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(g_MapInfoWindow.pStateButton), GTK_WIDGET(g_MapInfoWindow.pStateLabel));
	gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(g_MapInfoWindow.pCityButton), GTK_WIDGET(g_MapInfoWindow.pCityLabel));

	g_signal_connect(G_OBJECT(g_MapInfoWindow.pCountryButton), "clicked", (GCallback)mapinfowindow_on_country_button_clicked, NULL);
	g_signal_connect(G_OBJECT(g_MapInfoWindow.pStateButton), "clicked", (GCallback)mapinfowindow_on_state_button_clicked, NULL);
	g_signal_connect(G_OBJECT(g_MapInfoWindow.pCityButton), "clicked", (GCallback)mapinfowindow_on_city_button_clicked, NULL);

	g_MapInfoWindow.nCurrentCountryID = -1;	//
	g_MapInfoWindow.nCurrentStateID = -1;
	g_MapInfoWindow.nCurrentCityID = -1;
}

void mapinfowindow_update(const map_t* pMap)
{
	// Step 1. Get current country/state/city from map
	gint nNewCountryID = 1;	// XXX: get these from a hittest?
	gint nNewStateID = 0;
	gint nNewCityID = 0;

	gint nZoomScale = map_get_zoomlevel_scale(pMap);

	// Step 2. Set button text and drop-down menus

	//
	// Update country label
	//
	if(nNewCountryID != g_MapInfoWindow.nCurrentCountryID) {
		// If we are now too high up to have a country selected, we clear the country label and
		// hide the state list (of course, the state is 0 too, so 
		if(nNewCountryID == 0) {
			gtk_label_set_markup(g_MapInfoWindow.pCountryLabel, "<i>Country</i>");

			// hide state list
			gtk_widget_hide(GTK_WIDGET(g_MapInfoWindow.pStateButton));
		}
		else {
			// XXX: Set new country button text
			gchar* pszLabel = g_strdup_printf("<b>%s</b>", "United States");
			gtk_label_set_markup(g_MapInfoWindow.pCountryLabel, pszLabel);
			g_free(pszLabel);
	
			// Update state list
			mapinfowindow_load_states(nNewCountryID);
		}
		g_MapInfoWindow.nCurrentCountryID = nNewCountryID;
	}

	//
	// Update state label
	//
	if(nNewStateID != g_MapInfoWindow.nCurrentStateID) {
		if(nNewStateID == 0) {
			gtk_label_set_markup(g_MapInfoWindow.pStateLabel, "<i>State</i>");

			// hide city list
			gtk_widget_hide(GTK_WIDGET(g_MapInfoWindow.pCityButton));
		}
		else {
			// XXX: Set new state button text
			gtk_label_set_markup(g_MapInfoWindow.pStateLabel, "<b>State</b>");

			// Update city list
			mapinfowindow_load_cities(nNewStateID);
		}
		g_MapInfoWindow.nCurrentStateID = nNewStateID;
	}

	//
	// Update city label
	//
	if(nNewCityID != g_MapInfoWindow.nCurrentCityID) {
		if(nNewCityID == 0) {
			// XXX: Set new city button text
			gtk_label_set_markup(g_MapInfoWindow.pCityLabel, "<i>City</i>");
		}
		else {
			// XXX: Set new city button text
			gtk_label_set_markup(g_MapInfoWindow.pCityLabel, "<b>City</b>");
		}
		g_MapInfoWindow.nCurrentCityID = nNewCityID;
	}
}

static void mapinfowindow_on_city_chosen(GtkWidget* _unused, gint nCityID)
{
	g_debug("centering on CityID %d", nCityID);
}

static void mapinfowindow_on_state_chosen(GtkWidget* _unused, gint nStateID)
{
	g_debug("centering on nStateID %d", nStateID);
}

static void mapinfowindow_load_cities(gint nStateID)
{
	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	//
	// Load up all cities for this state
	//
	gchar* pszSQL = g_strdup_printf("SELECT City.ID, City.Name FROM City WHERE StateID=%d ORDER BY Name ASC;", nStateID);
	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);
	if(pResultSet == NULL) return;

	// New menu
	if(g_MapInfoWindow.pCityMenu) {
		gtk_widget_destroy(GTK_WIDGET(g_MapInfoWindow.pCityMenu));
	}
	g_MapInfoWindow.pCityMenu = GTK_MENU(gtk_menu_new());

	// Fill menu
	while((aRow = db_fetch_row(pResultSet)) != NULL) {
		gint nCityID = atoi(aRow[0]);
		gchar* pszName = aRow[1];

		GtkMenuItem* pNewMenuItem = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(pszName));
		g_signal_connect(G_OBJECT(pNewMenuItem), "activate", (GCallback)mapinfowindow_on_city_chosen, (gpointer)nCityID);

		gtk_menu_shell_append(GTK_MENU_SHELL(g_MapInfoWindow.pCityMenu), GTK_WIDGET(pNewMenuItem));
	}
	db_free_result(pResultSet);

	// Install new menu
	gtk_widget_show_all(GTK_WIDGET(g_MapInfoWindow.pCityMenu));
	gtk_menu_tool_button_set_menu(g_MapInfoWindow.pCityButton, GTK_WIDGET(g_MapInfoWindow.pCityMenu));
}

static void mapinfowindow_load_states(gint nCountryID)
{
	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	//
	// Load up all states for this country
	//
	gchar* pszSQL = g_strdup_printf("SELECT State.ID, State.Name FROM State WHERE CountryID=%d ORDER BY Name ASC;", nCountryID);
	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);
	if(pResultSet == NULL) return;

	// New menu
	if(g_MapInfoWindow.pStateMenu) {
		gtk_widget_destroy(GTK_WIDGET(g_MapInfoWindow.pStateMenu));
	}
	g_MapInfoWindow.pStateMenu = GTK_MENU(gtk_menu_new());

	// Fill menu
	while((aRow = db_fetch_row(pResultSet)) != NULL) {
		gint nID = atoi(aRow[0]);
		gchar* pszName = aRow[1];

		GtkMenuItem* pNewMenuItem = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(pszName));
		g_signal_connect(G_OBJECT(pNewMenuItem), "activate", (GCallback)mapinfowindow_on_state_chosen, (gpointer)nID);

		gtk_menu_shell_append(GTK_MENU_SHELL(g_MapInfoWindow.pStateMenu), GTK_WIDGET(pNewMenuItem));
	}
	db_free_result(pResultSet);

	// Install new menu
	gtk_widget_show_all(GTK_WIDGET(g_MapInfoWindow.pStateMenu));
	gtk_menu_tool_button_set_menu(g_MapInfoWindow.pStateButton, GTK_WIDGET(g_MapInfoWindow.pStateMenu));
}
