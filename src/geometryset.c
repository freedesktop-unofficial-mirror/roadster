/***************************************************************************
 *            geometryset.c
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
	A GeometrySet holds an array of geometry objects (used for roads, railroads and parks right now)
*/

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "geometryset.h"
#include "map.h"
#include "util.h"
#include "point.h"
#include "pointstring.h"
#include "db.h"
#include "layers.h"


/*******************************************************
** construction / destruction
*******************************************************/
gboolean geometryset_new(geometryset_t** ppGeometrySet)
{
	g_return_val_if_fail(ppGeometrySet != NULL, FALSE);
	g_return_val_if_fail(*ppGeometrySet == NULL, FALSE);	// must be a pointer to a NULL pointer

	// create geometryset
	geometryset_t* pNew = g_new0(geometryset_t, 1);

	// initialize it
	pNew->m_pPointStringsArray = g_ptr_array_sized_new(20);
	g_return_val_if_fail(pNew->m_pPointStringsArray != NULL, FALSE);

	*ppGeometrySet = pNew;
	return TRUE;
}

// free all shapes
void geometryset_clear(geometryset_t* pGeometrySet)
{
	int i;

	// Free each pointstring
	for(i = (pGeometrySet->m_pPointStringsArray->len - 1) ; i>=0 ; i--) {
		pointstring_t* pPointString = g_ptr_array_remove_index_fast(pGeometrySet->m_pPointStringsArray, i);
		pointstring_free(pPointString);
	}
}

void geometryset_free(geometryset_t* pGeometrySet)
{
	g_assert("FALSE");	// not used/tested/written yet

	g_return_if_fail(pGeometrySet != NULL);
	g_return_if_fail(pGeometrySet->m_pPointStringsArray != NULL);

	// Empty it first...
	geometryset_clear(pGeometrySet);

	g_assert(pGeometrySet->m_pPointStringsArray->len == 0);

	// Free the data structures themselves
	g_ptr_array_free(pGeometrySet->m_pPointStringsArray, FALSE);	// FALSE means don't delete items
	pGeometrySet->m_pPointStringsArray = NULL;
}

