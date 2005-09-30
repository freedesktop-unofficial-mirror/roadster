/***************************************************************************
 *            downloader.c
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

/*
Purpose: A general purpose, easy to use, asyncronous downloader.



*/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libgnomevfs/gnome-vfs.h>
#include <gnome.h>
#include "downloader.h"

#define TEMP_FILE_TEMPLATE	("roadster_download_XXXXXX")	// format of temp files

typedef struct {
	gchar* pszRemoteFilePath;
	gchar* pszLocalFilePath;								// will be NULL until file moves into 'active' list
	gint nBytesDownloaded;								// a count

	downloader_t* pDownloader;					// a handy pointer to the parent
	GnomeVFSAsyncHandle* pGnomeVFSHandle;
	DownloaderCallbackFileResult pCallbackFileResult;	// called when file succeeds or fails
} download_t;

//
// prototypes
//
static gboolean _downloader_begin_download(download_t* pDownload);
static void _downloader_move_pending_to_active(downloader_t* pDownloader);

//
// functions
//
void downloader_init(void)
{
	// nothing
}

downloader_t* downloader_new(gint nMaxConcurrentActive)
{
	downloader_t* pNew = g_new0(downloader_t, 1);
	pNew->pActiveArray = g_ptr_array_new();
	pNew->pPendingArray = g_ptr_array_new();
	pNew->nMaxConcurrentActive = nMaxConcurrentActive;
	return pNew;
}

void downloader_add_uri(downloader_t* pDownloader, const gchar* pszRemoteFilePath, DownloaderCallbackFileResult pCallbackFileResult)
{
	g_assert(pDownloader != NULL);
	g_assert(pszRemoteFilePath != NULL);
	g_assert(pCallbackFileResult != NULL);

	download_t* pNewDownload = g_new0(download_t, 1);
	pNewDownload->pszRemoteFilePath = g_strdup(pszRemoteFilePath);
	pNewDownload->pCallbackFileResult = pCallbackFileResult;
	pNewDownload->pDownloader = pDownloader;

	g_ptr_array_add(pDownloader->pPendingArray, pNewDownload);
	_downloader_move_pending_to_active(pDownloader);
}

// Check to see if we can add any pending files to active list
static void _downloader_move_pending_to_active(downloader_t* pDownloader)
{
	if((pDownloader->pActiveArray->len < pDownloader->nMaxConcurrentActive) && (pDownloader->pPendingArray->len > 0)) {
		// time to promote one from pending
		download_t* pNext = g_ptr_array_index(pDownloader->pPendingArray, 0);
		if(_downloader_begin_download(pNext)) {
			g_ptr_array_remove(pDownloader->pPendingArray, pNext);
			g_ptr_array_add(pDownloader->pActiveArray, pNext);
		}
	}
}

// static void _downloader_gnome_vfs_close_callback(GnomeVFSAsyncHandle *_unused, GnomeVFSResult __unused, gpointer ___unused)
// {
//     //g_print("downloader: a file has been closed\n");
// }

static void _downloader_download_free(downloader_t* pDownloader, download_t* pDownload)
{
	// Empty struct
	g_free(pDownload->pszRemoteFilePath);
	g_free(pDownload->pszLocalFilePath);
	if(pDownload->pGnomeVFSHandle != NULL) {
		// XXX: do we need to close this?
		// gnome_vfs_async_close(pDownload->pGnomeVFSHandle, _downloader_gnome_vfs_close_callback, NULL);
	}

	// XXX: store a 'state' so we know which array it's in?
	g_ptr_array_remove_fast(pDownloader->pActiveArray, pDownload);
	g_ptr_array_remove(pDownloader->pPendingArray, pDownload);

	// Free struct
	g_free(pDownload);
}

static gint _downloader_gnome_vfs_progress_callback(GnomeVFSAsyncHandle *pHandle, GnomeVFSXferProgressInfo *pInfo, download_t* pDownload)
{
	g_assert(pHandle != NULL);
	g_assert(pInfo != NULL);
	g_assert(pDownload != NULL);

	pDownload->nBytesDownloaded = pInfo->bytes_copied;

	if(pInfo->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
		g_print("downloader: downloaded '%s' to '%s' (%d bytes)\n", pDownload->pszRemoteFilePath, pDownload->pszLocalFilePath, pDownload->nBytesDownloaded);

		// Call user-supplied callback
		pDownload->pCallbackFileResult(pDownload->pszRemoteFilePath, DOWNLOADER_RESULT_SUCCESS, pDownload->pszLocalFilePath);

		// callback shouldn't leave the file in the temp directory
		if(g_file_test(pDownload->pszLocalFilePath, G_FILE_TEST_EXISTS)) {
			g_warning("downloader: callback failed to move/delete local file '%s' (from remote '%s')\n", pDownload->pszLocalFilePath, pDownload->pszRemoteFilePath);
		}

		downloader_t* pDownloader = pDownload->pDownloader;
		_downloader_download_free(pDownloader, pDownload);

		// 
		_downloader_move_pending_to_active(pDownloader);
	}
	// XXX: what other statuses messages do we care about?  (failed?)
}

static gboolean _downloader_begin_download(download_t* pDownload)
{
	g_assert(pDownload != NULL);
	//g_print("downloader: beginning download of %s\n", pDownload->pszRemoteFilePath);

	g_assert(pDownload->pszLocalFilePath == NULL);
	gint nHandle = g_file_open_tmp(TEMP_FILE_TEMPLATE, &(pDownload->pszLocalFilePath), NULL);
    if(nHandle == -1) {
		g_warning("downloader: failed to create a temporary file\n");
		return FALSE;
	}
	g_assert(pDownload->pszLocalFilePath != NULL);
	close(nHandle);	// we don't use the file here.  gnome-vfs overwrites it.

	//g_print("downloader: using temp file '%s'\n", pDownload->pszLocalFilePath);

	GnomeVFSURI* pSrcURI = gnome_vfs_uri_new(pDownload->pszRemoteFilePath);
	GList* pSrcList = g_list_prepend(pSrcList, pSrcURI);

	GnomeVFSURI* pDestURI = gnome_vfs_uri_new(pDownload->pszLocalFilePath);
	GList* pDestList = g_list_prepend(pDestList, pDestURI);

	GnomeVFSResult res = gnome_vfs_async_xfer(&(pDownload->pGnomeVFSHandle),
                                             pSrcList, pDestList,
											 GNOME_VFS_XFER_DEFAULT,
											 GNOME_VFS_XFER_ERROR_MODE_ABORT,
											 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,		// overwrite the tmp file we just made.
                                             GNOME_VFS_PRIORITY_MIN,
											 (GnomeVFSAsyncXferProgressCallback)_downloader_gnome_vfs_progress_callback,
											 (gpointer)pDownload,	// callback userdata
                                             NULL,
											 NULL);

	gnome_vfs_uri_unref(pSrcURI);
	gnome_vfs_uri_unref(pDestURI);
	g_list_free(pSrcList);
	g_list_free(pDestList);

	if(res != GNOME_VFS_OK) {
		// return the download_t to a 'pending' state
		g_free(pDownload->pszLocalFilePath); pDownload->pszLocalFilePath = NULL;
		g_assert(pDownload->pGnomeVFSHandle == NULL);
		return FALSE;
	}
	g_assert(pDownload->pGnomeVFSHandle != NULL);
	return TRUE;
}
