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

typedef enum {
	kSublayerBottom,
	kSublayerTop,
} ESubLayer;

#define MIN_LINE_LENGTH_FOR_LABEL  	(40)
#define LABEL_PIXELS_ABOVE_LINE 	(2)
#define LABEL_PIXEL_RELIEF_INSIDE_LINE	(2)	// when drawing a label inside a line, only do so if we would have at least this much blank space above+below the text

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

struct GtkWidget;

#define MIN_ZOOM_LEVEL	1
#define MAX_ZOOM_LEVEL	10

#include "layers.h"
#include "scenemanager.h"

// World space
typedef struct mappoint {
	gdouble m_fLatitude;
	gdouble m_fLongitude;
} mappoint_t;

typedef struct maprect {
	mappoint_t m_A;
	mappoint_t m_B;
} maprect_t;

// Screen space
typedef struct screenpoint {
	gint16 m_nX;
	gint16 m_nY;
} screenpoint_t;

typedef struct screenrect {
	screenpoint_t m_A;
	screenpoint_t m_B;
} screenrect_t;

typedef struct windowdimensions {
	guint16 m_uWidth;
	guint16 m_uHeight;
} dimensions_t;

typedef struct zoomlevel {
	guint32 m_uScale;		// ex. 10000 for 1:10000 scale
	gchar* m_szName;
} zoomlevel_t;

extern zoomlevel_t g_sZoomLevels[];

typedef enum {
	UNIT_FIRST=0,	
		UNIT_FEET=0,
		UNIT_MILES=1,
		UNIT_METERS=2,
		UNIT_KILOMETERS=3,
	UNIT_LAST=3,
} EDistanceUnits;

#define DEFAULT_UNIT	(UNIT_MILES)

extern gchar* g_aDistanceUnitNames[];

typedef struct {
	gint m_nZoomLevel;
	gdouble m_fScreenLatitude;
	gdouble m_fScreenLongitude;
	maprect_t m_rWorldBoundingBox;
	gint m_nWindowWidth;
	gint m_nWindowHeight;
} rendermetrics_t;
#define SCALE_X(p, x)  ((((x) - (p)->m_rWorldBoundingBox.m_A.m_fLongitude) / (p)->m_fScreenLongitude) * (p)->m_nWindowWidth)
#define SCALE_Y(p, y)  ((p)->m_nWindowHeight - ((((y) - (p)->m_rWorldBoundingBox.m_A.m_fLatitude) / (p)->m_fScreenLatitude) * (p)->m_nWindowHeight))

typedef struct {
	GPtrArray* m_pPointStringsArray;	// this should probably change to an array of 'roads'
} maplayer_data_t;

typedef struct {
	// Mutex and the data it controls (always lock before reading/writing)
	//GMutex* m_pDataMutex;
	 mappoint_t 			m_MapCenter;
	 dimensions_t 			m_MapDimensions;
	 guint16 			m_uZoomLevel;
	 maplayer_data_t* m_apLayerData[ NUM_LAYERS + 1 ];
	 GtkWidget*			m_pTargetWidget;
	 scenemanager_t*		m_pSceneManager;

	// Mutex and the data it controls (always lock before reading/writing)
	//GMutex* m_pPixmapMutex;
	 GdkPixmap* m_pPixmap;
} map_t;

typedef enum {
	SUBLAYER_RENDERTYPE_LINES,
	SUBLAYER_RENDERTYPE_POLYGONS,
	SUBLAYER_RENDERTYPE_LINE_LABELS,
	SUBLAYER_RENDERTYPE_POLYGON_LABELS
} ESubLayerRenderType;

typedef struct {
	gint nLayer;
	gint nSubLayer;
	ESubLayerRenderType eSubLayerRenderType;

//	void (*pFunc)(map_t*, cairo_t*, rendermetrics_t*, GPtrArray*, sublayerstyle_t*, textlabelstyle_t*);
} draworder_t;

// Draw flags
#define DRAWFLAG_LABELS 	(1)
#define DRAWFLAG_BACKGROUND	(2)
#define DRAWFLAG_GEOMETRY	(4) // next is 8


#define NUM_SUBLAYER_TO_DRAW (22)
extern draworder_t layerdraworder[NUM_SUBLAYER_TO_DRAW];	//

void map_init(void);
gboolean map_new(map_t** ppMap, GtkWidget* pTargetWidget);


// Gets and Sets
guint16 map_get_zoomlevel(map_t* pMap);
guint32 map_get_zoomlevel_scale(map_t* pMap);
void map_set_zoomlevel(map_t* pMap, guint16 uZoomLevel);
//void map_get_render_metrics(rendermetrics_t* pMetrics);

void map_set_redraw_needed(map_t* pMap, gboolean bNeeded);
gboolean map_get_redraw_needed(map_t* pMap);

guint32 map_get_scale(map_t* pMap);

void map_set_centerpoint(map_t* pMap, const mappoint_t* pPoint);
void map_get_centerpoint(map_t* pMap, mappoint_t* pReturnPoint);
void map_set_dimensions(map_t* pMap, const dimensions_t* pDimensions);

// Conversions
void map_windowpoint_to_mappoint(map_t* pMap, screenpoint_t* pScreenPoint, mappoint_t* pMapPoint);
gdouble map_distance_in_units_to_degrees(map_t* pMap, gdouble fDistance, gint nDistanceUnit);


// remove this!
void map_center_on_windowpoint(map_t* pMap, guint16 uX, guint16 uY);

GdkPixmap* map_get_pixmap(map_t* pMap);
void map_release_pixmap(map_t* pMap);
//void map_draw_thread_begin(map_t* pMap, GtkWidget* pTargetWidget);

void map_draw(map_t* pMap);
double map_degrees_to_pixels(map_t* pMap, gdouble fDegrees, guint16 uZoomLevel);

#endif
