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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "location.h"
#include "locationset.h"
#include "db.h"
#include "util.h"

#define DB_LOCATIONS_TABLENAME		("Location")
#define DB_LOCATIONSETS_TABLENAME	("LocationSet")

struct {
	GPtrArray* m_pLocationSetArray;	// an array of locationsets
	GHashTable* m_pLocationSetHash;	// stores pointers to locationsets, indexed by ID

	GMemChunk* m_pLocationSetChunkAllocator;	// allocs locationset_t objects
} g_LocationSet;

void locationset_init()
{
	g_LocationSet.m_pLocationSetArray = g_ptr_array_new();
	g_LocationSet.m_pLocationSetHash = g_hash_table_new(g_int_hash, g_int_equal);

	// create memory allocator
	g_LocationSet.m_pLocationSetChunkAllocator = g_mem_chunk_new("ROADSTER locationsets",
			sizeof(locationset_t), 1000, G_ALLOC_AND_FREE);
	g_return_if_fail(g_LocationSet.m_pLocationSetChunkAllocator != NULL);
}

// get a new locationset struct from the allocator
static locationset_t* locationset_new(void)
{
	g_return_val_if_fail(g_LocationSet.m_pLocationSetChunkAllocator != NULL, NULL);

	return g_mem_chunk_alloc0(g_LocationSet.m_pLocationSetChunkAllocator);
}

static void locationset_clear(locationset_t* pLocationSet)
{
	g_return_if_fail(pLocationSet != NULL);
	g_return_if_fail(g_LocationSet.m_pLocationSetChunkAllocator != NULL);

	// free all location objects
	gint i;
	for(i=((pLocationSet->m_pLocationsArray->len)-1) ; i>=0 ; i--) {
		location_t* pLocation = g_ptr_array_remove_index_fast(pLocationSet->m_pLocationsArray, i);
		location_free(pLocation);
	}
}

// return a locationset struct (and all locations) to the allocator
static void locationset_free(locationset_t* pLocationSet)
{
	locationset_clear(pLocationSet);

	// give back to allocator
	g_mem_chunk_free(g_LocationSet.m_pLocationSetChunkAllocator, pLocationSet);
}

static void locationset_clear_all_locations(void)
{
	// delete all sets but don't delete array of sets
	gint i;
	for(i=(g_LocationSet.m_pLocationSetArray->len)-1 ; i>=0 ; i--) {
		locationset_t* pLocationSet = g_ptr_array_index(g_LocationSet.m_pLocationSetArray, i);
		locationset_clear(pLocationSet);
	}
//	g_hash_table_foreach_steal(g_LocationSet.m_pLocationSets, callback_delete_pointset, NULL);
//	g_assert(g_hash_table_size(g_LocationSet.m_pLocationSets) == 0);
}

void locationset_load_locationsets()
{
	gchar* pszSQL = g_strdup_printf("SELECT ID, Name FROM %s;", DB_LOCATIONSETS_TABLENAME);

	db_resultset_t* pResultSet = NULL;
	if(db_query(pszSQL, &pResultSet)) {
		db_row_t aRow;

		while((aRow = db_fetch_row(pResultSet))) {
			locationset_t* pNewLocationSet = locationset_new();

			pNewLocationSet->m_nID = atoi(aRow[0]);
			pNewLocationSet->m_pszName = g_strdup(aRow[1]);
			//pNewLocationSet->m_pLocationsArray = g_ptr_array_new();

			// Add the new set to both data structures
			g_ptr_array_add(g_LocationSet.m_pLocationSetArray, pNewLocationSet);
			g_hash_table_insert(g_LocationSet.m_pLocationSetHash, &pNewLocationSet->m_nID, pNewLocationSet);
		}
		db_free_result(pResultSet);	
	}
	g_free(pszSQL);
	return;
}

const GPtrArray* locationset_get_set_array()
{
	return g_LocationSet.m_pLocationSetArray;
}

