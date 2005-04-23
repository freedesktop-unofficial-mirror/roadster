/***************************************************************************
 *            location.c
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
#include "main.h"
#include "map.h"
#include "location.h"
#include "db.h"

struct {
	GMemChunk* m_pLocationChunkAllocator;
} g_Location;

void location_init()
{
	g_Location.m_pLocationChunkAllocator = g_mem_chunk_new("ROADSTER locations",
			sizeof(location_t), 1000, G_ALLOC_AND_FREE);
	g_return_if_fail(g_Location.m_pLocationChunkAllocator != NULL);
}

// get a new point struct from the allocator
gboolean location_alloc(location_t** ppLocation)
{
	g_return_val_if_fail(ppLocation != NULL, FALSE);
	g_return_val_if_fail(*ppLocation == NULL, FALSE);	// must be a pointer to a NULL pointer
	g_return_val_if_fail(g_Location.m_pLocationChunkAllocator != NULL, FALSE);

	location_t* pNew = g_mem_chunk_alloc0(g_Location.m_pLocationChunkAllocator);
	if(pNew) {
		*ppLocation = pNew;
		return TRUE;
	}
	return FALSE;
}

// return a location_t struct to the allocator
void location_free(location_t* pLocation)
{
	g_return_if_fail(pLocation != NULL);
	g_return_if_fail(g_Location.m_pLocationChunkAllocator != NULL);

	g_free(pLocation->m_pszName);

	// give back to allocator
	g_mem_chunk_free(g_Location.m_pLocationChunkAllocator, pLocation);
}

gboolean location_insert(gint nLocationSetID, mappoint_t* pPoint, gint* pnReturnID)
{
	g_assert(pPoint != NULL);
	g_assert(nLocationSetID > 0);

	g_assert(pnReturnID != NULL);
	g_assert(*pnReturnID == 0);	// must be pointer to an int==0

	// create query SQL
	gchar* pszSQL = g_strdup_printf(
		"INSERT INTO Location SET ID=NULL, LocationSetID=%d, Coordinates=GeometryFromText('POINT(%f %f)');",
		nLocationSetID, pPoint->m_fLatitude, pPoint->m_fLongitude);

	db_query(pszSQL, NULL);
	g_free(pszSQL);

	*pnReturnID = db_get_last_insert_id();
	return TRUE;
}

gboolean location_insert_attribute(gint nLocationID, gint nAttributeID, const gchar* pszValue, gint* pnReturnID)
{
	g_assert(nLocationID != 0);
	g_assert(nAttributeID != 0);
	g_assert(pszValue != NULL);

	gchar* pszSQL = g_strdup_printf(
		"INSERT INTO LocationAttributeValue"
		" SET LocationID=%d, AttributeNameID=%d, Value='%s';", 
		nLocationID, nAttributeID, pszValue
		);

	db_query(pszSQL, NULL);
	g_free(pszSQL);

	if(pnReturnID) {
		*pnReturnID = db_get_last_insert_id();
	}
	return TRUE;
}

gboolean location_load(gint nLocationID, mappoint_t* pReturnCoordinates, gint* pnReturnLocationSetID)
{
	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	gchar* pszSQL = g_strdup_printf(
		"SELECT LocationSetID, AsBinary(Coordinates)"
		" FROM Location"
		" WHERE ID=%d", nLocationID);

	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);

	g_return_val_if_fail(pResultSet, FALSE);
	
	aRow = db_fetch_row(pResultSet);

	if(aRow) {
		if(pnReturnLocationSetID != NULL) {
			*pnReturnLocationSetID = atoi(aRow[0]);
		}
		if(pReturnCoordinates != NULL) {
			db_parse_wkb_point(aRow[1], pReturnCoordinates);
		}
		db_free_result(pResultSet);
		return TRUE;
	}
	else {
		db_free_result(pResultSet);
		return FALSE;
	}
}

void location_load_attributes(gint nLocationID, GPtrArray* pAttributeArray)
{
	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	g_assert(pAttributeArray->len == 0);

	gchar* pszSQL = g_strdup_printf(
		"SELECT LocationAttributeValue.ID, LocationAttributeName.Name, LocationAttributeValue.Value"
		" FROM LocationAttributeValue"
		" LEFT JOIN LocationAttributeName ON (LocationAttributeValue.AttributeNameID=LocationAttributeName.ID)"
		" WHERE LocationAttributeValue.LocationID=%d",
		nLocationID
		);

	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);

	if(pResultSet) {
		while((aRow = db_fetch_row(pResultSet))) {
			locationattribute_t* pNew = g_new0(locationattribute_t, 1);

			pNew->m_nValueID = atoi(aRow[0]);
			pNew->m_pszName = g_strdup((aRow[1] == NULL) ? "" : aRow[1]);
			pNew->m_pszValue = g_strdup((aRow[2] == NULL) ? "" : aRow[2]);

			g_ptr_array_add(pAttributeArray, pNew);
		}
		db_free_result(pResultSet);
	}
}

void location_free_attributes(GPtrArray* pAttributeArray)
{
	gint i;
	for(i=(pAttributeArray->len-1) ; i>=0 ; i--) {
		locationattribute_t* pAttribute = g_ptr_array_remove_index_fast(pAttributeArray, i);
		g_free(pAttribute->m_pszName);
		g_free(pAttribute->m_pszValue);
		g_free(pAttribute);
	}
	g_assert(pAttributeArray->len == 0);
}
