/***************************************************************************
 *            map_math.c
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
#include "map_math.h"

// ========================================================
//  Coordinate Conversion Functions
// ========================================================

// convert pixels to a span of degrees
gdouble map_pixels_to_degrees(const map_t* pMap, gint16 nPixels, guint16 uZoomLevel)
{
	gdouble fMonitorPixelsPerInch = 85.333;	// XXX: don't hardcode this
	gdouble fPixelsPerMeter = fMonitorPixelsPerInch * INCHES_PER_METER;
	gdouble fMetersOfPixels = ((float)nPixels) / fPixelsPerMeter;

	// If we had 3 meters of pixels (a very big monitor:) and the scale was 1000:1 then
	// we would want to show 3000 meters worth of world space
	gdouble fMetersOfWorld = ((float)g_sZoomLevels[uZoomLevel-1].uScale) * fMetersOfPixels;

	//g_print("nPixels (%d) = fMetersOfPixels (%f) = fMetersOfWorld (%f) = fDegrees (%f)\n", nPixels, fMetersOfPixels, fMetersOfWorld, WORLD_METERS_TO_DEGREES(fMetersOfWorld));
	return WORLD_METERS_TO_DEGREES(fMetersOfWorld);
}

gdouble map_degrees_to_pixels(map_t* pMap, gdouble fDegrees, guint16 uZoomLevel)
{
	gdouble fMonitorPixelsPerInch = 85.333;	// XXX: don't hardcode this

	gdouble fResultInMeters = WORLD_DEGREES_TO_METERS(fDegrees);
	gdouble fResultInPixels = (INCHES_PER_METER * fResultInMeters) * fMonitorPixelsPerInch;
	fResultInPixels /= (float)g_sZoomLevels[uZoomLevel-1].uScale;
	return fResultInPixels;
}

void map_windowpoint_to_mappoint(map_t* pMap, screenpoint_t* pScreenPoint, mappoint_t* pMapPoint)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = (gint)(pScreenPoint->nX) - (pMap->MapDimensions.uWidth / 2);
	gint16 nPixelDeltaY = (gint)(pScreenPoint->nY) - (pMap->MapDimensions.uHeight / 2);

	// Convert pixels to world coordinates
	pMapPoint->fLongitude = pMap->MapCenter.fLongitude + map_pixels_to_degrees(pMap, nPixelDeltaX, pMap->uZoomLevel);
	// reverse the X, clicking above
	pMapPoint->fLatitude = pMap->MapCenter.fLatitude - map_pixels_to_degrees(pMap, nPixelDeltaY, pMap->uZoomLevel);
}

EOverlapType map_rect_a_overlap_type_with_rect_b(const maprect_t* pA, const maprect_t* pB)
{
	// First, quickly determine if there is no overlap
	if(map_rects_overlap(pA,pB) == FALSE) return OVERLAP_NONE;

	if(pA->A.fLatitude < pB->A.fLatitude) return OVERLAP_PARTIAL;
	if(pA->B.fLatitude > pB->B.fLatitude) return OVERLAP_PARTIAL;

	if(pA->A.fLongitude < pB->A.fLongitude) return OVERLAP_PARTIAL;
	if(pA->B.fLongitude > pB->B.fLongitude) return OVERLAP_PARTIAL;

	return OVERLAP_FULL;
}

gboolean map_rects_overlap(const maprect_t* p1, const maprect_t* p2)
{
	if(p1->B.fLatitude < p2->A.fLatitude) return FALSE;
	if(p1->B.fLongitude < p2->A.fLongitude) return FALSE;
	if(p1->A.fLatitude > p2->B.fLatitude) return FALSE;
	if(p1->A.fLongitude > p2->B.fLongitude) return FALSE;

	return TRUE;
}

gboolean map_math_screenpoint_in_screenrect(screenpoint_t* pPt, screenrect_t* pRect)
{
	return(pPt->nX >= pRect->A.nX && pPt->nX <= pRect->B.nX && pPt->nY >= pRect->A.nY && pPt->nY <= pRect->B.nY);
}

gint map_screenrect_width(const screenrect_t* pRect)
{
	gint nDelta = pRect->B.nX - pRect->A.nX;	// NOTE: this works no matter which point has bigger values
	return abs(nDelta);
}
gint map_screenrect_height(const screenrect_t* pRect)
{
	gint nDelta = pRect->B.nY - pRect->A.nY;	// NOTE: this works no matter which point has bigger values
	return abs(nDelta);
}

void map_get_screenrect_centerpoint(const screenrect_t* pRect, screenpoint_t* pPoint)
{
	pPoint->nX = (pRect->A.nX + pRect->B.nX) / 2;		// NOTE: this works no matter which point has bigger values
	pPoint->nY = (pRect->A.nY + pRect->B.nY) / 2;
}

gdouble map_get_distance_in_meters(mappoint_t* pA, mappoint_t* pB)
{
	// XXX: this function is broken.

	// This functions calculates the length of the arc of the "greatcircle" that goes through
	// the two points A and B and whos center is the center of the sphere, O.

	// When we multiply this angle (in radians) by the radius, we get the length of the arc.

	// NOTE: This algorithm wrongly assumes that Earth is a perfect sphere.
	//       It is actually slightly egg shaped.  But it's good enough.

	// All trig functions expect arguments in radians.
	gdouble fLonA_Rad = DEG2RAD(pA->fLongitude);
	gdouble fLonB_Rad = DEG2RAD(pB->fLongitude);
	gdouble fLatA_Rad = DEG2RAD(pA->fLatitude);
	gdouble fLatB_Rad = DEG2RAD(pB->fLatitude);

	// Step 1. Calculate AOB (in radians).
	// An explanation of this equation is at http://mathforum.org/library/drmath/view/51722.html
	gdouble fAOB_Rad = acos((cos(fLatA_Rad) * cos(fLatB_Rad) * cos(fLonB_Rad - fLonA_Rad)) + (sin(fLatA_Rad) * sin(fLatB_Rad)));

	// Step 2. Multiply the angle by the radius of the sphere to get arc length.
	return fAOB_Rad * RADIUS_OF_WORLD_IN_METERS;
}

gdouble map_get_straight_line_distance_in_degrees(mappoint_t* p1, mappoint_t* p2)
{
	gdouble fDeltaX = ((p2->fLongitude) - (p1->fLongitude));
	gdouble fDeltaY = ((p2->fLatitude) - (p1->fLatitude));

	return sqrt((fDeltaX*fDeltaX) + (fDeltaY*fDeltaY));
}

gdouble map_get_distance_in_pixels(map_t* pMap, mappoint_t* p1, mappoint_t* p2)
{
	rendermetrics_t metrics;
	map_get_render_metrics(pMap, &metrics);

	gdouble fX1 = SCALE_X(&metrics, p1->fLongitude);
	gdouble fY1 = SCALE_Y(&metrics, p1->fLatitude);

	gdouble fX2 = SCALE_X(&metrics, p2->fLongitude);
	gdouble fY2 = SCALE_Y(&metrics, p2->fLatitude);

	gdouble fDeltaX = fX2 - fX1;
	gdouble fDeltaY = fY2 - fY1;

	return sqrt((fDeltaX*fDeltaX) + (fDeltaY*fDeltaY));
}


gboolean map_points_equal(mappoint_t* p1, mappoint_t* p2)
{
	return( p1->fLatitude == p2->fLatitude && p1->fLongitude == p2->fLongitude);
}

gboolean map_math_maprects_equal(maprect_t* pA, maprect_t* pB)
{
	return map_points_equal(&(pA->A), &(pB->A)) && map_points_equal(&(pA->B), &(pB->B));
}

//
// clipping a map polygon (array of mappoints) to a maprect
//
typedef enum { EDGE_NORTH, EDGE_SOUTH, EDGE_EAST, EDGE_WEST, EDGE_FIRST=0, EDGE_LAST=3 } ERectEdge;

// static map_math_clip_line_to_worldrect_edge_recursive(mappoint_t* pA, mappoint_t* pB, maprect_t* pRect, ERectEdge eEdge, GArray* pOutput)
// {
//
// }

gboolean map_math_mappoint_in_maprect(const mappoint_t* pPoint, const maprect_t* pRect)
{
	if(pPoint->fLatitude < pRect->A.fLatitude) return FALSE;
	if(pPoint->fLatitude > pRect->B.fLatitude) return FALSE;
	if(pPoint->fLongitude < pRect->A.fLongitude) return FALSE;
	if(pPoint->fLongitude > pRect->B.fLongitude) return FALSE;
	return TRUE;
}

void map_math_clip_pointstring_to_worldrect(GArray* pMapPointsArray, maprect_t* pRect, GArray* pOutput)
{
	gint nLen = pMapPointsArray->len;
	if(nLen <= 2) return;

	mappoint_t* pA = &g_array_index(pMapPointsArray, mappoint_t, 0);
	mappoint_t* pB = NULL;

	gboolean bPointAIsInside = map_math_mappoint_in_maprect(pA, pRect);

	gint i;
	for(i=1 ; i<pMapPointsArray->len ; i++) {
		gint iEdge;
		for(iEdge=EDGE_FIRST ; iEdge<=EDGE_LAST ; iEdge++) {
			switch(iEdge) {
			case EDGE_NORTH:
				break;
			case EDGE_SOUTH:
				break;
			case EDGE_EAST:
				break;
			case EDGE_WEST:
				break;
			}
		}
	}
}

void static map_math_simplify_pointstring_recursive(const GArray* pInput, gint8* pabInclude, gdouble fTolerance, gint iFirst, gint iLast)
{
	if(iFirst+1 >= iLast) return;	// no points between first and last?

	mappoint_t* pA = &g_array_index(pInput, mappoint_t, iFirst);
	mappoint_t* pB = &g_array_index(pInput, mappoint_t, iLast);

	// Init to bad values
	gint iFarthestIndex = -1;
	gdouble fBiggestDistanceSquared = 0.0;

	// Of all points between A and B, which is farthest from the line AB?
	mappoint_t* pPoint;
	gint i;
	for(i=(iFirst+1) ; i<=(iLast-1) ; i++) {
		pPoint = &g_array_index(pInput, mappoint_t, i);
		gdouble fDistanceSquared = map_math_point_distance_squared_from_line(pPoint, pA, pB);

		if(fDistanceSquared > fBiggestDistanceSquared) {
			fBiggestDistanceSquared = fDistanceSquared;
			iFarthestIndex = i;
		}
	}
	if(fBiggestDistanceSquared > (fTolerance * fTolerance) && (iFarthestIndex != -1)) {	// add last test just in case fTolerance == 0.0
		// Mark for inclusion
		pabInclude[iFarthestIndex] = 1;

		map_math_simplify_pointstring_recursive(pInput, pabInclude, fTolerance, iFirst, iFarthestIndex);
		map_math_simplify_pointstring_recursive(pInput, pabInclude, fTolerance, iFarthestIndex, iLast);
	}
}

void map_math_simplify_pointstring(const GArray* pInput, gdouble fTolerance, GArray* pOutput)
{
	if(pInput->len < 2) return;

	gint8* pabInclude = g_new0(gint8, pInput->len + 20);

	// Mark first and last points
	pabInclude[0] = 1;
	pabInclude[pInput->len-1] = 1;

	map_math_simplify_pointstring_recursive(pInput, pabInclude, fTolerance, 0, pInput->len-1);  

	//
	// cleanup
	//
	mappoint_t* pPoint;
	gint i;
	for(i=0 ; i<pInput->len ; i++) {
		pPoint = &g_array_index(pInput, mappoint_t, i);
		if(pabInclude[i] == 1) {
			g_array_append_val(pOutput, *pPoint);
		}
	}
	g_free(pabInclude);
}

// Does the given point come close enough to the line segment to be considered a hit?
gdouble map_math_point_distance_squared_from_line(mappoint_t* pHitPoint, mappoint_t* pPoint1, mappoint_t* pPoint2)
{
	// Some bad ASCII art demonstrating the situation:
	//
	//             / (u)
	//          /  |
	//       /     |
	// (0,0) =====(a)========== (v)

	// v is the translated-to-origin vector of line
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

	return fDistanceSquared;
}


#ifdef ROADSTER_DEAD_CODE
/*
gdouble map_distance_in_units_to_degrees(map_t* pMap, gdouble fDistance, gint nDistanceUnit)
{
	switch(nDistanceUnit) {
		case UNIT_FEET:
			return WORLD_FEET_TO_DEGREES(fDistance);
		case UNIT_MILES:
			return WORLD_MILES_TO_DEGREES(fDistance);
		case UNIT_METERS:
			return WORLD_METERS_TO_DEGREES(fDistance);
		case UNIT_KILOMETERS:
			return WORLD_KILOMETERS_TO_DEGREES(fDistance);
		default:
			g_warning("UNKNOWN DISTANCE UNIT (%d)\n", nDistanceUnit);
			return 0;
	}
}
*/
#endif
