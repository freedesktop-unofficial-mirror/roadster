/***************************************************************************
 *            welcomewindow.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "mainwindow.h"
#include "welcomewindow.h"

#define URL_CENSUS_GOV_TIGER_DATA_WEBSITE ("http://www.census.gov/geo/www/tiger/tiger2003/tgr2003.html")

struct {
	GtkWindow* m_pWindow;
} g_WelcomeWindow;

void welcomewindow_init(GladeXML* pGladeXML)
{
	g_WelcomeWindow.m_pWindow = GTK_WINDOW(glade_xml_get_widget(pGladeXML, "welcomewindow"));			g_return_if_fail(g_WelcomeWindow.m_pWindow != NULL);
}

void welcomewindow_show(void)
{
	gtk_widget_show(GTK_WIDGET(g_WelcomeWindow.m_pWindow));
	gtk_window_present(g_WelcomeWindow.m_pWindow);
}

static void welcomewindow_hide(void)
{
	gtk_widget_hide(GTK_WIDGET(g_WelcomeWindow.m_pWindow));
}

/* Callbacks */

void welcomewindow_on_url_clicked(GtkWidget* pButton, gpointer data)
{
	if(!gnome_url_show(URL_CENSUS_GOV_TIGER_DATA_WEBSITE, NULL)) {
		// TODO: something?
	}
}

void welcomewindow_on_okbutton_clicked(GtkWidget* pButton, gpointer data)
{
	welcomewindow_hide();
	mainwindow_show();
}
