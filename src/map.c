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
//#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

#include "main.h"
#include "map_style.h"
#include "gui.h"
#include "map.h"
#include "mainwindow.h"
#include "util.h"
#include "db.h"
#include "road.h"
#include "locationset.h"
#include "location.h"
#include "scenemanager.h"

#define ENABLE_RIVER_TO_LAKE_LOADTIME_HACK	// change circular rivers to lakes when loading from disk
//#define ENABLE_SCENEMANAGER_DEBUG_TEST
//#define ENABLE_LABELS_WHILE_DRAGGING
//#define ENABLE_RIVER_SMOOTHING	// hacky. rivers are animated when scrolling. :) only good for proof-of-concept screenshots

#ifdef THREADED_RENDERING
#define RENDERING_THREAD_YIELD          g_thread_yield()
#else
#define RENDERING_THREAD_YIELD
#endif

#define	RENDERMODE_FAST 	1	// Use 'fast' until Cairo catches up. :)
#define	RENDERMODE_PRETTY 	2

// NOTE on choosing tile size.
// A) It is arbitrary and could be changed (even at runtime, although this would render useless everything in the cache)
// B) Too big, and you'll see noticable pauses while scrolling.
// C) Too small, and you make a ton of extra work with all the extra queries.
// D) The current value is the result of some casual testing.

// XXX: The names below aren't very clear.
#define TILE_SHIFT			(1000.0)	// the units we care about (1000ths of a degree)
#define TILE_MODULUS			(23)		// how many of the above units each tile is on a side
#define MAP_TILE_WIDTH			(TILE_MODULUS / TILE_SHIFT)	// width and height of a tile, in degrees

#define MIN_ROAD_HIT_TARGET_WIDTH	(6)	// make super thin roads a bit easier to hover over/click, in pixels

#define MIN_ZOOMLEVEL_FOR_LOCATIONS	(6)

/* Prototypes */

// data loading
static gboolean map_data_load_tiles(map_t* pMap, maprect_t* pRect);
static gboolean map_data_load_geometry(map_t* pMap, maprect_t* pRect);
static gboolean map_data_load_locations(map_t* pMap, maprect_t* pRect);

// hit testing
static gboolean map_hit_test_layer_roads(GPtrArray* pPointStringsArray, gdouble fMaxDistance, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);
static gboolean map_hit_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pHitPoint, gdouble fMaxDistance, mappoint_t* pReturnClosestPoint, gdouble* pfReturnPercentAlongLine);
static ESide map_side_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pClosestPointOnLine, mappoint_t* pHitPoint);
static gboolean map_hit_test_locationselections(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionsArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);

static gboolean map_hit_test_locationsets(map_t* pMap, rendermetrics_t* pRenderMetrics, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);
static gboolean map_hit_test_locations(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct);

static void map_store_location(map_t* pMap, location_t* pLocation, gint nLocationSetID);

static void map_data_clear(map_t* pMap);
void map_get_render_metrics(map_t* pMap, rendermetrics_t* pMetrics);

gdouble map_get_straight_line_distance_in_degrees(mappoint_t* p1, mappoint_t* p2);

// Each zoomlevel has a scale and an optional name (name isn't used for anything)
zoomlevel_t g_sZoomLevels[NUM_ZOOM_LEVELS+1] = {
	{1,0,0,0,0,"undefined"},	// no zoom level 0

//     { 1600000, ""},
//     {  800000, ""},
//     {  400000, ""},
//     {  200000, ""},
//     {  100000, ""},

	{   80000, UNIT_MILES, 2, 	UNIT_KILOMETERS, 2, "", },     	// 1
	{   40000, UNIT_MILES, 1, 	UNIT_KILOMETERS, 1, "", },     	// 2
	{   20000, UNIT_FEET, 2000, UNIT_METERS, 400, "", }, 		// 3
	{   10000, UNIT_FEET, 1000, UNIT_METERS, 200, "", },		// 4
	{    5000, UNIT_FEET, 500, 	UNIT_METERS, 100, "", },		// 5
};

gchar* g_apszMapObjectTypeNames[] = {	// XXX: would be nice to remove this. although we *do* need to maintain a link between imported data and styles
	"",
	"minor-roads",
	"major-roads",
	"minor-highways",
	"minor-highway-ramps",
	"major-highways",
	"major-highway-ramps",
	"railroads",
	"parks",
	"rivers",
	"lakes",
	"misc-areas",
	"urban-areas",
};

// ========================================================
//  Init
// ========================================================

// init the module
void map_init(void)
{
}

gboolean map_new(map_t** ppMap, GtkWidget* pTargetWidget)
{
	g_assert(ppMap != NULL);
	g_assert(*ppMap == NULL);	// must be a pointer to null pointer

	// create a new map struct
	map_t* pMap = g_new0(map_t, 1);

	// Create array of Track handles
	pMap->pTracksArray = g_array_new(FALSE, /* not zero-terminated */
					  TRUE,  /* clear to 0 (?) */
					  sizeof(gint));

	map_init_location_hash(pMap);

	pMap->pTargetWidget = pTargetWidget;

	scenemanager_new(&(pMap->pSceneManager));
	g_assert(pMap->pSceneManager);

	pMap->uZoomLevel = 1;		// XXX: better way to init this?  or should we force the GUI to reflect this #?

	// init containers for geometry data
	gint i;
	for(i=0 ; i<G_N_ELEMENTS(pMap->apLayerData) ; i++) {
		maplayer_data_t* pLayer = g_new0(maplayer_data_t, 1);
		pLayer->pRoadsArray = g_ptr_array_new();
		pMap->apLayerData[i] = pLayer;
	}

	// init POI selection
	pMap->pLocationSelectionArray = g_ptr_array_new();
	pMap->pLocationSelectionAllocator = g_free_list_new(sizeof(locationselection_t), 100);

	// save it
	*ppMap = pMap;
	return TRUE;
}

