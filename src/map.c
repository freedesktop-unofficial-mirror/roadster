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
#include <gtk/gtk.h>
#include <math.h>

#include "main.h"
#include "map_style.h"
#include "map_tilemanager.h"
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
// #define TILE_SHIFT          (1000.0)
// #define TILE_MODULUS            (23)
// #define TILE_WIDTH          (TILE_MODULUS / TILE_SHIFT)

#define MIN_ROAD_HIT_TARGET_WIDTH	(6)	// make super thin roads a bit easier to hover over/click, in pixels


/* Prototypes */
static void map_init_location_hash(map_t* pMap);

static void map_store_location(map_t* pMap, location_t* pLocation, gint nLocationSetID);

static void map_data_clear(map_t* pMap);
void map_get_render_metrics(const map_t* pMap, rendermetrics_t* pMetrics);

gdouble map_get_straight_line_distance_in_degrees(mappoint_t* p1, mappoint_t* p2);

// Each zoomlevel has a scale (XXX: this should really be in an XML file)
zoomlevel_t g_sZoomLevels[NUM_ZOOM_LEVELS] = {
	{150000000, UNIT_MILES,3000,UNIT_KILOMETERS,4000, 	1, 3},	// *1
	{123000000, UNIT_MILES,3000,UNIT_KILOMETERS,4000, 	1, 3},	// 2
	{ 96000000, UNIT_MILES,2000,UNIT_KILOMETERS,2000, 	1, 3},	// 3
	{ 69000000, UNIT_MILES,2000,UNIT_KILOMETERS,2000, 	1, 3},	// 4

	{ 42000000, UNIT_MILES,1000,UNIT_KILOMETERS,1000, 	1, 3},	// *5
	{ 35000000, UNIT_MILES,1000,UNIT_KILOMETERS,1000, 	1, 3},	// 6
	{ 28000000, UNIT_MILES,500,UNIT_KILOMETERS,500, 	1, 3},	// 7
	{ 21000000, UNIT_MILES,500,UNIT_KILOMETERS,500, 	1, 3},	// 8

	{ 14000000, UNIT_MILES,10,	UNIT_KILOMETERS,12, 	2, 2},	// *9
	{ 11600000, UNIT_MILES,10,	UNIT_KILOMETERS,12, 	2, 2},	// 10
	{  9200000, UNIT_MILES, 5,	UNIT_KILOMETERS, 7, 	2, 2},	// 11
	{  6800000, UNIT_MILES, 5,	UNIT_KILOMETERS, 7, 	2, 2},	// 12

	{  4400000, UNIT_MILES, 5,	UNIT_KILOMETERS, 7, 	3, 2},	// *13
	{  3850000, UNIT_MILES, 5,	UNIT_KILOMETERS, 7, 	3, 2},	// 14
	{  3300000, UNIT_MILES, 5,	UNIT_KILOMETERS, 7, 	3, 2},	// 15
	{  2750000, UNIT_MILES, 5,	UNIT_KILOMETERS, 7, 	3, 2},	// 16

	{  2200000, UNIT_MILES,20,	UNIT_KILOMETERS,15, 	4, 1},	// *17
	{  1832250, UNIT_MILES,20,	UNIT_KILOMETERS,15, 	4, 1},	// 18
	{  1464500, UNIT_MILES,20,	UNIT_KILOMETERS,15, 	4, 1},	// 19
	{  1100000, UNIT_MILES,20,	UNIT_KILOMETERS,15, 	4, 1},	// 20

	{   729000, UNIT_MILES,10,	UNIT_KILOMETERS, 8, 	5, 1},	// *21
	{   607500, UNIT_MILES,10,	UNIT_KILOMETERS, 8, 	5, 1},	// 22
	{   486000, UNIT_MILES,10,	UNIT_KILOMETERS, 8,		5, 1},	// 23
	{   364500, UNIT_MILES,10,	UNIT_KILOMETERS, 8,		5, 1},	// 24

	{   243000, UNIT_MILES, 5,	UNIT_KILOMETERS, 4,		6, 1},	// *25
	{   209750, UNIT_MILES, 5,	UNIT_KILOMETERS, 4, 	6, 1},	// 26
	{   176500, UNIT_MILES, 5,	UNIT_KILOMETERS, 4, 	6, 1},	// 27
	{  	143250, UNIT_MILES, 5,	UNIT_KILOMETERS, 4, 	6, 1},	// 28

	{   110000, UNIT_MILES, 2,	UNIT_KILOMETERS,2, 		7, 0},	// *29
	{    89250, UNIT_MILES, 2,	UNIT_KILOMETERS,2, 		7, 0},	// 30
	{    68500, UNIT_MILES, 2,	UNIT_KILOMETERS,2, 		7, 0},	// 31
	{    47750, UNIT_MILES, 2,	UNIT_KILOMETERS,2, 		7, 0},	// 32

	{    27000, UNIT_FEET, 2000,UNIT_METERS, 500, 		8, 0},	// *33	
	{    22500, UNIT_FEET, 2000,UNIT_METERS, 500, 		8, 0},	// 34
	{    18000, UNIT_FEET, 2000,UNIT_METERS, 500, 		8, 0},	// 35
	{    13500, UNIT_FEET, 2000,UNIT_METERS, 500, 		8, 0},	// 36

	{     9000, UNIT_FEET, 500,	UNIT_METERS, 200, 		9,  0},	// *37
	{     7500, UNIT_FEET, 500,	UNIT_METERS, 200, 		9,  0},	// 38
	{     6000, UNIT_FEET, 500,	UNIT_METERS, 200, 		9,  0},	// 39
	{     4500, UNIT_FEET, 500,	UNIT_METERS, 200, 		9,  0},	// 40

	{     3000, UNIT_FEET, 200,	UNIT_METERS, 50, 		10, 0},	// *41
};

