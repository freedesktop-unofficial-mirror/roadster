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
#include <gtk/gtk.h>
#include <math.h>

#include "main.h"
#include "gui.h"
#include "map.h"
#include "mainwindow.h"
#include "util.h"
#include "db.h"
#include "road.h"
#include "point.h"
#include "layers.h"
#include "locationset.h"
#include "scenemanager.h"

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

#define MIN_ROAD_HIT_TARGET_WIDTH	(4)	// make super thin roads a bit easier to hover over/click, in pixels


/* Prototypes */

// data loading
static gboolean map_data_load_tiles(map_t* pMap, maprect_t* pRect);	// ensure tiles
static gboolean map_data_load(map_t* pMap, maprect_t* pRect);

// hit testing
static gboolean map_hit_test_layer_roads(GPtrArray* pPointStringsArray, gdouble fMaxDistance, mappoint_t* pHitPoint, gchar** ppReturnString);
static gboolean map_hit_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pHitPoint, gdouble fDistance, mappoint_t* pReturnClosestPoint);

static ESide map_side_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pClosestPointOnLine, mappoint_t* pHitPoint);

static void map_data_clear(map_t* pMap);
void map_get_render_metrics(map_t* pMap, rendermetrics_t* pMetrics);

// Each zoomlevel has a scale and an optional name (name isn't used for anything)
zoomlevel_t g_sZoomLevels[NUM_ZOOMLEVELS+1] = {
	{1,"undefined"},	// no zoom level 0

	{ 1600000, ""},		// 1
	{  800000, ""},		// 2
	{  400000, ""},		// 3
	{  200000, ""},		// 4
	{  100000, ""},		// 5

	{   35000, ""},		// 6
	{   20000, ""}, 	// 7
	{   10000, ""},		// 8
	{    4000, ""},		// 9
	{    1700, ""},		// 10
};

draworder_t layerdraworder[NUM_SUBLAYER_TO_DRAW] = {

	{LAYER_MISC_AREA, 0, SUBLAYER_RENDERTYPE_POLYGONS}, //map_draw_layer_polygons},

	{LAYER_PARK, 0, SUBLAYER_RENDERTYPE_POLYGONS}, //map_draw_layer_polygons},
	{LAYER_PARK, 1, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},

	{LAYER_RIVER, 0, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},	// single-line rivers

	{LAYER_LAKE, 0, SUBLAYER_RENDERTYPE_POLYGONS}, //map_draw_layer_polygons},	// lakes and fat rivers
//	{LAYER_LAKE, 1, map_draw_layer_lines},

	{LAYER_MINORHIGHWAY_RAMP, 0, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},
	{LAYER_MINORSTREET, 0, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},
	{LAYER_MAJORSTREET, 0, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},

	{LAYER_MINORHIGHWAY_RAMP, 1, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},
	{LAYER_MINORSTREET, 1, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},

	{LAYER_MAJORSTREET, 1, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},

	{LAYER_RAILROAD, 0, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},
	{LAYER_RAILROAD, 1, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},

	{LAYER_MINORHIGHWAY, 0, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},
	{LAYER_MINORHIGHWAY, 1, SUBLAYER_RENDERTYPE_LINES}, //map_draw_layer_lines},

	// LABELS
	{LAYER_MINORSTREET, 0, SUBLAYER_RENDERTYPE_LINE_LABELS},
	{LAYER_MAJORSTREET, 0, SUBLAYER_RENDERTYPE_LINE_LABELS},
	{LAYER_RAILROAD, 0, SUBLAYER_RENDERTYPE_LINE_LABELS},
	{LAYER_MINORHIGHWAY, 0, SUBLAYER_RENDERTYPE_LINE_LABELS},
///     {LAYER_MAJORHIGHWAY, 0, SUBLAYER_RENDERTYPE_LABELS},

	{LAYER_MISC_AREA, 0, SUBLAYER_RENDERTYPE_POLYGON_LABELS},
	{LAYER_PARK, 0, SUBLAYER_RENDERTYPE_POLYGON_LABELS},
	{LAYER_LAKE, 0, SUBLAYER_RENDERTYPE_POLYGON_LABELS},
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
	pMap->m_pTracksArray = g_array_new(FALSE, /* not zero-terminated */
					  TRUE,  /* clear to 0 (?) */
					  sizeof(gint));

	pMap->m_pTargetWidget = pTargetWidget;

	scenemanager_new(&(pMap->m_pSceneManager));
	g_assert(pMap->m_pSceneManager);

	pMap->m_uZoomLevel = 8;

	// init containers for geometry data
	gint i;
	for(i=0 ; i<NUM_ELEMS(pMap->m_apLayerData) ; i++) {
		maplayer_data_t* pLayer = g_new0(maplayer_data_t, 1);
		pLayer->m_pRoadsArray = g_ptr_array_new();
		pMap->m_apLayerData[i] = pLayer;
	}

	// save it
	*ppMap = pMap;
	return TRUE;
}

