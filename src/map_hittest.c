/***************************************************************************
 *            map_hittest.c
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
#include <math.h>
#include <stdlib.h>
#include "map.h"
#include "map_hittest.h"
#include "map_math.h"
#include "map_style.h"
#include "road.h"
#include "location.h"
#include "locationset.h"

static gboolean map_hittest_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pHitPoint, gdouble fMaxDistance, mappoint_t* pReturnClosestPoint, gdouble* pfReturnPercentAlongLine);
static ESide map_hittest_side_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pClosestPointOnLine, mappoint_t* pHitPoint);
//static gboolean map_hittest_locations(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);
//static gboolean map_hittest_locationsets(map_t* pMap, rendermetrics_t* pRenderMetrics, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);

static gboolean map_hittest_layer_lines(GPtrArray* pRoadsArray, gdouble fMaxDistance, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);
static gboolean map_hittest_layer_polygons(GPtrArray* pMapObjectArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);

#define EXTRA_CLICKABLE_ROAD_IN_PIXELS	(3)

// ========================================================
//  Hit Testing
// ========================================================

gboolean map_hittest(map_t* pMap, mappoint_t* pMapPoint, maphit_t** ppReturnStruct)
{
	rendermetrics_t rendermetrics;
	map_get_render_metrics(pMap, &rendermetrics);

	GPtrArray* pTiles = pMap->pLastActiveTilesArray;
	g_return_val_if_fail(pTiles != NULL, FALSE);

//     if(map_hittest_locationselections(pMap, &rendermetrics, pMap->pLocationSelectionArray, pMapPoint, ppReturnStruct)) {
//         return TRUE;
//     }
//
//     if(map_hittest_locationsets(pMap, &rendermetrics, pMapPoint, ppReturnStruct)) {
//         return TRUE;
//     }

	gint nStyleZoomLevel = g_sZoomLevels[ map_get_zoomlevel(pMap) -1 ].nStyleZoomLevel;

	// Test things in the REVERSE order they are drawn (otherwise we'll match things that have been painted-over)
	gint i;
	for(i=0 ; i<pMap->pLayersArray->len ; i++) {
		maplayer_t* pLayer = g_ptr_array_index(pMap->pLayersArray, i);
		maplayerstyle_t* pLayerStyle = pLayer->paStylesAtZoomLevels[nStyleZoomLevel-1];

		if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_LINES) {
			gdouble fLineWidth = pLayerStyle->fLineWidth;
	
			// XXX: hack, map_pixels should really take a floating point instead.
			gdouble fMaxDistance = map_math_pixels_to_degrees_at_scale(1, map_get_scale(pMap)) * ((fLineWidth/2) + EXTRA_CLICKABLE_ROAD_IN_PIXELS);  // half width on each side

			gint iTile;
			for(iTile=0 ; iTile < pTiles->len ; iTile++) {
				maptile_t* pTile = g_ptr_array_index(pTiles, iTile);

				if(map_hittest_layer_lines(pTile->apMapObjectArrays[pLayer->nDataSource],
										   fMaxDistance,
										   pMapPoint,
										   ppReturnStruct))
				{
					return TRUE;
				}
			}
		}
		else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_POLYGONS) {
			gint iTile;
			for(iTile=0 ; iTile < pTiles->len ; iTile++) {
				maptile_t* pTile = g_ptr_array_index(pTiles, iTile);

				if(map_hittest_layer_polygons(pTile->apMapObjectArrays[pLayer->nDataSource],
											  pMapPoint,
											  ppReturnStruct))
				{
					return TRUE;
				}
			}
		}
	}
//     gint i;
//     for(i=G_N_ELEMENTS(layerdraworder)-1 ; i>=0 ; i--) {
//         if(layerdraworder[i].eSubLayerRenderType != SUBLAYER_RENDERTYPE_LINES) continue;
//
//         gint nLayer = layerdraworder[i].nLayer;
//
//         // use width from whichever layer it's wider in
//         gdouble fLineWidth = max(g_aLayers[nLayer]->Style.aSubLayers[0].afLineWidths[pMap->uZoomLevel-1],
//                      g_aLayers[nLayer]->Style.aSubLayers[1].afLineWidths[pMap->uZoomLevel-1]);
//
//         // XXX: hack, map_pixels should really take a floating point instead.
//         gdouble fMaxDistance = map_pixels_to_degrees(pMap, 1, pMap->uZoomLevel) * ((fLineWidth/2) + EXTRA_CLICKABLE_ROAD_IN_PIXELS);  // half width on each side
//
//         if(map_hittest_layer_lines(pMap->apLayerData[nLayer]->pRoadsArray, fMaxDistance, pMapPoint, ppReturnStruct)) {
//             return TRUE;
//         }
//         // otherwise try next layer...
//     }
	return FALSE;
}

void map_hittest_maphit_free(map_t* pMap, maphit_t* pHitStruct)
{
	if(pHitStruct == NULL) return;

	// free type-specific stuff
	if(pHitStruct->eHitType == MAP_HITTYPE_URL) {
		g_free(pHitStruct->URLHit.pszURL);
	}

	// free common stuff
	g_free(pHitStruct->pszText);
	g_free(pHitStruct);
}

static gboolean map_hittest_layer_lines(GPtrArray* pMapObjectArray, gdouble fMaxDistance, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	g_assert(ppReturnStruct != NULL);
	g_assert(*ppReturnStruct == NULL);	// pointer to null pointer

	/* this is helpful for testing with the g_print()s in map_hit_test_line() */
