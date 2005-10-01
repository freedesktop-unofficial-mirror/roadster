/***************************************************************************
 *            downloader.h
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

#ifndef _DOWNLOADMANAGER_H_
#define _DOWNLOADMANAGER_H_

#include <libgnomevfs/gnome-vfs.h>
#include <gtk/gtk.h>

typedef enum { DOWNLOADMANAGER_RESULT_FAILURE=0, DOWNLOADMANAGER_RESULT_SUCCESS } EDownloadManagerFileResult;

typedef void (*DownloadManagerCallbackFileResult)(const gchar* pszRemotePath, EDownloadManagerFileResult eDownloadManagerResult, const gchar* pszLocalPath);

typedef struct {
	GPtrArray* pPendingArray;
	GPtrArray* pActiveArray;
	gint nMaxConcurrentActive;
} downloadmanager_t;

// public API
downloadmanager_t* downloadmanager_new(gint nMaxConcurrentActive);
void downloadmanager_add_uri(downloadmanager_t* pDownloader, const gchar* pszRemoteFilePath, DownloadManagerCallbackFileResult pCallbackFileResult);

#endif




