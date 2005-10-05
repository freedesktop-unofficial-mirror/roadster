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

// ========================================================
//  Coordinate Conversion Functions
// ========================================================

// convert pixels to a span of degrees
gdouble map_pixels_to_degrees(map_t* pMap, gint16 nPixels, guint16 uZoomLevel)
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
