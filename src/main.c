/***************************************************************************
 *            main.c
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

#include <gnome.h>
#include "../include/gui.h"
#include "../include/db.h"
#include "../include/geometryset.h"
#include "../include/mainwindow.h"
#include "../include/map.h"
#include "../include/import.h"
#include "../include/gpsclient.h"
#include "../include/locationset.h"

int main (int argc, char *argv[])
{
	#ifdef ENABLE_NLS
		bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
		textdomain(PACKAGE);
	#endif

	gnome_init(PACKAGE, VERSION, argc, argv);

	/*
	** init our modules
	*/
	geometryset_init();
	locationset_init();
	gpsclient_init();
	gui_init();
	layers_init();
	db_init();

	map_set_zoomlevel(7);
	mainwindow_statusbar_update_zoomscale();

	/* Go! */
	gui_run();

	/* deinit -- never gets here? :( */
	g_message("shutting down...");
	db_deinit();
	return 0;
}
