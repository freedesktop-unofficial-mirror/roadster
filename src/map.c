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

//#define THREADED_RENDERING

// #ifdef THREADED_RENDERING
// #define RENDERING_THREAD_YIELD          g_thread_yield()
// #else
#define RENDERING_THREAD_YIELD
// #endif

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

// ADD:
// 'Mal' - ?
// 'Trce - Trace

/* Prototypes */

static gboolean map_data_load(map_t* pMap, maprect_t* pRect);
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
	{   40000, ""},		// 6
	{   20000, ""}, 	// 7

	{   10000, ""},		// 8
	{    4000, ""},		// 9
	{    1800, ""},		// 10
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

//         pMap->m_pDataMutex = g_mutex_new();
//         pMap->m_pPixmapMutex = g_mutex_new();

	pMap->m_pTargetWidget = pTargetWidget;

	scenemanager_new(&(pMap->m_pSceneManager));
	g_assert(pMap->m_pSceneManager);

	pMap->m_uZoomLevel = 8;

	// init containers for geometry data
	gint i;
	for(i=0 ; i<NUM_ELEMS(pMap->m_apLayerData) ; i++) {
		maplayer_data_t* pLayer = g_new0(maplayer_data_t, 1);
		pLayer->m_pPointStringsArray = g_ptr_array_new();
		pMap->m_apLayerData[i] = pLayer;
	}

	// save it
	*ppMap = pMap;
	return TRUE;
}

//         pointstring_t* pTrackPointString = track_get_pointstring(g_MainWindow.m_nCurrentGPSPath);
//         if(pTrackPointString) {
//                 map_draw_gps_trail(pCairoInstance, pTrackPointString);
//         }


#define	RENDERMODE_FAST 	1
#define	RENDERMODE_PRETTY 	2

void map_draw(map_t* pMap, gint nDrawFlags)
{
	g_assert(pMap != NULL);

	scenemanager_clear(pMap->m_pSceneManager);

	// Get render metrics
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	//
	// Load geometry
	//
	TIMER_BEGIN(loadtimer, "--- BEGIN ALL DB LOAD");
	map_data_load(pMap, &(pRenderMetrics->m_rWorldBoundingBox));
//	locationset_load_locations(&(pRenderMetrics->m_rWorldBoundingBox));
	TIMER_END(loadtimer, "--- END ALL DB LOAD");

	gint nRenderMode = RENDERMODE_FAST;

	// XXX test
	GdkRectangle rect = {200,200,100,100};
	scenemanager_claim_rectangle(pMap->m_pSceneManager, &rect);

	if(nRenderMode == RENDERMODE_FAST) {
		if(nDrawFlags & DRAWFLAG_GEOMETRY) {
			map_draw_gdk(pMap, pRenderMetrics, pMap->m_pPixmap, DRAWFLAG_GEOMETRY);
		}
		if(nDrawFlags & DRAWFLAG_LABELS) {
			map_draw_cairo(pMap, pRenderMetrics, pMap->m_pPixmap, DRAWFLAG_LABELS);
		}
	}
	else {	// nRenderMode == RENDERMODE_PRETTY
		map_draw_cairo(pMap, pRenderMetrics, pMap->m_pPixmap, nDrawFlags);
	}
	
	// XXX test
	gdk_draw_rectangle(pMap->m_pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			FALSE, 200,200, 100, 100);

	gtk_widget_queue_draw(pMap->m_pTargetWidget);
}

// ========================================================
//  Get and release the map image
// ========================================================

GdkPixmap* map_get_pixmap(map_t* pMap)
{
//	g_mutex_lock(pMap->m_pPixmapMutex);
	return pMap->m_pPixmap;
}

void map_release_pixmap(map_t* pMap)
{
//	g_mutex_unlock(pMap->m_pPixmapMutex);
}


// ========================================================
//  Get/Set Functions
// ========================================================

