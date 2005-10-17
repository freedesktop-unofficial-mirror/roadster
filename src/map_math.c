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
// gdouble map_pixels_to_degrees(const map_t* pMap, gint16 nPixels, guint16 uZoomLevel)
// {
//     gdouble fMonitorPixelsPerInch = 85.333; // XXX: don't hardcode this
//     gdouble fPixelsPerMeter = fMonitorPixelsPerInch * INCHES_PER_METER;
//     gdouble fMetersOfPixels = ((float)nPixels) / fPixelsPerMeter;
//
//     // If we had 3 meters of pixels (a very big monitor:) and the scale was 1000:1 then
//     // we would want to show 3000 meters worth of world space
//     gdouble fMetersOfWorld = ((float)g_sZoomLevels[uZoomLevel-1].uScale) * fMetersOfPixels;
//
//     //g_print("nPixels (%d) = fMetersOfPixels (%f) = fMetersOfWorld (%f) = fDegrees (%f)\n", nPixels, fMetersOfPixels, fMetersOfWorld, WORLD_METERS_TO_DEGREES(fMetersOfWorld));
//     return WORLD_METERS_TO_DEGREES(fMetersOfWorld);
// }

gdouble map_math_pixels_to_degrees_at_scale(gint nPixels, gint nScale)
{
	gdouble fMonitorPixelsPerInch = 85.333;	// XXX: don't hardcode this
	gdouble fPixelsPerMeter = fMonitorPixelsPerInch * INCHES_PER_METER;
	gdouble fMetersOfPixels = ((gdouble)nPixels) / fPixelsPerMeter;

	// If we had 3 meters of pixels (a very big monitor:) and the scale was 1:1000 then
	// we would want to show 3000 meters worth of world space
	gdouble fMetersOfWorld = ((gdouble)nScale) * fMetersOfPixels;

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
	pMapPoint->fLongitude = pMap->MapCenter.fLongitude + map_math_pixels_to_degrees_at_scale(nPixelDeltaX, map_get_scale(pMap));
	// reverse the X, clicking above
	pMapPoint->fLatitude = pMap->MapCenter.fLatitude - map_math_pixels_to_degrees_at_scale(nPixelDeltaY, map_get_scale(pMap));
}

EOverlapType map_rect_a_overlap_type_with_rect_b(const maprect_t* pA, const maprect_t* pB)
{
	// First, quickly determine if there is no overlap
	if(map_rects_overlap(pA,pB) == FALSE) return OVERLAP_NONE;

	if(pA->A.fLongitude < pB->A.fLongitude) return OVERLAP_PARTIAL;
	if(pA->B.fLongitude > pB->B.fLongitude) return OVERLAP_PARTIAL;

	if(pA->A.fLatitude < pB->A.fLatitude) return OVERLAP_PARTIAL;
	if(pA->B.fLatitude > pB->B.fLatitude) return OVERLAP_PARTIAL;

	return OVERLAP_FULL;
}

gboolean map_rects_overlap(const maprect_t* p1, const maprect_t* p2)
{
	if(p1->A.fLongitude > p2->B.fLongitude) return FALSE;
	if(p1->B.fLongitude < p2->A.fLongitude) return FALSE;
	if(p1->A.fLatitude > p2->B.fLatitude) return FALSE;
	if(p1->B.fLatitude < p2->A.fLatitude) return FALSE;

	return TRUE;
}

// gboolean map_math_mappoint_in_maprect(mappoint_t* pPt, maprect_t* pRect)
// {
//     return(pPt->fLatitude >= pRect->A.fLatitude && pPt->fLatitude <= pRect->B.fLatitude && pPt->fLongitude >= pRect->A.fLongitude && pPt->fLongitude <= pRect->B.fLongitude);
// }

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
typedef enum { EDGE_NORTH, EDGE_EAST, EDGE_SOUTH, EDGE_WEST, EDGE_FIRST=0, EDGE_LAST=3 } ERectEdge;

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

gboolean map_math_line_segments_overlap(const mappoint_t* pA1, const mappoint_t* pA2, const mappoint_t* pB1, const mappoint_t* pB2)
{
	// XXX: unwritten
	return FALSE;
}

