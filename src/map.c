/***************************************************************************
 *            map.c
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

#include <gdk/gdkx.h>
#include <cairo.h>
#include <gnome.h>
#include <math.h>

#include "../include/gui.h"
#include "../include/map.h"
#include "../include/geometryset.h"
#include "../include/mainwindow.h"
#include "../include/util.h"
#include "../include/db.h"
#include "../include/layers.h"
#include "../include/locationset.h"

// 4,382,755 inches per degree
// 363 inches of world represented in 3.63 inches

struct {
	mappoint_t 			m_MapCenter;	// XXX
	windowdimensions_t 	m_WindowDimensions;					// XXX
	guint16 			m_uZoomLevel;							// XXX
	gboolean 			m_bRedrawNeeded;
} g_Map =
{
	{42.29886, -73.24393},
	{0,0},
	7,
	TRUE
};

struct {
	gchar* m_pszLong;
	gchar* m_pszShort;
} g_RoadNameSuffix[] = {
	{"",""},
	{"Road", "Rd"},
	{"Street", "St"},
	{"Drive", "Dr"},
	{"Boulevard", "Bvd"},
	{"Avenue", "Ave"},
	{"Circle", "Crl"},
	{"Square", "Sq"},
	{"Path", "Pth"},
	{"Way", "Wy"},
	{"Plaza", "Plz"},
	{"Trail", "Trl"},
	{"Lane", "Ln"},
	{"Crossing", "Xing"},
	{"Place", "Pl"},
	{"Court", "Ct"},
	{"Turnpike", "Tpke"},
	{"Terrace", "Ter"},
	{"Row", "Row"},
	{"Parkway", "Pky"},
	
	{"Bridge", "Brg"},
	{"Highway", "Hwy"},
	{"Run", "Run"},
	{"Pass", "Pass"},
	
};

struct {
	gchar* m_pszName;
	gint m_nID;
} g_RoadNameSuffixLookup[] = {
	{"Rd", ROAD_SUFFIX_ROAD},
	{"Road", ROAD_SUFFIX_ROAD},

	{"St", ROAD_SUFFIX_STREET},
	{"Street", ROAD_SUFFIX_STREET},

	{"Dr", ROAD_SUFFIX_DRIVE},
	{"Drive", ROAD_SUFFIX_DRIVE},

	{"Blv", ROAD_SUFFIX_BOULEVARD},
	{"Blvd", ROAD_SUFFIX_BOULEVARD},
	{"Boulevard", ROAD_SUFFIX_BOULEVARD},

	{"Av", ROAD_SUFFIX_AVENUE},
	{"Ave", ROAD_SUFFIX_AVENUE},
	{"Avenue", ROAD_SUFFIX_AVENUE},
	
	{"Cir", ROAD_SUFFIX_CIRCLE},
	{"Crl", ROAD_SUFFIX_CIRCLE},
	{"Circle", ROAD_SUFFIX_CIRCLE},

	{"Sq", ROAD_SUFFIX_SQUARE},
	{"Square", ROAD_SUFFIX_SQUARE},	

	{"Pl", ROAD_SUFFIX_PLACE},
	{"Place", ROAD_SUFFIX_PLACE},
	
	{"Xing", ROAD_SUFFIX_CROSSING},
	{"Crossing", ROAD_SUFFIX_CROSSING},

	{"Ct", ROAD_SUFFIX_COURT},
	{"Court", ROAD_SUFFIX_COURT},

	{"Tpke", ROAD_SUFFIX_TURNPIKE},
	{"Turnpike", ROAD_SUFFIX_TURNPIKE},

	{"Ter", ROAD_SUFFIX_TERRACE},
	{"Terrace", ROAD_SUFFIX_TERRACE},
	
	{"Row", ROAD_SUFFIX_ROW},

	{"Pth", ROAD_SUFFIX_PATH},
	{"Path", ROAD_SUFFIX_PATH},	

	{"Wy", ROAD_SUFFIX_WAY},
	{"Way", ROAD_SUFFIX_WAY},	

	{"Plz", ROAD_SUFFIX_PLAZA},
	{"Plaza", ROAD_SUFFIX_PLAZA},	

	{"Trl", ROAD_SUFFIX_TRAIL},
	{"Trail", ROAD_SUFFIX_TRAIL},	
	
	{"Ln", ROAD_SUFFIX_LANE},
	{"Lane", ROAD_SUFFIX_LANE},	
	
	{"Pky", ROAD_SUFFIX_PARKWAY},
	{"Parkway", ROAD_SUFFIX_PARKWAY},

	{"Brg", ROAD_SUFFIX_BRIDGE},
	{"Bridge", ROAD_SUFFIX_BRIDGE},

	{"Hwy", ROAD_SUFFIX_HIGHWAY},
	{"Highway", ROAD_SUFFIX_HIGHWAY},

	{"Run", ROAD_SUFFIX_RUN},

	{"Pass", ROAD_SUFFIX_PASS},
};

#define SCALE_X(p, x)  ((((x) - (p)->m_rWorldBoundingBox.m_A.m_fLongitude) / (p)->m_fScreenLongitude) * (p)->m_nWindowWidth)
#define SCALE_Y(p, y)  ((p)->m_nWindowHeight - ((((y) - (p)->m_rWorldBoundingBox.m_A.m_fLatitude) / (p)->m_fScreenLatitude) * (p)->m_nWindowHeight))

typedef enum {
	kSublayerBottom,
	kSublayerTop,
} ESubLayer;

#define MIN_LINE_LENGTH_FOR_LABEL  	(40)
#define LABEL_PIXELS_ABOVE_LINE 	(2)
#define LABEL_PIXEL_RELIEF_INSIDE_LINE	(2)	// when drawing a label inside a line, only do so if we would have at least this much blank space above+below the text

/* Prototypes */