void map_set_zoomlevel(map_t* pMap, guint16 uZoomLevel)
{
//         g_mutex_lock(pMap->m_pDataMutex);

	if(uZoomLevel > MAX_ZOOMLEVEL) uZoomLevel = MAX_ZOOMLEVEL;
	else if(uZoomLevel < MIN_ZOOMLEVEL) uZoomLevel = MIN_ZOOMLEVEL;

	if(uZoomLevel != pMap->m_uZoomLevel) {
		pMap->m_uZoomLevel = uZoomLevel;
//		map_set_redraw_needed(TRUE);
	}
//	g_mutex_unlock(pMap->m_pDataMutex);
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
static double map_pixels_to_degrees(map_t* pMap, gint16 nPixels, guint16 uZoomLevel)
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

//         g_mutex_lock(pMap->m_pDataMutex);
//         g_mutex_lock(pMap->m_pPixmapMutex);

	pMap->m_MapDimensions.m_uWidth = pDimensions->m_uWidth;
	pMap->m_MapDimensions.m_uHeight = pDimensions->m_uHeight;

	pMap->m_pPixmap = gdk_pixmap_new(
			pMap->m_pTargetWidget->window,
			pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight,
			-1);

//         g_mutex_unlock(pMap->m_pPixmapMutex);
//         g_mutex_unlock(pMap->m_pDataMutex);
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

/*
void map_draw(map_t* pMap, cairo_t *pCairo)
{
	// Get render metrics
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	scenemanager_clear(pMap->m_pSceneManager);

	//
	// Load geometry
	//
	TIMER_BEGIN(loadtimer, "--- BEGIN ALL DB LOAD");
	map_data_load(pMap, &(pRenderMetrics->m_rWorldBoundingBox));
	locationset_load_locations(&(pRenderMetrics->m_rWorldBoundingBox));
	TIMER_END(loadtimer, "--- END ALL DB LOAD");

//	const GPtrArray* pLocationSets = locationset_get_set_array();

	//
	// Draw map
	//

//         TIMER_BEGIN(loctimer, "\nBEGIN RENDER LOCATIONS");
//                 // Render Locations
//                 gint iLocationSet;
//                 for(iLocationSet=0 ; iLocationSet<pLocationSets->len ; iLocationSet++) {
//                         RENDERING_THREAD_YIELD;
//
//                         locationset_t* pLocationSet = g_ptr_array_index(pLocationSets, iLocationSet);
//                         map_draw_layer_points(pMap, pCairo, pRenderMetrics, pLocationSet->m_pLocationsArray);
//                 }
//         TIMER_END(loctimer, "END RENDER LOCATIONS");

//         map_draw_crosshair(pMap, pCairo, pRenderMetrics);

	cairo_restore(pCairo);

	// We don't need another redraw until something changes
//	map_set_redraw_needed(FALSE);
}
*/

static gboolean map_data_load(map_t* pMap, maprect_t* pRect)
{
	g_return_val_if_fail(pRect != NULL, FALSE);

	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;

	gint nZoomLevel = map_get_zoomlevel(pMap);

	map_data_clear(pMap);

	TIMER_BEGIN(mytimer, "BEGIN Geometry LOAD");

	// HACKY: make a list of layer IDs "2,3,5,6"
//         gchar azLayerNumberList[200] = {0};
//         gint nActiveLayerCount = 0;
//         gint i;
//         for(i=LAYER_FIRST ; i <= LAYER_LAST ;i++) {
//                 if(g_aLayers[i]->m_Style.m_aSubLayers[0].m_afLineWidths[nZoomLevel-1] != 0.0 ||
//                    g_aLayers[i]->m_Style.m_aSubLayers[1].m_afLineWidths[nZoomLevel-1] != 0.0)
//                 {
//                         gchar azLayerNumber[10];
//
//                         if(nActiveLayerCount > 0) g_snprintf(azLayerNumber, 10, ",%d", i);
//                         else g_snprintf(azLayerNumber, 10, "%d", i);
//
//                         g_strlcat(azLayerNumberList, azLayerNumber, 200);
//                         nActiveLayerCount++;
//                 }
//         }
//         if(nActiveLayerCount == 0) {
//                 g_print("no visible layers!\n");
//                 return TRUE;
//         }

	// MySQL doesn't optimize away GeomFromText(...) and instead executes this ONCE PER ROW.
	// That's a whole lot of parsing and was causing my_strtod to eat up 9% of Roadster's CPU time.
	// Assinging it to a temp variable alleviates that problem.

	gchar* pszSQL;
	pszSQL = g_strdup_printf("SET @wkb=GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))')",
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude, 	// upper left
		pRect->m_A.m_fLatitude, pRect->m_B.m_fLongitude, 	// upper right
		pRect->m_B.m_fLatitude, pRect->m_B.m_fLongitude, 	// bottom right
		pRect->m_B.m_fLatitude, pRect->m_A.m_fLongitude, 	// bottom left
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude		// upper left again
		);
	db_query(pszSQL, NULL);
	g_free(pszSQL);

	// generate SQL
	pszSQL = g_strdup_printf(
		"SELECT Road.ID, Road.TypeID, AsBinary(Road.Coordinates), RoadName.Name, RoadName.SuffixID"
		" FROM Road "
		" LEFT JOIN RoadName ON (Road.RoadNameID=RoadName.ID)"
		" WHERE"
//		" TypeID IN (%s) AND"
		" MBRIntersects(@wkb, Coordinates)"
//		azLayerNumberList,
		);
	//g_print("sql: %s\n", pszSQL);

	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);

	TIMER_SHOW(mytimer, "after query");

	guint32 uRowCount = 0;
	if(pResultSet) {
		TIMER_SHOW(mytimer, "after clear layers");
		while((aRow = db_fetch_row(pResultSet))) {
			RENDERING_THREAD_YIELD;

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
			db_parse_wkb_pointstring(aRow[2], pNewPointString, point_alloc);

			// Build name by adding suffix, if one is present
			gchar azFullName[100] = "";

			// does it have a name?			
			if(aRow[3] != NULL && aRow[4] != NULL) {
				gint nSuffixID = atoi(aRow[4]);
				const gchar* pszSuffix = road_suffix_itoa(nSuffixID, ROAD_SUFFIX_LENGTH_SHORT);
				g_snprintf(azFullName, 100, "%s%s%s",
					aRow[3], (pszSuffix[0] != '\0') ? " " : "", pszSuffix);
			}
			pNewPointString->m_pszName = g_strdup(azFullName);

			// Add this item to layer's list of pointstrings
			g_ptr_array_add(
				pMap->m_apLayerData[nTypeID]->m_pPointStringsArray, pNewPointString);
		} // end while loop on rows
		g_print("[%d rows]\n", uRowCount);
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

		// Free each pointstring
		for(j = (pLayerData->m_pPointStringsArray->len - 1) ; j>=0 ; j--) {
			RENDERING_THREAD_YIELD;

			pointstring_t* pPointString = g_ptr_array_remove_index_fast(pLayerData->m_pPointStringsArray, j);
			pointstring_free(pPointString);
		}
	}
}

