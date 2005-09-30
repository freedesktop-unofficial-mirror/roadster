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

#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include <libgnomevfs/gnome-vfs.h>
#include <gtk/gtk.h>

typedef enum { DOWNLOADER_RESULT_FAILURE=0, DOWNLOADER_RESULT_SUCCESS } EDownloaderFileResult;

typedef void (*DownloaderCallbackFileResult)(const gchar* pszRemotePath, EDownloaderFileResult eDownloaderResult, const gchar* pszLocalPath);

typedef struct {
	GPtrArray* pPendingArray;
	GPtrArray* pActiveArray;
	gint nMaxConcurrentActive;
} downloader_t;

// public API
downloader_t* downloader_new(gint nMaxConcurrentActive);
void downloader_add_uri(downloader_t* pDownloader, const gchar* pszRemoteFilePath, DownloaderCallbackFileResult pCallbackFileResult);

#endif