gboolean map_math_mappoint_in_polygon(const mappoint_t* pPoint, const GArray* pMapPointsArray)
{
	gint i;
	mappoint_t ptDistant = {1000.0, 1000.0};		// Outside of the world rect, so should do..?

	gint nNumLineIntersections = 0;

	// Loop through all line segments in pMapPointsArray
	for(i=0 ; i<(pMapPointsArray->len-1) ; i++) {
		mappoint_t* pA = &g_array_index(pMapPointsArray, mappoint_t, i);
		mappoint_t* pB = &g_array_index(pMapPointsArray, mappoint_t, i+1);

		// If segment [pPoint,ptDistant] overlaps [pA,pB], add one to the intersection count
		if(map_math_line_segments_overlap(pPoint, &ptDistant, pA, pB)) {
			nNumLineIntersections++;
		}
	}

	return ((nNumLineIntersections & 1) == 1);	// An odd count means point is in polygon
}

void map_math_get_intersection_of_line_segment_and_horizontal_line(const mappoint_t* pA, const mappoint_t* pB, gdouble fLineY, mappoint_t* pReturnPoint)
{
	// NOTE: this function ASSUMES a collision

	gdouble fDeltaX = (pB->fLongitude - pA->fLongitude);
	gdouble fDeltaY = (pB->fLatitude - pA->fLatitude);

	// Very simple algorithm here: if the line is 30% of the way from A.y to B.y, it's also 30% of X!
	//
	//        /
	// ------/--------
	//      /
	gdouble fCrossX = pA->fLongitude + (((fLineY - pA->fLatitude) / fDeltaY) * fDeltaX);

	pReturnPoint->fLongitude = fCrossX;
	pReturnPoint->fLatitude = fLineY;
}

void map_math_get_intersection_of_line_segment_and_vertical_line(const mappoint_t* pA, const mappoint_t* pB, gdouble fLineX, mappoint_t* pReturnPoint)
{
	gdouble fDeltaX = (pB->fLongitude - pA->fLongitude);
	gdouble fDeltaY = (pB->fLatitude - pA->fLatitude);
	gdouble fCrossY = pA->fLatitude + (((fLineX - pA->fLongitude) / fDeltaX) * fDeltaY);

	pReturnPoint->fLongitude = fLineX;
	pReturnPoint->fLatitude = fCrossY;
}

gboolean map_math_mappoints_equal(const mappoint_t* pA, const mappoint_t* pB)
{
	return (pA->fLatitude == pB->fLatitude && pA->fLongitude == pB->fLongitude);
}

typedef struct {
	gint nPointsSeen;
	mappoint_t ptFirst;
	mappoint_t ptPrevious;
	gboolean bPreviousWasInside;
} clip_stage_data_t;

typedef struct {
	clip_stage_data_t aStageData[4];
	GArray* pOutputArray;
	maprect_t* pClipRect;
} clip_data_t;

static void map_math_clip_linesegment_to_worldrect_edge_recursive(clip_data_t* pClipData, const mappoint_t* pCurrent, gint nEdge)
{
	// A function pointer and argument for finding a crossing point (where a line segment crosses an axis-aligned line)
	void (*segment_vs_axis_line_intersection_function)(const mappoint_t* pA, const mappoint_t* pB, gdouble fLinePosition, mappoint_t* pReturnPoint);
	gdouble fLinePosition;

	//g_debug("edge %d got point %f,%f", nEdge, pCurrent->fLongitude, pCurrent->fLatitude);

	clip_stage_data_t* pStage = &pClipData->aStageData[nEdge];	// can obviously only access this if nEdge is valid

	gboolean bCurrentIsInside;
	switch(nEdge) {
	case EDGE_NORTH:
		bCurrentIsInside = (pCurrent->fLatitude < pClipData->pClipRect->B.fLatitude);
		segment_vs_axis_line_intersection_function = map_math_get_intersection_of_line_segment_and_horizontal_line;
		fLinePosition = pClipData->pClipRect->B.fLatitude;
	break;
	case EDGE_EAST:
		bCurrentIsInside = (pCurrent->fLongitude < pClipData->pClipRect->B.fLongitude);
		segment_vs_axis_line_intersection_function = map_math_get_intersection_of_line_segment_and_vertical_line;
		fLinePosition = pClipData->pClipRect->B.fLongitude;
	break;
	case EDGE_SOUTH:
		bCurrentIsInside = (pCurrent->fLatitude >= pClipData->pClipRect->A.fLatitude);
		segment_vs_axis_line_intersection_function = map_math_get_intersection_of_line_segment_and_horizontal_line;
		fLinePosition = pClipData->pClipRect->A.fLatitude;
	break;
	case EDGE_WEST:
		bCurrentIsInside = (pCurrent->fLongitude >= pClipData->pClipRect->A.fLongitude);
		segment_vs_axis_line_intersection_function = map_math_get_intersection_of_line_segment_and_vertical_line;
		fLinePosition = pClipData->pClipRect->A.fLongitude;
	break;
	default:
		// We get here when we've completed all four clipping planes.
		g_array_append_val(pClipData->pOutputArray, *pCurrent);
		return;
	}

	mappoint_t ptCrossing;
	if(pStage->nPointsSeen == 0) {
		// Just save the point.  We can't do anything with one point!
		pStage->ptFirst = *pCurrent;
	}
	else {
		if(bCurrentIsInside) {
			if(pStage->bPreviousWasInside == FALSE) {
				//g_debug("point entering");
				segment_vs_axis_line_intersection_function(pCurrent, &(pStage->ptPrevious), fLinePosition, &ptCrossing);
				map_math_clip_linesegment_to_worldrect_edge_recursive(pClipData, &ptCrossing, nEdge + 1);
			}
			map_math_clip_linesegment_to_worldrect_edge_recursive(pClipData, pCurrent, nEdge + 1);
		}
		else if(pStage->bPreviousWasInside) {
			//g_debug("point leaving");
			segment_vs_axis_line_intersection_function(pCurrent, &(pStage->ptPrevious), fLinePosition, &ptCrossing);
			map_math_clip_linesegment_to_worldrect_edge_recursive(pClipData, &ptCrossing, nEdge + 1);
		}
	}
	pStage->ptPrevious = *pCurrent;
	pStage->bPreviousWasInside = bCurrentIsInside;
	pStage->nPointsSeen++;
}