void map_draw_layer_polygons(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, geometryset_t* pGeometry, sublayerstyle_t* pSubLayerStyle);
void map_draw_layer_lines(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, geometryset_t* pGeometry, sublayerstyle_t* pSubLayerStyle);
void map_draw_layer_labels(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, geometryset_t* pGeometry, sublayerstyle_t* pSubLayerStyle);
void map_draw_layer_points(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray);
//void map_draw_layer_railroad(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, sublayerstyle_t* pSubLayerStyle);

// For road names: Bitstream Vera Sans Mono ?

#define INCHES_PER_METER (39.37007)

#define MIN_ZOOMLEVEL (1)
#define MAX_ZOOMLEVEL (10)
#define NUM_ZOOMLEVELS (10)

#define WORLD_CIRCUMFERENCE_IN_METERS (40076000)
#define WORLD_METERS_PER_DEGREE (WORLD_CIRCUMFERENCE_IN_METERS / 360.0)
#define WORLD_METERS_TO_DEGREES(x)	((x) / WORLD_METERS_PER_DEGREE)
#define WORLD_DEGREES_TO_METERS(x)	((x) * WORLD_METERS_PER_DEGREE)
#define KILOMETERS_PER_METER 	(1000)
#define WORLD_KILOMETERS_TO_DEGREES(x)	((x * KILOMETERS_PER_METER) / WORLD_METERS_PER_DEGREE)

#define WORLD_CIRCUMFERENCE_IN_FEET (131482939.8324)
#define WORLD_FEET_PER_DEGREE 		(WORLD_CIRCUMFERENCE_IN_FEET / 360.0)
#define WORLD_FEET_TO_DEGREES(X)	((X) / WORLD_FEET_PER_DEGREE)
#define FEET_PER_MILE				(5280)
#define WORLD_MILES_TO_DEGREES(x)	((x * FEET_PER_MILE) / WORLD_FEET_PER_DEGREE)

// Earth is slightly egg shaped so there are infinite radius measurements:

// at poles: ?
// average: 6,371,010
// at equator: 6,378,136 meters

#define RADIUS_OF_WORLD_IN_METERS 	(6371010)

#define DEG2RAD(x)	((x) * (M_PI / 180.0))
#define RAD2DEG(x)	((x) * (180.0 / M_PI))

// Each zoomlevel has a scale and an optional name (name isn't used for anything)
zoomlevel_t g_sZoomLevels[NUM_ZOOMLEVELS+1] = {
	{1,"undefined"}, 	// no zoom level 0

	{18000000, "country"},			// 1
	{ 6000000, ""},					// 2
	{ 1800000, "state"},			// 3
	{  600000, ""},					// 4
	{  200000, "region"},			// 5
	{   70000, ""},					// 6
	{   25000, "city"}, 			// 7
	{   10000, ""},					// 8
	{    4000, ""},					// 9
	{    1000, "street"},			//10
};

struct {
	gint nLayer;
	gint nSubLayer;
	void (*pFunc)(cairo_t*, rendermetrics_t*, geometryset_t*, sublayerstyle_t*);
} layerdraworder[] = {
	{LAYER_PARK, 0, map_draw_layer_polygons},
	{LAYER_PARK, 1, map_draw_layer_lines},

	{LAYER_RIVER, 0, map_draw_layer_lines},

	{LAYER_LAKE, 0, map_draw_layer_polygons},
	{LAYER_LAKE, 1, map_draw_layer_lines},

	{LAYER_MINORHIGHWAY_RAMP, 0, map_draw_layer_lines},
	{LAYER_MINORSTREET, 0, map_draw_layer_lines},
	{LAYER_MAJORSTREET, 0, map_draw_layer_lines},

	{LAYER_MINORHIGHWAY_RAMP, 1, map_draw_layer_lines},
	{LAYER_MINORSTREET, 1, map_draw_layer_lines},
	{LAYER_MAJORSTREET, 1, map_draw_layer_lines},

	{LAYER_RAILROAD, 0, map_draw_layer_lines},
	{LAYER_RAILROAD, 1, map_draw_layer_lines},

	{LAYER_MINORHIGHWAY, 0, map_draw_layer_lines},
	{LAYER_MINORHIGHWAY, 1, map_draw_layer_lines},

	//
	// Labels
	//
     	{LAYER_MINORSTREET, 0, map_draw_layer_labels},
     	{LAYER_MAJORSTREET, 0, map_draw_layer_labels},

     	{LAYER_MINORHIGHWAY, 0, map_draw_layer_labels},
     	{LAYER_MAJORHIGHWAY, 0, map_draw_layer_labels},

     	{LAYER_RAILROAD, 0, map_draw_layer_labels},
};

// ========================================================
//  Get/Set Functions
// ========================================================

void map_set_zoomlevel(guint16 uZoomLevel)
{
	if(uZoomLevel > MAX_ZOOMLEVEL) uZoomLevel = MAX_ZOOMLEVEL;
	else if(uZoomLevel < MIN_ZOOMLEVEL) uZoomLevel = MIN_ZOOMLEVEL;

	if(uZoomLevel != g_Map.m_uZoomLevel) {
		g_Map.m_uZoomLevel = uZoomLevel;
		map_set_redraw_needed(TRUE);
	}
}

guint16 map_get_zoomlevel()
{
	return g_Map.m_uZoomLevel;	// between MIN_ZOOMLEVEL and MAX_ZOOMLEVEL
}

guint32 map_get_zoomlevel_scale()
{
	return g_sZoomLevels[g_Map.m_uZoomLevel].m_uScale;	// returns "5000" for 1:5000 scale
}

// ========================================================
//  Coordinate Conversion Functions
// ========================================================

gchar* g_aDistanceUnitNames[] = {
	"Feet",
	"Miles",
	"Meters",
	"Kilometers",
};

gdouble map_distance_in_units_to_degrees(gdouble fDistance, gint nDistanceUnit)
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