void map_draw(map_t* pMap, GdkPixmap* pTargetPixmap, gint nDrawFlags)
{
	g_assert(pMap != NULL);

	// Get area of world to draw and screen dimensions to draw to, etc.
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	// Load geometry
	TIMER_BEGIN(loadtimer, "--- BEGIN ALL DB LOAD");
	map_data_clear(pMap);
	map_data_load_tiles(pMap, &(pRenderMetrics->rWorldBoundingBox));
	TIMER_END(loadtimer, "--- END ALL DB LOAD");

	scenemanager_clear(pMap->pSceneManager);
	scenemanager_set_screen_dimensions(pMap->pSceneManager, pRenderMetrics->nWindowWidth, pRenderMetrics->nWindowHeight);

	gint nRenderMode = RENDERMODE_FAST; //RENDERMODE_FAST; // RENDERMODE_PRETTY

#ifdef ENABLE_LABELS_WHILE_DRAGGING
	nDrawFlags |= DRAWFLAG_LABELS;	// always turn on labels
#endif

#ifdef ENABLE_SCENEMANAGER_DEBUG_TEST
	GdkPoint aPoints[] = {{200,150},{250,200},{200,250},{150,200}};
	scenemanager_claim_polygon(pMap->pSceneManager, aPoints, 4);
#endif

	if(nRenderMode == RENDERMODE_FAST) {
		// 
		if(nDrawFlags & DRAWFLAG_GEOMETRY) {
			map_draw_gdk(pMap, pRenderMetrics, pTargetPixmap, DRAWFLAG_GEOMETRY);
			nDrawFlags &= ~DRAWFLAG_GEOMETRY;
		}

		// Call cairo for finishing the scene
		map_draw_cairo(pMap, pRenderMetrics, pTargetPixmap, nDrawFlags);
	}
	else {	// nRenderMode == RENDERMODE_PRETTY
		map_draw_cairo(pMap, pRenderMetrics, pTargetPixmap, nDrawFlags);
	}

#ifdef ENABLE_SCENEMANAGER_DEBUG_TEST
	gdk_draw_polygon(pTargetPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
					 FALSE, aPoints, 4);
#endif

	gtk_widget_queue_draw(pMap->pTargetWidget);
}

// ========================================================
//  Get and release the map image
// ========================================================

GdkPixmap* map_get_pixmap(map_t* pMap)
{
	return pMap->pPixmap;
}

void map_release_pixmap(map_t* pMap)
{
	// nothing since we're not using mutexes
}

// ========================================================
//  Get/Set Functions
// ========================================================

void map_set_zoomlevel(map_t* pMap, guint16 uZoomLevel)
{
	if(uZoomLevel > MAX_ZOOM_LEVEL) uZoomLevel = MAX_ZOOM_LEVEL;
	else if(uZoomLevel < MIN_ZOOM_LEVEL) uZoomLevel = MIN_ZOOM_LEVEL;

	if(uZoomLevel != pMap->uZoomLevel) {
		pMap->uZoomLevel = uZoomLevel;
//		map_set_redraw_needed(TRUE);
	}
}

guint16 map_get_zoomlevel(const map_t* pMap)
{
	return pMap->uZoomLevel;	// between MIN_ZOOM_LEVEL and MAX_ZOOM_LEVEL
}

guint32 map_get_zoomlevel_scale(const map_t* pMap)
{
	return g_sZoomLevels[pMap->uZoomLevel].uScale;	// returns "5000" for 1:5000 scale
}

