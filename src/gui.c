/***************************************************************************
 *            gui.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glade/glade.h>
#include <gtk/gtk.h>

#include "main.h"
#include "gui.h"
#include "db.h"
#include "mainwindow.h"
#include "gotowindow.h"
#include "importwindow.h"
#include "searchwindow.h"
#include "locationeditwindow.h"

void gui_init()
{
	GladeXML* pGladeXML = gui_load_xml(GLADE_FILE_NAME, NULL);
	glade_xml_signal_autoconnect(pGladeXML);

	// init all windows/dialogs
	g_print("- initializing mainwindow\n");
	mainwindow_init(pGladeXML);
	g_print("- initializing searchwindow\n");
	searchwindow_init(pGladeXML);
	g_print("- initializing gotowindow\n");
	gotowindow_init(pGladeXML);
	g_print("- initializing importwindow\n");
	importwindow_init(pGladeXML);
	//datasetwindow_init(pGladeXML);
	g_print("- initializing locationeditwindow\n");
	locationeditwindow_init(pGladeXML);
}

GladeXML* gui_load_xml(gchar* pszFileName, gchar* pszXMLTreeRoot)
{
	GladeXML *pGladeXML;
	gchar* pszPath;

	// Load glade UI definition file and connect to callback functions	
	// try source directory first (good for development)
	pszPath = g_strdup_printf(PACKAGE_SOURCE_DIR"/data/%s", pszFileName);
	pGladeXML = glade_xml_new(pszPath, pszXMLTreeRoot, NULL);
	g_free(pszPath);

	if(pGladeXML == NULL) {
		pszPath = g_strdup_printf(PACKAGE_DATA_DIR"/data/%s", pszFileName);
		pGladeXML = glade_xml_new(pszPath, pszXMLTreeRoot, NULL);
		g_free(pszPath);

		if(pGladeXML == NULL) {
			g_message("cannot find glade file '%s'\n", pszFileName);
			gtk_main_quit();
		}
	}
	return pGladeXML;
}

void gui_run()
{
	mainwindow_show();
	gtk_main();
}

void gui_exit()
{
	// Hide first, then quit (makes the UI feel snappier)
	mainwindow_hide();
	gotowindow_hide();
	importwindow_hide();
	locationeditwindow_hide();

	gtk_main_quit();
}