double map_get_distance_in_meters(mappoint_t* pA, mappoint_t* pB)
{
//	g_assert_not_reached();	// unused/tested

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

#if ROADSTER_DEAD_CODE
/*

// ========================================================
//  Redraw
// ========================================================

void map_set_redraw_needed(gboolean bNeeded)
{
	pMap->m_bRedrawNeeded = bNeeded;
}

gboolean map_get_redraw_needed()
{
	return pMap->m_bRedrawNeeded;
}

gpointer map_draw_thread(gpointer);

void map_draw_thread_begin(map_t* pMap, GtkWidget* pTargetWidget)
{
#ifdef THREADED_RENDERING
	g_thread_create(map_draw_thread, pMap, FALSE, NULL);
#else
	map_draw_thread(pMap);
#endif
}

#include <gdk/gdk.h>

gpointer map_draw_thread(gpointer pData)
{
g_print("THREAD: begin\n");
	map_t* pMap = (map_t*)pData;
	g_assert(pMap != NULL);

//	db_lock();

#ifdef THREADED_RENDERING
	db_begin_thread();	// database needs to know we're a new thread
#endif

	g_mutex_lock(pMap->m_pDataMutex);

#ifdef THREADED_RENDERING
        gdk_threads_enter();
#endif
		// create pixel buffer of appropriate size
		GdkPixmap* pPixmapTemp = gdk_pixmap_new(pMap->m_pTargetWidget->window, pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight, -1);
		g_assert(pPixmapTemp);
	
		Display* dpy;
		Drawable drawable;
		dpy = gdk_x11_drawable_get_xdisplay(pPixmapTemp);
		drawable = gdk_x11_drawable_get_xid(pPixmapTemp);

g_print("THREAD: creating cairo...\n");
	cairo_t* pCairo = cairo_create ();

g_print("THREAD: calling cairo_set_target_drawable...\n");
	// draw on the off-screen buffer
	cairo_set_target_drawable(pCairo, dpy, drawable);
#ifdef THREADED_RENDERING
        gdk_threads_leave();
#endif

g_print("THREAD: drawing...\n");
	map_draw(pMap, pCairo);

g_print("THREAD: destroying cairo...\n");
#ifdef THREADED_RENDERING
        gdk_threads_enter();
#endif
	cairo_destroy(pCairo);
#ifdef THREADED_RENDERING
        gdk_threads_leave();
#endif

	// Copy final image to (pMap->m_pPixmap)
g_print("THREAD: copying pixmap\n");
	g_mutex_lock(pMap->m_pPixmapMutex);

#ifdef THREADED_RENDERING
        gdk_threads_enter();
#endif
		gdk_draw_pixmap(pMap->m_pPixmap,
		  pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
		  pPixmapTemp,
		  0, 0,
		  0, 0,
		  pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight);
		gdk_pixmap_unref(pPixmapTemp);
#ifdef THREADED_RENDERING
        gdk_threads_leave();
#endif

	g_mutex_unlock(pMap->m_pDataMutex);
	g_mutex_unlock(pMap->m_pPixmapMutex);
//	db_unlock();

g_print("THREAD: done drawing\n");
#ifdef THREADED_RENDERING
        gdk_threads_enter();
#endif
	gtk_widget_queue_draw(pMap->m_pTargetWidget);
#ifdef THREADED_RENDERING
        gdk_threads_leave();
	db_end_thread();
#endif
}

	TIMER_BEGIN(gdktimer, "starting gdk");
	gint i;

	for(i=0 ; i<500 ; i++) {
	GdkPoint points[5];
	points[0].x = random() % 10000;
	points[0].y = random() % 10000;
	points[1].x = random() % 10000;
	points[1].y = random() % 10000;
	points[2].x = random() % 10000;
	points[2].y = random() % 10000;
	points[3].x = random() % 10000;
	points[3].y = random() % 10000;
	points[4].x = random() % 10000;
	points[4].y = random() % 10000;

		GdkColor clr;
		clr.red = clr.green = clr.blue = 45535;
		gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);
		
		gdk_gc_set_line_attributes(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
                                           12,GDK_LINE_SOLID,GDK_CAP_ROUND,GDK_JOIN_MITER);
		gdk_draw_lines(pPixmapTemp, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			//TRUE,
			points, 5);

		gdk_gc_set_line_attributes(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
					   9,GDK_LINE_SOLID,GDK_CAP_ROUND,GDK_JOIN_MITER);

		clr.red = clr.green = clr.blue = 65535;
		gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);
		gdk_draw_lines(pPixmapTemp, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			//TRUE,
			points, 5);
	}
	TIMER_END(gdktimer, "ending gdk");
*/
#endif /* ROADSTER_DEAD_CODE */