gboolean map_can_zoom_in(const map_t* pMap)
{
	// can we increase zoom level?
	return (pMap->uZoomLevel < MAX_ZOOM_LEVEL);
}
gboolean map_can_zoom_out(const map_t* pMap)
{
	return (pMap->uZoomLevel > MIN_ZOOM_LEVEL);
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

// convert pixels to a span of degrees
double map_pixels_to_degrees(map_t* pMap, gint16 nPixels, guint16 uZoomLevel)
{
	double fMonitorPixelsPerInch = 85.333;	// XXX: don't hardcode this
	double fPixelsPerMeter = fMonitorPixelsPerInch * INCHES_PER_METER;
	double fMetersOfPixels = ((float)nPixels) / fPixelsPerMeter;

	// If we had 3 meters of pixels (a very big monitor:) and the scale was 1000:1 then
	// we would want to show 3000 meters worth of world space
	double fMetersOfWorld = ((float)g_sZoomLevels[uZoomLevel].uScale) * fMetersOfPixels;

	return WORLD_METERS_TO_DEGREES(fMetersOfWorld);
}

double map_degrees_to_pixels(map_t* pMap, gdouble fDegrees, guint16 uZoomLevel)
{
	double fMonitorPixelsPerInch = 85.333;	// XXX: don't hardcode this

	double fResultInMeters = WORLD_DEGREES_TO_METERS(fDegrees);
	double fResultInPixels = (INCHES_PER_METER * fResultInMeters) * fMonitorPixelsPerInch;
	fResultInPixels /= (float)g_sZoomLevels[uZoomLevel].uScale;
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

// Call this to pan around the map
void map_center_on_windowpoint(map_t* pMap, guint16 uX, guint16 uY)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = uX - (pMap->MapDimensions.uWidth / 2);
	gint16 nPixelDeltaY = uY - (pMap->MapDimensions.uHeight / 2);

	// Convert pixels to world coordinates
	double fWorldDeltaX = map_pixels_to_degrees(pMap, nPixelDeltaX, pMap->uZoomLevel);
	// reverse the X, clicking above
	double fWorldDeltaY = -map_pixels_to_degrees(pMap, nPixelDeltaY, pMap->uZoomLevel);

//	g_message("panning %d,%d pixels (%.10f,%.10f world coords)\n", nPixelDeltaX, nPixelDeltaY, fWorldDeltaX, fWorldDeltaY);

	mappoint_t pt;
	pt.fLatitude = pMap->MapCenter.fLatitude + fWorldDeltaY;
	pt.fLongitude = pMap->MapCenter.fLongitude + fWorldDeltaX;
	map_set_centerpoint(pMap, &pt);
}

void map_set_centerpoint(map_t* pMap, const mappoint_t* pPoint)
{
	g_assert(pPoint != NULL);

	pMap->MapCenter.fLatitude = pPoint->fLatitude;
	pMap->MapCenter.fLongitude = pPoint->fLongitude;

//	map_set_redraw_needed(TRUE);
}

void map_get_centerpoint(const map_t* pMap, mappoint_t* pReturnPoint)
{
	g_assert(pReturnPoint != NULL);

	pReturnPoint->fLatitude = pMap->MapCenter.fLatitude;
	pReturnPoint->fLongitude = pMap->MapCenter.fLongitude;
}

//
void map_set_dimensions(map_t* pMap, const dimensions_t* pDimensions)
{
	g_assert(pDimensions != NULL);

	pMap->MapDimensions.uWidth = pDimensions->uWidth;
	pMap->MapDimensions.uHeight = pDimensions->uHeight;

	// XXX: free old pixmap?
	//g_assert(pMap->pPixmap == NULL);

	pMap->pPixmap = gdk_pixmap_new(
			pMap->pTargetWidget->window,
			pMap->MapDimensions.uWidth, pMap->MapDimensions.uHeight,
			-1);
}

// ========================================================
//  Draw Functions
// ========================================================

void map_get_render_metrics(map_t* pMap, rendermetrics_t* pMetrics)
{
	g_assert(pMetrics != NULL);

	//
	// Set up renderMetrics array
	//
	pMetrics->nZoomLevel = map_get_zoomlevel(pMap);
	pMetrics->nWindowWidth = pMap->MapDimensions.uWidth;
	pMetrics->nWindowHeight = pMap->MapDimensions.uHeight;

	// Calculate how many world degrees we'll be drawing
	pMetrics->fScreenLatitude = map_pixels_to_degrees(pMap, pMap->MapDimensions.uHeight, pMetrics->nZoomLevel);
	pMetrics->fScreenLongitude = map_pixels_to_degrees(pMap, pMap->MapDimensions.uWidth, pMetrics->nZoomLevel);

	// The world bounding box (expressed in lat/lon) of the data we will be drawing
	pMetrics->rWorldBoundingBox.A.fLongitude = pMap->MapCenter.fLongitude - pMetrics->fScreenLongitude/2;
	pMetrics->rWorldBoundingBox.A.fLatitude = pMap->MapCenter.fLatitude - pMetrics->fScreenLatitude/2;
	pMetrics->rWorldBoundingBox.B.fLongitude = pMap->MapCenter.fLongitude + pMetrics->fScreenLongitude/2;
	pMetrics->rWorldBoundingBox.B.fLatitude = pMap->MapCenter.fLatitude + pMetrics->fScreenLatitude/2;	
}

static gboolean map_data_load_tiles(map_t* pMap, maprect_t* pRect)
{
//         g_print("*****\n"
//                 "rect is (%f,%f)(%f,%f)\n", pRect->A.fLatitude,pRect->A.fLongitude, pRect->B.fLatitude,pRect->B.fLongitude);
	gint32 nLatStart = (gint32)(pRect->A.fLatitude * TILE_SHIFT);
	// round it DOWN (south)
	if(pRect->A.fLatitude > 0) {
		nLatStart -= (nLatStart % TILE_MODULUS);
	}
	else {
		nLatStart -= (nLatStart % TILE_MODULUS);
		nLatStart -= TILE_MODULUS;
	}

	gint32 nLonStart = (gint32)(pRect->A.fLongitude * TILE_SHIFT);
	// round it DOWN (west)
	if(pRect->A.fLongitude > 0) {
		nLonStart -= (nLonStart % TILE_MODULUS);
	}
	else {
		nLonStart -= (nLonStart % TILE_MODULUS);
		nLonStart -= TILE_MODULUS;
	}

	gint32 nLatEnd = (gint32)(pRect->B.fLatitude * TILE_SHIFT);
	// round it UP (north)
	if(pRect->B.fLatitude > 0) {
		nLatEnd -= (nLatEnd % TILE_MODULUS);
		nLatEnd += TILE_MODULUS;
	}
	else {
		nLatEnd -= (nLatEnd % TILE_MODULUS);
	}

	gint32 nLonEnd = (gint32)(pRect->B.fLongitude * TILE_SHIFT);
	// round it UP (east)
	if(pRect->B.fLongitude > 0) {
		nLonEnd -= (nLonEnd % TILE_MODULUS);
		nLonEnd += TILE_MODULUS;
	}
	else {
		nLonEnd -= (nLonEnd % TILE_MODULUS);
	}

	// how many tiles are we loading in each direction?  (nice and safe as integer math...)
	gint nLatNumTiles = (nLatEnd - nLatStart) / TILE_MODULUS;
	gint nLonNumTiles = (nLonEnd - nLonStart) / TILE_MODULUS;

	gdouble fLatStart = (gdouble)nLatStart / TILE_SHIFT;
	gdouble fLonStart = (gdouble)nLonStart / TILE_SHIFT;

	if(fLatStart > pRect->A.fLatitude) {
		g_print("fLatStart %f > pRect->A.fLatitude %f\n", fLatStart, pRect->A.fLatitude);
		g_assert(fLatStart <= pRect->A.fLatitude);
	}
	if(fLonStart > pRect->A.fLongitude) {
		g_print("fLonStart %f > pRect->A.fLongitude %f!!\n", fLonStart, pRect->A.fLongitude);
		g_assert_not_reached();
	}

	gint nLat,nLon;
	for(nLat = 0 ; nLat < nLatNumTiles ; nLat++) {
		for(nLon = 0 ; nLon < nLonNumTiles ; nLon++) {

			maprect_t rect;
			rect.A.fLatitude = fLatStart + ((gdouble)(nLat) * MAP_TILE_WIDTH);
			rect.A.fLongitude = fLonStart + ((gdouble)(nLon) * MAP_TILE_WIDTH);
			rect.B.fLatitude = fLatStart + ((gdouble)(nLat+1) * MAP_TILE_WIDTH);
			rect.B.fLongitude = fLonStart + ((gdouble)(nLon+1) * MAP_TILE_WIDTH);

			map_data_load_geometry(pMap, &rect);
			map_data_load_locations(pMap, &rect);
		}
	}
}

static gboolean map_enhance_linestring(GPtrArray* pSourceArray, GPtrArray* pDestArray, gboolean (*callback_alloc_point)(mappoint_t**), gdouble fMaxDistanceBetweenPoints, gdouble fMaxRandomDistance)
{
	g_assert(pSourceArray->len >= 2);
//g_print("pSourceArray->len = %d\n", pSourceArray->len);

	// add first point
	g_ptr_array_add(pDestArray, g_ptr_array_index(pSourceArray, 0));

	gint i = 0;
	for(i=0 ; i<(pSourceArray->len-1) ; i++) {
		mappoint_t* pPoint1 = g_ptr_array_index(pSourceArray, i);
		mappoint_t* pPoint2 = g_ptr_array_index(pSourceArray, i+1);

		gdouble fRise = (pPoint2->fLatitude - pPoint1->fLatitude);
		gdouble fRun = (pPoint2->fLongitude - pPoint1->fLongitude);

		gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));
//		g_print("fLineLength = %f\n", fLineLength);
		gint nNumMiddlePoints = (gint)(fLineLength / fMaxDistanceBetweenPoints);
//		g_print("(fLineLength / fMaxDistanceBetweenPoints) = nNumNewPoints; %f / %f = %d\n", fLineLength, fMaxDistanceBetweenPoints, nNumNewPoints);
		if(nNumMiddlePoints == 0) continue;	// nothing to add

//		g_print("fDistanceBetweenPoints = %f\n", fDistanceBetweenPoints);

		gdouble fNormalizedX = fRun / fLineLength;
		gdouble fNormalizedY = fRise / fLineLength;
//		g_print("fNormalizedX = %f\n", fNormalizedX);
//		g_print("fNormalizedY = %f\n", fNormalizedY);

		gdouble fPerpendicularNormalizedX = fRise / fLineLength;
		gdouble fPerpendicularNormalizedY = -(fRun / fLineLength);

		// add points along the line
		gdouble fDistanceBetweenPoints = fLineLength / (gdouble)(nNumMiddlePoints + 1);

		gint j;
		for(j=0 ; j<nNumMiddlePoints ; j++) {
			mappoint_t* pNewPoint = NULL;
			callback_alloc_point(&pNewPoint);
			gdouble fDistanceFromPoint1 = (j+1) * fDistanceBetweenPoints;

			pNewPoint->fLongitude = pPoint1->fLongitude + (fDistanceFromPoint1 * fNormalizedX);
			pNewPoint->fLatitude = pPoint1->fLatitude + (fDistanceFromPoint1 * fNormalizedY);

			gdouble fRandomMovementLength = fMaxRandomDistance * g_random_double_range(-1.0, 1.0);
			pNewPoint->fLongitude += (fPerpendicularNormalizedX * fRandomMovementLength);	// move each component
			pNewPoint->fLatitude += (fPerpendicularNormalizedY * fRandomMovementLength);

			g_ptr_array_add(pDestArray, pNewPoint);
		}
	}
	// add last point
	g_ptr_array_add(pDestArray, g_ptr_array_index(pSourceArray, pSourceArray->len-1));
