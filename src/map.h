/***************************************************************************
 *            map.h
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

#ifndef _MAP_H_
#define _MAP_H_

#include <math.h>
#include "gfreelist.h"

//
// Map Object Types
//
#define MAP_OBJECT_TYPE_NONE					(0)
#define MAP_OBJECT_TYPE_MINORROAD				(1)
#define MAP_OBJECT_TYPE_MAJORROAD				(2)
#define MAP_OBJECT_TYPE_MINORHIGHWAY			(3)
#define MAP_OBJECT_TYPE_MINORHIGHWAY_RAMP		(4)
#define MAP_OBJECT_TYPE_MAJORHIGHWAY			(5)	// Unused
#define MAP_OBJECT_TYPE_MAJORHIGHWAY_RAMP		(6)	// Unused
#define MAP_OBJECT_TYPE_RAILROAD				(7)
#define MAP_OBJECT_TYPE_PARK					(8)
#define MAP_OBJECT_TYPE_RIVER					(9)
#define MAP_OBJECT_TYPE_LAKE					(10)
#define MAP_OBJECT_TYPE_MISC_AREA				(11)
#define MAP_OBJECT_TYPE_URBAN_AREA				(12)

#define MAP_NUM_OBJECT_TYPES 					(13)

#define MAP_OBJECT_TYPE_FIRST					(1)
#define MAP_OBJECT_TYPE_LAST					(12)

#define MAP_LEVEL_OF_DETAIL_BEST	(0)
#define MAP_LEVEL_OF_DETAIL_WORST	(3)
#define MAP_NUM_LEVELS_OF_DETAIL	(4)

//
// Line CAP styles
//
#define MAP_CAP_STYLE_ROUND		(1)
#define MAP_CAP_STYLE_SQUARE	(2)
#define MAP_CAP_STYLE_DEFAULT	(MAP_CAP_STYLE_ROUND)	// if not specified by the style

#define MIN_LATITUDE	(-90.0)
#define MAX_LATITUDE	(90.0)
#define MIN_LONGITUDE	(-180.0)
#define MAX_LONGITUDE	(180.0)

typedef enum {
	kSublayerBottom,
	kSublayerTop,
} ESubLayer;

#define LABEL_PIXELS_ABOVE_LINE 	(2)
#define LABEL_PIXEL_RELIEF_INSIDE_LINE	(2)	// when drawing a label inside a line, only do so if we would have at least this much blank space above+below the text

// For road names: Bitstream Vera Sans Mono ?

#define INCHES_PER_METER (39.37007)
#define FEET_PER_MILE				(5280.0)

#define WORLD_CIRCUMFERENCE_IN_METERS (40075452.7)
#define WORLD_METERS_PER_DEGREE (WORLD_CIRCUMFERENCE_IN_METERS / 360.0)
#define WORLD_METERS_TO_DEGREES(x)	((x) / WORLD_METERS_PER_DEGREE)
#define WORLD_DEGREES_TO_METERS(x)	((x) * WORLD_METERS_PER_DEGREE)
#define KILOMETERS_PER_METER 	(1000.0)
#define WORLD_KILOMETERS_TO_DEGREES(x)	(((x) * KILOMETERS_PER_METER) / WORLD_METERS_PER_DEGREE)

#define WORLD_CIRCUMFERENCE_IN_FEET (131482939.8324)

#define WORLD_MILES_PER_DEGREE		((WORLD_CIRCUMFERENCE_IN_FEET / 360.0) / FEET_PER_MILE)
#define WORLD_FEET_PER_DEGREE 		(WORLD_CIRCUMFERENCE_IN_FEET / 360.0)
#define WORLD_FEET_TO_DEGREES(X)	((X) / WORLD_FEET_PER_DEGREE)

#define WORLD_MILES_TO_DEGREES(x)	((x * FEET_PER_MILE) / WORLD_FEET_PER_DEGREE)

#define WORLD_DEGREES_TO_MILES(x)	((x) * WORLD_MILES_PER_DEGREE)

// Earth is slightly egg shaped so there are infinite radius measurements:

// at poles: ?
// average: 6,371,010
// at equator: 6,378,136 meters

#define RADIUS_OF_WORLD_IN_METERS 	(6371010)

#define DEG2RAD(x)	((x) * (M_PI / 180.0))
#define RAD2DEG(x)	((x) * (180.0 / M_PI))

struct GtkWidget;

#define MIN_ZOOM_LEVEL					(1)
#define MAX_ZOOM_LEVEL					(41)
#define NUM_ZOOM_LEVELS					(41)
#define MIN_ZOOM_LEVEL_FOR_LOCATIONS	(25)		// don't show POI above this level

#include "scenemanager.h"

// World space
typedef struct {
	gdouble fLatitude;
	gdouble fLongitude;
} mappoint_t;

typedef struct {
	mappoint_t A;
	mappoint_t B;
} maprect_t;

// Screen space
typedef struct {
	gint16 nX;
	gint16 nY;
} screenpoint_t;

typedef struct {
	screenpoint_t A;
	screenpoint_t B;
} screenrect_t;

typedef struct {
	guint16 uWidth;
	guint16 uHeight;
} dimensions_t;

typedef enum {
	UNIT_FIRST=0,	
		UNIT_FEET=0,
		UNIT_MILES=1,
		UNIT_METERS=2,
		UNIT_KILOMETERS=3,
	UNIT_LAST=3,
} EDistanceUnits;
#define DEFAULT_UNIT	(UNIT_MILES)

typedef struct {
	guint32 uScale;		// ex. 10000 for 1:10000 scale

	EDistanceUnits eScaleImperialUnit;	// eg. "feet"
	gint nScaleImperialNumber;			// eg. 200

	EDistanceUnits eScaleMetricUnit;
	gint nScaleMetricNumber;

	//gchar* szName;
	
	gint nStyleZoomLevel;				// pretend we're zoomlevel X because there are more real zoomlevels than in the map layer style definitions
	gint nLevelOfDetail;				// pretend we're zoomlevel X because there are more real zoomlevels than in the map layer style definitions
} zoomlevel_t;

extern zoomlevel_t g_sZoomLevels[];


typedef enum {
	SIDE_LEFT=1,
	SIDE_RIGHT=2,
} ESide;

extern gchar* g_aDistanceUnitNames[];

typedef struct {
	gint nZoomLevel;
	gdouble fScreenLatitude;
	gdouble fScreenLongitude;
	maprect_t rWorldBoundingBox;
	gint nWindowWidth;
	gint nWindowHeight;
	gint nLevelOfDetail;
} rendermetrics_t;

#define SCALE_X(p, x)  ((((x) - (p)->rWorldBoundingBox.A.fLongitude) / (p)->fScreenLongitude) * (p)->nWindowWidth)
#define SCALE_Y(p, y)  ((p)->nWindowHeight - ((((y) - (p)->rWorldBoundingBox.A.fLatitude) / (p)->fScreenLatitude) * (p)->nWindowHeight))

// typedef struct {
//     GPtrArray* pRoadsArray;
// } maplayer_data_t;

typedef struct {
	GPtrArray* pLocationsArray;
} maplayer_locations_t;

#include "map_tilemanager.h"

typedef struct {
	mappoint_t 		MapCenter;
	dimensions_t 	MapDimensions;
	guint16 		uZoomLevel;
	GtkWidget*		pTargetWidget;
	scenemanager_t* pSceneManager;

	// data
	GArray			*pTracksArray;
//	maplayer_data_t	*apLayerData[ MAP_NUM_OBJECT_TYPES + 1 ];

	maptilemanager_t* pTileManager;

	// Locationsets
	GHashTable		*pLocationArrayHashTable;

	GPtrArray		*pLocationSelectionArray;
	GFreeList		*pLocationSelectionAllocator;

	GdkPixmap* pPixmap;

	GPtrArray* pLayersArray;
} map_t;

typedef enum {
	MAP_LAYER_RENDERTYPE_LINES,
	MAP_LAYER_RENDERTYPE_POLYGONS,
	MAP_LAYER_RENDERTYPE_LINE_LABELS,
	MAP_LAYER_RENDERTYPE_POLYGON_LABELS,
	MAP_LAYER_RENDERTYPE_FILL,
	MAP_LAYER_RENDERTYPE_LOCATIONS,
	MAP_LAYER_RENDERTYPE_LOCATION_LABELS,
} EMapLayerRenderType;

typedef struct {
	gint nLayer;
	gint nSubLayer;
	EMapLayerRenderType eSubLayerRenderType;
} draworder_t;

#define MAX_LOCATIONSELECTION_URLS	(5)

typedef struct {
	gint nLocationID;
	gboolean bVisible;

	mappoint_t Coordinates;
	GPtrArray* pAttributesArray;

	screenrect_t InfoBoxRect;
	screenrect_t InfoBoxCloseRect;
	screenrect_t EditRect;

	gint nNumURLs;
	struct {
		screenrect_t Rect;
		gchar* pszURL;
	} aURLs[MAX_LOCATIONSELECTION_URLS];
} locationselection_t;

// Draw flags
#define DRAWFLAG_LABELS 	(1)
#define DRAWFLAG_GEOMETRY	(2)
// next is 4 :)

#define DRAWFLAG_ALL 		(1|2)

// #define NUM_SUBLAYER_TO_DRAW (21) //(24)
// extern draworder_t layerdraworder[NUM_SUBLAYER_TO_DRAW];    //


void map_init(void);
gboolean map_new(map_t** ppMap, GtkWidget* pTargetWidget);

// Gets and Sets
guint16 map_get_zoomlevel(const map_t* pMap);
guint32 map_get_zoomlevel_scale(const map_t* pMap);

gboolean map_can_zoom_in(const map_t* pMap);
gboolean map_can_zoom_out(const map_t* pMap);

void map_set_zoomlevel(map_t* pMap, guint16 uZoomLevel);

void map_set_redraw_needed(map_t* pMap, gboolean bNeeded);
gboolean map_get_redraw_needed(const map_t* pMap);
guint32 map_get_scale(const map_t* pMap);

void map_set_centerpoint(map_t* pMap, const mappoint_t* pPoint);
void map_get_centerpoint(const map_t* pMap, mappoint_t* pReturnPoint);
void map_set_dimensions(map_t* pMap, const dimensions_t* pDimensions);

// Conversions
void map_windowpoint_to_mappoint(map_t* pMap, screenpoint_t* pScreenPoint, mappoint_t* pMapPoint);
gdouble map_distance_in_units_to_degrees(map_t* pMap, gdouble fDistance, gint nDistanceUnit);
gdouble map_get_distance_in_meters(mappoint_t* pA, mappoint_t* pB);
gdouble map_get_straight_line_distance_in_degrees(mappoint_t* p1, mappoint_t* p2);

gdouble map_pixels_to_degrees(const map_t* pMap, gint16 nPixels, guint16 uZoomLevel);
gdouble map_degrees_to_pixels(map_t* pMap, gdouble fDegrees, guint16 uZoomLevel);
gboolean map_points_equal(mappoint_t* p1, mappoint_t* p2);
gdouble map_get_distance_in_pixels(map_t* pMap, mappoint_t* p1, mappoint_t* p2);

// remove this!
void map_center_on_windowpoint(map_t* pMap, guint16 uX, guint16 uY);

GdkPixmap* map_get_pixmap(map_t* pMap);
void map_release_pixmap(map_t* pMap);

void map_draw(map_t* pMap, GdkPixmap* pTargetPixmap, gint nDrawFlags);
void map_add_track(map_t* pMap, gint hTrack);

gboolean map_location_selection_add(map_t* pMap, gint nLocationID);
gboolean map_location_selection_remove(map_t* pMap, gint nLocationID);

const gchar* map_location_selection_get_attribute(const map_t* pMap, const locationselection_t* pLocationSelection, const gchar* pszAttributeName);

gboolean map_object_type_atoi(const gchar* pszName, gint* pnReturnObjectTypeID);
gboolean map_layer_render_type_atoi(const gchar* pszName, gint* pnReturnRenderTypeID);

gboolean map_rects_overlap(const maprect_t* p1, const maprect_t* p2);

void map_zoom_to_screenrect(map_t* pMap, const screenrect_t* pRect);
gint map_screenrect_width(const screenrect_t* pRect);
gint map_screenrect_height(const screenrect_t* pRect);
void map_get_screenrect_centerpoint(const screenrect_t* pRect, screenpoint_t* pPoint);

void map_get_visible_maprect(const map_t* pMap, maprect_t* pReturnMapRect);
gdouble map_get_altitude(const map_t* pMap, EDistanceUnits eUnit);

#endif
