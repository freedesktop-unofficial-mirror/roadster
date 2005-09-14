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
#include "point.h"
#include "locationset.h"
#include "location.h"
#include "scenemanager.h"

#define ENABLE_RIVER_TO_LAKE_LOADTIME_HACK	// change circular rivers to lakes when loading from disk
//#define ENABLE_SCENEMANAGER_DEBUG_TEST
//#define ENABLE_LABELS_WHILE_DRAGGING

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

static void map_data_clear(map_t* pMap);
void map_get_render_metrics(map_t* pMap, rendermetrics_t* pMetrics);

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
	pMap->m_pTracksArray = g_array_new(FALSE, /* not zero-terminated */
					  TRUE,  /* clear to 0 (?) */
					  sizeof(gint));

	map_init_location_hash(pMap);

	pMap->m_pTargetWidget = pTargetWidget;

	scenemanager_new(&(pMap->m_pSceneManager));
	g_assert(pMap->m_pSceneManager);

	pMap->m_uZoomLevel = 1;		// XXX: better way to init this?  or should we force the GUI to reflect this #?

	// init containers for geometry data
	gint i;
	for(i=0 ; i<G_N_ELEMENTS(pMap->m_apLayerData) ; i++) {
		maplayer_data_t* pLayer = g_new0(maplayer_data_t, 1);
		pLayer->m_pRoadsArray = g_ptr_array_new();
		pMap->m_apLayerData[i] = pLayer;
	}

	// init POI selection
	pMap->m_pLocationSelectionArray = g_ptr_array_new();
	pMap->m_pLocationSelectionAllocator = g_free_list_new(sizeof(locationselection_t), 100);

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
	map_data_load_tiles(pMap, &(pRenderMetrics->m_rWorldBoundingBox));
	TIMER_END(loadtimer, "--- END ALL DB LOAD");

	scenemanager_clear(pMap->m_pSceneManager);
	scenemanager_set_screen_dimensions(pMap->m_pSceneManager, pRenderMetrics->m_nWindowWidth, pRenderMetrics->m_nWindowHeight);

	gint nRenderMode = RENDERMODE_FAST; //RENDERMODE_FAST; // RENDERMODE_PRETTY

#ifdef ENABLE_LABELS_WHILE_DRAGGING
	nDrawFlags |= DRAWFLAG_LABELS;	// always turn on labels
#endif

#ifdef ENABLE_SCENEMANAGER_DEBUG_TEST
	GdkPoint aPoints[] = {{200,150},{250,200},{200,250},{150,200}};
	scenemanager_claim_polygon(pMap->m_pSceneManager, aPoints, 4);
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
	gdk_draw_polygon(pTargetPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
					 FALSE, aPoints, 4);
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
	if(uZoomLevel > MAX_ZOOM_LEVEL) uZoomLevel = MAX_ZOOM_LEVEL;
	else if(uZoomLevel < MIN_ZOOM_LEVEL) uZoomLevel = MIN_ZOOM_LEVEL;

	if(uZoomLevel != pMap->m_uZoomLevel) {
		pMap->m_uZoomLevel = uZoomLevel;
//		map_set_redraw_needed(TRUE);
	}
}

guint16 map_get_zoomlevel(const map_t* pMap)
{
	return pMap->m_uZoomLevel;	// between MIN_ZOOM_LEVEL and MAX_ZOOM_LEVEL
}

guint32 map_get_zoomlevel_scale(const map_t* pMap)
{
	return g_sZoomLevels[pMap->m_uZoomLevel].m_uScale;	// returns "5000" for 1:5000 scale
}

gboolean map_can_zoom_in(const map_t* pMap)
{
	// can we increase zoom level?
	return (pMap->m_uZoomLevel < MAX_ZOOM_LEVEL);
}
gboolean map_can_zoom_out(const map_t* pMap)
{
	return (pMap->m_uZoomLevel > MIN_ZOOM_LEVEL);
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
	double fMetersOfWorld = ((float)g_sZoomLevels[uZoomLevel].m_uScale) * fMetersOfPixels;

	return WORLD_METERS_TO_DEGREES(fMetersOfWorld);
}