//g_print("pDestArray->len = %d\n", pDestArray->len);
}

static gboolean map_data_load_geometry(map_t* pMap, maprect_t* pRect)
{
	g_return_val_if_fail(pMap != NULL, FALSE);
	g_return_val_if_fail(pRect != NULL, FALSE);

	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	gint nZoomLevel = map_get_zoomlevel(pMap);

	TIMER_BEGIN(mytimer, "BEGIN Geometry LOAD");

	// generate SQL
	gchar azCoord1[20], azCoord2[20], azCoord3[20], azCoord4[20], azCoord5[20], azCoord6[20], azCoord7[20], azCoord8[20];
	gchar* pszSQL;
	pszSQL = g_strdup_printf(
		"SELECT Road.ID, Road.TypeID, AsBinary(Road.Coordinates), RoadName.Name, RoadName.SuffixID, AddressLeftStart, AddressLeftEnd, AddressRightStart, AddressRightEnd"
		" FROM Road "
		" LEFT JOIN RoadName ON (Road.RoadNameID=RoadName.ID)"
		" WHERE"
		" MBRIntersects(GeomFromText('Polygon((%s %s,%s %s,%s %s,%s %s,%s %s))'), Coordinates)"
		,
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
			pNewRoad->nAddressLeftStart = atoi(aRow[5]);
			pNewRoad->nAddressLeftEnd = atoi(aRow[6]);
			pNewRoad->nAddressRightStart = atoi(aRow[7]);
			pNewRoad->nAddressRightEnd = atoi(aRow[8]);

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
			g_ptr_array_add(pMap->apLayerData[nTypeID]->pRoadsArray, pNewRoad);
		} // end while loop on rows
		//g_print("[%d rows]\n", uRowCount);
		TIMER_SHOW(mytimer, "after rows retrieved");

		db_free_result(pResultSet);
		TIMER_SHOW(mytimer, "after free results");
		TIMER_END(mytimer, "END Geometry LOAD");

		return TRUE;
	}
	else {
		//g_print(" no rows\n");
		return FALSE;
	}	
}

