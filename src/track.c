/***************************************************************************
 *            track.c
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
#include "map.h"
#include "pointstring.h"
#include "point.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

typedef struct track {
	pointstring_t* m_pPointString;
	gint m_nID;
} track_t;

struct {
	GHashTable* m_pTracksHash;
	GMemChunk* g_pTrackChunkAllocator;
} g_Tracks;

void track_init(void)
{
	g_Tracks.m_pTracksHash = g_hash_table_new(g_int_hash, g_int_equal);
	g_Tracks.g_pTrackChunkAllocator = g_mem_chunk_new("ROADSTER tracks",
			sizeof(track_t), 1000, G_ALLOC_AND_FREE);
	g_return_if_fail(g_Tracks.g_pTrackChunkAllocator != NULL);
}

gint track_new()
{
	// insert into DB to get a unique ID
	gint nID = 1;

	// allocate structure for it
	track_t* pNew = g_mem_chunk_alloc0(g_Tracks.g_pTrackChunkAllocator);
	pNew->m_nID = nID;
	pointstring_alloc(&pNew->m_pPointString);

	// save in hash for fast lookups
	g_hash_table_insert(g_Tracks.m_pTracksHash, &pNew->m_nID, pNew);

	return nID;
}

void track_add_point(gint nTrackID, const mappoint_t *pPoint)
{
//	g_print("adding point to track %d\n", nTrackID);

	track_t* pTrack = g_hash_table_lookup(g_Tracks.m_pTracksHash, &nTrackID);
	if(pTrack == NULL) {
		// lookup in DB?

		g_warning("not adding to track %d\n", nTrackID);
		return;
	}

	pointstring_append_point(pTrack->m_pPointString, pPoint);
}

const pointstring_t* track_get_pointstring(gint nID)
{
	track_t* pTrack = g_hash_table_lookup(g_Tracks.m_pTracksHash, &nID);
	if(pTrack != NULL) {
		return pTrack->m_pPointString;
	}
	return NULL;
}
