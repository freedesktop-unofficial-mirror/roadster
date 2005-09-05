/***************************************************************************
 *            map_draw_cairo.c
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

#define MAX_GDK_LINE_SEGMENTS (2000)

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
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
#include "track.h"
#include "locationset.h"
#include "location.h"
#include "scenemanager.h"

static void map_draw_gdk_background(map_t* pMap, GdkPixmap* pPixmap);
static void map_draw_gdk_layer_polygons(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, layerstyle_t* pLayerStyle);
static void map_draw_gdk_layer_lines(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, layerstyle_t* pLayerStyle);
static void map_draw_gdk_tracks(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics);
static void map_draw_gdk_locations(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics);
static void map_draw_gdk_locationset(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, locationset_t* pLocationSet, GPtrArray* pLocationsArray);

void map_draw_gdk_set_color(GdkGC* pGC, color_t* pColor)
{
	GdkColor clr;
	clr.red = pColor->m_fRed * 65535;
	clr.green = pColor->m_fGreen * 65535;
	clr.blue = pColor->m_fBlue * 65535;
	gdk_gc_set_rgb_fg_color(pGC, &clr);
}

void map_draw_gdk(map_t* pMap, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags)
{
	TIMER_BEGIN(maptimer, "BEGIN RENDER MAP (gdk)");

	// 1. Save values (so we can restore them)
	GdkGCValues gcValues;
	gdk_gc_get_values(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &gcValues);

	// 2. Drawing

	// 2.1. Draw Background
	if(nDrawFlags & DRAWFLAG_GEOMETRY) {
		map_draw_gdk_background(pMap, pPixmap);
	}

	// 2.2. Render Layers
	if(nDrawFlags & DRAWFLAG_GEOMETRY) {
		gint i;

		// draw list in reverse order (painter's algorithm: http://en.wikipedia.org/wiki/Painter's_algorithm )
		for(i=g_pLayersArray->len-1 ; i>=0 ; i--) {
			layer_t* pLayer = g_ptr_array_index(g_pLayersArray, i);

			if(pLayer->m_nDrawType == MAP_LAYER_RENDERTYPE_LINES) {
				map_draw_gdk_layer_lines(pMap, pPixmap,	pRenderMetrics,
										 pMap->m_apLayerData[pLayer->m_nDataSource]->m_pRoadsArray,				// data
										 pLayer->m_paStylesAtZoomLevels[pRenderMetrics->m_nZoomLevel-1]);		// style
			}
			else if(pLayer->m_nDrawType == MAP_LAYER_RENDERTYPE_POLYGONS) {
				map_draw_gdk_layer_polygons(pMap, pPixmap, pRenderMetrics,
											pMap->m_apLayerData[pLayer->m_nDataSource]->m_pRoadsArray,          // data
											pLayer->m_paStylesAtZoomLevels[pRenderMetrics->m_nZoomLevel-1]); 	// style
			}
		}

		map_draw_gdk_tracks(pMap, pPixmap, pRenderMetrics);
		map_draw_gdk_locations(pMap, pPixmap, pRenderMetrics);
	}

	// 3. Labels
	// ...GDK just shouldn't attempt text. :)

	// 4. Restore values
	gdk_gc_set_values(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &gcValues, GDK_GC_FOREGROUND | GDK_GC_BACKGROUND | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE | GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE);
	TIMER_END(maptimer, "END RENDER MAP (gdk)");
}

static void map_draw_gdk_background(map_t* pMap, GdkPixmap* pPixmap)
{
	GdkColor clr;
	clr.red = 236/255.0 * 65535;
	clr.green = 230/255.0 * 65535;
	clr.blue = 230/255.0 * 65535;
	gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);

	gdk_draw_rectangle(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			TRUE, 0,0, pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight);
}

static void map_draw_gdk_layer_polygons(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, layerstyle_t* pLayerStyle)
{
	mappoint_t* pPoint;
	road_t* pRoad;
	gint iString;
	gint iPoint;

	if(pLayerStyle->m_clrPrimary.m_fAlpha == 0.0) return;	// invisible?  (not that we respect it in gdk drawing anyway)

	map_draw_gdk_set_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &(pLayerStyle->m_clrPrimary));

	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
		gdouble fMinLat = MAX_LATITUDE;
		gdouble fMaxLon = MIN_LONGITUDE;
		gdouble fMinLon = MAX_LONGITUDE;

		if(pRoad->m_pPointsArray->len >= 2) {
			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			if(pRoad->m_pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
				//g_warning("not drawing line with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
				continue;
			}

			for(iPoint=0 ; iPoint<pRoad->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pRoad->m_pPointsArray, iPoint);

				// find extents
				fMaxLat = max(pPoint->m_fLatitude,fMaxLat);
				fMinLat = min(pPoint->m_fLatitude,fMinLat);
				fMaxLon = max(pPoint->m_fLongitude,fMaxLon);
				fMinLon = min(pPoint->m_fLongitude,fMinLon);

				aPoints[iPoint].x = (gint)SCALE_X(pRenderMetrics, pPoint->m_fLongitude);
				aPoints[iPoint].y = (gint)SCALE_Y(pRenderMetrics, pPoint->m_fLatitude);
			}

			// rectangle overlap test
			if(fMaxLat < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
			   || fMaxLon < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
			   || fMinLat > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
			   || fMinLon > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
			{
			    continue;	// not visible
			}
			gdk_draw_polygon(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
				TRUE, aPoints, pRoad->m_pPointsArray->len);
   		}
	}
}

static void map_draw_gdk_layer_lines(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, layerstyle_t* pLayerStyle)
{
	road_t* pRoad;
	mappoint_t* pPoint;
	gint iString;
	gint iPoint;

	if(pLayerStyle->m_fLineWidth <= 0.0) return;			// Don't draw invisible lines
	if(pLayerStyle->m_clrPrimary.m_fAlpha == 0.0) return;	// invisible?  (not that we respect it in gdk drawing anyway)

	// Use GDK dash style if ANY dash pattern is set
	gint nDashStyle = GDK_LINE_SOLID;
	if(g_aDashStyles[pLayerStyle->m_nDashStyle].m_nDashCount > 1) {
		nDashStyle = GDK_LINE_ON_OFF_DASH;

		gdk_gc_set_dashes(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
				0, /* offset to start at */
				g_aDashStyles[pLayerStyle->m_nDashStyle].m_panDashList,
				g_aDashStyles[pLayerStyle->m_nDashStyle].m_nDashCount);
	}

	// Translate generic cap style into GDK constant
	gint nCapStyle;
	if(pLayerStyle->m_nCapStyle == MAP_CAP_STYLE_ROUND) {
		nCapStyle = GDK_CAP_ROUND;
	}
	else {
		nCapStyle = GDK_CAP_PROJECTING;
	}

	// Convert to integer width.  Ouch!
	gint nLineWidth = (gint)(pLayerStyle->m_fLineWidth);

	// Set line style
	gdk_gc_set_line_attributes(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			   nLineWidth, nDashStyle, nCapStyle, GDK_JOIN_ROUND);

	map_draw_gdk_set_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &(pLayerStyle->m_clrPrimary));

	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		if(pRoad->m_pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
			//g_warning("not drawing line with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
			continue;
		}

		gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
		gdouble fMinLat = MAX_LATITUDE;
		gdouble fMaxLon = MIN_LONGITUDE;
		gdouble fMinLon = MAX_LONGITUDE;

		if(pRoad->m_pPointsArray->len >= 2) {
			// Copy all points into this array.  Yuuup this is slow. :)
			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			for(iPoint=0 ; iPoint<pRoad->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pRoad->m_pPointsArray, iPoint);

				// find extents
				fMaxLat = max(pPoint->m_fLatitude,fMaxLat);
				fMinLat = min(pPoint->m_fLatitude,fMinLat);
				fMaxLon = max(pPoint->m_fLongitude,fMaxLon);
				fMinLon = min(pPoint->m_fLongitude,fMinLon);

				aPoints[iPoint].x = (gint)SCALE_X(pRenderMetrics, pPoint->m_fLongitude);
				aPoints[iPoint].y = (gint)SCALE_Y(pRenderMetrics, pPoint->m_fLatitude);
			}

			// basic rectangle overlap test
			// XXX: not quite right. the points that make up a road may be offscreen,
			// but a thick road should still be visible
			if(fMaxLat < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
			   || fMaxLon < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
			   || fMinLat > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
			   || fMinLon > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
			{
			    continue;	// not visible
			}

			gdk_draw_lines(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], aPoints, pRoad->m_pPointsArray->len);
   		}
	}
}

