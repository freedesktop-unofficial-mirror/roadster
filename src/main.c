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
#include "gui.h"
#include "db.h"
#include "geometryset.h"
#include "mainwindow.h"
#include "map.h"
#include "import.h"
#include "gpsclient.h"
#include "locationset.h"
#include "scenemanager.h"
#include "point.h"
#include "pointstring.h"
#include "track.h"

int main (int argc, char *argv[])
{
	#ifdef ENABLE_NLS
		bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
		textdomain(PACKAGE);
	#endif

	gnome_init(PACKAGE, VERSION, argc, argv);

	if(!gnome_vfs_init()) {	
		g_warning("gnome_vfs_init failed\n");
		return 1;
	}
	gchar* pszApplicationDir = g_strdup_printf("%s/.roadster", g_get_home_dir());
	if(GNOME_VFS_OK != gnome_vfs_make_directory(pszApplicationDir, 0700)) {
		g_print("failed to create directory: %s\n", pszApplicationDir);
	}

	/*
	** init our modules
	*/
	point_init();
	pointstring_init();
	track_init();

	scenemanager_init();
	//geometryset_init();
	locationset_init();
	gpsclient_init();
	gui_init();
	layers_init();
	db_init();

	gboolean bConnected = db_connect(NULL, NULL, NULL, "");
	db_create_tables();

	map_set_zoomlevel(7);
	mainwindow_statusbar_update_zoomscale();

	/* Go! */
	gui_run();

	/* deinit -- never gets here? :( */
	g_message("shutting down...");
	db_deinit();
	return 0;
}