/*         mappoint_t p1 = {2,2};                */
/*         mappoint_t p2 = {-10,10};             */
/*         mappoint_t p3 = {0,10};               */
/*         map_hit_test_line(&p1, &p2, &p3, 20); */
/*         return FALSE;                         */

	// Loop through line strings, order doesn't matter here since they're all on the same level.
	gint iString;
	for(iString=0 ; iString<pMapObjectArray->len ; iString++) {
		road_t* pRoad = g_ptr_array_index(pMapObjectArray, iString);
		if(pRoad->pMapPointsArray->len < 2) continue;
		// Can't do bounding box test on lines (unless we expand the box by fMaxDistance pixels)
		//if(!map_math_mappoint_in_maprect(pHitPoint, &(pRoad->rWorldBoundingBox))) continue;

		// start on 1 so we can do -1 trick below
		gint iPoint;
		for(iPoint=1 ; iPoint<pRoad->pMapPointsArray->len ; iPoint++) {
			mappoint_t* pPoint1 = &g_array_index(pRoad->pMapPointsArray, mappoint_t, iPoint-1);
			mappoint_t* pPoint2 = &g_array_index(pRoad->pMapPointsArray, mappoint_t, iPoint);

			mappoint_t pointClosest;
			gdouble fPercentAlongLine;

			// hit test this line
			if(map_hittest_line(pPoint1, pPoint2, pHitPoint, fMaxDistance, &pointClosest, &fPercentAlongLine)) {
				//g_print("fPercentAlongLine = %f\n",fPercentAlongLine);

				// fill out a new maphit_t struct with details
				maphit_t* pHitStruct = g_new0(maphit_t, 1);
				pHitStruct->eHitType = MAP_HITTYPE_ROAD;

				if(pRoad->pszName[0] == '\0') {
					pHitStruct->pszText = g_strdup("<i>unnamed</i>");
				}
				else {
					ESide eSide = map_hittest_side_test_line(pPoint1, pPoint2, &pointClosest, pHitPoint);

					gint nAddressStart;
					gint nAddressEnd;
					if(eSide == SIDE_LEFT) {
						nAddressStart = pRoad->nAddressLeftStart;
						nAddressEnd = pRoad->nAddressLeftEnd;
					}
					else {
						nAddressStart = pRoad->nAddressRightStart;
						nAddressEnd = pRoad->nAddressRightEnd;
					}

					if(nAddressStart == 0 || nAddressEnd == 0) {
						pHitStruct->pszText = g_strdup_printf("%s", pRoad->pszName);
					}
					else {
						gint nMinAddres = MIN(nAddressStart, nAddressEnd);
						gint nMaxAddres = MAX(nAddressStart, nAddressEnd);

						pHitStruct->pszText = g_strdup_printf("%s <b>#%d-%d</b>", pRoad->pszName, nMinAddres, nMaxAddres);
					}
				}
				*ppReturnStruct = pHitStruct;
				return TRUE;
			}
		}
	}
	return FALSE;
}

static gboolean map_hittest_layer_polygons(GPtrArray* pMapObjectArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	g_assert(ppReturnStruct != NULL);
	g_assert(*ppReturnStruct == NULL);	// pointer to null pointer

	/* this is helpful for testing with the g_print()s in map_hit_test_line() */
/*         mappoint_t p1 = {2,2};                */
/*         mappoint_t p2 = {-10,10};             */
/*         mappoint_t p3 = {0,10};               */
/*         map_hit_test_line(&p1, &p2, &p3, 20); */
/*         return FALSE;                         */

	// Loop through line strings, order doesn't matter here since they're all on the same level.
	gint iString;
	for(iString=0 ; iString<pMapObjectArray->len ; iString++) {
		road_t* pRoad = g_ptr_array_index(pMapObjectArray, iString);
		if(pRoad->pMapPointsArray->len < 2) continue;
		if(!map_math_mappoint_in_maprect(pHitPoint, &(pRoad->rWorldBoundingBox))) continue;

		if(map_math_mappoint_in_polygon(pHitPoint, pRoad->pMapPointsArray)) {
			maphit_t* pHitStruct = g_new0(maphit_t, 1);
			pHitStruct->pszText = g_strdup("polygon hit");
			pHitStruct->eHitType = MAP_HITTYPE_POLYGON;
			
			*ppReturnStruct = pHitStruct;
			return TRUE;
		}
	}
	return FALSE;
}