// convert pixels to a span of degrees
double map_pixels_to_degrees(gint16 nPixels, guint16 uZoomLevel)
{
	double fMonitorPixelsPerInch = 85 + 1/3;
	double fPixelsPerMeter = fMonitorPixelsPerInch * INCHES_PER_METER;
	double fMetersOfPixels = ((float)nPixels) / fPixelsPerMeter;

	// If we had 3 meters of pixels (a very big monitor:) and the scale was 1000:1 then
	// we would want to show 3000 meters worth of world space
	double fMetersOfWorld = ((float)g_sZoomLevels[uZoomLevel].m_uScale) * fMetersOfPixels;

	return WORLD_METERS_TO_DEGREES(fMetersOfWorld);
}

double map_degrees_to_pixels(gdouble fDegrees, guint16 uZoomLevel)
{
	double fMonitorPixelsPerInch = 85 + 1/3;

	double fResultInMeters = WORLD_DEGREES_TO_METERS(fDegrees);
	double fResultInPixels = (INCHES_PER_METER * fResultInMeters) * fMonitorPixelsPerInch;
	fResultInPixels /= (float)g_sZoomLevels[uZoomLevel].m_uScale;
	return fResultInPixels;
}

void map_windowpoint_to_mappoint(screenpoint_t* pScreenPoint, mappoint_t* pMapPoint)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = (gint)(pScreenPoint->m_nX) - (g_Map.m_WindowDimensions.m_uWidth / 2);
	gint16 nPixelDeltaY = (gint)(pScreenPoint->m_nY) - (g_Map.m_WindowDimensions.m_uHeight / 2);

	// Convert pixels to world coordinates
	pMapPoint->m_fLongitude = g_Map.m_MapCenter.m_fLongitude + map_pixels_to_degrees(nPixelDeltaX, g_Map.m_uZoomLevel);
	// reverse the X, clicking above
	pMapPoint->m_fLatitude = g_Map.m_MapCenter.m_fLatitude - map_pixels_to_degrees(nPixelDeltaY, g_Map.m_uZoomLevel);
}

// Call this to pan around the map
void map_center_on_windowpoint(guint16 uX, guint16 uY)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = uX - (g_Map.m_WindowDimensions.m_uWidth / 2);
	gint16 nPixelDeltaY = uY - (g_Map.m_WindowDimensions.m_uHeight / 2);

	// Convert pixels to world coordinates
	double fWorldDeltaX = map_pixels_to_degrees(nPixelDeltaX, g_Map.m_uZoomLevel);
	// reverse the X, clicking above
	double fWorldDeltaY = -map_pixels_to_degrees(nPixelDeltaY, g_Map.m_uZoomLevel);

//	g_message("panning %d,%d pixels (%.10f,%.10f world coords)\n", nPixelDeltaX, nPixelDeltaY, fWorldDeltaX, fWorldDeltaY);

	map_center_on_worldpoint(g_Map.m_MapCenter.m_fLatitude + fWorldDeltaY, g_Map.m_MapCenter.m_fLongitude + fWorldDeltaX);
}

void map_center_on_worldpoint(double fLatitude, double fLongitude)
{
	g_Map.m_MapCenter.m_fLatitude = fLatitude;
	g_Map.m_MapCenter.m_fLongitude = fLongitude;

	map_set_redraw_needed(TRUE);
}

void map_get_world_coordinate_point(mappoint_t* pPoint)
{
	pPoint->m_fLatitude = g_Map.m_MapCenter.m_fLatitude;
	pPoint->m_fLongitude = g_Map.m_MapCenter.m_fLongitude;
}

void map_get_world_coordinates(float* pLatitude, float* pLongitude)
{
	g_return_if_fail(pLatitude != NULL);
	g_return_if_fail(pLongitude != NULL);

	*pLatitude = g_Map.m_MapCenter.m_fLatitude;
	*pLongitude = g_Map.m_MapCenter.m_fLongitude;
}

// Call this on init and every time the window changes size
void map_set_view_dimensions(guint16 uWidth, guint16 uHeight)
{
	g_Map.m_WindowDimensions.m_uWidth = uWidth;
	g_Map.m_WindowDimensions.m_uHeight = uHeight;
}


double map_get_distance_in_meters(mappoint_t* pA, mappoint_t* pB)
{
	g_assert_not_reached();	// unused/tested

	// This functions calculates the length of the arc of the "greatcircle" that goes through
	// the two points A and B and whos center is the center of the sphere, O.

	// When we multiply this angle (in radians) by the radius, we get the length of the arc.

	// NOTE: This algorithm wrongly assumes that Earth is a perfect sphere.
	//       It is actually slightly egg shaped.

	// All trig functions expect arguments in radians.
	double fLonA_Rad = DEG2RAD(pA->m_fLongitude);
	double fLonB_Rad = DEG2RAD(pB->m_fLongitude);
	double fLatA_Rad = DEG2RAD(pA->m_fLatitude);
	double fLatB_Rad = DEG2RAD(pB->m_fLatitude);

	// Step 1. Calculate AOB (in radians).
	// An explanation of this equation is at http://mathforum.org/library/drmath/view/51722.html
	double fAOB_Rad = acos((cos(fLatA_Rad) * cos(fLatB_Rad) * cos(fLonB_Rad - fLonA_Rad)) + (sin(fLatA_Rad) * sin(fLatB_Rad)));

	// Step 2. Multiply the angle by the radius of the sphere to get arc length.
	return fAOB_Rad * RADIUS_OF_WORLD_IN_METERS;
}

// ========================================================
//  Draw Functions
// ========================================================

// ========================================================
//  Redraw
// ========================================================

void map_set_redraw_needed(gboolean bNeeded)
{
	g_Map.m_bRedrawNeeded = bNeeded;
}

gboolean map_is_redraw_needed()
{
	return g_Map.m_bRedrawNeeded;
}

gboolean map_redraw_if_needed()
{
	return TRUE;
}

static void map_draw_background(cairo_t *pCairo)
{
	cairo_save(pCairo);
		cairo_set_rgb_color(pCairo, 239/255.0, 231/255.0, 230/255.0);
		cairo_rectangle(pCairo, 0, 0, g_Map.m_WindowDimensions.m_uWidth, g_Map.m_WindowDimensions.m_uHeight);
		cairo_fill(pCairo);
	cairo_restore(pCairo);
}

