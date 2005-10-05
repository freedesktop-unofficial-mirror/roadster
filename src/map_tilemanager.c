/***************************************************************************
 *            map_tilemanager.c
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
#include "util.h"
#include "map_tilemanager.h"
#include "db.h"
#include "road.h"

// Prototypes
static void _map_tilemanager_tile_load_map_objects(maptile_t* pTile, maprect_t* pRect, gint nLOD);
static maptile_t* map_tilemanager_tile_cache_lookup(maptilemanager_t* pTileManager, maprect_t* pRect, gint nLOD);
static maptile_t* map_tilemanager_tile_new(maptilemanager_t* pTileManager, maprect_t* pRect, gint nLOD);

struct {
	gdouble fShift;					// the units we care about (eg. 1000 = 1000ths of a degree)
	gint nModulus;					// how many of the above units each tile is on a side
	gdouble fWidth;					// width and height of a tile, in degrees
} g_aTileSizeAtLevelOfDetail[MAP_NUM_LEVELS_OF_DETAIL] = {
	{1000.0, 	70, 	70.0 / 1000.0},
	{100.0, 	35, 	35.0 / 100.0},
	{10.0, 		35, 	35.0 / 10.0},
	{1.0, 		100, 	100.0 / 1.0},
};

// Public API
maptilemanager_t* map_tilemanager_new()
{
	maptilemanager_t* pNew = g_new0(maptilemanager_t, 1);
	
	gint i;
	for(i=0 ; i<MAP_NUM_LEVELS_OF_DETAIL ; i++) {
		pNew->apTileCachedArrays[i] = g_ptr_array_new();
	}
	return pNew;
}

GPtrArray* map_tilemanager_load_tiles_for_worldrect(maptilemanager_t* pTileManager, maprect_t* pRect, gint nLOD)
{
	//
	// Break the worldrect up into the aligned squares that we load
	//
	gdouble fTileShift = g_aTileSizeAtLevelOfDetail[nLOD].fShift;
	gint nTileModulus = g_aTileSizeAtLevelOfDetail[nLOD].nModulus;
	gdouble fTileWidth = g_aTileSizeAtLevelOfDetail[nLOD].fWidth;

	gint32 nLatStart = (gint32)(pRect->A.fLatitude * fTileShift);
	// round it DOWN (south)
	if(pRect->A.fLatitude > 0) {
		nLatStart -= (nLatStart % nTileModulus);
	}
	else {
		nLatStart -= (nLatStart % nTileModulus);
		nLatStart -= nTileModulus;
	}

	gint32 nLonStart = (gint32)(pRect->A.fLongitude * fTileShift);
	// round it DOWN (west)
	if(pRect->A.fLongitude > 0) {
		nLonStart -= (nLonStart % nTileModulus);
	}
	else {
		nLonStart -= (nLonStart % nTileModulus);
		nLonStart -= nTileModulus;
	}

	gint32 nLatEnd = (gint32)(pRect->B.fLatitude * fTileShift);
	// round it UP (north)
	if(pRect->B.fLatitude > 0) {
		nLatEnd -= (nLatEnd % nTileModulus);
		nLatEnd += nTileModulus;
	}
	else {
		nLatEnd -= (nLatEnd % nTileModulus);
	}

	gint32 nLonEnd = (gint32)(pRect->B.fLongitude * fTileShift);
	// round it UP (east)
	if(pRect->B.fLongitude > 0) {
		nLonEnd -= (nLonEnd % nTileModulus);
		nLonEnd += nTileModulus;
	}
	else {
		nLonEnd -= (nLonEnd % nTileModulus);
	}

	// how many tiles are we loading in each direction?
	gint nLatNumTiles = (nLatEnd - nLatStart) / nTileModulus;
	gint nLonNumTiles = (nLonEnd - nLonStart) / nTileModulus;

	gdouble fLatStart = (gdouble)nLatStart / fTileShift;
	gdouble fLonStart = (gdouble)nLonStart / fTileShift;

	if(fLatStart > pRect->A.fLatitude) {
		g_print("fLatStart %f > pRect->A.fLatitude %f\n", fLatStart, pRect->A.fLatitude);
		g_assert(fLatStart <= pRect->A.fLatitude);
	}
	if(fLonStart > pRect->A.fLongitude) {
		g_print("fLonStart %f > pRect->A.fLongitude %f!!\n", fLonStart, pRect->A.fLongitude);
		g_assert_not_reached();
	}

	GPtrArray* pTileArray = g_ptr_array_new();
	g_assert(pTileArray);

	gint nLat,nLon;
	for(nLat = 0 ; nLat < nLatNumTiles ; nLat++) {
		for(nLon = 0 ; nLon < nLonNumTiles ; nLon++) {

			maprect_t rect;
			rect.A.fLatitude = fLatStart + ((gdouble)(nLat) * fTileWidth);
			rect.A.fLongitude = fLonStart + ((gdouble)(nLon) * fTileWidth);
			rect.B.fLatitude = fLatStart + ((gdouble)(nLat+1) * fTileWidth);
			rect.B.fLongitude = fLonStart + ((gdouble)(nLon+1) * fTileWidth);

			maptile_t* pTile = map_tilemanager_tile_cache_lookup(pTileManager, &rect, nLOD);
			if(pTile) {
				// cache hit
				g_ptr_array_add(pTileArray, pTile);
			}
			else {
				// cache miss
				pTile = map_tilemanager_tile_new(pTileManager, &rect, nLOD);
				g_ptr_array_add(pTileArray, pTile);
			}
		}
	}
	return pTileArray;
}

static maptile_t* map_tilemanager_tile_new(maptilemanager_t* pTileManager, maprect_t* pRect, gint nLOD)
{
	//g_print("New tile for (%f,%f),(%f,%f)\n", pRect->A.fLongitude, pRect->A.fLatitude, pRect->B.fLongitude, pRect->B.fLatitude);

	maptile_t* pNewTile = g_new0(maptile_t, 1);
	memcpy(&(pNewTile->rcWorldBoundingBox), pRect, sizeof(maprect_t));

	gint i;
	for(i=0 ; i<MAP_NUM_OBJECT_TYPES ; i++) {
		pNewTile->apMapObjectArrays[i] = g_ptr_array_new();
	}
	g_print("(");
	_map_tilemanager_tile_load_map_objects(pNewTile, pRect, nLOD);
//	_map_tilemanager_tile_load_locations(pNewTile, pRect);
	g_print(")");

	// Add to cache
	g_ptr_array_add(pTileManager->apTileCachedArrays[nLOD], pNewTile);
	return pNewTile;
}

//
// Private functions
//
static maptile_t* map_tilemanager_tile_cache_lookup(maptilemanager_t* pTileManager, maprect_t* pRect, gint nLOD)
{
	// XXX: this should not match on rect, it should match on LOD and Tile ID only
	GPtrArray* pArray = pTileManager->apTileCachedArrays[nLOD];

	gint i;
	for(i=0 ; i<pArray->len ; i++) {
		maptile_t* pTile = g_ptr_array_index(pArray, i);

		if(map_math_maprects_equal(&(pTile->rcWorldBoundingBox), pRect)) {
			//g_print("Cache hit\n");
			return pTile;
		}
	}
	//g_print("cache miss for (%f,%f),(%f,%f)\n", pRect->A.fLongitude, pRect->A.fLatitude, pRect->B.fLongitude, pRect->B.fLatitude);
	return NULL;
}

static void _map_tilemanager_tile_load_map_objects(maptile_t* pTile, maprect_t* pRect, gint nLOD)
{
	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	TIMER_BEGIN(mytimer, "BEGIN Geometry LOAD");

	gchar* pszRoadTableName = g_strdup_printf("Road%d", nLOD);

	// generate SQL
	gchar azCoord1[20], azCoord2[20], azCoord3[20], azCoord4[20], azCoord5[20], azCoord6[20], azCoord7[20], azCoord8[20];
	gchar* pszSQL;
	pszSQL = g_strdup_printf(
		"SELECT 0 AS ID, %s.TypeID, AsBinary(%s.Coordinates), RoadName.Name, RoadName.SuffixID %s"
		" FROM %s "
		" LEFT JOIN RoadName ON (%s.RoadNameID=RoadName.ID)"
		" WHERE"
		" MBRIntersects(GeomFromText('Polygon((%s %s,%s %s,%s %s,%s %s,%s %s))'), Coordinates)",

		//pszRoadTableName,	 no ID column 
		pszRoadTableName, pszRoadTableName,

		// Load all details for LOD 0
		(nLOD == 0) ? ", AddressLeftStart, AddressLeftEnd, AddressRightStart, AddressRightEnd" : "",
		pszRoadTableName, pszRoadTableName,

		// upper left
		g_ascii_dtostr(azCoord1, 20, pRect->A.fLatitude), g_ascii_dtostr(azCoord2, 20, pRect->A.fLongitude), 
		// upper right
		g_ascii_dtostr(azCoord3, 20, pRect->A.fLatitude), g_ascii_dtostr(azCoord4, 20, pRect->B.fLongitude), 
		// bottom right
		g_ascii_dtostr(azCoord5, 20, pRect->B.fLatitude), g_ascii_dtostr(azCoord6, 20, pRect->B.fLongitude), 
		// bottom left
		g_ascii_dtostr(azCoord7, 20, pRect->B.fLatitude), g_ascii_dtostr(azCoord8, 20, pRect->A.fLongitude), 
		// upper left again
		azCoord1, azCoord2);

	//g_print("sql: %s\n", pszSQL);

	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);
	g_free(pszRoadTableName);

	TIMER_SHOW(mytimer, "after query");

	guint32 uRowCount = 0;
	if(pResultSet) {
		while((aRow = db_fetch_row(pResultSet))) {
			uRowCount++;

			// aRow[0] is ID
			// aRow[1] is TypeID
			// aRow[2] is Coordinates in mysql's text format
			// aRow[3] is road name
			// aRow[4] is road name suffix id
			// aRow[5] is road address left start
			// aRow[6] is road address left end
			// aRow[7] is road address right start 
			// aRow[8] is road address right end

			// Get layer type that this belongs on
			gint nTypeID = atoi(aRow[1]);
			if(nTypeID < MAP_OBJECT_TYPE_FIRST || nTypeID > MAP_OBJECT_TYPE_LAST) {
				g_warning("geometry record '%s' has bad type '%s'\n", aRow[0], aRow[1]);
				continue;
			}

			//road_t* pNewRoad = NULL;
			//road_alloc(&pNewRoad);
			road_t* pNewRoad = g_new0(road_t, 1);

			// Build name by adding suffix, if one is present
			if(aRow[3] != NULL && aRow[4] != NULL) {
				const gchar* pszSuffix = road_suffix_itoa(atoi(aRow[4]), ROAD_SUFFIX_LENGTH_SHORT);
				pNewRoad->pszName = g_strdup_printf("%s%s%s", aRow[3], (pszSuffix[0] != '\0') ? " " : "", pszSuffix);
			}
			else {
				pNewRoad->pszName = g_strdup("");	// XXX: could we maybe not do this?
			}
			// We only load this st
			if(nLOD == MAP_LEVEL_OF_DETAIL_BEST) {
				pNewRoad->nAddressLeftStart = atoi(aRow[5]);
				pNewRoad->nAddressLeftEnd = atoi(aRow[6]);
				pNewRoad->nAddressRightStart = atoi(aRow[7]);
				pNewRoad->nAddressRightEnd = atoi(aRow[8]);
			}

			// perhaps let the wkb parser create the array (at the perfect size)
			pNewRoad->pMapPointsArray = g_array_new(FALSE, FALSE, sizeof(road_t));
			db_parse_wkb_linestring(aRow[2], pNewRoad->pMapPointsArray, &(pNewRoad->rWorldBoundingBox));

#ifdef ENABLE_RIVER_TO_LAKE_LOADTIME_HACK	// XXX: combine this and above hack and you get lakes with squiggly edges. whoops. :)
			if(nTypeID == MAP_OBJECT_TYPE_RIVER) {
				mappoint_t* pPointA = &g_array_index(pNewRoad->pMapPointsArray, mappoint_t, 0);
				mappoint_t* pPointB = &g_array_index(pNewRoad->pMapPointsArray, mappoint_t, pNewRoad->pMapPointsArray->len-1);

				if(pPointA->fLatitude == pPointB->fLatitude && pPointA->fLongitude == pPointB->fLongitude) {
					nTypeID = MAP_OBJECT_TYPE_LAKE;
				}
			}
#endif

			// Add this item to layer's list of pointstrings
			g_ptr_array_add(pTile->apMapObjectArrays[nTypeID], pNewRoad);
		} // end while loop on rows
		//g_print("[%d rows]\n", uRowCount);
		TIMER_SHOW(mytimer, "after rows retrieved");

		db_free_result(pResultSet);
		TIMER_SHOW(mytimer, "after free results");
		TIMER_END(mytimer, "END Geometry LOAD");
	}
}

// static gboolean map_data_load_locations(map_t* pMap, maprect_t* pRect)
// {
//     g_return_val_if_fail(pMap != NULL, FALSE);
//     g_return_val_if_fail(pRect != NULL, FALSE);
//
// //     if(map_get_zoomlevel(pMap) < MIN_ZOOM_LEVEL_FOR_LOCATIONS) {
// //         return TRUE;
// //     }
//
//     TIMER_BEGIN(mytimer, "BEGIN Locations LOAD");
//
//     // generate SQL
//     gchar* pszSQL;
//     gchar azCoord1[20], azCoord2[20], azCoord3[20], azCoord4[20], azCoord5[20], azCoord6[20], azCoord7[20], azCoord8[20];
//     pszSQL = g_strdup_printf(
//         "SELECT Location.ID, Location.LocationSetID, AsBinary(Location.Coordinates), LocationAttributeValue_Name.Value" // LocationAttributeValue_Name.Value is the "Name" field of this Location
//         " FROM Location"
//         " LEFT JOIN LocationAttributeValue AS LocationAttributeValue_Name ON (LocationAttributeValue_Name.LocationID=Location.ID AND LocationAttributeValue_Name.AttributeNameID=%d)"
//         " WHERE"
//         " MBRIntersects(GeomFromText('Polygon((%s %s,%s %s,%s %s,%s %s,%s %s))'), Coordinates)",
//         LOCATION_ATTRIBUTE_ID_NAME, // attribute ID for 'name'
//         // upper left
//         g_ascii_dtostr(azCoord1, 20, pRect->A.fLatitude), g_ascii_dtostr(azCoord2, 20, pRect->A.fLongitude),
//         // upper right
//         g_ascii_dtostr(azCoord3, 20, pRect->A.fLatitude), g_ascii_dtostr(azCoord4, 20, pRect->B.fLongitude),
//         // bottom right
//         g_ascii_dtostr(azCoord5, 20, pRect->B.fLatitude), g_ascii_dtostr(azCoord6, 20, pRect->B.fLongitude),
//         // bottom left
//         g_ascii_dtostr(azCoord7, 20, pRect->B.fLatitude), g_ascii_dtostr(azCoord8, 20, pRect->A.fLongitude),
//         // upper left again
//         azCoord1, azCoord2);
//     //g_print("sql: %s\n", pszSQL);
//
//     db_resultset_t* pResultSet = NULL;
//     db_query(pszSQL, &pResultSet);
//     g_free(pszSQL);
//
//     TIMER_SHOW(mytimer, "after query");
//
//     guint32 uRowCount = 0;
//     if(pResultSet) {
//         db_row_t aRow;
//         while((aRow = db_fetch_row(pResultSet))) {
//             uRowCount++;
//
//             // aRow[0] is ID
//             // aRow[1] is LocationSetID
//             // aRow[2] is Coordinates in mysql's binary format
//             // aRow[3] is Name
//
//             // Get layer type that this belongs on
//             gint nLocationSetID = atoi(aRow[1]);
//
//             // Extract
//             location_t* pNewLocation = NULL;
//             location_alloc(&pNewLocation);
//
//             pNewLocation->nID = atoi(aRow[0]);
//
//             // Parse coordinates
//             db_parse_wkb_point(aRow[2], &(pNewLocation->Coordinates));
//
//             // make a copy of the name field, or "" (never leave it == NULL)
//             pNewLocation->pszName = g_strdup(aRow[3] != NULL ? aRow[3] : "");
//             map_store_location(pMap, pNewLocation, nLocationSetID);
//         } // end while loop on rows
//         //g_print("[%d rows]\n", uRowCount);
//         TIMER_SHOW(mytimer, "after rows retrieved");
//
//         db_free_result(pResultSet);
//         TIMER_END(mytimer, "END Locations LOAD");
//         return TRUE;
//     }
//     else {
//         TIMER_END(mytimer, "END Locations LOAD (0 results)");
//         return FALSE;
//     }
// }
//
// static void map_data_clear(map_t* pMap)
// {
//     // Clear layers
//     gint i,j;
//     for(i=0 ; i<G_N_ELEMENTS(pMap->apLayerData) ; i++) {
//         maplayer_data_t* pLayerData = pMap->apLayerData[i];
//
//         // Free each
//         for(j = (pLayerData->pRoadsArray->len - 1) ; j>=0 ; j--) {
//             road_t* pRoad = g_ptr_array_remove_index_fast(pLayerData->pRoadsArray, j);
//             g_array_free(pRoad->pMapPointsArray, TRUE);
//             g_free(pRoad);
//         }
//         g_assert(pLayerData->pRoadsArray->len == 0);
//     }
//
//     // Clear locations
//     map_init_location_hash(pMap);
// }