gboolean locationset_add_location(gint nLocationSetID, mappoint_t* pPoint, gint* pReturnID)
{
	g_assert(pPoint != NULL);
	g_assert(pReturnID != NULL);
	g_return_val_if_fail(nLocationSetID != 0, FALSE);

	// create query SQL
	gchar* pszSQL = g_strdup_printf(
		"INSERT INTO %s SET ID=NULL, LocationSetID=%d, Coordinates=GeometryFromText('POINT(%f %f)');",
		DB_LOCATIONS_TABLENAME,
		nLocationSetID,
		pPoint->m_fLatitude, pPoint->m_fLongitude);

	db_query(pszSQL, NULL);
	g_free(pszSQL);

	// return the new ID
	*pReturnID = db_get_last_insert_id();
	return TRUE;
}

static gboolean locationset_find_by_id(gint nLocationSetID, locationset_t** ppLocationSet)
{
	locationset_t* pLocationSet = g_hash_table_lookup(g_LocationSet.m_pLocationSetHash, &nLocationSetID);
	if(pLocationSet) {
		*ppLocationSet = pLocationSet;
		return TRUE;
	}
	return FALSE;	
}

// reads points in given rect into memory
gboolean locationset_load_locations(maprect_t* pRect)
{
	g_assert_not_reached();	// not used/tested

//	TIMER_BEGIN(mytimer, "BEGIN POINT LOAD");

//~ //	g_return_val_if_fail(pGeometrySet != NULL, FALSE);
//~ //	gint nZoomLevel = map_get_zoomlevel();

//	if(!db_is_connected(g_pDB)) return FALSE;

	//~ // HACKY: make a list of layer IDs "2,3,5,6"
	//~ gchar azPointSetIDList[5000] = {0};
	//~ gint nActivePointSetCount = 0;
	//~ gint i;
	//~ for(i=0 ; i <= pPointSetArray->len ;i++) {
		//~ pointset_t* pPointSet = g_ptr_array_index(pPointSetArray, i);

		//~ gchar azPointSetID[10];
		//~ if(nActivePointSetCount > 0) g_snprintf(azPointSetID, 10, ",%d", pPointSet->m_nID);
		//~ else g_snprintf(azPointSetID, 10, "%d", pPointSet->m_nID);
		//~ g_strlcat(azPointSetIDList, azPointSetID, 5000);
		//~ nActivePointSetCount++;
	//~ }
	//~ if(nActivePointSetCount == 0) {
		//~ g_print("no active pointsets!\n");
//~ //		layers_clear();
		//~ return TRUE;
	//~ }
#define LC_EXTRALOAD	(0.05)

	// generate SQL
	gchar azQuery[2000];
	g_snprintf(azQuery, 2000,
		"SELECT ID, LocationSetID, AsText(Coordinates) FROM %s WHERE"
//		" PointSetID IN (%s) AND" //
		" MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)",
		DB_LOCATIONS_TABLENAME,
//		azPointSetIDList,
		pRect->m_A.m_fLatitude + LC_EXTRALOAD, pRect->m_A.m_fLongitude - LC_EXTRALOAD, 	// upper left
		pRect->m_A.m_fLatitude + LC_EXTRALOAD, pRect->m_B.m_fLongitude + LC_EXTRALOAD, 	// upper right
		pRect->m_B.m_fLatitude - LC_EXTRALOAD, pRect->m_B.m_fLongitude + LC_EXTRALOAD, 	// bottom right
		pRect->m_B.m_fLatitude - LC_EXTRALOAD, pRect->m_A.m_fLongitude - LC_EXTRALOAD, 	// bottom left
		pRect->m_A.m_fLatitude + LC_EXTRALOAD, pRect->m_A.m_fLongitude - LC_EXTRALOAD	// upper left again
		);
//	TIMER_SHOW(mytimer, "after SQL generation");
//~ g_print("sql: %s\n", azQuery);
	db_resultset_t* pResultSet = NULL;
	
	if(db_query(azQuery, &pResultSet)) {
//		TIMER_SHOW(mytimer, "after query");
		guint32 uRowCount = 0;

		locationset_clear_all_locations();

		db_row_t aRow;
		while((aRow = mysql_fetch_row(pResultSet))) {
			uRowCount++;

			// aRow[0] is ID
			// aRow[1] is LocationSetID
			// aRow[2] is Coordinates in mysql's text format
//			g_print("data: %s, %s, %s\n", aRow[0], aRow[1], aRow[2]);

			gint nLocationID = atoi(aRow[0]);
			gint nLocationSetID = atoi(aRow[1]);

			// find the locationset to add this to
			locationset_t* pLocationSet = NULL;
			if(locationset_find_by_id(nLocationSetID, &pLocationSet)) {
				// if found (and it should be, at least once we filter by SetID in the SQL above)
				// allocate a new location_t and add it to the set
				location_t* pNewLocation = NULL;
				if(location_new(&pNewLocation)) {
					pNewLocation->m_nID = nLocationID;
					
					g_assert_not_reached();
					// XXX: need to write this using MySQL WKB format
					// db_parse_point(aRow[2], &pNewLocation->m_Coordinates);
					g_ptr_array_add(pLocationSet->m_pLocationsArray, pNewLocation);
				}
			}
			else {
				g_warning("locationset_load_locations: loaded a point but setID (%d) not found to put it in\n", nLocationSetID);
			}
		} // end while loop on rows
//		g_print(" -- got %d location(s)\n", uRowCount);
//		TIMER_SHOW(mytimer, "after rows retrieved");

		db_free_result(pResultSet);
//		TIMER_SHOW(mytimer, "after free results");
//		TIMER_END(mytimer, "END DB LOAD");
		return TRUE;
	}
	return FALSE;
}