#define MIN_ZOOMLEVEL_FOR_LOCATIONS	(6)

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

gchar* g_apszMapRenderTypeNames[] = {"lines", "polygons", "line-labels", "polygon-labels", "fill", "locations", "location-labels"};

// ========================================================
//  Init
// ========================================================

// init the module
void map_init(void)
{
	//g_print("*********************************** %f\n", WORLD_FEET_PER_DEGREE);
}

gboolean map_new(map_t** ppMap, GtkWidget* pTargetWidget)
{
	g_assert(ppMap != NULL);
	g_assert(*ppMap == NULL);	// must be a pointer to null pointer

	// create a new map struct
	map_t* pMap = g_new0(map_t, 1);

	// Create array of Track handles
//     pMap->pTracksArray = g_array_new(FALSE, /* not zero-terminated */
//                       TRUE,  /* clear to 0 (?) */
//                       sizeof(gint));

	map_init_location_hash(pMap);

	pMap->pTargetWidget = pTargetWidget;

	scenemanager_new(&(pMap->pSceneManager));
	g_assert(pMap->pSceneManager);

	pMap->uZoomLevel = 1;

	// init containers for geometry data
//     gint i;
//     for(i=0 ; i<G_N_ELEMENTS(pMap->apLayerData) ; i++) {
//         maplayer_data_t* pLayer = g_new0(maplayer_data_t, 1);
//         pLayer->pRoadsArray = g_ptr_array_new();
//         pMap->apLayerData[i] = pLayer;
//     }

	pMap->pTileManager = map_tilemanager_new();

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
//	map_data_clear(pMap);
	GPtrArray* pTileArray = map_tilemanager_load_tiles_for_worldrect(pMap->pTileManager, &(pRenderMetrics->rWorldBoundingBox), pRenderMetrics->nLevelOfDetail);
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
			map_draw_gdk(pMap, pTileArray, pRenderMetrics, pTargetPixmap, DRAWFLAG_GEOMETRY);
			nDrawFlags &= ~DRAWFLAG_GEOMETRY;
		}

		// Call cairo for finishing the scene
		map_draw_cairo(pMap, pTileArray, pRenderMetrics, pTargetPixmap, nDrawFlags);
	}
	else {	// nRenderMode == RENDERMODE_PRETTY
		map_draw_cairo(pMap, pTileArray, pRenderMetrics, pTargetPixmap, nDrawFlags);
	}

