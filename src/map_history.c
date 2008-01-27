/***************************************************************************
 *            history.c
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

#include <gtk/gtk.h>
#include <string.h>
#include "main.h"
#include "map.h"
#include "map_history.h"

static void map_history_debug_print(maphistory_t* pHistory);

typedef struct {
	mappoint_t MapPoint;
	gint nZoomLevel;
} mapview_t;

maphistory_t* map_history_new()
{
	maphistory_t* pNew = g_new0(maphistory_t, 1);
	pNew->MapViewArray = g_array_new(FALSE, FALSE, sizeof(mapview_t));
	pNew->nCurrentIndex = -1;
	return pNew;
}

void map_history_add(maphistory_t* pHistory, mappoint_t* pPoint, gint nZoomLevel)
{
	g_assert(pHistory != NULL);
	g_assert(pPoint != NULL);

	// If user has clicked BACK a few times, we won't be at the last index in the array...
	if(pHistory->nCurrentIndex < (pHistory->MapViewArray->len - 1)) {
		// ...so clear out everything after where we are
		g_array_remove_range(pHistory->MapViewArray, pHistory->nCurrentIndex + 1, (pHistory->MapViewArray->len - pHistory->nCurrentIndex) - 1);

		pHistory->nTotalItems = (pHistory->nCurrentIndex + 1);	// +1 to change it from an index to a count, it's NOT for the new item we're adding
	}

	// Move to next one
	pHistory->nCurrentIndex++;

	// Grow array if necessary
	if(pHistory->nCurrentIndex >= pHistory->MapViewArray->len) {
		// XXX: is this doing a realloc every time?  ouch. :)
		g_array_set_size(pHistory->MapViewArray, pHistory->MapViewArray->len + 1);
	}

	// Get pointer to new current index
	mapview_t* pNew = &g_array_index(pHistory->MapViewArray, mapview_t, pHistory->nCurrentIndex);
	g_return_if_fail(pNew != NULL);

	// Save details
	memcpy(&(pNew->MapPoint), pPoint, sizeof(mappoint_t));
	pNew->nZoomLevel = nZoomLevel;

	pHistory->nTotalItems++;

	g_assert(pHistory->nCurrentIndex < pHistory->nTotalItems);
	map_history_debug_print(pHistory);
}

gboolean map_history_can_go_forward(maphistory_t* pHistory)
{
	return(pHistory->nCurrentIndex < (pHistory->nTotalItems - 1));
}

gboolean map_history_can_go_back(maphistory_t* pHistory)
{
	return(pHistory->nCurrentIndex > 0);
}

gboolean map_history_go_forward(maphistory_t* pHistory)
{
	if(map_history_can_go_forward(pHistory)) {
		pHistory->nCurrentIndex++;
		map_history_debug_print(pHistory);
		return TRUE;
	}

	return FALSE;
}

gboolean map_history_go_back(maphistory_t* pHistory)
{
	if(map_history_can_go_back(pHistory)) {
		pHistory->nCurrentIndex--;
		map_history_debug_print(pHistory);
		return TRUE;
	}

	return FALSE;
}

void map_history_get_current(maphistory_t* pHistory, mappoint_t* pReturnPoint, gint* pnReturnZoomLevel)
{
	g_assert(pHistory != NULL);
	g_assert(pReturnPoint != NULL);
	g_assert(pnReturnZoomLevel != NULL);
	g_assert(pHistory->nCurrentIndex >= 0);
	g_assert(pHistory->nCurrentIndex < pHistory->nTotalItems);

	mapview_t* pCurrent = &g_array_index(pHistory->MapViewArray, mapview_t, pHistory->nCurrentIndex);

	memcpy(pReturnPoint, &(pCurrent->MapPoint), sizeof(mappoint_t));
	*pnReturnZoomLevel  = pCurrent->nZoomLevel;
}

static void map_history_debug_print(maphistory_t* pHistory)
{
	return;

	gint i;
	g_print("Map History:\n");
	for(i=0 ; i<pHistory->nTotalItems ; i++) {
		mapview_t* pCurrent = &g_array_index(pHistory->MapViewArray, mapview_t, i);

		g_print("%s(%f,%f @ %d)\n", (i==pHistory->nCurrentIndex) ? "*" : "", pCurrent->MapPoint.fLongitude, pCurrent->MapPoint.fLatitude, pCurrent->nZoomLevel);
	}
}