/**************************************************************
** PointSets
***************************************************************/

#ifdef ROADSTER_DEAD_CODE
/*
gboolean db_pointset_insert(const gchar* pszName, gint* pReturnID)
{
	if(!db_is_connected()) return FALSE;
	g_assert(pszName != NULL);
	g_assert(pReturnID != NULL);

	// create query SQL
	gchar azQuery[MAX_SQLBUFFER_LEN];
	gchar* pszEscapedName = db_make_escaped_string(pszName);
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "INSERT INTO %s SET ID=NULL, Name='%s';", DB_LOCATIONSETS_TABLENAME, pszEscapedName);
	db_free_escaped_string(pszEscapedName);

	// run query
	if(!db_query(azQuery, NULL)) {
		return FALSE;
	}
	// return the new ID
	*pReturnID = db_insert_id();
	return TRUE;	
}
gboolean db_pointset_delete(gint nPointSetID)
{
	if(!db_is_connected(g_pDB)) return FALSE;

	gchar azQuery[MAX_SQLBUFFER_LEN];
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "DELETE FROM %s WHERE ID=%d;", DB_LOCATIONSETS_TABLENAME, nPointSetID);
	if(MYSQL_RESULT_SUCCESS != mysql_query(g_pDB->m_pMySQLConnection, azQuery)) {
		g_warning("db_pointset_delete: deleting pointset failed: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), azQuery);
		return FALSE;
	}

	g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "DELETE FROM %s WHERE LocationSetID=%d;", DB_LOCATIONS_TABLENAME, nPointSetID);
	if(MYSQL_RESULT_SUCCESS != mysql_query(g_pDB->m_pMySQLConnection, azQuery)) {
		g_warning("db_pointset_delete: deleting points failed: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), azQuery);
		return FALSE;
	}
	return TRUE;
}

gboolean db_point_delete(gint nPointID)
{
	if(!db_is_connected()) return FALSE;

	gchar azQuery[MAX_SQLBUFFER_LEN];
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "DELETE FROM %s WHERE ID=%d;", DB_LOCATIONS_TABLENAME, nPointID);
		
	if(MYSQL_RESULT_SUCCESS != mysql_query(g_pDB->m_pMySQLConnection, azQuery)) {
		g_warning("db_point_delete: deleting point failed: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), azQuery);
		return FALSE;
	}
	return TRUE;
}
*/
#endif
