/***************************************************************************
 *            prefs.c
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

#include "main.h"

void prefs_init() {}
void pref_deinit() {}
void prefs_read() {}

/*
#include <glib.h>
#include <glib.h>

struct {
	GKeyFile* m_pKeyFile;
} g_Prefs = {0};


#define KEYFILE_GROUP_GUI_STATE  	("state")
#define KEYFILE_KEY_LATITUDE		("latitude")

void prefs_load()
{
	gchar* pszFilePath = ".roadster/prefs";

	// load existing keyfile
	g_Prefs.m_pKeyFile = g_key_file_new();
	if(!g_key_file_load_from_file(g_Prefs.m_pKeyFile, pszFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL)) {
		
	}

        gchar* pszValue;

	//
	pszValue = g_key_file_get_value(g_Prefs.m_pKeyFile,
                                             KEYFILE_GROUP_GUI_STATE,
                                             KEYFILE_KEY_LATITUDE,
                                             NULL);
	g_print("value = %s\n", pszValue);
}

void prefs_save()
{

}
*/