static void map_draw_gdk_tracks(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics)
{
	gint i;
	for(i=0 ; i<pMap->m_pTracksArray->len ; i++) {
		gint hTrack = g_array_index(pMap->m_pTracksArray, gint, i);

		GdkColor clr;
		clr.red = (gint)(0.5 * 65535.0);
		clr.green = (gint)(0.5 * 65535.0);
		clr.blue = (gint)(1.0 * 65535.0);
		gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);

		pointstring_t* pPointString = track_get_pointstring(hTrack);
		if(pPointString == NULL) continue;

		if(pPointString->m_pPointsArray->len >= 2) {

			if(pPointString->m_pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
				//g_warning("not drawing track with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
				continue;
			}

			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
			gdouble fMinLat = MAX_LATITUDE;
			gdouble fMaxLon = MIN_LONGITUDE;
			gdouble fMinLon = MAX_LONGITUDE;

			gint iPoint;
			for(iPoint=0 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				mappoint_t* pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);

				// find extents
				fMaxLat = max(pPoint->m_fLatitude,fMaxLat);
				fMinLat = min(pPoint->m_fLatitude,fMinLat);
				fMaxLon = max(pPoint->m_fLongitude,fMaxLon);
				fMinLon = min(pPoint->m_fLongitude,fMinLon);

				aPoints[iPoint].x = (gint)SCALE_X(pRenderMetrics, pPoint->m_fLongitude);
				aPoints[iPoint].y = (gint)SCALE_Y(pRenderMetrics, pPoint->m_fLatitude);
			}

			// rectangle overlap test
			if(fMaxLat < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
			   || fMaxLon < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
			   || fMinLat > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
			   || fMinLon > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
			{
			    continue;	// not visible
			}

			gdk_draw_lines(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], aPoints, pPointString->m_pPointsArray->len);
   		}
	}
}

