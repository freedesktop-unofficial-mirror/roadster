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

#ifdef ENABLE_NLS
#	include <libintl.h>
#endif

#include <gtk/gtk.h>
#include "main.h"
#include "gui.h"
#include "road.h"
#include "db.h"
#include "map.h"
#include "map_style.h"
#include "gpsclient.h"
#include "locationset.h"
#include "location.h"
#include "search.h"
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

	g_print("Running %s\n", g_thread_supported() ? "multi-threaded" : "single-threaded");

	gui_run();
	main_deinit();

	return 0;
}

void main_debug_insert_test_data()
{
	//
	// New POI Attributes 'name' and 'address'
	//

#if 0	// only run with this enabled once ;)
	gint nNameAttributeID = 0;
	location_insert_attribute_name("name", &nNameAttributeID);
	g_assert(nNameAttributeID == LOCATION_ATTRIBUTE_ID_NAME);

	gint nAddressAttributeID = 0;
	location_insert_attribute_name("address", &nAddressAttributeID);
	g_assert(nAddressAttributeID == LOCATION_ATTRIBUTE_ID_ADDRESS);

	// and 'review'
	gint nAttributeIDReview = 0;
	location_insert_attribute_name("review", &nAttributeIDReview);

	//
	// New POI Type
	//
	gint nNewLocationSetID = 0;
	gint nNewLocationID;

	locationset_insert("Coffee Shops", "coffee-shops", &nNewLocationSetID);

	// New POI
	mappoint_t pt;
	pt.fLatitude = 42.37391;
	pt.fLongitude = -71.10045;
	nNewLocationID = 0;
	location_insert(nNewLocationSetID, &pt, &nNewLocationID);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_NAME, "1369 Coffee House", NULL);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_ADDRESS, "1369 Cambridge Street\nCambridge, MA, 02141", NULL);
	location_insert_attribute(nNewLocationID, nAttributeIDReview, "1369 Coffee House (specifically, the one on Mass Ave) has a special place in my heart; it's sort of my office away from the office, or my vacation home away from my tiny Central Square rented home. It's cozy. It's hip.", NULL);

	// New POI
	pt.fLatitude = 42.36654;
	pt.fLongitude = -71.10541;
	nNewLocationID = 0;
	location_insert(nNewLocationSetID, &pt, &nNewLocationID);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_NAME, "1369 Coffee House", NULL);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_ADDRESS, "757 Massachusetts Avenue\nCambridge, MA 02139", NULL);
	location_insert_attribute(nNewLocationID, nAttributeIDReview, "Don't know if I've ever been to the Central Square version, but I've tried 1369 at Inman Square three times and will not go back. I drink regular coffee and am incredulous at how poorly 1369 serves it. Most real coffee drinkers have specific preferences for their sugar and milk and know that the sugar needs to go in first in order for it to dissolve, but 1369 has the milk and cream behind the counter and the sugar at the condiments bar. I could also taste the filthiness of the urn in each cup. Cool enough crowd, good location, but not for truly down-to-earth folks.", NULL);

	nNewLocationSetID = 0;
	locationset_insert("Restaurants", "restaurants", &nNewLocationSetID);

	// New POI (Anna's)
	pt.fLatitude = 42.38821;
	pt.fLongitude = -71.11799;
	nNewLocationID = 0;
	location_insert(nNewLocationSetID, &pt, &nNewLocationID);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_NAME, "Anna's Taqueria", NULL);
	location_insert_attribute(nNewLocationID, LOCATION_ATTRIBUTE_ID_ADDRESS, "822 Somerville Avenue\nCambridge, MA 02140", NULL);
	location_insert_attribute(nNewLocationID, nAttributeIDReview, "Anna's kicks ass. Some of the best Mexican food I've ever had, and I've been to Mexico. You won't get better Mexican food than this anywhere in a day's drive.", NULL);
#endif
}

gboolean main_init(void)
{
	char *db_host = NULL, *db_user = NULL;
	char *db_passwd = NULL, *db_dbname = NULL;
	GKeyFile *keyfile;

	char *conffile = g_strdup_printf("%s/.roadster/roadster.conf", g_get_home_dir());

#ifdef USE_GNOME_VFS
	if(!gnome_vfs_init()) {	
		g_warning("gnome_vfs_init failed\n");
		return FALSE;
	}
	gchar* pszApplicationDir = g_strdup_printf("%s/.roadster", g_get_home_dir());
	gnome_vfs_make_directory(pszApplicationDir, 0700);
	g_free(pszApplicationDir);
#endif

	g_print("initializing road\n");
	road_init();

	g_print("initializing map styles\n");
	map_style_init();
	g_print("initializing map\n");
	map_init();

	g_print("initializing gpsclient\n");
	gpsclient_init();

	//
	// Database
	//
	g_print("initializing db\n");
	db_init();

	keyfile = g_key_file_new();
	if (g_key_file_load_from_file(keyfile, conffile, G_KEY_FILE_NONE, NULL))
	{
		db_host   = g_key_file_get_string(keyfile, "mysql", "host", NULL);
		db_user   = g_key_file_get_string(keyfile, "mysql", "user", NULL);
		db_passwd = g_key_file_get_string(keyfile, "mysql", "password", NULL);
		db_dbname = g_key_file_get_string(keyfile, "mysql", "database", NULL);
	}
	g_print("connecting to db\n");
	db_connect(db_host, db_user, db_passwd, db_dbname);

	g_print("creating database tables\n");
	db_create_tables();

	main_debug_insert_test_data();

	g_print("initializing glyphs\n");
	glyph_init();

	//
	// Load location sets from DB.  This is "coffee shops", "ATMs", etc.
	//
	g_print("initializing locationsets\n");
	locationset_init();

	g_print("initializing gui\n");
	gui_init();

	locationset_load_locationsets();	// needs glyph

	g_print("initializing search\n");
	search_init();

	g_print("initializing locations\n");
	location_init();

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
