/***************************************************************************
 *            locationset.c
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

// This module has two functions:
// 1. to provide a list of locationset names, and
// 2. to provide functions to alloc/free and manage locationsets.

#include <stdlib.h>
#include <gtk/gtk.h>
#include "main.h"
#include "location.h"
#include "locationset.h"
#include "db.h"
#include "util.h"
#include "searchwindow.h"		// for defines about glyph size

struct {
	GPtrArray* pLocationSetArray;	// an array of locationsets
	GHashTable* pLocationSetHash;	// stores pointers to locationsets, indexed by ID

	GMemChunk* pLocationSetChunkAllocator;	// allocs locationset_t objects
} g_LocationSet;

void locationset_init()
{
	g_LocationSet.pLocationSetArray = g_ptr_array_new();
	g_LocationSet.pLocationSetHash = g_hash_table_new(g_int_hash, g_int_equal);

	// create memory allocator
	g_LocationSet.pLocationSetChunkAllocator = g_mem_chunk_new("ROADSTER locationsets",
			sizeof(locationset_t), sizeof(locationset_t) * 20, G_ALLOC_AND_FREE);
	g_return_if_fail(g_LocationSet.pLocationSetChunkAllocator != NULL);
}

// get a new locationset struct from the allocator
static gboolean locationset_alloc(locationset_t** ppReturn)
{
	g_return_val_if_fail(g_LocationSet.pLocationSetChunkAllocator != NULL, FALSE);

	locationset_t* pNew = g_mem_chunk_alloc0(g_LocationSet.pLocationSetChunkAllocator);

	// set defaults
	pNew->bVisible = TRUE;

	// return it
	*ppReturn = pNew;
	return TRUE;
}

gboolean locationset_insert(const gchar* pszName, const gchar* pszIconName, gint* pnReturnID)
{
	g_assert(pszName != NULL);
	g_assert(pszIconName != NULL);
	g_assert(pnReturnID != NULL);
	g_assert(*pnReturnID == 0);

	gchar* pszSafeName = db_make_escaped_string(pszName);
	gchar* pszSafeIconName = db_make_escaped_string(pszIconName);
	gchar* pszSQL = g_strdup_printf("INSERT INTO LocationSet SET Name='%s', IconName='%s'", pszSafeName, pszSafeIconName);
	db_free_escaped_string(pszSafeIconName);
	db_free_escaped_string(pszSafeName);

	// create query SQL
	db_query(pszSQL, NULL);
	g_free(pszSQL);

	*pnReturnID = db_get_last_insert_id();
	g_assert(*pnReturnID > 0);
	return TRUE;
}

// Load all locationsets into memory
void locationset_load_locationsets(void)
{
	gchar* pszSQL = g_strdup_printf("SELECT LocationSet.ID, LocationSet.Name, LocationSet.IconName, COUNT(Location.ID) FROM LocationSet LEFT JOIN Location ON (LocationSet.ID=Location.LocationSetID) GROUP BY LocationSet.ID;");

	db_resultset_t* pResultSet = NULL;
	if(db_query(pszSQL, &pResultSet)) {
		db_row_t aRow;

		while((aRow = db_fetch_row(pResultSet))) {
			locationset_t* pNewLocationSet = NULL;
			locationset_alloc(&pNewLocationSet);

			pNewLocationSet->nID = atoi(aRow[0]);
			pNewLocationSet->pszName = g_strdup(aRow[1]);
			pNewLocationSet->pszIconName = g_strdup(aRow[2]);
			pNewLocationSet->pGlyph = glyph_load_at_size(pNewLocationSet->pszIconName, SEARCHWINDOW_SEARCH_RESULT_GLYPH_WIDTH, SEARCHWINDOW_SEARCH_RESULT_GLYPH_HEIGHT);
			pNewLocationSet->pMapGlyph = glyph_load_at_size(pNewLocationSet->pszIconName, 24, 24);
			pNewLocationSet->pMapGlyphSmall = glyph_load_at_size(pNewLocationSet->pszIconName, 6, 6);
			pNewLocationSet->nLocationCount = atoi(aRow[3]);

			// Add the new set to both data structures
			g_ptr_array_add(g_LocationSet.pLocationSetArray, pNewLocationSet);
			g_hash_table_insert(g_LocationSet.pLocationSetHash, &pNewLocationSet->nID, pNewLocationSet);
		}
		db_free_result(pResultSet);	
	}
	g_free(pszSQL);
	return;
}

const GPtrArray* locationset_get_array(void)
{
	return g_LocationSet.pLocationSetArray;
}

gboolean locationset_find_by_id(gint nLocationSetID, locationset_t** ppLocationSet)
{
	locationset_t* pLocationSet = g_hash_table_lookup(g_LocationSet.pLocationSetHash, &nLocationSetID);
	if(pLocationSet) {
		*ppLocationSet = pLocationSet;
		return TRUE;
	}
	return FALSE;	
}

gboolean locationset_is_visible(locationset_t* pLocationSet)
{
	return pLocationSet->bVisible;
}

void locationset_set_visible(locationset_t* pLocationSet, gboolean bVisible)
{
	pLocationSet->bVisible = bVisible;
}

#ifdef ROADSTER_DEAD_CODE
/*
// return a locationset struct (and all locations) to the allocator
static void locationset_free(locationset_t* pLocationSet)
{
	locationset_clear(pLocationSet);

	// give back to allocator
	g_mem_chunk_free(g_LocationSet.pLocationSetChunkAllocator, pLocationSet);
}

static void locationset_clear_all_locations(void)
{
	// delete all sets but don't delete array of sets
	gint i;
	for(i=(g_LocationSet.pLocationSetArray->len)-1 ; i>=0 ; i--) {
		locationset_t* pLocationSet = g_ptr_array_index(g_LocationSet.pLocationSetArray, i);
		locationset_clear(pLocationSet);
	}
//	g_hash_table_foreach_steal(g_LocationSet.pLocationSets, callback_delete_pointset, NULL);
//	g_assert(g_hash_table_size(g_LocationSet.pLocationSets) == 0);
}
*/
#endif