// EXPERIMENTAL TEXT RENDERING
void map_draw_line_label(cairo_t *pCairo, rendermetrics_t* pRenderMetrics, pointstring_t* pPointString, gdouble fLineWidth, const gchar* pszLabel)
{
	// HACK
//	if(random()%1000 > 100) return;
	
//	if(pPointString->m_pPointsArray->len <= 2) return;
	if(pPointString->m_pPointsArray->len < 2) return;
		
#define ROAD_MAX_SEGMENTS 100
	if(pPointString->m_pPointsArray->len > ROAD_MAX_SEGMENTS) { g_warning("road with > %d segments!\n", ROAD_MAX_SEGMENTS); return; }
	
	mappoint_t* apPoints[ROAD_MAX_SEGMENTS];
	gint nNumPoints = pPointString->m_pPointsArray->len;

	// figure out which way the road goes overall, by looking at the first and last points
	mappoint_t* pMapPoint1 = g_ptr_array_index(pPointString->m_pPointsArray, 0);
	mappoint_t* pMapPoint2 = g_ptr_array_index(pPointString->m_pPointsArray, pPointString->m_pPointsArray->len-1);

	// Does it go left-to-right?
	// NOTE: a better test would be to figure out the total length of roadsegment that goes left-to-right
	// and the total length that goes right-to-left, and swap the whole thing if right-to-left wins
	if(pMapPoint1->m_fLongitude < pMapPoint2->m_fLongitude) {
		// YES-- just copy the array
		gint iRead;
		for(iRead=0 ; iRead<pPointString->m_pPointsArray->len ; iRead++) {
			apPoints[iRead] = g_ptr_array_index(pPointString->m_pPointsArray, iRead);
		}
	}
	else {
		// NO-- (right-to-left) so reverse the array
		gint iRead,iWrite;
		for(iWrite=0, iRead=pPointString->m_pPointsArray->len-1 ; iRead>= 0 ; iWrite++, iRead--) {
			apPoints[iWrite] = g_ptr_array_index(pPointString->m_pPointsArray, iRead);
		}
	}

	//
	// Measure total line length	(perhaps this should be passed in)
	//
	gdouble fTotalLineLength = 0.0;
	gint iPoint;
	for(iPoint=1 ; iPoint<nNumPoints ; iPoint++) {
		pMapPoint1 = apPoints[iPoint-1];
		pMapPoint2 = apPoints[iPoint];

		gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
		gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
		gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
		gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

		// determine slope of the line
		gdouble fRise = fY2 - fY1;
		gdouble fRun = fX2 - fX1;
		gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

		fTotalLineLength += fLineLength;
	}

	gfloat fFontSize = 14.0;

	cairo_save(pCairo);
	cairo_select_font(pCairo, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_scale_font(pCairo, fFontSize);

	// Get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszLabel, &extents);
	if(extents.width > fTotalLineLength) {
		cairo_restore(pCairo);
		return;
	}

	// CENTER IT on the line ?
	// (padding) |-text-| (padding)
	// ============================
	gdouble fFrontPadding = 0.0;
	gdouble fFrontPaddingNext = (fTotalLineLength - extents.width) / 2;	
	// NOTE: we only worry about padding at the start, the padding at the end should just happen...	

	gint nTotalStringLength = strlen(pszLabel);
	gint nStringStartIndex = 0;

//	g_print("=== NEW STRING: %s (padding %f)\n", pszLabel, fPaddingRemaining);

	for(iPoint=1 ; iPoint<nNumPoints ; iPoint++) {
		if(nTotalStringLength == nStringStartIndex) break;	// done

		pMapPoint1 = apPoints[iPoint-1];
		pMapPoint2 = apPoints[iPoint];

		gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
		gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
		gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
		gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

		// determine slope of the line
		gdouble fRise = fY2 - fY1;
		gdouble fRun = fX2 - fX1;

		gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

		fFrontPadding = fFrontPaddingNext;
		fFrontPaddingNext = 0.0;

		if(fFrontPadding > fLineLength) {
			fFrontPaddingNext = fFrontPadding - fLineLength;
			continue;
		}

		// do this after the padding calculation to possibly save some CPU cycles
		//~ gdouble fAngleInRadians = atan2(fRise, fRun); // * (M_PI/180.0);

		// this way works too:
		gdouble fAngleInRadians = atan(fRise / fRun);
		if(fRun < 0.0) fAngleInRadians += M_PI;

//		g_print("(fRise(%f) / fRun(%f)) = %f, atan(fRise / fRun) = %f: ", fRise, fRun, fRise / fRun, fAngleInRadians);

//		g_print("=== NEW SEGMENT, pixel (deltaY=%f, deltaX=%f), line len=%f, (%f,%f)->(%f,%f)\n",fRise, fRun, fLineLength, pMapPoint1->m_fLatitude,pMapPoint1->m_fLongitude,pMapPoint2->m_fLatitude,pMapPoint2->m_fLongitude);
//		g_print("  has screen coords (%f,%f)->(%f,%f)\n", fX1,fY1,fX2,fY2);

#define DRAW_LABEL_BUFFER_LEN	(200)
		gchar azLabelSegment[DRAW_LABEL_BUFFER_LEN];

		//
		// Figure out how much of the string we can put in this line segment
		//
		gboolean bFoundWorkableStringLength = FALSE;
		gint nWorkableStringLength;
		if(iPoint == (nNumPoints-1)) {
			// last segment, use all of string
			nWorkableStringLength = (nTotalStringLength - nStringStartIndex);

			// copy it in to the temp buffer
			memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
			azLabelSegment[nWorkableStringLength] = '\0';
		}
		else {
			g_assert((nTotalStringLength - nStringStartIndex) > 0);

			for(nWorkableStringLength = (nTotalStringLength - nStringStartIndex) ; nWorkableStringLength >= 1 ; nWorkableStringLength--) {
//				g_print("trying nWorkableStringLength = %d\n", nWorkableStringLength);

				if(nWorkableStringLength >= DRAW_LABEL_BUFFER_LEN) break;

				// copy it in to the temp buffer
				memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
				azLabelSegment[nWorkableStringLength] = '\0';
	
//				g_print("azLabelSegment = %s\n", azLabelSegment);
	
				// measure the label
				cairo_text_extents(pCairo, azLabelSegment, &extents);

				// if we're skipping ahead some (frontpadding), effective line length is smaller, so subtract padding
				if(extents.width <= (fLineLength - fFrontPadding)) {
//					g_print("found length %d for %s\n", nWorkableStringLength, azLabelSegment);	
					bFoundWorkableStringLength = TRUE;

					// if we have 3 pixels, and we use 2, this should be NEGATIVE 1
					// TODO: we should only really do this if the next segment doesn't take a huge bend
					fFrontPaddingNext = extents.width - (fLineLength - fFrontPadding);
		fFrontPaddingNext /= 2;	// no good explanation for this
					break;
				}
			}

			if(!bFoundWorkableStringLength) {
				// write one character and set some frontpadding for the next piece
				g_assert(nWorkableStringLength == 0);
				nWorkableStringLength = 1;

				// give the next segment some padding if we went over on this segment
				fFrontPaddingNext = extents.width - (fLineLength - fFrontPadding);
	//			g_print("forcing a character (%s) on small segment, giving next segment front-padding: %f\n", azLabelSegment, fFrontPaddingNext);
			}
		}
//		g_print("%s\n", azLabelSegment);

		// move the current index up
		nStringStartIndex += nWorkableStringLength;

		// otherwise draw this bit of text
		//~ g_print("drawing buffer: %s\n", azLabelSegment);

		// Normalize (make length = 1.0) by dividing by line length
		// This makes a line with length 1 from the origin (0,0)
		gdouble fNormalizedX = fRun / fLineLength;
		gdouble fNormalizedY = fRise / fLineLength;

		// CENTER IT on the line ?
		// (buffer space) |-text-| (buffer space)
		// ======================================
		//gdouble fHalfBufferSpace = ((fLineLength - extents.width)/2);
		gdouble fDrawX = fX1 + (fNormalizedX * fFrontPadding);
		gdouble fDrawY = fY1 + (fNormalizedY * fFrontPadding);

		// NOTE: ***Swap the X and Y*** and normalize (make length = 1.0) by dividing by line length
		// This makes a perpendicular line with length 1 from the origin (0,0)
		gdouble fPerpendicularNormalizedX = fRise / fLineLength;
		gdouble fPerpendicularNormalizedY = -(fRun / fLineLength);

		// we want the normal pointing towards the top of the screen.  that's the negative Y direction.
//		if(fPerpendicularNormalizedY > 0) fPerpendicularNormalizedY = -fPerpendicularNormalizedY;	

		// text too big to fit?  then move the text "up"
		//~ if(extents.height > (fLineWidth - LABEL_PIXEL_RELIEF_INSIDE_LINE)) {
			//~ // Raise the text "up" (away from center of line) half the width of the line
			//~ // This leaves it resting on the line.  Then add a few pixels of relief.
	
			//~ // NOTE: the point started in the dead-center of the line
			//~ fDrawX += (fPerpendicularNormalizedX * ((fLineWidth / 2) + LABEL_PIXELS_ABOVE_LINE));
			//~ fDrawY += (fPerpendicularNormalizedY * ((fLineWidth / 2) + LABEL_PIXELS_ABOVE_LINE));
		//~ }
		//~ else {
			//~ // just nudge it "down" slightly-- the text shows up "ABOVE" and to the "RIGHT" of the point
			fDrawX -= (fPerpendicularNormalizedX * extents.height/2);
			fDrawY -= (fPerpendicularNormalizedY * extents.height/2);
		//~ }

		// draw!
		
		//~ cairo_set_rgb_color(pCairo, 0.0,0.0,0.0);
		//~ cairo_move_to(pCairo, fX1, fY1);
		//~ cairo_arc(pCairo, fX1, fY1, 3, 0, 2*M_PI);
		//~ cairo_fill(pCairo);
		//~ cairo_move_to(pCairo, fX2, fY2);
		//~ cairo_arc(pCairo, fX2, fY2, 3, 0, 2*M_PI);
		//~ cairo_fill(pCairo);

		//
		cairo_save(pCairo);
			cairo_move_to(pCairo, fDrawX, fDrawY);
			cairo_set_rgb_color(pCairo, 0.1,0.1,0.1);
			cairo_set_alpha(pCairo, 1.0);
//			cairo_arc(pCairo, fDrawX, fDrawY, 3, 0, 2*M_PI);
			cairo_rotate(pCairo, fAngleInRadians);
			cairo_text_path(pCairo, azLabelSegment);
			gboolean bDrawHalo = FALSE;
			if(bDrawHalo) {
				cairo_save(pCairo);
					cairo_set_line_width(pCairo, 3);
					cairo_set_rgb_color(pCairo, 1.0,1.0,1.0);
					cairo_stroke(pCairo);
				cairo_restore(pCairo);
			}
			cairo_fill(pCairo);
		cairo_restore(pCairo);
		
		// if we've drawn some text, we know there should be NO front-padding remaining
//		fFrontPadding = 0.0;
	}
	cairo_restore(pCairo);
}

// g_print("map (%f,%f)->(%f,%f) screen (%f,%f)->(%f,%f) center (%f,%f)\n",pMapPoint1->m_fLatitude,pMapPoint1->m_fLongitude,  pMapPoint2->m_fLatitude,pMapPoint2->m_fLongitude,  fX1,fY1,  fX2,fY2,  fX,fY);
		// Calculate line length (in screen pixels)

/*
void map_draw_line_label(cairo_t *pCairo, rendermetrics_t* pRenderMetrics, pointstring_t* pPointString, gdouble fLineWidth, const gchar* pszLabel)
{
	
	// HACK
	if(random()%1000 > 100) return;
	
	if(pPointString->m_pPointsArray->len < 2) return;

//	if(pPointString->m_pPointsArray->len > 2) {
//		g_print("(has %d)\n", pPointString->m_pPointsArray->len);
//	}
		
	// for now just take the first line segment
	mappoint_t* pMapPoint1 = g_ptr_array_index(pPointString->m_pPointsArray, 0);
	mappoint_t* pMapPoint2 = g_ptr_array_index(pPointString->m_pPointsArray, 1);
	mappoint_t* pTemp;
	
	if(pMapPoint1->m_fLongitude > pMapPoint2->m_fLongitude) {
		// swap them so point 1 is to the left
		pTemp = pMapPoint1;
		pMapPoint1 = pMapPoint2;
		pMapPoint2 = pTemp;
	}

	gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
	gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
	gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
	gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

	// determine slope of the line
	gdouble fRise = fY2 - fY1;
	gdouble fRun = fX2 - fX1;

	gdouble fAngleInRadians = atan(fRise / fRun); // * (M_PI/180.0);

// g_print("map (%f,%f)->(%f,%f) screen (%f,%f)->(%f,%f) center (%f,%f)\n",pMapPoint1->m_fLatitude,pMapPoint1->m_fLongitude,  pMapPoint2->m_fLatitude,pMapPoint2->m_fLongitude,  fX1,fY1,  fX2,fY2,  fX,fY);
		// Calculate line length (in screen pixels)
	gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));
	gfloat fFontSize;
	cairo_text_extents_t extents;

	// try several font sizes until one fits
	gboolean bFound = FALSE;
	for(fFontSize = 12.0 ; fFontSize >= 10.0 ; fFontSize -= 0.5) {
		cairo_save(pCairo);
		cairo_select_font(pCairo, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_scale_font(pCairo, fFontSize);
		cairo_text_extents(pCairo, pszLabel, &extents);

		if(extents.width > fLineLength) {
			cairo_restore(pCairo);
			continue;
		}
		else {
			bFound = TRUE;
			break;	
		}
	}
	if(!bFound) {
		//cairo_restore(pCairo);
		return;
	}

		// Normalize (make length = 1.0) by dividing by line length
		// This makes a line with length 1 from the origin (0,0)
		gdouble fNormalizedX = fRun / fLineLength;
		gdouble fNormalizedY = fRise / fLineLength;

		// (buffer space) |-text-| (buffer space)
		// ======================================
		gdouble fHalfBufferSpace = ((fLineLength - extents.width)/2);
		gdouble fDrawX = fX1 + (fNormalizedX * fHalfBufferSpace);
		gdouble fDrawY = fY1 + (fNormalizedY * fHalfBufferSpace);

		// *Swap the X and Y* and normalize (make length = 1.0) by dividing by line length
		// This makes a perpendicular line with length 1 from the origin (0,0)
		gdouble fPerpendicularNormalizedX = fRise / fLineLength;
		gdouble fPerpendicularNormalizedY = fRun / fLineLength;
		// we want the normal pointing towards the top of the screen.  that's the negative Y direction.
		if(fPerpendicularNormalizedY > 0) fPerpendicularNormalizedY = -fPerpendicularNormalizedY;	
		
		// text too big to fit?  then move the text "up"
		if(extents.height > (fLineWidth - LABEL_PIXEL_RELIEF_INSIDE_LINE)) {
			// Raise the text "up" (away from center of line) half the width of the line
			// This leaves it resting on the line.  Then add a few pixels of relief.

			// NOTE: the point started in the dead-center of the line
			fDrawX += (fPerpendicularNormalizedX * ((fLineWidth / 2) + LABEL_PIXELS_ABOVE_LINE));
			fDrawY += (fPerpendicularNormalizedY * ((fLineWidth / 2) + LABEL_PIXELS_ABOVE_LINE));
		}
		else {
			// just nudge it down slightly-- the text shows up "ABOVE" and to the "RIGHT" of the point
			fDrawX -= (fPerpendicularNormalizedX * extents.height/2);
			fDrawY -= (fPerpendicularNormalizedY * extents.height/2);
		}

		// draw!
		cairo_move_to(pCairo, fDrawX, fDrawY);
		cairo_rotate(pCairo, fAngleInRadians);
		cairo_text_path(pCairo, pszLabel);
		cairo_set_rgb_color(pCairo, 0.1,0.1,0.1);
		cairo_set_alpha(pCairo, 1.0);
		cairo_fill(pCairo);
	cairo_restore(pCairo);
}
*/
void map_get_render_metrics(rendermetrics_t* pMetrics)
{
	g_assert(pMetrics != NULL);

	//
	// Set up renderMetrics array
	//
	pMetrics->m_nZoomLevel = map_get_zoomlevel();
	pMetrics->m_nWindowWidth = g_Map.m_WindowDimensions.m_uWidth;
	pMetrics->m_nWindowHeight = g_Map.m_WindowDimensions.m_uHeight;

	// Calculate how many world degrees we'll be drawing
	pMetrics->m_fScreenLatitude = map_pixels_to_degrees(g_Map.m_WindowDimensions.m_uHeight, pMetrics->m_nZoomLevel);
	pMetrics->m_fScreenLongitude = map_pixels_to_degrees(g_Map.m_WindowDimensions.m_uWidth, pMetrics->m_nZoomLevel);

	// The world bounding box (expressed in lat/lon) of the data we will be drawing
	pMetrics->m_rWorldBoundingBox.m_A.m_fLongitude = g_Map.m_MapCenter.m_fLongitude - pMetrics->m_fScreenLongitude/2;
	pMetrics->m_rWorldBoundingBox.m_A.m_fLatitude = g_Map.m_MapCenter.m_fLatitude - pMetrics->m_fScreenLatitude/2;
	pMetrics->m_rWorldBoundingBox.m_B.m_fLongitude = g_Map.m_MapCenter.m_fLongitude + pMetrics->m_fScreenLongitude/2;
	pMetrics->m_rWorldBoundingBox.m_B.m_fLatitude = g_Map.m_MapCenter.m_fLatitude + pMetrics->m_fScreenLatitude/2;	
}

void map_draw(cairo_t *pCairo)
{
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(&renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	//
	// Load geometry
	//
	geometryset_load_geometry(&(pRenderMetrics->m_rWorldBoundingBox));
	locationset_load_locations(&(pRenderMetrics->m_rWorldBoundingBox));

	const GPtrArray* pLocationSets = locationset_get_set_array();

	//
	// Draw map
	//
	TIMER_BEGIN(maptimer, "BEGIN RENDER MAP");
	cairo_save(pCairo);
		map_draw_background(pCairo);

		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
		cairo_scale_font(pCairo, 18.00);

		// Render Layers
		gint iLayerDraw;
		for(iLayerDraw=0 ; iLayerDraw<NUM_ELEMS(layerdraworder) ; iLayerDraw++) {
			layerdraworder[iLayerDraw].pFunc(pCairo, pRenderMetrics,
				/* geometry */ 	g_aLayers[layerdraworder[iLayerDraw].nLayer].m_pGeometrySet,
				/* style */ 	&g_aLayers[layerdraworder[iLayerDraw].nLayer].m_Style.m_aSubLayers[layerdraworder[iLayerDraw].nSubLayer]);
		}
	TIMER_END(maptimer, "END RENDER MAP");

	TIMER_BEGIN(loctimer, "\nBEGIN RENDER LOCATIONS");
		// Render Locations
		gint iLocationSet;
		for(iLocationSet=0 ; iLocationSet<pLocationSets->len ; iLocationSet++) {
			locationset_t* pLocationSet = g_ptr_array_index(pLocationSets, iLocationSet);
			map_draw_layer_points(pCairo, pRenderMetrics, pLocationSet->m_pLocationsArray);
		}			

#define CROSSHAIR_LINE_RELIEF	(6)
#define CROSSHAIR_LINE_LENGTH	(12)
#define CROSSHAIR_CIRCLE_RADIUS (12)

		cairo_set_line_width(pCairo, 1.0);
		cairo_set_rgb_color(pCairo, 0.1, 0.1, 0.1);
		cairo_set_alpha(pCairo, 1.0);

		// left line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) - (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH), (pRenderMetrics->m_nWindowHeight/2));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) - (CROSSHAIR_LINE_RELIEF), (pRenderMetrics->m_nWindowHeight/2));
		// right line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) + (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH), (pRenderMetrics->m_nWindowHeight/2));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) + (CROSSHAIR_LINE_RELIEF), (pRenderMetrics->m_nWindowHeight/2));
		// top line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) - (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) - (CROSSHAIR_LINE_RELIEF));		
		// bottom line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) + (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) + (CROSSHAIR_LINE_RELIEF));
		cairo_stroke(pCairo);

		cairo_arc(pCairo, pRenderMetrics->m_nWindowWidth/2, pRenderMetrics->m_nWindowHeight/2, CROSSHAIR_CIRCLE_RADIUS, 0, 2*M_PI);		
		cairo_stroke(pCairo);
	cairo_restore(pCairo);
	TIMER_END(loctimer, "END RENDER LOCATIONS");

	// We don't need another redraw until something changes
	map_set_redraw_needed(FALSE);
}

void map_draw_layer_points(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray)
{
	gfloat fRadius = map_degrees_to_pixels(0.0007, map_get_zoomlevel());
	gboolean bAddition = FALSE;

	cairo_save(pCairo);
		cairo_set_rgb_color(pCairo, 123.0/255.0, 48.0/255.0, 1.0);
		cairo_set_alpha(pCairo, 0.3);

		gint iLocation;
		for(iLocation=0 ; iLocation<pLocationsArray->len ; iLocation++) {
			location_t* pLocation = g_ptr_array_index(pLocationsArray, iLocation);

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude), fRadius, 0, 2*M_PI);
	
			if(bAddition) {
				// fill each circle as we create them so they double-up on coverage
				cairo_fill(pCairo);
			}
		}
		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
		if(!bAddition) cairo_fill(pCairo);

		// point centers
		cairo_set_rgb_color(pCairo, 128.0/255.0, 128.0/255.0, 128.0/255.0);
		cairo_set_alpha(pCairo, 1.0);
		fRadius = 2;
		for(iLocation=0 ; iLocation<pLocationsArray->len ; iLocation++) {
			location_t* pLocation = g_ptr_array_index(pLocationsArray, iLocation);

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude), fRadius, 0, 2*M_PI);

			cairo_fill(pCairo);
		}
		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
