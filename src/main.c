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
#include "map.h"
#include "gpsclient.h"
#include "scenemanager.h"
#include "prefs.h"

static gboolean main_init(void);
static void main_deinit(void);

int main (int argc, char *argv[])
{
	#ifdef ENABLE_NLS
		bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
		textdomain(PACKAGE);
	#endif

	gnome_program_init(PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);

	if(!main_init())
		return 1;

	prefs_read();

	gui_run();
	main_deinit();

	return 0;
}

gboolean main_init(void)
{
	// Initialize GLib thread system
	//g_thread_init(NULL);
	//gdk_threads_init();

	if(!gnome_vfs_init()) {	
		g_warning("gnome_vfs_init failed\n");
		return FALSE;
	}

	gchar* pszApplicationDir = g_strdup_printf("%s/.roadster", g_get_home_dir());
	gnome_vfs_make_directory(pszApplicationDir, 0700);
	g_free(pszApplicationDir);

	g_print("initializing prefs\n");
	prefs_init();	// note: doesn't READ prefs yet

	g_print("initializing points\n");
	point_init();
	g_print("initializing pointstrings\n");
	pointstring_init();
	g_print("initializing tracks\n");
	track_init();
	g_print("initializing layers\n");
	layers_init();
	g_print("initializing glyphs\n");
	glyph_init();
	g_print("initializing map\n");
	map_init();

	g_print("initializing scenemanager\n");
	scenemanager_init();

	g_print("initializing locationsets\n");
	locationset_init();

	g_print("initializing gpsclient\n");
	gpsclient_init();

	g_print("initializing gui\n");
	gui_init();

	g_print("initializing db\n");
	db_init();

	g_print("connecting to db\n");
	gboolean bConnected = db_connect(NULL, NULL, NULL, "");

	g_print("creating database tables\n");
	db_create_tables();

	g_print("initialization complete\n");

	return TRUE;
}

static void main_deinit(void)
{
	g_print("deinitializating database\n");
	db_deinit();
	// others?

	g_print("deinitialization complete\n");
}