double map_degrees_to_pixels(map_t* pMap, gdouble fDegrees, guint16 uZoomLevel)
{
	double fMonitorPixelsPerInch = 85.333;	// XXX: don't hardcode this

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

void map_get_centerpoint(const map_t* pMap, mappoint_t* pReturnPoint)
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

	// XXX: free old pixmap?
	//g_assert(pMap->m_pPixmap == NULL);

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

	if(fLatStart > pRect->m_A.m_fLatitude) {
		g_print("fLatStart %f > pRect->m_A.m_fLatitude %f\n", fLatStart, pRect->m_A.m_fLatitude);
		g_assert(fLatStart <= pRect->m_A.m_fLatitude);
	}
	if(fLonStart > pRect->m_A.m_fLongitude) {
		g_print("fLonStart %f > pRect->m_A.m_fLongitude %f!!\n", fLonStart, pRect->m_A.m_fLongitude);
		g_assert_not_reached();
	}

	gint nLat,nLon;
	for(nLat = 0 ; nLat < nLatNumTiles ; nLat++) {
		for(nLon = 0 ; nLon < nLonNumTiles ; nLon++) {

			maprect_t rect;
			rect.m_A.m_fLatitude = fLatStart + ((gdouble)(nLat) * MAP_TILE_WIDTH);
			rect.m_A.m_fLongitude = fLonStart + ((gdouble)(nLon) * MAP_TILE_WIDTH);
			rect.m_B.m_fLatitude = fLatStart + ((gdouble)(nLat+1) * MAP_TILE_WIDTH);
			rect.m_B.m_fLongitude = fLonStart + ((gdouble)(nLon+1) * MAP_TILE_WIDTH);

			map_data_load_geometry(pMap, &rect);
			map_data_load_locations(pMap, &rect);
		}
	}
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
		//" TypeID IN (%s) AND"
                //" MBRIntersects(@wkb, Coordinates)"
		" MBRIntersects(GeomFromText('Polygon((%s %s,%s %s,%s %s,%s %s,%s %s))'), Coordinates)"
		,
		g_ascii_dtostr(azCoord1, 20, pRect->m_A.m_fLatitude), g_ascii_dtostr(azCoord2, 20, pRect->m_A.m_fLongitude), 
		g_ascii_dtostr(azCoord3, 20, pRect->m_A.m_fLatitude), g_ascii_dtostr(azCoord4, 20, pRect->m_B.m_fLongitude), 
		g_ascii_dtostr(azCoord5, 20, pRect->m_B.m_fLatitude), g_ascii_dtostr(azCoord6, 20, pRect->m_B.m_fLongitude), 
		g_ascii_dtostr(azCoord7, 20, pRect->m_B.m_fLatitude), g_ascii_dtostr(azCoord8, 20, pRect->m_A.m_fLongitude), 
		azCoord1, azCoord2);

//         pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude,    // upper left
//         pRect->m_A.m_fLatitude, pRect->m_B.m_fLongitude,    // upper right
//         pRect->m_B.m_fLatitude, pRect->m_B.m_fLongitude,    // bottom right
//         pRect->m_B.m_fLatitude, pRect->m_A.m_fLongitude,    // bottom left
//         pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude     // upper left again
//         );
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
			if(nTypeID < MAP_OBJECT_TYPE_FIRST || nTypeID > MAP_OBJECT_TYPE_LAST) {
				g_warning("geometry record '%s' has bad type '%s'\n", aRow[0], aRow[1]);
				continue;
			}

			if(nTypeID == 12) g_warning("(got a 12)");
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

#ifdef ENABLE_RIVER_TO_LAKE_LOADTIME_HACK
			if(nTypeID == MAP_OBJECT_TYPE_RIVER) {
				mappoint_t* pPointA = g_ptr_array_index(pNewRoad->m_pPointsArray, 0);
				mappoint_t* pPointB = g_ptr_array_index(pNewRoad->m_pPointsArray, pNewRoad->m_pPointsArray->len-1);

				if(pPointA->m_fLatitude == pPointB->m_fLatitude && pPointA->m_fLongitude == pPointB->m_fLongitude) {
					nTypeID = MAP_OBJECT_TYPE_LAKE;
				}
			}