void map_draw(map_t* pMap, gint nDrawFlags)
{
	g_assert(pMap != NULL);

	scenemanager_clear(pMap->m_pSceneManager);

	// Get render metrics
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	//g_print("drawing at %f,%f\n", pMap->m_MapCenter.m_fLatitude, pMap->m_MapCenter.m_fLongitude);

	//
	// Load geometry
	//
	TIMER_BEGIN(loadtimer, "--- BEGIN ALL DB LOAD");
	map_data_clear(pMap);
	map_data_load_tiles(pMap, &(pRenderMetrics->m_rWorldBoundingBox));
//	locationset_load_locations(&(pRenderMetrics->m_rWorldBoundingBox));
	TIMER_END(loadtimer, "--- END ALL DB LOAD");

	gint nRenderMode = RENDERMODE_FAST;

#ifdef SCENEMANAGER_DEBUG_TEST
        GdkRectangle rect = {200,200,100,100};
        scenemanager_claim_rectangle(pMap->m_pSceneManager, &rect);
#endif

	if(nRenderMode == RENDERMODE_FAST) {
		// 
		if(nDrawFlags & DRAWFLAG_GEOMETRY) {
			map_draw_gdk(pMap, pRenderMetrics, pMap->m_pPixmap, DRAWFLAG_GEOMETRY);
		}
		// Always draw labels with Cairo
		if(nDrawFlags & DRAWFLAG_LABELS) {
			map_draw_cairo(pMap, pRenderMetrics, pMap->m_pPixmap, DRAWFLAG_LABELS);
		}
	}
	else {	// nRenderMode == RENDERMODE_PRETTY
		map_draw_cairo(pMap, pRenderMetrics, pMap->m_pPixmap, nDrawFlags);
	}

#ifdef SCENEMANAGER_DEBUG_TEST
        gdk_draw_rectangle(pMap->m_pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
                           FALSE, 200,200, 100, 100);
#endif

	gtk_widget_queue_draw(pMap->m_pTargetWidget);
}

// ========================================================
//  Get and release the map image
// ========================================================

GdkPixmap* map_get_pixmap(map_t* pMap)
{
	return pMap->m_pPixmap;
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
	if(uZoomLevel > MAX_ZOOMLEVEL) uZoomLevel = MAX_ZOOMLEVEL;
	else if(uZoomLevel < MIN_ZOOMLEVEL) uZoomLevel = MIN_ZOOMLEVEL;

	if(uZoomLevel != pMap->m_uZoomLevel) {
		pMap->m_uZoomLevel = uZoomLevel;
//		map_set_redraw_needed(TRUE);
	}
}

guint16 map_get_zoomlevel(map_t* pMap)
{
	return pMap->m_uZoomLevel;	// between MIN_ZOOMLEVEL and MAX_ZOOMLEVEL
}