static void map_draw_gdk_locations(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics)
{
	const GPtrArray* pLocationSetsArray = locationset_get_array();
	gint i;
	for(i=0 ; i<pLocationSetsArray->len ; i++) {
		locationset_t* pLocationSet = g_ptr_array_index(pLocationSetsArray, i);

		if(!locationset_is_visible(pLocationSet)) continue;

		// 2. Get array of Locations from the hash table using LocationSetID
		GPtrArray* pLocationsArray;
		pLocationsArray = g_hash_table_lookup(pMap->m_pLocationArrayHashTable, &(pLocationSet->m_nID));
		if(pLocationsArray != NULL) {
			// found existing array
			map_draw_gdk_locationset(pMap, pPixmap, pRenderMetrics, pLocationSet, pLocationsArray);
		}
		else {
			// none to draw
		}
	}
}

static void map_draw_gdk_locationset(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, locationset_t* pLocationSet, GPtrArray* pLocationsArray)
{
	gint i;
	for(i=0 ; i<pLocationsArray->len ; i++) {
		location_t* pLocation = g_ptr_array_index(pLocationsArray, i);

		// bounding box test
		if(pLocation->m_Coordinates.m_fLatitude < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
		   || pLocation->m_Coordinates.m_fLongitude < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
		   || pLocation->m_Coordinates.m_fLatitude > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
		   || pLocation->m_Coordinates.m_fLongitude > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
		{
		    continue;   // not visible
		}

		gint nX = (gint)SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude);
		gint nY = (gint)SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude);

//		g_print("drawing at %d,%d\n", nX,nY);
		
		GdkGC* pGC = pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)];

		GdkColor clr1;
		clr1.red = 255/255.0 * 65535;
		clr1.green = 80/255.0 * 65535;
		clr1.blue = 80/255.0 * 65535;
		GdkColor clr2;
		clr2.red = 255/255.0 * 65535;
		clr2.green = 255/255.0 * 65535;
		clr2.blue = 255/255.0 * 65535;

		gdk_gc_set_rgb_fg_color(pGC, &clr1);
		gdk_draw_rectangle(pPixmap, pGC, TRUE, 
					nX-3,nY-3,
					7, 7);
		gdk_gc_set_rgb_fg_color(pGC, &clr2);
		gdk_draw_rectangle(pPixmap, pGC, TRUE, 
					nX-2,nY-2,
					5, 5);
		gdk_gc_set_rgb_fg_color(pGC, &clr1);
		gdk_draw_rectangle(pPixmap, pGC, TRUE, 
					nX-1,nY-1,
					3, 3);
	}
}