//		cairo_fill(pCairo);

	cairo_restore(pCairo);
}

void map_draw_layer_polygons(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, geometryset_t* pGeometry, sublayerstyle_t* pSubLayerStyle)
{
	mappoint_t* pPoint;
	pointstring_t* pPointString;
	gint iString;
	gint iPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth == 0.0) return;	// Don't both drawing with line width 0
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?
	
	// Set layer attributes	
	cairo_set_rgb_color(pCairo, pSubLayerStyle->m_clrColor.m_fRed, pSubLayerStyle->m_clrColor.m_fGreen, pSubLayerStyle->m_clrColor.m_fBlue);
	cairo_set_alpha(pCairo, pSubLayerStyle->m_clrColor.m_fAlpha);
	cairo_set_line_width(pCairo, fLineWidth);

	cairo_set_line_join(pCairo, pSubLayerStyle->m_nJoinStyle);
	cairo_set_line_cap(pCairo, pSubLayerStyle->m_nCapStyle);	/* CAIRO_LINE_CAP_BUTT, CAIRO_LINE_CAP_ROUND, CAIRO_LINE_CAP_CAP */
//	cairo_set_dash(pCairo, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_pfList, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nCount, 0.0);

	for(iString=0 ; iString<pGeometry->m_pPointStringsArray->len ; iString++) {
		pPointString = g_ptr_array_index(pGeometry->m_pPointStringsArray, iString);

		if(pPointString->m_pPointsArray->len >= 3) {
			pPoint = g_ptr_array_index(pPointString->m_pPointsArray, 0);

			// move to index 0
			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

			// start at index 1 (0 was used above)
			for(iPoint=1 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);				
				cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
			}
			//cairo_close_path(pCairo);
		}
		else {
//			g_print("pPointString->m_pPointsArray->len = %d\n", pPointString->m_pPointsArray->len);
		}

		// TODO: this is debugging of polygons.
		//~ cairo_save(pCairo);
		//~ cairo_set_rgb_color(pCairo, 1,0,0);

		//~ if(pPointString->m_pPointsArray->len >= 3) {
			//~ gdouble fRadius = 2;
			//~ // start at index 1 (0 was used above)
			//~ for(iPoint=0 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				//~ pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);

				//~ cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
				
	//~ //				util_random_color(&clr);
	//~ //				cairo_set_rgb_color(pCairo, clr.m_fRed, clr.m_fGreen, clr.m_fBlue);

				//~ gchar buf[20];
				//~ g_snprintf(buf, 20, "%d", iPoint);
	//~ //				cairo_select_font(pCairo, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
				//~ cairo_text_path(pCairo, buf);

	//~ //				cairo_arc(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude), fRadius, 0, 360.0  * (M_PI/180.0));
				//~ cairo_fill(pCairo);
				//~ fRadius += 2;
			//~ }
		//~ }
		//~ cairo_restore(pCairo);
	}
	cairo_fill(pCairo);
}

