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
#include "mainwindow.h"
#include "gotowindow.h"

struct {
	GtkWindow* m_pWindow;
	GtkEntry* m_pLongitudeEntry;
	GtkEntry* m_pLatitudeEntry;
} g_GotoWindow;

void gotowindow_init(GladeXML* pGladeXML)
{
	g_GotoWindow.m_pWindow					= GTK_WINDOW(glade_xml_get_widget(pGladeXML, "gotowindow")); g_return_if_fail(g_GotoWindow.m_pWindow != NULL);
	g_GotoWindow.m_pLongitudeEntry				= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "longitudeentry")); g_return_if_fail(g_GotoWindow.m_pLongitudeEntry != NULL);
	g_GotoWindow.m_pLatitudeEntry				= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "latitudeentry")); g_return_if_fail(g_GotoWindow.m_pLatitudeEntry != NULL);

	// don't delete window on X, just hide it
	g_signal_connect(G_OBJECT(g_GotoWindow.m_pWindow), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
}

void gotowindow_show(void)
{
	if(!GTK_WIDGET_VISIBLE(g_GotoWindow.m_pWindow)) {
		gtk_widget_grab_focus(GTK_WIDGET(g_GotoWindow.m_pLatitudeEntry));
		gtk_widget_show(GTK_WIDGET(g_GotoWindow.m_pWindow));
	}
	gtk_window_present(g_GotoWindow.m_pWindow);
}

void gotowindow_hide(void)
{
	gtk_widget_hide(GTK_WIDGET(g_GotoWindow.m_pWindow));
}

static gboolean util_string_to_double(const gchar* psz, gdouble* pReturn)
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

static gboolean gotowindow_go(void)
{
	const gchar* pszLatitude = gtk_entry_get_text(g_GotoWindow.m_pLatitudeEntry);
	const gchar* pszLongitude = gtk_entry_get_text(g_GotoWindow.m_pLongitudeEntry);

	mappoint_t pt;
	if(!util_string_to_double(pszLatitude, &(pt.m_fLatitude))) return FALSE;
	if(!util_string_to_double(pszLongitude, &(pt.m_fLongitude))) return FALSE;

	mainwindow_set_centerpoint(&pt);
	mainwindow_draw_map();
	mainwindow_statusbar_update_position();
	return TRUE;
}

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
