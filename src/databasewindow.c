/***************************************************************************
 *            databasewindow.c
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

#include "db.h"
#include "gui.h"
#include "databasewindow.h"

struct {
	GtkDialog* m_pWindow;	
	GtkEntry* m_pHostEntry;
	GtkEntry* m_pHostPortEntry;
	GtkEntry* m_pUserNameEntry;
	GtkEntry* m_pPasswordEntry;
} g_DatabaseWindow = {0};

void databasewindow_init(GladeXML* pGladeXML)
{
	g_DatabaseWindow.m_pWindow		= GTK_DIALOG(glade_xml_get_widget(pGladeXML, "databasewindow")); g_return_if_fail(g_DatabaseWindow.m_pWindow != NULL);	
	g_DatabaseWindow.m_pHostEntry	= GTK_ENTRY(glade_xml_get_widget(pGladeXML, "databasewindowhostentry")); g_return_if_fail(g_DatabaseWindow.m_pHostEntry != NULL);	
	g_DatabaseWindow.m_pHostPortEntry = GTK_ENTRY(glade_xml_get_widget(pGladeXML, "databasewindowhostportentry")); g_return_if_fail(g_DatabaseWindow.m_pHostPortEntry != NULL);	
	g_DatabaseWindow.m_pUserNameEntry = GTK_ENTRY(glade_xml_get_widget(pGladeXML, "databasewindowusernameentry")); g_return_if_fail(g_DatabaseWindow.m_pUserNameEntry != NULL);	
	g_DatabaseWindow.m_pPasswordEntry = GTK_ENTRY(glade_xml_get_widget(pGladeXML, "databasewindowpasswordentry")); g_return_if_fail(g_DatabaseWindow.m_pPasswordEntry != NULL);	
}

gboolean databasewindow_connect(void)
{
	while(TRUE) {
		gint nResponse = gtk_dialog_run(g_DatabaseWindow.m_pWindow);
		if(nResponse == GTK_RESPONSE_CANCEL) {
			return FALSE;
		}

		// attempt a connection
		const gchar* pszHost = gtk_entry_get_text(g_DatabaseWindow.m_pHostEntry);
		const gchar* pszHostPort = gtk_entry_get_text(g_DatabaseWindow.m_pHostPortEntry);
		const gchar* pszUserName = gtk_entry_get_text(g_DatabaseWindow.m_pUserNameEntry);
		const gchar* pszPassword = gtk_entry_get_text(g_DatabaseWindow.m_pPasswordEntry);

		// build full host name with :port
		gchar* pszFullHost = g_strdup_printf("%s%s%s", pszHost, (pszHostPort[0] != '\0')?":":"", pszHostPort);
		gboolean bConnected = db_connect(pszFullHost, pszUserName, pszPassword, "");
		g_free(pszFullHost);

		if(bConnected) {
			gtk_widget_hide(GTK_WIDGET(g_DatabaseWindow.m_pWindow));
			return TRUE;
		}
		else {
			GtkWidget* pDialog = gtk_message_dialog_new(GTK_WINDOW(g_DatabaseWindow.m_pWindow), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
				"Database connection failed.\n");
			gtk_dialog_run(GTK_DIALOG(pDialog));
			gtk_widget_destroy(pDialog);
	
			// loop...
		}
	}
}

void databasewindow_on_connectbutton_clicked(void)
{
}