guint32 map_get_zoomlevel_scale(map_t* pMap)
{
	return g_sZoomLevels[pMap->m_uZoomLevel].m_uScale;	// returns "5000" for 1:5000 scale
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
	double fMonitorPixelsPerInch = 85 + 1/3;
	double fPixelsPerMeter = fMonitorPixelsPerInch * INCHES_PER_METER;
	double fMetersOfPixels = ((float)nPixels) / fPixelsPerMeter;

	// If we had 3 meters of pixels (a very big monitor:) and the scale was 1000:1 then
	// we would want to show 3000 meters worth of world space
	double fMetersOfWorld = ((float)g_sZoomLevels[uZoomLevel].m_uScale) * fMetersOfPixels;

	return WORLD_METERS_TO_DEGREES(fMetersOfWorld);
}

double map_degrees_to_pixels(map_t* pMap, gdouble fDegrees, guint16 uZoomLevel)
{
	double fMonitorPixelsPerInch = 85 + 1/3;

	double fResultInMeters = WORLD_DEGREES_TO_METERS(fDegrees);
	double fResultInPixels = (INCHES_PER_METER * fResultInMeters) * fMonitorPixelsPerInch;
	fResultInPixels /= (float)g_sZoomLevels[uZoomLevel].m_uScale;
	return fResultInPixels;
}

void map_windowpoint_to_mappoint(map_t* pMap, screenpoint_t* pScreenPoint, mappoint_t* pMapPoint)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = (gint)(pScreenPoint->m_nX) - (pMap->m_MapDimensions.m_uWidth / 2);
	gint16 nPixelDeltaY = (gint)(pScreenPoint->m_nY) - (pMap->m_MapDimensions.m_uHeight / 2);

	// Convert pixels to world coordinates
	pMapPoint->m_fLongitude = pMap->m_MapCenter.m_fLongitude + map_pixels_to_degrees(pMap, nPixelDeltaX, pMap->m_uZoomLevel);
	// reverse the X, clicking above
	pMapPoint->m_fLatitude = pMap->m_MapCenter.m_fLatitude - map_pixels_to_degrees(pMap, nPixelDeltaY, pMap->m_uZoomLevel);
}

// Call this to pan around the map
void map_center_on_windowpoint(map_t* pMap, guint16 uX, guint16 uY)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = uX - (pMap->m_MapDimensions.m_uWidth / 2);
	gint16 nPixelDeltaY = uY - (pMap->m_MapDimensions.m_uHeight / 2);

	// Convert pixels to world coordinates
	double fWorldDeltaX = map_pixels_to_degrees(pMap, nPixelDeltaX, pMap->m_uZoomLevel);
	// reverse the X, clicking above
	double fWorldDeltaY = -map_pixels_to_degrees(pMap, nPixelDeltaY, pMap->m_uZoomLevel);

//	g_message("panning %d,%d pixels (%.10f,%.10f world coords)\n", nPixelDeltaX, nPixelDeltaY, fWorldDeltaX, fWorldDeltaY);

	mappoint_t pt;
	pt.m_fLatitude = pMap->m_MapCenter.m_fLatitude + fWorldDeltaY;
	pt.m_fLongitude = pMap->m_MapCenter.m_fLongitude + fWorldDeltaX;
	map_set_centerpoint(pMap, &pt);
}

void map_set_centerpoint(map_t* pMap, const mappoint_t* pPoint)
{
	g_assert(pPoint != NULL);

	pMap->m_MapCenter.m_fLatitude = pPoint->m_fLatitude;
	pMap->m_MapCenter.m_fLongitude = pPoint->m_fLongitude;

//	map_set_redraw_needed(TRUE);
}

void map_get_centerpoint(map_t* pMap, mappoint_t* pReturnPoint)
{
	g_assert(pReturnPoint != NULL);

	pReturnPoint->m_fLatitude = pMap->m_MapCenter.m_fLatitude;
	pReturnPoint->m_fLongitude = pMap->m_MapCenter.m_fLongitude;
}

