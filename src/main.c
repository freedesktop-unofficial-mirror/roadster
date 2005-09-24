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

#include <gtk/gtk.h>
#include "main.h"
#include "gui.h"
#include "db.h"
#include "map.h"
#include "gpsclient.h"
#include "scenemanager.h"
#include "animator.h"
#include "road.h"
#include "locationset.h"
#include "location.h"
#include "search.h"

static gboolean main_init(void);
static void main_deinit(void);


#include <libgnomevfs/gnome-vfs.h>
#include <gnome.h>

int main (int argc, char *argv[])
{
	#ifdef ENABLE_NLS
		bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
		textdomain(PACKAGE);
	#endif

	gtk_init(&argc, &argv);

	g_type_init();
	if(!main_init())
		return 1;

// Insert some POI for testing...
/*
	gint nNewLocationSetID = 0;
	locationset_insert("Coffee Shops", &nNewLocationSetID);

	gint nNewLocationID;

	mappoint_t pt;
	pt.m_fLatitude = 42.37382;
	pt.m_fLongitude = -71.10054;
	nNewLocationID = 0;
	location_insert(nNewLocationSetID, &pt, &nNewLocationID);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_NAME, "1369 Coffee House", NULL);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_ADDRESS, "1369 Cambridge Street\nCambridge, MA, 02141", NULL);

	pt.m_fLatitude = 42.36650;
	pt.m_fLongitude = -71.10554;
	nNewLocationID = 0;
	location_insert(nNewLocationSetID, &pt, &nNewLocationID);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_NAME, "1369 Coffee House", NULL);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_ADDRESS, "757 Massachusetts Avenue\nCambridge, MA 02139", NULL);
*/
	g_print("Running %s\n", g_thread_supported() ? "multi-threaded" : "single-threaded");

	gui_run();
	main_deinit();

	return 0;
}

gboolean main_init(void)
{
#ifdef USE_GNOME_VFS
	if(!gnome_vfs_init()) {	
		g_warning("gnome_vfs_init failed\n");
		return FALSE;
	}

	gchar* pszApplicationDir = g_strdup_printf("%s/.roadster", g_get_home_dir());
	gnome_vfs_make_directory(pszApplicationDir, 0700);
	g_free(pszApplicationDir);
#endif
	g_print("initializing points\n");
	point_init();
	g_print("initializing pointstrings\n");
	pointstring_init();

	g_print("initializing roads\n");
	road_init();

	g_print("initializing tracks\n");
	track_init();
	g_print("initializing map styles\n");
	map_style_init();
	g_print("initializing glyphs\n");
	glyph_init();
	g_print("initializing map\n");
	map_init();

	g_print("initializing search\n");
	search_init();

	g_print("initializing scenemanager\n");
	scenemanager_init();

	g_print("initializing gpsclient\n");
	gpsclient_init();

	g_print("initializing db\n");
	db_init();

	g_print("initializing locationsets\n");
	locationset_init();

	g_print("initializing locations\n");
	location_init();

	g_print("initializing animator\n");
	animator_init();

	g_print("connecting to db\n");
	gboolean bConnected = db_connect(NULL, NULL, NULL, "");

	g_print("creating database tables\n");
	db_create_tables();

	// Load location sets from DB.  This is "coffee shops", "ATMs", etc.
	locationset_load_locationsets();

	g_print("initializing gui\n");
	gui_init();

	g_print("initialization complete\n");

	return TRUE;
}

static void main_deinit(void)
{
	g_print("deinitializing database\n");
	db_deinit();
	// others?

	g_print("deinitialization complete\n");
}