void map_draw_layer_lines(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, geometryset_t* pGeometry, sublayerstyle_t* pSubLayerStyle)
{
	mappoint_t* pPoint;
	pointstring_t* pPointString;
	gint iString;
	gint iPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth == 0.0) return;	// Don't both drawing with line width 0
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?

	if(fLineWidth <= 2.0) {
		cairo_set_tolerance(pCairo, 8);
	}
	else if(fLineWidth <= 6.0) {
		cairo_set_tolerance(pCairo, 6);
	}
	else {
		cairo_set_tolerance(pCairo, 0.5);
	}

	gint nCapStyle = pSubLayerStyle->m_nCapStyle;
//     if(nCapStyle == CAIRO_LINE_CAP_ROUND && fLineWidth <= 6.0) {
//         nCapStyle = CAIRO_LINE_CAP_SQUARE;
//     }
	
	// Set layer attributes	
	cairo_set_rgb_color(pCairo, pSubLayerStyle->m_clrColor.m_fRed, pSubLayerStyle->m_clrColor.m_fGreen, pSubLayerStyle->m_clrColor.m_fBlue);
	cairo_set_alpha(pCairo, pSubLayerStyle->m_clrColor.m_fAlpha);
	cairo_set_line_width(pCairo, fLineWidth);

	cairo_set_line_join(pCairo, pSubLayerStyle->m_nJoinStyle);
	cairo_set_line_cap(pCairo, nCapStyle);	/* CAIRO_LINE_CAP_BUTT, CAIRO_LINE_CAP_ROUND, CAIRO_LINE_CAP_CAP */
	cairo_set_dash(pCairo, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_pfList, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nCount, 0.0);

	for(iString=0 ; iString<pGeometry->m_pPointStringsArray->len ; iString++) {
		pPointString = g_ptr_array_index(pGeometry->m_pPointStringsArray, iString);

//~ cairo_set_rgb_color(pCairo, pSubLayerStyle->m_clrColor.m_fRed, pSubLayerStyle->m_clrColor.m_fGreen, pSubLayerStyle->m_clrColor.m_fBlue);
//~ if(pPointString->m_pPointsArray->len >= 3) {
	//~ cairo_set_rgb_color(pCairo, 1, 0, 0);
//~ }
	
		if(pPointString->m_pPointsArray->len >= 2) {
			pPoint = g_ptr_array_index(pPointString->m_pPointsArray, 0);

			// go to index 0
			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

			// start at index 1 (0 was used above)
			for(iPoint=1 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);//~ g_print("  point (%.05f,%.05f)\n", ScaleX(pPoint->m_fLongitude), ScaleY(pPoint->m_fLatitude));
				cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
			}
		}
	}
	cairo_stroke(pCairo);
}