static ESide map_hittest_side_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pClosestPointOnLine, mappoint_t* pHitPoint)
{
	// make a translated-to-origin *perpendicular* vector of the line (points to the "left" of the line when walking from point 1 to 2)
	mappoint_t v;
	v.fLatitude = (pPoint2->fLongitude - pPoint1->fLongitude);	// NOTE: swapping lat and lon to make perpendicular
	v.fLongitude = -(pPoint2->fLatitude - pPoint1->fLatitude);

	// make a translated-to-origin vector of the line from closest point to hitpoint
	mappoint_t u;
	u.fLatitude = pHitPoint->fLatitude - pClosestPointOnLine->fLatitude;
	u.fLongitude = pHitPoint->fLongitude - pClosestPointOnLine->fLongitude;

	// figure out the signs of the elements of the vectors
	gboolean bULatPositive = (u.fLatitude >= 0);
	gboolean bULonPositive = (u.fLongitude >= 0);
	gboolean bVLatPositive = (v.fLatitude >= 0);
	gboolean bVLonPositive = (v.fLongitude >= 0);

	//g_print("%s,%s %s,%s\n", bVLonPositive?"Y":"N", bVLatPositive?"Y":"N", bULonPositive?"Y":"N", bULatPositive?"Y":"N");

	// if the signs agree, it's to the left, otherwise to the right
	if(bULatPositive == bVLatPositive && bULonPositive == bVLonPositive) {
		return SIDE_LEFT;
	}
	else {
		// let's check our algorithm: if the signs aren't both the same, they should be both opposite
		// ...unless the vector from hitpoint to line center is 0, which doesn't have sign

		//g_print("%f,%f %f,%f\n", u.fLatitude, u.fLongitude, v.fLatitude, v.fLongitude);
		g_assert(bULatPositive != bVLatPositive || u.fLatitude == 0.0);
		g_assert(bULonPositive != bVLonPositive || u.fLongitude == 0.0);
		return SIDE_RIGHT;
	}
}

// hit test all locations
#if 0
static gboolean map_hittest_locationsets(map_t* pMap, rendermetrics_t* pRenderMetrics, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	//gdouble fMaxDistance = map_math_pixels_to_degrees_at_scale(1, map_get_scale(pMap)) * 3;	// XXX: don't hardcode distance :)

	const GPtrArray* pLocationSetsArray = locationset_get_array();
	gint i;
	for(i=0 ; i<pLocationSetsArray->len ; i++) {
		locationset_t* pLocationSet = g_ptr_array_index(pLocationSetsArray, i);

		// the user is NOT trying to click on invisible things :)
		if(!locationset_is_visible(pLocationSet)) continue;

		// 2. Get array of Locations from the hash table using LocationSetID
		GPtrArray* pLocationsArray;
		pLocationsArray = g_hash_table_lookup(pMap->pLocationArrayHashTable, &(pLocationSet->nID));
		if(pLocationsArray != NULL) {
			if(map_hittest_locations(pMap, pRenderMetrics, pLocationsArray, pHitPoint, ppReturnStruct)) {
				return TRUE;
			}
		}
		else {
			// none loaded
		}
	}
	return FALSE;
}
#endif

