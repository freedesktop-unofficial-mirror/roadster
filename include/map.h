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

#include <cairo.h>

struct GtkWidget;

#define MIN_ZOOM_LEVEL	1
#define MAX_ZOOM_LEVEL	10

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

// Window space (0,0 being upper left of the draw area)
typedef struct windowdimensions {
	guint16 m_uWidth;
	guint16 m_uHeight;
} windowdimensions_t;

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

enum ERoadNameSuffix {			// these can't change once stored in DB
	ROAD_SUFFIX_FIRST = 0,
	ROAD_SUFFIX_NONE = 0,

	ROAD_SUFFIX_ROAD = 1,
	ROAD_SUFFIX_STREET,
	ROAD_SUFFIX_DRIVE,
	ROAD_SUFFIX_BOULEVARD,	// blvd
	ROAD_SUFFIX_AVENUE,
	ROAD_SUFFIX_CIRCLE,
	ROAD_SUFFIX_SQUARE,
	ROAD_SUFFIX_PATH,
	ROAD_SUFFIX_WAY,
	ROAD_SUFFIX_PLAZA,
	ROAD_SUFFIX_TRAIL,
	ROAD_SUFFIX_LANE,
	ROAD_SUFFIX_CROSSING,
	ROAD_SUFFIX_PLACE,
	ROAD_SUFFIX_COURT,
	ROAD_SUFFIX_TURNPIKE,
	ROAD_SUFFIX_TERRACE,
	ROAD_SUFFIX_ROW,
	ROAD_SUFFIX_PARKWAY,

	ROAD_SUFFIX_BRIDGE,
	ROAD_SUFFIX_HIGHWAY,
	ROAD_SUFFIX_RUN,
	ROAD_SUFFIX_PASS,
	
	ROAD_SUFFIX_LAST = ROAD_SUFFIX_TRAIL
};

//~ type 'Ctr' couldn't be looked up
//~ type 'Walk' couldn't be looked up
//~ type 'Ramp' couldn't be looked up
//~ type 'Cv' couldn't be looked up
//~ type 'Byp' couldn't be looked up
//~ type 'Ramp' couldn't be looked up
//~ type 'Br' couldn't be looked up
//~ type 'Trce' couldn't be looked up

typedef struct rendermetrics {
	gint m_nZoomLevel;
	gdouble m_fScreenLatitude;
	gdouble m_fScreenLongitude;
	maprect_t m_rWorldBoundingBox;
	gint m_nWindowWidth;
	gint m_nWindowHeight;
} rendermetrics_t;

// SuffixTypes
#define SUFFIX_TYPE_SHORT	(0)
#define SUFFIX_TYPE_LONG		(1)

const gchar* map_road_suffix_itoa(gint nSuffixID, gint nSuffixType);
gboolean map_road_suffix_atoi(const gchar* pszSuffix, gint* pReturnSuffixID);

#include "db.h"

GtkWidget* map_create_canvas(void);

void map_set_zoomlevel(guint16 uZoomLevel);
guint16 map_get_zoomlevel(void);
guint32 map_get_zoomlevel_scale(void);

void map_set_redraw_needed(gboolean bNeeded);
gboolean map_is_redraw_needed(void);
guint32 map_get_scale(void);
//void map_draw(void* pDBConnection, cairo_t * cr);

void map_get_world_coordinates(float* pLongitude, float* pLatitude);
void map_get_world_coordinate_point(mappoint_t* pPoint);

void map_center_on_worldpoint(double fX, double fY);
void map_center_on_windowpoint(guint16 uX, guint16 uY);
void map_set_window_dimensions(guint16 uWidth, guint16 uHeight);

gboolean map_redraw_if_needed(void);

void map_set_view_dimensions(guint16 uWidth, guint16 uHeight);
void map_windowpoint_to_mappoint(screenpoint_t* pScreenPoint, mappoint_t* pMapPoint);


void map_draw(cairo_t *cr);

void map_get_render_metrics(rendermetrics_t* pMetrics);

gdouble map_distance_in_units_to_degrees(gdouble fDistance, gint nDistanceUnit);

#endif