void map_draw_layer_labels(cairo_t* pCairo, rendermetrics_t* pRenderMetrics, geometryset_t* pGeometry, sublayerstyle_t* pSubLayerStyle)
{
	gint iString;
	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];

	for(iString=0 ; iString<pGeometry->m_pPointStringsArray->len ; iString++) {
		pointstring_t* pPointString = g_ptr_array_index(pGeometry->m_pPointStringsArray, iString);
		if(pPointString->m_pszName[0] != '\0') {
			map_draw_line_label(pCairo, pRenderMetrics, pPointString, fLineWidth, pPointString->m_pszName);
		}
	}
}

const gchar* map_road_suffix_itoa(gint nSuffixID, gint nSuffixType)
{
	if(nSuffixID >= ROAD_SUFFIX_FIRST && nSuffixID <= ROAD_SUFFIX_LAST) {
		if(nSuffixType == SUFFIX_TYPE_SHORT) {
			return g_RoadNameSuffix[nSuffixID].m_pszShort;
		}
		else {
			return g_RoadNameSuffix[nSuffixID].m_pszLong;			
		}
	}
	if(nSuffixID != ROAD_SUFFIX_NONE) return "???";
	return "";
}

gboolean map_road_suffix_atoi(const gchar* pszSuffix, gint* pReturnSuffixID)
{
	gint i;
	for(i=0 ; i<NUM_ELEMS(g_RoadNameSuffixLookup) ; i++) {
		if(g_ascii_strcasecmp(pszSuffix, g_RoadNameSuffixLookup[i].m_pszName) == 0) {
			*pReturnSuffixID = g_RoadNameSuffixLookup[i].m_nID;
			return TRUE;
		}
	}
	return FALSE;
}