//
void map_set_dimensions(map_t* pMap, const dimensions_t* pDimensions)
{
	g_assert(pDimensions != NULL);

	pMap->m_MapDimensions.m_uWidth = pDimensions->m_uWidth;
	pMap->m_MapDimensions.m_uHeight = pDimensions->m_uHeight;

	pMap->m_pPixmap = gdk_pixmap_new(
			pMap->m_pTargetWidget->window,
			pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight,
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
	pMetrics->m_nZoomLevel = map_get_zoomlevel(pMap);
	pMetrics->m_nWindowWidth = pMap->m_MapDimensions.m_uWidth;
	pMetrics->m_nWindowHeight = pMap->m_MapDimensions.m_uHeight;

	// Calculate how many world degrees we'll be drawing
	pMetrics->m_fScreenLatitude = map_pixels_to_degrees(pMap, pMap->m_MapDimensions.m_uHeight, pMetrics->m_nZoomLevel);
	pMetrics->m_fScreenLongitude = map_pixels_to_degrees(pMap, pMap->m_MapDimensions.m_uWidth, pMetrics->m_nZoomLevel);

	// The world bounding box (expressed in lat/lon) of the data we will be drawing
	pMetrics->m_rWorldBoundingBox.m_A.m_fLongitude = pMap->m_MapCenter.m_fLongitude - pMetrics->m_fScreenLongitude/2;
	pMetrics->m_rWorldBoundingBox.m_A.m_fLatitude = pMap->m_MapCenter.m_fLatitude - pMetrics->m_fScreenLatitude/2;
	pMetrics->m_rWorldBoundingBox.m_B.m_fLongitude = pMap->m_MapCenter.m_fLongitude + pMetrics->m_fScreenLongitude/2;
	pMetrics->m_rWorldBoundingBox.m_B.m_fLatitude = pMap->m_MapCenter.m_fLatitude + pMetrics->m_fScreenLatitude/2;	
}

static gboolean map_data_load_tiles(map_t* pMap, maprect_t* pRect)
{
//         g_print("*****\n"
//                 "rect is (%f,%f)(%f,%f)\n", pRect->m_A.m_fLatitude,pRect->m_A.m_fLongitude, pRect->m_B.m_fLatitude,pRect->m_B.m_fLongitude);
	gint32 nLatStart = (gint32)(pRect->m_A.m_fLatitude * TILE_SHIFT);
	// round it DOWN (south)
	if(pRect->m_A.m_fLatitude > 0) {
		nLatStart -= (nLatStart % TILE_MODULUS);
	}
	else {
		nLatStart -= (nLatStart % TILE_MODULUS);
		nLatStart -= TILE_MODULUS;
	}

	gint32 nLonStart = (gint32)(pRect->m_A.m_fLongitude * TILE_SHIFT);
	// round it DOWN (west)
	if(pRect->m_A.m_fLongitude > 0) {
		nLonStart -= (nLonStart % TILE_MODULUS);
	}
	else {
		nLonStart -= (nLonStart % TILE_MODULUS);
		nLonStart -= TILE_MODULUS;
	}

	gint32 nLatEnd = (gint32)(pRect->m_B.m_fLatitude * TILE_SHIFT);
	// round it UP (north)
	if(pRect->m_B.m_fLatitude > 0) {
		nLatEnd -= (nLatEnd % TILE_MODULUS);
		nLatEnd += TILE_MODULUS;
	}
	else {
		nLatEnd -= (nLatEnd % TILE_MODULUS);
	}

	gint32 nLonEnd = (gint32)(pRect->m_B.m_fLongitude * TILE_SHIFT);
	// round it UP (east)
	if(pRect->m_B.m_fLongitude > 0) {
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

//	g_print("%f < %f\n", fLatStart, pRect->m_A.m_fLatitude);
	g_assert(fLatStart <= pRect->m_A.m_fLatitude);
        g_assert(fLonStart <= pRect->m_A.m_fLongitude);

	gint nLat,nLon;
	for(nLat = 0 ; nLat < nLatNumTiles ; nLat++) {
		for(nLon = 0 ; nLon < nLonNumTiles ; nLon++) {

			maprect_t rect;
			rect.m_A.m_fLatitude = fLatStart + ((gdouble)(nLat) * MAP_TILE_WIDTH);
			rect.m_A.m_fLongitude = fLonStart + ((gdouble)(nLon) * MAP_TILE_WIDTH);
			rect.m_B.m_fLatitude = fLatStart + ((gdouble)(nLat+1) * MAP_TILE_WIDTH);
			rect.m_B.m_fLongitude = fLonStart + ((gdouble)(nLon+1) * MAP_TILE_WIDTH);

			map_data_load(pMap, &rect);
		}
	}
}

static gboolean map_data_load(map_t* pMap, maprect_t* pRect)
{
	g_return_val_if_fail(pMap != NULL, FALSE);
	g_return_val_if_fail(pRect != NULL, FALSE);

	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	gint nZoomLevel = map_get_zoomlevel(pMap);

	TIMER_BEGIN(mytimer, "BEGIN Geometry LOAD");

	// generate SQL
	gchar* pszSQL;
	pszSQL = g_strdup_printf(
		"SELECT Road.ID, Road.TypeID, AsBinary(Road.Coordinates), RoadName.Name, RoadName.SuffixID, AddressLeftStart, AddressLeftEnd, AddressRightStart, AddressRightEnd"
		" FROM Road "
		" LEFT JOIN RoadName ON (Road.RoadNameID=RoadName.ID)"
		" WHERE"
		//" TypeID IN (%s) AND"
                //" MBRIntersects(@wkb, Coordinates)"
		" MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)"
		,pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude, 	// upper left
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
		TIMER_SHOW(mytimer, "after clear layers");
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
//			g_print("data: %s, %s, %s, %s, %s\n", aRow[0], aRow[1], aRow[2], aRow[3], aRow[4]);

			// Get layer type that this belongs on
			gint nTypeID = atoi(aRow[1]);
			if(nTypeID < LAYER_FIRST || nTypeID > LAYER_LAST) {
				g_warning("geometry record '%s' has bad type '%s'\n", aRow[0], aRow[1]);
				continue;
			}

			// Extract points
			road_t* pNewRoad = NULL;
			road_alloc(&pNewRoad);

			//pointstring_t* pNewPointString = NULL;
			//if(!pointstring_alloc(&pNewPointString)) {
			//	g_warning("out of memory loading pointstrings\n");
			//	continue;
			//}
			db_parse_wkb_linestring(aRow[2], pNewRoad->m_pPointsArray, point_alloc);

			// Build name by adding suffix, if one is present
			gchar azFullName[100] = "";

			// does it have a name?			
			if(aRow[3] != NULL && aRow[4] != NULL) {
				gint nSuffixID = atoi(aRow[4]);
				const gchar* pszSuffix = road_suffix_itoa(nSuffixID, ROAD_SUFFIX_LENGTH_SHORT);
				g_snprintf(azFullName, 100, "%s%s%s",
					aRow[3], (pszSuffix[0] != '\0') ? " " : "", pszSuffix);
			}
			pNewRoad->m_nAddressLeftStart = atoi(aRow[5]);
			pNewRoad->m_nAddressLeftEnd = atoi(aRow[6]);
			pNewRoad->m_nAddressRightStart = atoi(aRow[7]);
			pNewRoad->m_nAddressRightEnd = atoi(aRow[8]);

			pNewRoad->m_pszName = g_strdup(azFullName);

			// Add this item to layer's list of pointstrings
			g_ptr_array_add(pMap->m_apLayerData[nTypeID]->m_pRoadsArray, pNewRoad);
		} // end while loop on rows
		//g_print("[%d rows]\n", uRowCount);
		TIMER_SHOW(mytimer, "after rows retrieved");

		db_free_result(pResultSet);
		TIMER_SHOW(mytimer, "after free results");
		TIMER_END(mytimer, "END Geometry LOAD");

		return TRUE;
	}
	else {
//		g_print(" no rows\n");
		return FALSE;
	}	
}

static void map_data_clear(map_t* pMap)
{
	gint i,j;
	for(i=0 ; i<NUM_ELEMS(pMap->m_apLayerData) ; i++) {
		maplayer_data_t* pLayerData = pMap->m_apLayerData[i];

		// Free each
		for(j = (pLayerData->m_pRoadsArray->len - 1) ; j>=0 ; j--) {
			road_t* pRoad = g_ptr_array_remove_index_fast(pLayerData->m_pRoadsArray, j);
			road_free(pRoad);
		}
		g_assert(pLayerData->m_pRoadsArray->len == 0);
	}
}

double map_get_distance_in_meters(mappoint_t* pA, mappoint_t* pB)
{
	// This functions calculates the length of the arc of the "greatcircle" that goes through
	// the two points A and B and whos center is the center of the sphere, O.

	// When we multiply this angle (in radians) by the radius, we get the length of the arc.

	// NOTE: This algorithm wrongly assumes that Earth is a perfect sphere.
	//       It is actually slightly egg shaped.  But it's good enough.

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

gdouble map_get_distance_in_pixels(map_t* pMap, mappoint_t* p1, mappoint_t* p2)
{
	rendermetrics_t metrics;
	map_get_render_metrics(pMap, &metrics);

	gdouble fX1 = SCALE_X(&metrics, p1->m_fLongitude);
	gdouble fY1 = SCALE_Y(&metrics, p1->m_fLongitude);
	gdouble fX2 = SCALE_X(&metrics, p2->m_fLongitude);
	gdouble fY2 = SCALE_Y(&metrics, p2->m_fLongitude);

	gdouble fDeltaX = fX2 - fX1;
	gdouble fDeltaY = fY2 - fY1;

	gdouble d = sqrt((fDeltaX*fDeltaX) + (fDeltaY*fDeltaY));
//	g_print("%f\n", d);
	return d;
}

gboolean map_points_equal(mappoint_t* p1, mappoint_t* p2)
{
	return( p1->m_fLatitude == p2->m_fLatitude && p1->m_fLongitude == p2->m_fLongitude);
}

void map_add_track(map_t* pMap, gint hTrack)
{
	g_array_append_val(pMap->m_pTracksArray, hTrack);
}

// ========================================================
//  Hit Testing
// ========================================================

// XXX: perhaps make map_hit_test return a more complex structure indicating what type of hit it is?
gboolean map_hit_test(map_t* pMap, mappoint_t* pMapPoint, gchar** ppReturnString)
{
	// Test things in the REVERSE order they are drawn (otherwise we'll match things that have been painted-over)
	gint i;
	for(i=NUM_ELEMS(layerdraworder)-1 ; i>=0 ; i--) {
		gint nLayer = layerdraworder[i].nLayer;

		// use width from whichever layer it's wider in
		gdouble fLineWidth = max(g_aLayers[nLayer]->m_Style.m_aSubLayers[0].m_afLineWidths[pMap->m_uZoomLevel-1],
					 g_aLayers[nLayer]->m_Style.m_aSubLayers[1].m_afLineWidths[pMap->m_uZoomLevel-1]);

#define EXTRA_CLICKABLE_ROAD_IN_PIXELS	(8)

		// make thin roads a little easier to hit
		fLineWidth = max(fLineWidth, MIN_ROAD_HIT_TARGET_WIDTH);

		// XXX: hack, map_pixels should really take a floating point instead.
		gdouble fMaxDistance = map_pixels_to_degrees(pMap, 1, pMap->m_uZoomLevel) * ((fLineWidth/2) + EXTRA_CLICKABLE_ROAD_IN_PIXELS);  // half width on each side

		if(map_hit_test_layer_roads(pMap->m_apLayerData[nLayer]->m_pRoadsArray, fMaxDistance, pMapPoint, ppReturnString)) {
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean map_hit_test_layer_roads(GPtrArray* pRoadsArray, gdouble fMaxDistance, mappoint_t* pHitPoint, gchar** ppReturnString)
{
	g_assert(ppReturnString != NULL);
	g_assert(*ppReturnString == NULL);	// pointer to null pointer

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
		if(pRoad->m_pPointsArray->len < 2) continue;

		// start on 1 so we can do -1 trick below
		gint iPoint;
		for(iPoint=1 ; iPoint<pRoad->m_pPointsArray->len ; iPoint++) {
			mappoint_t* pPoint1 = g_ptr_array_index(pRoad->m_pPointsArray, iPoint-1);
			mappoint_t* pPoint2 = g_ptr_array_index(pRoad->m_pPointsArray, iPoint);

			mappoint_t pointClosest;

			// hit test this line
			if(map_hit_test_line(pPoint1, pPoint2, pHitPoint, fMaxDistance, &pointClosest)) {
				// got a hit
				if(pRoad->m_pszName[0] == '\0') {
					*ppReturnString = g_strdup("<i>unnamed road</i>");
				}
				else {
					ESide eSide = map_side_test_line(pPoint1, pPoint2, &pointClosest, pHitPoint);

					gint nAddressStart;
					gint nAddressEnd;
					if(eSide == SIDE_LEFT) {
						nAddressStart = pRoad->m_nAddressLeftStart;
						nAddressEnd = pRoad->m_nAddressLeftEnd;
					}
					else {
						nAddressStart = pRoad->m_nAddressRightStart;
						nAddressEnd = pRoad->m_nAddressRightEnd;
					}

					if(nAddressStart == 0 || nAddressEnd == 0) {
						*ppReturnString = g_strdup_printf("%s", pRoad->m_pszName);
					}
					else {
						gint nMinAddres = min(nAddressStart, nAddressEnd);
						gint nMaxAddres = max(nAddressStart, nAddressEnd);

						*ppReturnString = g_strdup_printf("%s <b>#%d-%d</b>", pRoad->m_pszName, nMinAddres, nMaxAddres);
					}
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}

// Does the given point come close enough to the line segment to be considered a hit?
static gboolean map_hit_test_line(mappoint_t* pPoint1, mappoint_t* pPoint2, mappoint_t* pHitPoint, gdouble fMaxDistance, mappoint_t* pReturnClosestPoint)
{
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
	v.m_fLatitude = pPoint2->m_fLatitude - pPoint1->m_fLatitude;	// 10->90 becomes 0->80 (just store 80)
	v.m_fLongitude = pPoint2->m_fLongitude - pPoint1->m_fLongitude;

	gdouble fLengthV = sqrt((v.m_fLatitude*v.m_fLatitude) + (v.m_fLongitude*v.m_fLongitude));
	if(fLengthV == 0.0) return FALSE;	// bad data: a line segment with no length?

	//
	// 2. Make a unit vector out of v (meaning same direction but length=1) by dividing v by v's length
	//
	mappoint_t unitv;
	unitv.m_fLatitude = v.m_fLatitude / fLengthV;
	unitv.m_fLongitude = v.m_fLongitude / fLengthV;	// unitv is now a unit (=1.0) length v

	//
	// 3. Translate the hitpoint in the same way we translated v
	//
	mappoint_t u;
	u.m_fLatitude = pHitPoint->m_fLatitude - pPoint1->m_fLatitude;
	u.m_fLongitude = pHitPoint->m_fLongitude - pPoint1->m_fLongitude;

	//
	// 4. Use the dot product of (unitv) and (u) to find (a), the point along (v) that is closest to (u). see diagram above.
	//
	gdouble fLengthAlongV = (unitv.m_fLatitude * u.m_fLatitude) + (unitv.m_fLongitude * u.m_fLongitude);

	// Does it fall along the length of the line *segment* v?  (we know it falls along the infinite line v, but that does us no good.)
	// (This produces false negatives on round/butt end caps, but that's better that a false positive when another line is actually there!)
	if(fLengthAlongV > 0 && fLengthAlongV < fLengthV) {
		mappoint_t a;
		a.m_fLatitude = v.m_fLatitude * (fLengthAlongV / fLengthV);	// multiply each component by the percentage
		a.m_fLongitude = v.m_fLongitude * (fLengthAlongV / fLengthV);
		// NOTE: (a) is *not* where it actually hit on the *map*.  don't draw this point!  we'd have to translate it back away from the origin.

		//
		// 5. Calculate the distance from the end of (u) to (a).  If it's less than the fMaxDistance, it's a hit.
		//
		gdouble fRise = u.m_fLatitude - a.m_fLatitude;
		gdouble fRun = u.m_fLongitude - a.m_fLongitude;
		gdouble fDistanceSquared = fRise*fRise + fRun*fRun;	// compare squared distances. same results but without the sqrt.

		if(fDistanceSquared <= (fMaxDistance*fMaxDistance)) {
			/* debug aids */
			/* g_print("pPoint1 (%f,%f)\n", pPoint1->m_fLatitude, pPoint1->m_fLongitude);       */
			/* g_print("pPoint2 (%f,%f)\n", pPoint2->m_fLatitude, pPoint2->m_fLongitude);       */
			/* g_print("pHitPoint (%f,%f)\n", pHitPoint->m_fLatitude, pHitPoint->m_fLongitude); */
			/* g_print("v (%f,%f)\n", v.m_fLatitude, v.m_fLongitude);                           */
			/* g_print("u (%f,%f)\n", u.m_fLatitude, u.m_fLongitude);                           */
			/* g_print("unitv (%f,%f)\n", unitv.m_fLatitude, unitv.m_fLongitude);               */
			/* g_print("fDotProduct = %f\n", fDotProduct);                                      */
			/* g_print("a (%f,%f)\n", a.m_fLatitude, a.m_fLongitude);                           */
			/* g_print("fDistance = %f\n", sqrt(fDistanceSquared));                             */
			if(pReturnClosestPoint) {
				pReturnClosestPoint->m_fLatitude = a.m_fLatitude + pPoint1->m_fLatitude;
				pReturnClosestPoint->m_fLongitude = a.m_fLongitude + pPoint1->m_fLongitude;
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
	v.m_fLatitude = (pPoint2->m_fLongitude - pPoint1->m_fLongitude);	// NOTE: swapping lat and lon to make perpendicular
	v.m_fLongitude = -(pPoint2->m_fLatitude - pPoint1->m_fLatitude);

	// make a translated-to-origin vector of the line from closest point to hitpoint
	mappoint_t u;
	u.m_fLatitude = pHitPoint->m_fLatitude - pClosestPointOnLine->m_fLatitude;
	u.m_fLongitude = pHitPoint->m_fLongitude - pClosestPointOnLine->m_fLongitude;

	// figure out the signs of the elements of the vectors
	gboolean bULatPositive = (u.m_fLatitude >= 0);
	gboolean bULonPositive = (u.m_fLongitude >= 0);
	gboolean bVLatPositive = (v.m_fLatitude >= 0);
	gboolean bVLonPositive = (v.m_fLongitude >= 0);

	//g_print("%s,%s %s,%s\n", bVLonPositive?"Y":"N", bVLatPositive?"Y":"N", bULonPositive?"Y":"N", bULatPositive?"Y":"N");

	// if the signs agree, it's to the left, otherwise to the right
	if(bULatPositive == bVLatPositive && bULonPositive == bVLonPositive) {
		return SIDE_LEFT;
	}
	else {
		// let's check our algorithm: if the signs aren't both the same, they should be both opposite
		// ...unless the vector from hitpoint to line center is 0, which doesn't have sign

		//g_print("%f,%f %f,%f\n", u.m_fLatitude, u.m_fLongitude, v.m_fLatitude, v.m_fLongitude);
		g_assert(bULatPositive != bVLatPositive || u.m_fLatitude == 0.0);
		g_assert(bULonPositive != bVLonPositive || u.m_fLongitude == 0.0);
		return SIDE_RIGHT;
	}
}

gboolean map_can_zoom_in(map_t* pMap)
{
	// can we increase zoom level?
	return (pMap->m_uZoomLevel < MAX_ZOOMLEVEL);
}
gboolean map_can_zoom_out(map_t* pMap)
{
	return (pMap->m_uZoomLevel > MIN_ZOOMLEVEL);
}