// hit-test an array of locations
#if 0
static gboolean map_hittest_locations(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	gint i;
	for(i=(pLocationsArray->len-1) ; i>=0 ; i--) {	// NOTE: test in *reverse* order so we hit the ones drawn on top first
		location_t* pLocation = g_ptr_array_index(pLocationsArray, i);

		// bounding box test
		if(pLocation->Coordinates.fLatitude < pRenderMetrics->rWorldBoundingBox.A.fLatitude
		   || pLocation->Coordinates.fLongitude < pRenderMetrics->rWorldBoundingBox.A.fLongitude
		   || pLocation->Coordinates.fLatitude > pRenderMetrics->rWorldBoundingBox.B.fLatitude
		   || pLocation->Coordinates.fLongitude > pRenderMetrics->rWorldBoundingBox.B.fLongitude)
		{
		    continue;   // not visible
		}

		gdouble fX1 = SCALE_X(pRenderMetrics, pLocation->Coordinates.fLongitude);
		gdouble fY1 = SCALE_Y(pRenderMetrics, pLocation->Coordinates.fLatitude);
		gdouble fX2 = SCALE_X(pRenderMetrics, pHitPoint->fLongitude);
		gdouble fY2 = SCALE_Y(pRenderMetrics, pHitPoint->fLatitude);

		gdouble fDeltaX = fX2 - fX1;
		gdouble fDeltaY = fY2 - fY1;

		fDeltaX = abs(fDeltaX);
		fDeltaY = abs(fDeltaY);

		if(fDeltaX <= 3 && fDeltaY <= 3) {
			// fill out a new maphit_t struct with details
			maphit_t* pHitStruct = g_new0(maphit_t, 1);
			pHitStruct->eHitType = MAP_HITTYPE_LOCATION;
			pHitStruct->LocationHit.nLocationID = pLocation->nID;

			pHitStruct->LocationHit.Coordinates.fLatitude = pLocation->Coordinates.fLatitude;
			pHitStruct->LocationHit.Coordinates.fLongitude = pLocation->Coordinates.fLongitude;

			if(pLocation->pszName[0] == '\0') {
				pHitStruct->pszText = g_strdup_printf("<i>unnamed POI %d</i>", pLocation->nID);
			}
			else {
				pHitStruct->pszText = g_strdup(pLocation->pszName);
			}
			*ppReturnStruct = pHitStruct;
			return TRUE;
		}
	}
	return FALSE;
}
#endif

#if 0
static gboolean map_hittest_locationselections(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	screenpoint_t screenpoint;
	screenpoint.nX = (gint)SCALE_X(pRenderMetrics, pHitPoint->fLongitude);
	screenpoint.nY = (gint)SCALE_Y(pRenderMetrics, pHitPoint->fLatitude);

	gint i;
	for(i=(pLocationSelectionArray->len-1) ; i>=0 ; i--) {
		locationselection_t* pLocationSelection = g_ptr_array_index(pLocationSelectionArray, i);

		if(pLocationSelection->bVisible == FALSE) continue;

		if(map_math_screenpoint_in_screenrect(&screenpoint, &(pLocationSelection->InfoBoxRect))) {
			// fill out a new maphit_t struct with details
			maphit_t* pHitStruct = g_new0(maphit_t, 1);

			if(map_math_screenpoint_in_screenrect(&screenpoint, &(pLocationSelection->InfoBoxCloseRect))) {
				pHitStruct->eHitType = MAP_HITTYPE_LOCATIONSELECTION_CLOSE;
				pHitStruct->pszText = g_strdup("close");
				pHitStruct->LocationSelectionHit.nLocationID = pLocationSelection->nLocationID;
			}
			else if(map_math_screenpoint_in_screenrect(&screenpoint, &(pLocationSelection->EditRect))) {
				pHitStruct->eHitType = MAP_HITTYPE_LOCATIONSELECTION_EDIT;
				pHitStruct->pszText = g_strdup("edit");
				pHitStruct->LocationSelectionHit.nLocationID = pLocationSelection->nLocationID;
			}
			else {
				gboolean bURLMatch = FALSE;

				gint iURL;
				for(iURL=0 ; iURL<pLocationSelection->nNumURLs ; iURL++) {
					if(map_math_screenpoint_in_screenrect(&screenpoint, &(pLocationSelection->aURLs[iURL].Rect))) {
						pHitStruct->eHitType = MAP_HITTYPE_URL;
						pHitStruct->pszText = g_strdup("click to open location");
						pHitStruct->URLHit.pszURL = g_strdup(pLocationSelection->aURLs[iURL].pszURL);

						bURLMatch = TRUE;
						break;
					}
				}

				// no url match, just return a generic "hit the locationselection box"
				if(!bURLMatch) {
					pHitStruct->eHitType = MAP_HITTYPE_LOCATIONSELECTION;
					pHitStruct->pszText = g_strdup("");
					pHitStruct->LocationSelectionHit.nLocationID = pLocationSelection->nLocationID;
				}
			}
			*ppReturnStruct = pHitStruct;
			return TRUE;
		}
	}
	return FALSE;
}
#endif