static void map_math_clip_linesegment_to_worldrect_edge_finalize_recursive(clip_data_t* pClipData, gint nEdge)
{
	if(nEdge > EDGE_LAST) return;	// end recursion

	// Connect last point to first point for this stage
	clip_stage_data_t* pStage = &(pClipData->aStageData[nEdge]);	// can obviously only access this if nEdge is valid
	map_math_clip_linesegment_to_worldrect_edge_recursive(pClipData, &(pStage->ptFirst), nEdge);

	// Continue cleanup, recursive
	map_math_clip_linesegment_to_worldrect_edge_finalize_recursive(pClipData, nEdge + 1);
}

void map_math_clip_pointstring_to_worldrect(GArray* pMapPointsArray, maprect_t* pRect, GArray* pOutput)
{
	g_assert(EDGE_FIRST == 0);
	g_assert(EDGE_LAST == 3);	// we make these assumptions with our array indexing and nEdge incrementing

	if(pMapPointsArray->len <= 2) return;

	// Initialize clip data (most of it defaults to 0s)
	clip_data_t* pClipData = g_new0(clip_data_t, 1);
	pClipData->pOutputArray = pOutput;
	pClipData->pClipRect = pRect;

	// Pass each point through the clippers (recursively)
	gint i;
	for(i=0 ; i<pMapPointsArray->len ; i++) {
		mappoint_t* pCurrent = &g_array_index(pMapPointsArray, mappoint_t, i);
		map_math_clip_linesegment_to_worldrect_edge_recursive(pClipData, pCurrent, EDGE_FIRST);
	}

	// Finalize clippers (recursively)
	map_math_clip_linesegment_to_worldrect_edge_finalize_recursive(pClipData, EDGE_FIRST);
	g_free(pClipData);
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
	if((fBiggestDistanceSquared > (fTolerance * fTolerance)) && (iFarthestIndex != -1)) {	// add last test just in case fTolerance == 0.0
		// Mark for inclusion
		pabInclude[iFarthestIndex] = 1;

		map_math_simplify_pointstring_recursive(pInput, pabInclude, fTolerance, iFirst, iFarthestIndex);
		map_math_simplify_pointstring_recursive(pInput, pabInclude, fTolerance, iFarthestIndex, iLast);
	}
}

void map_math_simplify_pointstring(const GArray* pInput, gdouble fTolerance, GArray* pOutput)
{
	if(pInput->len <= 2) {
		// Can't simplify this.
		g_array_append_vals(pOutput, &g_array_index(pInput, mappoint_t, 0), pInput->len);
		return;
	}

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
	if(fLengthV == 0.0) {
		g_warning("fLengthV == 0.0 in map_math_point_distance_squared_from_line");
		return 0.0;	// bad data: a line segment with no length?
	}

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

	//
	// 5. Calculate the distance from the end of (u) to (a).  If it's less than the fMaxDistance, it's a hit.
	//
	gdouble fRise = u.fLatitude - a.fLatitude;
	gdouble fRun = u.fLongitude - a.fLongitude;
	gdouble fDistanceSquared = fRise*fRise + fRun*fRun;	// compare squared distances. same results but without the sqrt.

	return fDistanceSquared;
}