#ifdef ENABLE_SCENEMANAGER_DEBUG_TEST
	gdk_draw_polygon(pTargetPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
					 FALSE, aPoints, 4);
#endif

	gtk_widget_queue_draw(pMap->pTargetWidget);

	g_ptr_array_free(pTileArray, TRUE);
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

//	if(uZoomLevel != pMap->uZoomLevel) {
		pMap->uZoomLevel = uZoomLevel;
//		map_set_redraw_needed(TRUE);
//	}
}

guint16 map_get_zoomlevel(const map_t* pMap)
{
	return pMap->uZoomLevel;	// between MIN_ZOOM_LEVEL and MAX_ZOOM_LEVEL
}

guint32 map_get_zoomlevel_scale(const map_t* pMap)
{
	return g_sZoomLevels[pMap->uZoomLevel-1].uScale;	// returns "5000" for 1:5000 scale
}

gdouble map_get_altitude(const map_t* pMap, EDistanceUnits eUnit)
{
	g_warning("broken function :)\n");

	g_assert(eUnit == UNIT_MILES);

	// How high are we off the ground?

	// x = sqrt((SCALE*sqrt(1.25))^2  - (SCALE/2)^2)

//	gdouble fScale = (gdouble)(g_sZoomLevels[pMap->uZoomLevel-1].uScale);

	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);

	gdouble fBase = renderMetrics.fScreenLatitude;
	gdouble fEyeToTopOfScreen = sqrt( WORLD_FEET_TO_DEGREES(1) + 1 );

	gdouble fDistanceInDegrees = sqrt(((fBase * fEyeToTopOfScreen)*(fBase * fEyeToTopOfScreen)) - ((fBase/2.0)*(fBase/2.0)));

	return WORLD_DEGREES_TO_MILES(fDistanceInDegrees);
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

// Call this to pan around the map
void map_center_on_windowpoint(map_t* pMap, guint16 uX, guint16 uY)
{
	// Calculate the # of pixels away from the center point the click was
	gint16 nPixelDeltaX = uX - (pMap->MapDimensions.uWidth / 2);
	gint16 nPixelDeltaY = uY - (pMap->MapDimensions.uHeight / 2);

	// Convert pixels to world coordinates
	gdouble fWorldDeltaX = map_pixels_to_degrees(pMap, nPixelDeltaX, pMap->uZoomLevel);
	// reverse the X, clicking above
	gdouble fWorldDeltaY = -map_pixels_to_degrees(pMap, nPixelDeltaY, pMap->uZoomLevel);

//	g_message("panning %d,%d pixels (%.10f,%.10f world coords)\n", nPixelDeltaX, nPixelDeltaY, fWorldDeltaX, fWorldDeltaY);

	mappoint_t pt;
	pt.fLatitude = pMap->MapCenter.fLatitude + fWorldDeltaY;
	pt.fLongitude = pMap->MapCenter.fLongitude + fWorldDeltaX;
	map_set_centerpoint(pMap, &pt);
}

