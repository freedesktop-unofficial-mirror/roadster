/***************************************************************************
 *            import.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "../include/import.h"
#include "../include/importwindow.h"
#include "../include/import_tiger.h"
#include "../include/db.h"

#include <gnome-vfs-2.0/libgnomevfs/gnome-vfs.h>

#if ROADSTER_DEAD_CODE
static void import_progress_pulse(void)
{
	importwindow_progress_pulse();
}
#endif /* ROADSTER_DEAD_CODE */

gboolean import_from_uri(const gchar* pszURI)
{
	if(!gnome_vfs_init()) {	
		g_warning("gnome_vfs_init failed\n");
		return FALSE;
	}
	gboolean bResult = FALSE;

	GnomeVFSFileInfo info;
	if(GNOME_VFS_OK != gnome_vfs_get_file_info(pszURI, &info, GNOME_VFS_FILE_INFO_DEFAULT)) {
		importwindow_log_append("Couldn't read %s\n", pszURI);
		return FALSE;
	}

//	func_progress_callback(0.0, pCallbackData);
	gchar* pszFileBaseName = info.name;
	g_return_val_if_fail(pszFileBaseName != NULL, FALSE);
	// does it look like a tgr file name (tgr00000.zip) ?
	if(strlen(pszFileBaseName) == 12 && g_str_has_prefix(pszFileBaseName, "tgr") && g_str_has_suffix(pszFileBaseName, ".zip")) {
		importwindow_log_append("Importing TIGER file %s", info.name);	// NOTE: no "\n" so we can add ...
		gchar buf[6];
		memcpy(buf, &pszFileBaseName[3], 5);
		buf[5] = '\0';

		gint nTigerSetNumber = atoi(buf);
		// just assume it's a TIGER file for now since it's all we support
		
//		db_disable_keys();
		bResult = import_tiger_from_uri(pszURI, nTigerSetNumber);
//		db_enable_keys();

		if(bResult) {
			importwindow_log_append("success.\n\n");
		}
		else {
			importwindow_log_append("\n** Failed.\n\n");
		}
	}
	g_free(pszFileBaseName);

	// free file info
	gnome_vfs_file_info_unref(&info);

//	func_progress_callback(1.0, pCallbackData);
	return bResult;
}