gboolean geometryset_load_geometry(maprect_t* pRect)
{
	g_return_val_if_fail(pRect != NULL, FALSE);

	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	gint nZoomLevel = map_get_zoomlevel();

	TIMER_BEGIN(mytimer, "BEGIN Geometry LOAD");

	// HACKY: make a list of layer IDs "2,3,5,6"
	gchar azLayerNumberList[200] = {0};
	gint nActiveLayerCount = 0;
	gint i;
	for(i=LAYER_FIRST ; i <= LAYER_LAST ;i++) {
		if(g_aLayers[i].m_Style.m_aSubLayers[0].m_afLineWidths[nZoomLevel-1] != 0.0 ||
		   g_aLayers[i].m_Style.m_aSubLayers[1].m_afLineWidths[nZoomLevel-1] != 0.0)
		{
			gchar azLayerNumber[10];

			if(nActiveLayerCount > 0) g_snprintf(azLayerNumber, 10, ",%d", i);
			else g_snprintf(azLayerNumber, 10, "%d", i);

			g_strlcat(azLayerNumberList, azLayerNumber, 200);
			nActiveLayerCount++;
		}
	}
	if(nActiveLayerCount == 0) {
		g_print("no visible layers!\n");
		layers_clear();
		return TRUE;
	}

	// generate SQL
	gchar* pszSQL = g_strdup_printf(
		"SELECT Road.ID, Road.TypeID, AsText(Road.Coordinates), RoadName.Name, RoadName.SuffixID"
		" FROM Road "
		" LEFT JOIN Road_RoadName ON (Road.ID=Road_RoadName.RoadID)"
		" LEFT JOIN RoadName ON (Road_RoadName.RoadNameID=RoadName.ID)"
		" WHERE"
		" TypeID IN (%s) AND"
		" MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)",
		azLayerNumberList,
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude, 	// upper left
		pRect->m_A.m_fLatitude, pRect->m_B.m_fLongitude, 	// upper right
		pRect->m_B.m_fLatitude, pRect->m_B.m_fLongitude, 	// bottom right
		pRect->m_B.m_fLatitude, pRect->m_A.m_fLongitude, 	// bottom left
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude		// upper left again
		);
	//g_print("sql: %s\n", pszSQL);

	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);

	TIMER_SHOW(mytimer, "after query");

	guint32 uRowCount = 0;
	if(pResultSet) {
		// HACK: empty out old data, since we don't know how to merge yet
		layers_clear();
		TIMER_SHOW(mytimer, "after clear layers");
		while((aRow = db_fetch_row(pResultSet))) {
			uRowCount++;

			// aRow[0] is ID
			// aRow[1] is TypeID
			// aRow[2] is Coordinates in mysql's text format
			// aRow[3] is road name
			// aRow[4] is road name suffix id
//			g_print("data: %s, %s, %s, %s, %s\n", aRow[0], aRow[1], aRow[2], aRow[3], aRow[4]);

			// Get layer type that this belongs on
			gint nTypeID = atoi(aRow[1]);
			if(nTypeID < LAYER_FIRST || nTypeID > LAYER_LAST) {
				g_warning("geometry record '%s' has bad type '%s'\n", aRow[0], aRow[1]);
				continue;
			}

			// Extract points
			pointstring_t* pNewPointString = NULL;
			if(!pointstring_alloc(&pNewPointString)) {
				g_warning("out of memory loading pointstrings\n");
				continue;
			}
			db_parse_pointstring(aRow[2], pNewPointString, point_alloc);

			// Build name by adding suffix, if one is present
			gchar azFullName[100] = "";

			// does it have a name?			
			if(aRow[3] != NULL && aRow[4] != NULL) {
				gint nSuffixID = atoi(aRow[4]);
				const gchar* pszSuffix = map_road_suffix_itoa(nSuffixID, SUFFIX_LENGTH_SHORT);
				g_snprintf(azFullName, 100, "%s%s%s",
					aRow[3], (pszSuffix[0] != '\0') ? " " : "", pszSuffix);
			}
			pNewPointString->m_pszName = g_strdup(azFullName);

			// Add this item to layer's list of pointstrings
			g_ptr_array_add(g_aLayers[nTypeID].m_pGeometrySet->m_pPointStringsArray, pNewPointString);
		} // end while loop on rows
		g_print("[%d rows]\n", uRowCount);
		TIMER_SHOW(mytimer, "after rows retrieved");

		db_free_result(pResultSet);
		TIMER_SHOW(mytimer, "after free results");
		TIMER_END(mytimer, "END Geometry LOAD");

		return TRUE;
	}
	else {
		g_print(" no rows\n");
		return FALSE;
	}	
}

#ifdef ROADSTER_DEAD_CODE
/*******************************************************
** Debug functions
*******************************************************/
/*
void geometryset_debug_print(geometryset_t* pGeometrySet)
{
	if(pGeometrySet->m_pPointStringsArray == NULL) {
		g_print("m_pPointStringsArray is NULL\n");
	}
	else {
		g_print("pointstring list (%d items):\n", pGeometrySet->m_pPointStringsArray->len);
		int i;
		for(i=0 ; i<pGeometrySet->m_pPointStringsArray->len ; i++) {
			pointstring_t* pPointString = g_ptr_array_index(pGeometrySet->m_pPointStringsArray, i);

			g_print("- string (%d items): ", pPointString->m_pPointsArray->len);
			int j;
			for(j=0 ; j<pPointString->m_pPointsArray->len ; j++) {
				mappoint_t* pPoint = g_ptr_array_index(pPointString->m_pPointsArray, j);
				g_print("(%.5f,%.5f), ", pPoint->m_fLatitude, pPoint->m_fLongitude);
			}
			g_print("\n");
		}
	}
}
*/
#endif