void map_zoom_to_screenrect(map_t* pMap, const screenrect_t* pRect)
{
	// recenter on rect
	screenpoint_t ptCenter;
	map_get_screenrect_centerpoint(pRect, &ptCenter);
	map_center_on_windowpoint(pMap, ptCenter.nX, ptCenter.nY);
	//g_print("box centerpoint = %d,%d\n", ptCenter.nX, ptCenter.nY);
	
	// calculate size of rectangle in degrees
	gdouble fBoxLongitude = map_pixels_to_degrees(pMap, map_screenrect_width(pRect), pMap->uZoomLevel); 
	gdouble fBoxLatitude = map_pixels_to_degrees(pMap, map_screenrect_height(pRect), pMap->uZoomLevel); 
	//g_print("box size pixels = %d,%d\n", map_screenrect_width(pRect), map_screenrect_height(pRect));
	//g_print("box size = %f,%f\n", fBoxLongitude, fBoxLatitude);

	// go from zoomed all the way to all the way out, looking for a rectangle
	// that is wide & tall enough to show all of pRect
	gboolean bFound = FALSE;
	gint nZoomLevel;
	for(nZoomLevel = MAX_ZOOM_LEVEL ; nZoomLevel >= MIN_ZOOM_LEVEL ; nZoomLevel--) {
		pMap->uZoomLevel = nZoomLevel;

		rendermetrics_t metrics;
		map_get_render_metrics(pMap, &metrics);
		//g_print("Screen at zoomlevel %d = %f,%f\n", nZoomLevel, metrics.fScreenLatitude, metrics.fScreenLongitude);

		if(fBoxLatitude < metrics.fScreenLatitude && fBoxLongitude < metrics.fScreenLongitude) {
			//g_print("Success\n");
			break;
		}
	}
	// either we hit the 'break' and left pMap->uZoomLevel with the appropriate zoomlevel, or we
	// finished the loop and left pMap->uZoomLevel with MIN_ZOOM_LEVEL (farthest from the ground)
	// which is the best we can do.  either way we recentered.
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

	pMap->pPixmap = gdk_pixmap_new(pMap->pTargetWidget->window,	pMap->MapDimensions.uWidth, pMap->MapDimensions.uHeight, -1);
}

// ========================================================
//  Draw Functions
// ========================================================

void map_get_render_metrics(const map_t* pMap, rendermetrics_t* pMetrics)
{
	g_assert(pMetrics != NULL);

	//
	// Copy a few values in
	//
	pMetrics->nZoomLevel = map_get_zoomlevel(pMap);
	pMetrics->nWindowWidth = pMap->MapDimensions.uWidth;
	pMetrics->nWindowHeight = pMap->MapDimensions.uHeight;
	pMetrics->nLevelOfDetail = g_sZoomLevels[pMetrics->nZoomLevel-1].nLevelOfDetail;

	//
	// Calculate how many world degrees we'll be drawing
	//
	pMetrics->fScreenLatitude = map_pixels_to_degrees(pMap, pMap->MapDimensions.uHeight, pMetrics->nZoomLevel);
	pMetrics->fScreenLongitude = map_pixels_to_degrees(pMap, pMap->MapDimensions.uWidth, pMetrics->nZoomLevel);

	// The world bounding box (expressed in lat/lon) of the data we will be drawing
	pMetrics->rWorldBoundingBox.A.fLongitude = pMap->MapCenter.fLongitude - pMetrics->fScreenLongitude/2;
	pMetrics->rWorldBoundingBox.A.fLatitude = pMap->MapCenter.fLatitude - pMetrics->fScreenLatitude/2;
	pMetrics->rWorldBoundingBox.B.fLongitude = pMap->MapCenter.fLongitude + pMetrics->fScreenLongitude/2;
	pMetrics->rWorldBoundingBox.B.fLatitude = pMap->MapCenter.fLatitude + pMetrics->fScreenLatitude/2;	
}

// void map_add_track(map_t* pMap, gint hTrack)
// {
//     g_array_append_val(pMap->pTracksArray, hTrack);
// }

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
	gint i;
	for(i=0 ; i<G_N_ELEMENTS(g_apszMapRenderTypeNames) ; i++) {
		if(strcmp(pszName, g_apszMapRenderTypeNames[i]) == 0) {
			*pnReturnRenderTypeID = i;
			return TRUE;
		}
	}
	return FALSE;
}

//
//
//
//
//
//
//
//
//
//
//
//
//


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

void map_callback_free_locations_array(gpointer p)
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

void map_get_visible_maprect(const map_t* pMap, maprect_t* pReturnMapRect)
{
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);

	memcpy(pReturnMapRect, &(renderMetrics.rWorldBoundingBox), sizeof(maprect_t));
}


#ifdef ROADSTER_DEAD_CODE
/*
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
*/
#endif