static gboolean map_data_load_locations(map_t* pMap, maprect_t* pRect)
{
	g_return_val_if_fail(pMap != NULL, FALSE);
	g_return_val_if_fail(pRect != NULL, FALSE);

//     if(map_get_zoomlevel(pMap) < MIN_ZOOM_LEVEL_FOR_LOCATIONS) {
//         return TRUE;
//     }

	TIMER_BEGIN(mytimer, "BEGIN Locations LOAD");

	// generate SQL
	gchar* pszSQL;
	gchar azCoord1[20], azCoord2[20], azCoord3[20], azCoord4[20], azCoord5[20], azCoord6[20], azCoord7[20], azCoord8[20];
	pszSQL = g_strdup_printf(
		"SELECT Location.ID, Location.LocationSetID, AsBinary(Location.Coordinates), LocationAttributeValue_Name.Value"	// LocationAttributeValue_Name.Value is the "Name" field of this Location
		" FROM Location"
		" LEFT JOIN LocationAttributeValue AS LocationAttributeValue_Name ON (LocationAttributeValue_Name.LocationID=Location.ID AND LocationAttributeValue_Name.AttributeNameID=%d)"
		" WHERE"
		" MBRIntersects(GeomFromText('Polygon((%s %s,%s %s,%s %s,%s %s,%s %s))'), Coordinates)",
		LOCATION_ATTRIBUTE_ID_NAME,	// attribute ID for 'name'
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

	db_resultset_t* pResultSet = NULL;
	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);

	TIMER_SHOW(mytimer, "after query");

	guint32 uRowCount = 0;
	if(pResultSet) {
		db_row_t aRow;
		while((aRow = db_fetch_row(pResultSet))) {
			uRowCount++;

			// aRow[0] is ID
			// aRow[1] is LocationSetID
			// aRow[2] is Coordinates in mysql's binary format
			// aRow[3] is Name

			// Get layer type that this belongs on
			gint nLocationSetID = atoi(aRow[1]);

			// Extract
			location_t* pNewLocation = NULL;
			location_alloc(&pNewLocation);
			
			pNewLocation->nID = atoi(aRow[0]);

			// Parse coordinates
			db_parse_wkb_point(aRow[2], &(pNewLocation->Coordinates));

			// make a copy of the name field, or "" (never leave it == NULL)
			pNewLocation->pszName = g_strdup(aRow[3] != NULL ? aRow[3] : "");
			map_store_location(pMap, pNewLocation, nLocationSetID);
		} // end while loop on rows
		//g_print("[%d rows]\n", uRowCount);
		TIMER_SHOW(mytimer, "after rows retrieved");

		db_free_result(pResultSet);
		TIMER_END(mytimer, "END Locations LOAD");
		return TRUE;
	}
	else {
		TIMER_END(mytimer, "END Locations LOAD (0 results)");
		return FALSE;
	}	
}