#endif

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

static gboolean map_data_load_locations(map_t* pMap, maprect_t* pRect)
{
	g_return_val_if_fail(pMap != NULL, FALSE);
	g_return_val_if_fail(pRect != NULL, FALSE);

	if(map_get_zoomlevel(pMap) < MIN_ZOOM_LEVEL_FOR_LOCATIONS) {
		return TRUE;
	}

	TIMER_BEGIN(mytimer, "BEGIN Locations LOAD");

	// generate SQL
	gchar* pszSQL;
	pszSQL = g_strdup_printf(
		"SELECT Location.ID, Location.LocationSetID, AsBinary(Location.Coordinates), LocationAttributeValue_Name.Value"	// LocationAttributeValue_Name.Value is the "Name" field of this Location
		" FROM Location"
		" LEFT JOIN LocationAttributeValue AS LocationAttributeValue_Name ON (LocationAttributeValue_Name.LocationID=Location.ID AND LocationAttributeValue_Name.AttributeNameID=%d)"
		" WHERE"
		" MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)",
		LOCATION_ATTRIBUTE_ID_NAME,	// attribute ID for 'name'
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude, 	// upper left
		pRect->m_A.m_fLatitude, pRect->m_B.m_fLongitude, 	// upper right
		pRect->m_B.m_fLatitude, pRect->m_B.m_fLongitude, 	// bottom right
		pRect->m_B.m_fLatitude, pRect->m_A.m_fLongitude, 	// bottom left
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude		// upper left again
		);
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
			
			pNewLocation->m_nID = atoi(aRow[0]);

			// Parse coordinates
			db_parse_wkb_point(aRow[2], &(pNewLocation->m_Coordinates));

			// make a copy of the name field, or "" (never leave it == NULL)
			pNewLocation->m_pszName = g_strdup(aRow[3] != NULL ? aRow[3] : "");
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
	for(i=0 ; i<G_N_ELEMENTS(pMap->m_apLayerData) ; i++) {
		maplayer_data_t* pLayerData = pMap->m_apLayerData[i];

		// Free each
		for(j = (pLayerData->m_pRoadsArray->len - 1) ; j>=0 ; j--) {
			road_t* pRoad = g_ptr_array_remove_index_fast(pLayerData->m_pRoadsArray, j);
			road_free(pRoad);
		}
		g_assert(pLayerData->m_pRoadsArray->len == 0);
	}

	// Clear locations
	map_init_location_hash(pMap);
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
	gdouble fY1 = SCALE_Y(&metrics, p1->m_fLatitude);

	gdouble fX2 = SCALE_X(&metrics, p2->m_fLongitude);
	gdouble fY2 = SCALE_Y(&metrics, p2->m_fLatitude);

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

void map_hitstruct_free(map_t* pMap, maphit_t* pHitStruct)
{
	if(pHitStruct == NULL) return;

	// free type-specific stuff
	if(pHitStruct->m_eHitType == MAP_HITTYPE_URL) {
		g_free(pHitStruct->m_URLHit.m_pszURL);
	}

	// free common stuff
	g_free(pHitStruct->m_pszText);
	g_free(pHitStruct);
}

// XXX: perhaps make map_hit_test return a more complex structure indicating what type of hit it is?
gboolean map_hit_test(map_t* pMap, mappoint_t* pMapPoint, maphit_t** ppReturnStruct)
{
#if 0	// GGGGGGGGGGGGGGGG
	rendermetrics_t rendermetrics;
	map_get_render_metrics(pMap, &rendermetrics);

	if(map_hit_test_locationselections(pMap, &rendermetrics, pMap->m_pLocationSelectionArray, pMapPoint, ppReturnStruct)) {
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
		gdouble fLineWidth = max(g_aLayers[nLayer]->m_Style.m_aSubLayers[0].m_afLineWidths[pMap->m_uZoomLevel-1],
					 g_aLayers[nLayer]->m_Style.m_aSubLayers[1].m_afLineWidths[pMap->m_uZoomLevel-1]);

#define EXTRA_CLICKABLE_ROAD_IN_PIXELS	(3)

		// make thin roads a little easier to hit
		// fLineWidth = max(fLineWidth, MIN_ROAD_HIT_TARGET_WIDTH);

		// XXX: hack, map_pixels should really take a floating point instead.
		gdouble fMaxDistance = map_pixels_to_degrees(pMap, 1, pMap->m_uZoomLevel) * ((fLineWidth/2) + EXTRA_CLICKABLE_ROAD_IN_PIXELS);  // half width on each side

		if(map_hit_test_layer_roads(pMap->m_apLayerData[nLayer]->m_pRoadsArray, fMaxDistance, pMapPoint, ppReturnStruct)) {
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
		if(pRoad->m_pPointsArray->len < 2) continue;

		// start on 1 so we can do -1 trick below
		gint iPoint;
		for(iPoint=1 ; iPoint<pRoad->m_pPointsArray->len ; iPoint++) {
			mappoint_t* pPoint1 = g_ptr_array_index(pRoad->m_pPointsArray, iPoint-1);
			mappoint_t* pPoint2 = g_ptr_array_index(pRoad->m_pPointsArray, iPoint);

			mappoint_t pointClosest;
			gdouble fPercentAlongLine;

			// hit test this line
			if(map_hit_test_line(pPoint1, pPoint2, pHitPoint, fMaxDistance, &pointClosest, &fPercentAlongLine)) {
				//g_print("fPercentAlongLine = %f\n",fPercentAlongLine);

				// fill out a new maphit_t struct with details
				maphit_t* pHitStruct = g_new0(maphit_t, 1);
				pHitStruct->m_eHitType = MAP_HITTYPE_ROAD;

				if(pRoad->m_pszName[0] == '\0') {
					pHitStruct->m_pszText = g_strdup("<i>unnamed</i>");
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
						pHitStruct->m_pszText = g_strdup_printf("%s", pRoad->m_pszName);
					}
					else {
						gint nMinAddres = min(nAddressStart, nAddressEnd);
						gint nMaxAddres = max(nAddressStart, nAddressEnd);

						pHitStruct->m_pszText = g_strdup_printf("%s <b>#%d-%d</b>", pRoad->m_pszName, nMinAddres, nMaxAddres);
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
	if(pHitPoint->m_fLatitude < (pPoint1->m_fLatitude - fMaxDistance) && pHitPoint->m_fLatitude < (pPoint2->m_fLatitude - fMaxDistance)) return FALSE;
	if(pHitPoint->m_fLongitude < (pPoint1->m_fLongitude - fMaxDistance) && pHitPoint->m_fLongitude < (pPoint2->m_fLongitude - fMaxDistance)) return FALSE;

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

// hit test all locations
static gboolean map_hit_test_locationsets(map_t* pMap, rendermetrics_t* pRenderMetrics, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	gdouble fMaxDistance = map_pixels_to_degrees(pMap, 1, pMap->m_uZoomLevel) * 3;	// XXX: don't hardcode distance :)

	const GPtrArray* pLocationSetsArray = locationset_get_array();
	gint i;
	for(i=0 ; i<pLocationSetsArray->len ; i++) {
		locationset_t* pLocationSet = g_ptr_array_index(pLocationSetsArray, i);

		// the user is NOT trying to click on invisible things :)
		if(!locationset_is_visible(pLocationSet)) continue;

		// 2. Get array of Locations from the hash table using LocationSetID
		GPtrArray* pLocationsArray;
		pLocationsArray = g_hash_table_lookup(pMap->m_pLocationArrayHashTable, &(pLocationSet->m_nID));
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
		if(pLocation->m_Coordinates.m_fLatitude < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
		   || pLocation->m_Coordinates.m_fLongitude < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
		   || pLocation->m_Coordinates.m_fLatitude > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
		   || pLocation->m_Coordinates.m_fLongitude > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
		{
		    continue;   // not visible
		}

		gdouble fX1 = SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude);
		gdouble fY1 = SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude);
		gdouble fX2 = SCALE_X(pRenderMetrics, pHitPoint->m_fLongitude);
		gdouble fY2 = SCALE_Y(pRenderMetrics, pHitPoint->m_fLatitude);

		gdouble fDeltaX = fX2 - fX1;
		gdouble fDeltaY = fY2 - fY1;

		fDeltaX = abs(fDeltaX);
		fDeltaY = abs(fDeltaY);

		if(fDeltaX <= 3 && fDeltaY <= 3) {
			// fill out a new maphit_t struct with details
			maphit_t* pHitStruct = g_new0(maphit_t, 1);
			pHitStruct->m_eHitType = MAP_HITTYPE_LOCATION;
			pHitStruct->m_LocationHit.m_nLocationID = pLocation->m_nID;

			pHitStruct->m_LocationHit.m_Coordinates.m_fLatitude = pLocation->m_Coordinates.m_fLatitude;
			pHitStruct->m_LocationHit.m_Coordinates.m_fLongitude = pLocation->m_Coordinates.m_fLongitude;

			if(pLocation->m_pszName[0] == '\0') {
				pHitStruct->m_pszText = g_strdup_printf("<i>unnamed POI %d</i>", pLocation->m_nID);
			}
			else {
				pHitStruct->m_pszText = g_strdup(pLocation->m_pszName);
			}
			*ppReturnStruct = pHitStruct;
			return TRUE;
		}
	}
	return FALSE;
}

gboolean hit_test_screenpoint_in_rect(screenpoint_t* pPt, screenrect_t* pRect)
{
	return(pPt->m_nX >= pRect->m_A.m_nX && pPt->m_nX <= pRect->m_B.m_nX && pPt->m_nY >= pRect->m_A.m_nY && pPt->m_nY <= pRect->m_B.m_nY);
}

static gboolean map_hit_test_locationselections(map_t* pMap, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray, mappoint_t* pHitPoint, maphit_t** ppReturnStruct)
{
	screenpoint_t screenpoint;
	screenpoint.m_nX = (gint)SCALE_X(pRenderMetrics, pHitPoint->m_fLongitude);
	screenpoint.m_nY = (gint)SCALE_Y(pRenderMetrics, pHitPoint->m_fLatitude);

	gint i;
	for(i=(pLocationSelectionArray->len-1) ; i>=0 ; i--) {
		locationselection_t* pLocationSelection = g_ptr_array_index(pLocationSelectionArray, i);

		if(pLocationSelection->m_bVisible == FALSE) continue;

		if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->m_InfoBoxRect))) {
			// fill out a new maphit_t struct with details
			maphit_t* pHitStruct = g_new0(maphit_t, 1);

			if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->m_InfoBoxCloseRect))) {
				pHitStruct->m_eHitType = MAP_HITTYPE_LOCATIONSELECTION_CLOSE;
				pHitStruct->m_pszText = g_strdup("close");
				pHitStruct->m_LocationSelectionHit.m_nLocationID = pLocationSelection->m_nLocationID;
			}
			else if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->m_EditRect))) {
				pHitStruct->m_eHitType = MAP_HITTYPE_LOCATIONSELECTION_EDIT;
				pHitStruct->m_pszText = g_strdup("edit");
				pHitStruct->m_LocationSelectionHit.m_nLocationID = pLocationSelection->m_nLocationID;
			}
			else {
				gboolean bURLMatch = FALSE;

				gint iURL;
				for(iURL=0 ; iURL<pLocationSelection->m_nNumURLs ; iURL++) {
					if(hit_test_screenpoint_in_rect(&screenpoint, &(pLocationSelection->m_aURLs[iURL].m_Rect))) {
						pHitStruct->m_eHitType = MAP_HITTYPE_URL;
						pHitStruct->m_pszText = g_strdup("click to open location");
						pHitStruct->m_URLHit.m_pszURL = g_strdup(pLocationSelection->m_aURLs[iURL].m_pszURL);

						bURLMatch = TRUE;
						break;
					}
				}

				// no url match, just return a generic "hit the locationselection box"
				if(!bURLMatch) {
					pHitStruct->m_eHitType = MAP_HITTYPE_LOCATIONSELECTION;
					pHitStruct->m_pszText = g_strdup("");
					pHitStruct->m_LocationSelectionHit.m_nLocationID = pLocationSelection->m_nLocationID;
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
	for(i=0 ; i<pLocationSelection->m_pAttributesArray->len ; i++) {
		locationattribute_t* pAttribute = g_ptr_array_index(pLocationSelection->m_pAttributesArray, i);
		if(strcmp(pAttribute->m_pszName, pszAttributeName) == 0) {
			return pAttribute->m_pszValue;
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
	gchar* g_apszMapRenderTypeNames[] = {"lines", "polygons", "line-labels", "polygon-labels", "fill"};

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
	if(pMap->m_pLocationArrayHashTable != NULL) {
		g_hash_table_destroy(pMap->m_pLocationArrayHashTable);
	}
	pMap->m_pLocationArrayHashTable = g_hash_table_new_full(g_int_hash, g_int_equal, 
								g_free, 				/* key destroy function */
								map_callback_free_locations_array);	/* value destroy function */
}

static void map_store_location(map_t* pMap, location_t* pLocation, gint nLocationSetID)
{
	GPtrArray* pLocationsArray;
	pLocationsArray = g_hash_table_lookup(pMap->m_pLocationArrayHashTable, &nLocationSetID);
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
		g_hash_table_insert(pMap->m_pLocationArrayHashTable, pKey, pLocationsArray);
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
	for(i=0 ; i<pMap->m_pLocationSelectionArray->len ; i++) {
		locationselection_t* pSel = g_ptr_array_index(pMap->m_pLocationSelectionArray, i);
		if(pSel->m_nLocationID == nLocationID) return pSel;
	}
	return NULL;
}

gint map_location_selection_latitude_sort_callback(gconstpointer ppA, gconstpointer ppB)
{
	locationselection_t* pA = *((locationselection_t**)ppA);
	locationselection_t* pB = *((locationselection_t**)ppB);

	// we want them ordered from greatest latitude to smallest
	return ((pA->m_Coordinates.m_fLatitude > pB->m_Coordinates.m_fLatitude) ? -1 : 1);
}

// add a Location to the selected list
gboolean map_location_selection_add(map_t* pMap, gint nLocationID)
{
	if(map_location_selection_get(pMap, nLocationID) != NULL) {
		// already here
		return FALSE;
	}

	// create a new locationselection_t and initialize it
	locationselection_t* pNew = g_free_list_alloc(pMap->m_pLocationSelectionAllocator);

	pNew->m_nLocationID = nLocationID;
	pNew->m_pAttributesArray = g_ptr_array_new();

	// load all attributes
	location_load(nLocationID, &(pNew->m_Coordinates), NULL);
	location_load_attributes(nLocationID, pNew->m_pAttributesArray);

	g_ptr_array_add(pMap->m_pLocationSelectionArray, pNew);

	// sort in order of latitude
	// POI that seem "closer" to the user (lower on the screen) will be drawn last (on top)
	g_ptr_array_sort(pMap->m_pLocationSelectionArray, map_location_selection_latitude_sort_callback);

	return TRUE;	// added
}

// remove a Location to the selected list
gboolean map_location_selection_remove(map_t* pMap, gint nLocationID)
{
	gint i;
	for(i=0 ; i<pMap->m_pLocationSelectionArray->len ; i++) {
		locationselection_t* pSel = g_ptr_array_index(pMap->m_pLocationSelectionArray, i);
		if(pSel->m_nLocationID == nLocationID) {
			g_ptr_array_remove_index(pMap->m_pLocationSelectionArray, i);
			return TRUE;
		}
	}
	return FALSE;	// removed
}