// Attempt to add B onto A
gboolean map_math_try_connect_linestrings(GArray* pA, const GArray* pB)
{
	if(pB->len < 2) return TRUE;	// nothing to do

	if(pA->len == 0) {
		// copy all
        g_array_append_vals(pA, &g_array_index(pB, mappoint_t, 0), pB->len);
		return TRUE;
	}

	// Does A end at B?
	if(map_math_mappoints_equal(&g_array_index(pA, mappoint_t, pA->len-1), &g_array_index(pB, mappoint_t, 0))) {
        g_array_append_vals(pA, &g_array_index(pB, mappoint_t, 0), pB->len);
		return TRUE;
	}
	// Does B end at A?
	if(map_math_mappoints_equal(&g_array_index(pB, mappoint_t, pB->len-1), &g_array_index(pB, mappoint_t, 0))) {
        g_array_prepend_vals(pA, &g_array_index(pB, mappoint_t, 0), pB->len);
		return TRUE;
	}
	// Do they start in the same place?
	if(map_math_mappoints_equal(&g_array_index(pA, mappoint_t, 0), &g_array_index(pB, mappoint_t, 0))) {
		// flip B and prepend to A
		gint i;
		for(i=pB->len-1 ; i>=1 ; i--) {	// NOTE the >=1 to skip the first point of B
			g_array_prepend_val(pA, g_array_index(pB, mappoint_t, i));
		}
		return TRUE;
	}
	// Do they end in the same place?
	if(map_math_mappoints_equal(&g_array_index(pA, mappoint_t, pA->len-1), &g_array_index(pB, mappoint_t, pB->len-1))) {
		// flip B and append to A
		gint i;
		for(i=pB->len-2 ; i>=0 ; i--) {	// NOTE the -2 to skip the last point of B
			g_array_append_val(pA, g_array_index(pB, mappoint_t, i));
		}
		return TRUE;
	}
	return FALSE;
}

// Update pA to include pB
void map_util_bounding_box_union(maprect_t* pA, const maprect_t* pB)
{
	pA->A.fLatitude = MIN(pA->A.fLatitude, pB->A.fLatitude);
	pA->A.fLongitude = MIN(pA->A.fLongitude, pB->A.fLongitude);
	
	pA->B.fLatitude = MAX(pA->B.fLatitude, pB->B.fLatitude);
	pA->B.fLongitude = MAX(pA->B.fLongitude, pB->B.fLongitude);
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

gdouble map_get_distance_in_pixels(map_t* pMap, mappoint_t* p1, mappoint_t* p2)
{
	rendermetrics_t metrics;
	map_get_render_metrics(pMap, &metrics);

	// XXX: this can overflow, me thinks
	gdouble fX1 = SCALE_X(&metrics, p1->fLongitude);
	gdouble fY1 = SCALE_Y(&metrics, p1->fLatitude);

	gdouble fX2 = SCALE_X(&metrics, p2->fLongitude);
	gdouble fY2 = SCALE_Y(&metrics, p2->fLatitude);

	gdouble fDeltaX = fX2 - fX1;
	gdouble fDeltaY = fY2 - fY1;

	return sqrt((fDeltaX*fDeltaX) + (fDeltaY*fDeltaY));
}

void map_util_calculate_bounding_box(const GArray* pMapPointsArray, maprect_t* pBoundingRect)
{
	// untested
	g_assert_not_reacher();

	g_assert(pMapPointsArray != NULL);
	g_assert(pMapPointsArray->len > 0);
	g_assert(pBoundingRect != NULL);

	pBoundingRect->A.fLatitude = MAX_LATITUDE;
	pBoundingRect->A.fLongitude = MAX_LONGITUDE;
	pBoundingRect->B.fLatitude = MIN_LATITUDE;
	pBoundingRect->B.fLongitude = MIN_LONGITUDE;

	gint i = 0;
	for(i=0 ; i<pMapPointsArray->len ; i++) {
		mappoint_t* p = &g_array_index(pMapPointsArray, mappoint_t, i);

		pBoundingRect->A.fLatitude = min(pBoundingRect->A.fLatitude, p->fLatitude);
		pBoundingRect->B.fLatitude = max(pBoundingRect->B.fLatitude, p->fLatitude);
		pBoundingRect->A.fLongitude = min(pBoundingRect->A.fLongitude, p->fLongitude);
		pBoundingRect->B.fLongitude = max(pBoundingRect->B.fLongitude, p->fLongitude);
	}
}

*/
#endif