static gboolean map_hittest_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pHitPoint, gdouble fMaxDistance, mappoint_t* pReturnClosestPoint, gdouble* pfReturnPercentAlongLine)
{
//	if(pHitPoint->fLatitude < (pPoint1->fLatitude - fMaxDistance) && pHitPoint->fLatitude < (pPoint2->fLatitude - fMaxDistance)) return FALSE;
//	if(pHitPoint->fLongitude < (pPoint1->fLongitude - fMaxDistance) && pHitPoint->fLongitude < (pPoint2->fLongitude - fMaxDistance)) return FALSE;

	// Some bad ASCII art demonstrating the situation:
	//
	//             / (u)
	//          /  |
	//       /     |
	// (0,0) =====(a)========== (v)

	// v is the translated-to-origin vector of line (road)
	// u is the translated-to-origin vector of the hitpoint
	// a is the closest point on v to the end of u (the hit point)

	//
	// 1. Convert p1->p2 vector into a vector (v) that is assumed to come out of the origin (0,0)
	//
	mappoint_t v;
	v.fLatitude = pPoint2->fLatitude - pPoint1->fLatitude;	// 10->90 becomes 0->80 (just store 80)
	v.fLongitude = pPoint2->fLongitude - pPoint1->fLongitude;

	gdouble fLengthV = sqrt((v.fLatitude*v.fLatitude) + (v.fLongitude*v.fLongitude));
	if(fLengthV == 0.0) return FALSE;	// bad data: a line segment with no length?

	//
	// 2. Make a unit vector out of v (meaning same direction but length=1) by dividing v by v's length
	//
	mappoint_t unitv;
	unitv.fLatitude = v.fLatitude / fLengthV;
	unitv.fLongitude = v.fLongitude / fLengthV;	// unitv is now a unit (=1.0) length v

	//
	// 3. Translate the hitpoint in the same way we translated v
	//
	mappoint_t u;
	u.fLatitude = pHitPoint->fLatitude - pPoint1->fLatitude;
	u.fLongitude = pHitPoint->fLongitude - pPoint1->fLongitude;

	//
	// 4. Use the dot product of (unitv) and (u) to find (a), the point along (v) that is closest to (u). see diagram above.
	//
	gdouble fLengthAlongV = (unitv.fLatitude * u.fLatitude) + (unitv.fLongitude * u.fLongitude);

	// Does it fall along the length of the line *segment* v?  (we know it falls along the infinite line v, but that does us no good.)
	// (This produces false negatives on round/butt end caps, but that's better that a false positive when another line is actually there!)
	if(fLengthAlongV > 0 && fLengthAlongV < fLengthV) {
		mappoint_t a;
		a.fLatitude = v.fLatitude * (fLengthAlongV / fLengthV);	// multiply each component by the percentage
		a.fLongitude = v.fLongitude * (fLengthAlongV / fLengthV);
		// NOTE: (a) is *not* where it actually hit on the *map*.  don't draw this point!  we'd have to translate it back away from the origin.

		//
		// 5. Calculate the distance from the end of (u) to (a).  If it's less than the fMaxDistance, it's a hit.
		//
		gdouble fRise = u.fLatitude - a.fLatitude;
		gdouble fRun = u.fLongitude - a.fLongitude;
		gdouble fDistanceSquared = fRise*fRise + fRun*fRun;	// compare squared distances. same results but without the sqrt.

		if(fDistanceSquared <= (fMaxDistance*fMaxDistance)) {
			/* debug aids */
			/* g_print("pPoint1 (%f,%f)\n", pPoint1->fLatitude, pPoint1->fLongitude);       */
			/* g_print("pPoint2 (%f,%f)\n", pPoint2->fLatitude, pPoint2->fLongitude);       */
			/* g_print("pHitPoint (%f,%f)\n", pHitPoint->fLatitude, pHitPoint->fLongitude); */
			/* g_print("v (%f,%f)\n", v.fLatitude, v.fLongitude);                           */
			/* g_print("u (%f,%f)\n", u.fLatitude, u.fLongitude);                           */
			/* g_print("unitv (%f,%f)\n", unitv.fLatitude, unitv.fLongitude);               */
			/* g_print("fDotProduct = %f\n", fDotProduct);                                      */
			/* g_print("a (%f,%f)\n", a.fLatitude, a.fLongitude);                           */
			/* g_print("fDistance = %f\n", sqrt(fDistanceSquared));                             */
			if(pReturnClosestPoint) {
				pReturnClosestPoint->fLatitude = a.fLatitude + pPoint1->fLatitude;
				pReturnClosestPoint->fLongitude = a.fLongitude + pPoint1->fLongitude;
			}

			if(pfReturnPercentAlongLine) {
				*pfReturnPercentAlongLine = (fLengthAlongV / fLengthV);
			}
			return TRUE;
		}
	}
	return FALSE;
}
