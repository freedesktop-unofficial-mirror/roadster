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
#include <gnome.h>

#include "gui.h"
#include "db.h"

#include "mainwindow.h"
#include "gotowindow.h"
#include "importwindow.h"
#include "datasetwindow.h"
#include "welcomewindow.h"
#include "searchwindow.h"

void gui_init()
{
	GladeXML *pGladeXML;

	// Load glade UI definition file and connect to callback functions	
	// try source directory first (good for development)
	pGladeXML = glade_xml_new (PACKAGE_SOURCE_DIR"/data/roadster.glade", NULL, NULL);
	if(pGladeXML == NULL) {
		pGladeXML = glade_xml_new (PACKAGE_DATA_DIR"/roadster.glade", NULL, NULL);

		if(pGladeXML == NULL) {
			g_message("cannot find file roadster.glade\n");
			gtk_main_quit();
		}
	}
	glade_xml_signal_autoconnect(pGladeXML);

	// init all windows/dialogs
	mainwindow_init(pGladeXML);	
	searchwindow_init(pGladeXML);
	gotowindow_init(pGladeXML);
	importwindow_init(pGladeXML);
	datasetwindow_init(pGladeXML);
	welcomewindow_init(pGladeXML);
}

void gui_run()
{
	if(db_is_empty()) {
		welcomewindow_show();
	}
	else {
		mainwindow_show();
	}
	gtk_main();
}

void gui_exit()
{
	// Hide first, then quit (makes the UI feel snappier)
	mainwindow_hide();
	gotowindow_hide();

	gtk_main_quit();
}