static void map_data_clear(map_t* pMap)
{
	// Clear layers
	gint i,j;
	for(i=0 ; i<G_N_ELEMENTS(pMap->apLayerData) ; i++) {
		maplayer_data_t* pLayerData = pMap->apLayerData[i];

		// Free each
		for(j = (pLayerData->pRoadsArray->len - 1) ; j>=0 ; j--) {
			road_t* pRoad = g_ptr_array_remove_index_fast(pLayerData->pRoadsArray, j);
			g_array_free(pRoad->pMapPointsArray, TRUE);
			g_free(pRoad);
		}
		g_assert(pLayerData->pRoadsArray->len == 0);
	}

	// Clear locations
	map_init_location_hash(pMap);
}

double map_get_distance_in_meters(mappoint_t* pA, mappoint_t* pB)
{
	// XXX: this function is broken.

	// This functions calculates the length of the arc of the "greatcircle" that goes through
	// the two points A and B and whos center is the center of the sphere, O.

	// When we multiply this angle (in radians) by the radius, we get the length of the arc.

	// NOTE: This algorithm wrongly assumes that Earth is a perfect sphere.
	//       It is actually slightly egg shaped.  But it's good enough.

	// All trig functions expect arguments in radians.
	double fLonA_Rad = DEG2RAD(pA->fLongitude);
	double fLonB_Rad = DEG2RAD(pB->fLongitude);
	double fLatA_Rad = DEG2RAD(pA->fLatitude);
	double fLatB_Rad = DEG2RAD(pB->fLatitude);

	// Step 1. Calculate AOB (in radians).
	// An explanation of this equation is at http://mathforum.org/library/drmath/view/51722.html
	double fAOB_Rad = acos((cos(fLatA_Rad) * cos(fLatB_Rad) * cos(fLonB_Rad - fLonA_Rad)) + (sin(fLatA_Rad) * sin(fLatB_Rad)));

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

void map_add_track(map_t* pMap, gint hTrack)
{
	g_array_append_val(pMap->pTracksArray, hTrack);
}

// ========================================================
//  Hit Testing
// ========================================================

void map_hitstruct_free(map_t* pMap, maphit_t* pHitStruct)
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

// XXX: perhaps make map_hit_test return a more complex structure indicating what type of hit it is?
gboolean map_hit_test(map_t* pMap, mappoint_t* pMapPoint, maphit_t** ppReturnStruct)
{
#if 0	// GGGGGGGGGGGGGGGG
	rendermetrics_t rendermetrics;
	map_get_render_metrics(pMap, &rendermetrics);

	if(map_hit_test_locationselections(pMap, &rendermetrics, pMap->pLocationSelectionArray, pMapPoint, ppReturnStruct)) {
		return TRUE;
	}

	if(map_hit_test_locationsets(pMap, &rendermetrics, pMapPoint, ppReturnStruct)) {
		return TRUE;
	}

	// Test things in the REVERSE order they are drawn (otherwise we'll match things that have been painted-over)
	gint i;
	for(i=G_N_ELEMENTS(layerdraworder)-1 ; i>=0 ; i--) {
		if(layerdraworder[i].eSubLayerRenderType != SUBLAYER_RENDERTYPE_LINES) continue;

		gint nLayer = layerdraworder[i].nLayer;

		// use width from whichever layer it's wider in
		gdouble fLineWidth = max(g_aLayers[nLayer]->Style.aSubLayers[0].afLineWidths[pMap->uZoomLevel-1],
					 g_aLayers[nLayer]->Style.aSubLayers[1].afLineWidths[pMap->uZoomLevel-1]);

#define EXTRA_CLICKABLE_ROAD_IN_PIXELS	(3)

		// make thin roads a little easier to hit
		// fLineWidth = max(fLineWidth, MIN_ROAD_HIT_TARGET_WIDTH);

		// XXX: hack, map_pixels should really take a floating point instead.
		gdouble fMaxDistance = map_pixels_to_degrees(pMap, 1, pMap->uZoomLevel) * ((fLineWidth/2) + EXTRA_CLICKABLE_ROAD_IN_PIXELS);  // half width on each side

		if(map_hit_test_layer_roads(pMap->apLayerData[nLayer]->pRoadsArray, fMaxDistance, pMapPoint, ppReturnStruct)) {
			return TRUE;
		}
		// otherwise try next layer...
	}
#endif
	return FALSE;
}

static gboolean map_hit_test_layer_roads(GPtrArray* pRoadsArray, gdouble fMaxDistance, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
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
	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		road_t* pRoad = g_ptr_array_index(pRoadsArray, iString);
		if(pRoad->pMapPointsArray->len < 2) continue;

		// start on 1 so we can do -1 trick below
		gint iPoint;
		for(iPoint=1 ; iPoint<pRoad->pMapPointsArray->len ; iPoint++) {
			mappoint_t* pPoint1 = &g_array_index(pRoad->pMapPointsArray, mappoint_t, iPoint-1);
			mappoint_t* pPoint2 = &g_array_index(pRoad->pMapPointsArray, mappoint_t, iPoint);

			mappoint_t pointClosest;
			gdouble fPercentAlongLine;

			// hit test this line
			if(map_hit_test_line(pPoint1, pPoint2, pHitPoint, fMaxDistance, &pointClosest, &fPercentAlongLine)) {
				//g_print("fPercentAlongLine = %f\n",fPercentAlongLine);

				// fill out a new maphit_t struct with details
				maphit_t* pHitStruct = g_new0(maphit_t, 1);
				pHitStruct->eHitType = MAP_HITTYPE_ROAD;

				if(pRoad->pszName[0] == '\0') {
					pHitStruct->pszText = g_strdup("<i>unnamed</i>");
				}
				else {
					ESide eSide = map_side_test_line(pPoint1, pPoint2, &pointClosest, pHitPoint);

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
						gint nMinAddres = min(nAddressStart, nAddressEnd);
						gint nMaxAddres = max(nAddressStart, nAddressEnd);

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

// Does the given point come close enough to the line segment to be considered a hit?
static gboolean map_hit_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pHitPoint, gdouble fMaxDistance, mappoint_t* pReturnClosestPoint, gdouble* pfReturnPercentAlongLine)
{
	if(pHitPoint->fLatitude < (pPoint1->fLatitude - fMaxDistance) && pHitPoint->fLatitude < (pPoint2->fLatitude - fMaxDistance)) return FALSE;
	if(pHitPoint->fLongitude < (pPoint1->fLongitude - fMaxDistance) && pHitPoint->fLongitude < (pPoint2->fLongitude - fMaxDistance)) return FALSE;

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

static ESide map_side_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pClosestPointOnLine, mappoint_t* pHitPoint)
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
static gboolean map_hit_test_locationsets(map_t* pMap, rendermetrics_t* pRenderMetrics, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	gdouble fMaxDistance = map_pixels_to_degrees(pMap, 1, pMap->uZoomLevel) * 3;	// XXX: don't hardcode distance :)

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
			if(map_hit_test_locations(pMap, pRenderMetrics, pLocationsArray, pHitPoint, ppReturnStruct)) {
				return TRUE;
			}
		}
		else {
			// none loaded
		}
	}
	return FALSE;
}

// hit-test an array of locations
static gboolean map_hit_test_locations(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
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

gboolean hit_test_screenpoint_in_rect(screenpoint_t* pPt, screenrect_t* pRect)
{
	return(pPt->nX >= pRect->A.nX && pPt->nX <= pRect->B.nX && pPt->nY >= pRect->A.nY && pPt->nY <= pRect->B.nY);
}

static gboolean map_hit_test_locationselections(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	screenpoint_t screenpoint;
	screenpoint.nX = (gint)SCALE_X(pRenderMetrics, pHitPoint->fLongitude);
	screenpoint.nY = (gint)SCALE_Y(pRenderMetrics, pHitPoint->fLatitude);

	gint i;
	for(i=(pLocationSelectionArray->len-1) ; i>=0 ; i--) {
		locationselection_t* pLocationSelection = g_ptr_array_index(pLocationSelectionArray, i);

		if(pLocationSelection->bVisible == FALSE) continue;

		if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->InfoBoxRect))) {
			// fill out a new maphit_t struct with details
			maphit_t* pHitStruct = g_new0(maphit_t, 1);

			if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->InfoBoxCloseRect))) {
				pHitStruct->eHitType = MAP_HITTYPE_LOCATIONSELECTION_CLOSE;
				pHitStruct->pszText = g_strdup("close");
				pHitStruct->LocationSelectionHit.nLocationID = pLocationSelection->nLocationID;
			}
			else if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->EditRect))) {
				pHitStruct->eHitType = MAP_HITTYPE_LOCATIONSELECTION_EDIT;
				pHitStruct->pszText = g_strdup("edit");
				pHitStruct->LocationSelectionHit.nLocationID = pLocationSelection->nLocationID;
			}
			else {
				gboolean bURLMatch = FALSE;

				gint iURL;
				for(iURL=0 ; iURL<pLocationSelection->nNumURLs ; iURL++) {
					if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->aURLs[iURL].Rect))) {
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


// Get an attribute from a selected location
const gchar* map_location_selection_get_attribute(const map_t* pMap, const locationselection_t* pLocationSelection, const gchar* pszAttributeName)
{
	// NOTE: always gets the 'first' one.  this is only used for the few hardcoded attributes (name, address, ...)
	gint i;
	for(i=0 ; i<pLocationSelection->pAttributesArray->len ; i++) {
		locationattribute_t* pAttribute = g_ptr_array_index(pLocationSelection->pAttributesArray, i);
		if(strcmp(pAttribute->pszName, pszAttributeName) == 0) {
			return pAttribute->pszValue;
		}
	}
	return NULL;
}

gboolean map_object_type_atoi(const gchar* pszName, gint* pnReturnObjectTypeID)
{
	gint i;
	for(i=0 ; i<MAP_NUM_OBJECT_TYPES ; i++) {
		if(strcmp(pszName, g_apszMapObjectTypeNames[i]) == 0) {
			*pnReturnObjectTypeID = i;
			return TRUE;
		}
	}
	return FALSE;
}

gboolean map_layer_render_type_atoi(const gchar* pszName, gint* pnReturnRenderTypeID)
{
	gchar* g_apszMapRenderTypeNames[] = {"lines", "polygons", "line-labels", "polygon-labels", "fill", "locations", "location-labels"};

	gint i;
	for(i=0 ; i<G_N_ELEMENTS(g_apszMapRenderTypeNames) ; i++) {
		if(strcmp(pszName, g_apszMapRenderTypeNames[i]) == 0) {
			*pnReturnRenderTypeID = i;
			return TRUE;
		}
	}
	return FALSE;
}

void map_callback_free_locations_array(gpointer* p)
{
	GPtrArray* pLocationsArray = (GPtrArray*)p;
	gint i;
	for(i=(pLocationsArray->len-1) ; i>=0 ;i--) {
		location_t* pLocation = g_ptr_array_remove_index_fast(pLocationsArray, i);
		location_free(pLocation);
	}
}

static void map_init_location_hash(map_t* pMap)
{
	// destroy it if it exists... it will call map_callback_free_locations_array() on all children and g_free on all keys (see below)
	if(pMap->pLocationArrayHashTable != NULL) {
		g_hash_table_destroy(pMap->pLocationArrayHashTable);
	}
	pMap->pLocationArrayHashTable = g_hash_table_new_full(g_int_hash, g_int_equal, 
								g_free, 				/* key destroy function */
								map_callback_free_locations_array);	/* value destroy function */
}

static void map_store_location(map_t* pMap, location_t* pLocation, gint nLocationSetID)
{
	GPtrArray* pLocationsArray;
	pLocationsArray = g_hash_table_lookup(pMap->pLocationArrayHashTable, &nLocationSetID);
	if(pLocationsArray != NULL) {
		// found existing array
		//g_print("existing for set %d\n", nLocationSetID);
	}
	else {
		//g_print("new for set %d\n", nLocationSetID);

		// need a new array
		pLocationsArray = g_ptr_array_new();
		g_assert(pLocationsArray != NULL);

		gint* pKey = g_malloc(sizeof(gint));
		*pKey = nLocationSetID;
		g_hash_table_insert(pMap->pLocationArrayHashTable, pKey, pLocationsArray);
	}

	// add location to the array of locations!
	g_ptr_array_add(pLocationsArray, pLocation);
	//g_print("pLocationsArray->len = %d\n", pLocationsArray->len);
}

// ========================================================
//  Functions to deal with "selected" locations (POI), which are always kept in RAM
// ========================================================

// lookup a selected location by ID
locationselection_t* map_location_selection_get(map_t* pMap, gint nLocationID)
{
	// XXX: should we add a hash table on locationID to speed up searches?
	gint i;
	for(i=0 ; i<pMap->pLocationSelectionArray->len ; i++) {
		locationselection_t* pSel = g_ptr_array_index(pMap->pLocationSelectionArray, i);
		if(pSel->nLocationID == nLocationID) return pSel;
	}
	return NULL;
}

gint map_location_selection_latitude_sort_callback(gconstpointer ppA, gconstpointer ppB)
{
	locationselection_t* pA = *((locationselection_t**)ppA);
	locationselection_t* pB = *((locationselection_t**)ppB);

	// we want them ordered from greatest latitude to smallest
	return ((pA->Coordinates.fLatitude > pB->Coordinates.fLatitude) ? -1 : 1);
}

// add a Location to the selected list
gboolean map_location_selection_add(map_t* pMap, gint nLocationID)
{
	if(map_location_selection_get(pMap, nLocationID) != NULL) {
		// already here
		return FALSE;
	}

	// create a new locationselection_t and initialize it
	locationselection_t* pNew = g_free_list_alloc(pMap->pLocationSelectionAllocator);

	pNew->nLocationID = nLocationID;
	pNew->pAttributesArray = g_ptr_array_new();

	// load all attributes
	location_load(nLocationID, &(pNew->Coordinates), NULL);
	location_load_attributes(nLocationID, pNew->pAttributesArray);

	g_ptr_array_add(pMap->pLocationSelectionArray, pNew);

	// sort in order of latitude
	// POI that seem "closer" to the user (lower on the screen) will be drawn last (on top)
	g_ptr_array_sort(pMap->pLocationSelectionArray, map_location_selection_latitude_sort_callback);

	return TRUE;	// added
}

// remove a Location to the selected list
gboolean map_location_selection_remove(map_t* pMap, gint nLocationID)
{
	gint i;
	for(i=0 ; i<pMap->pLocationSelectionArray->len ; i++) {
		locationselection_t* pSel = g_ptr_array_index(pMap->pLocationSelectionArray, i);
		if(pSel->nLocationID == nLocationID) {
			g_ptr_array_remove_index(pMap->pLocationSelectionArray, i);
			return TRUE;
		}
	}
	return FALSE;	// removed
}

gboolean map_rects_overlap(const maprect_t* p1, const maprect_t* p2)
{
	if(p1->B.fLatitude < p2->A.fLatitude) return FALSE;
	if(p1->B.fLongitude < p2->A.fLongitude) return FALSE;
	if(p1->A.fLatitude > p2->B.fLatitude) return FALSE;
	if(p1->A.fLongitude > p2->B.fLongitude) return FALSE;

	return TRUE;
}
