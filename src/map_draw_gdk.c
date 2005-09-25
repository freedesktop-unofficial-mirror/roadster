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

//#define ENABLE_MAP_GRAYSCALE_HACK

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
#include "map_style.h"
#include "track.h"
#include "locationset.h"
#include "location.h"
#include "scenemanager.h"

//static void map_draw_gdk_background(map_t* pMap, GdkPixmap* pPixmap);
static void map_draw_gdk_layer_polygons(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle);
static void map_draw_gdk_layer_lines(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle);

static void map_draw_gdk_layer_fill(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, maplayerstyle_t* pLayerStyle);

static void map_draw_gdk_tracks(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics);
static void map_draw_gdk_locations(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics);
static void map_draw_gdk_locationset(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, locationset_t* pLocationSet, GPtrArray* pLocationsArray);

//#define ENABLE_MAP_GRAYSCALE_HACK 	// just a little test.  black and white might be good for something

void map_draw_gdk_set_color(GdkGC* pGC, color_t* pColor)
{
	GdkColor clr;

#ifdef ENABLE_MAP_GRAYSCALE_HACK
	clr.red = clr.green = clr.blue = ((pColor->fRed + pColor->fGreen + pColor->fBlue) / 3.0) * 65535;
#else
	clr.red = pColor->fRed * 65535;
	clr.green = pColor->fGreen * 65535;
	clr.blue = pColor->fBlue * 65535;
#endif
	gdk_gc_set_rgb_fg_color(pGC, &clr);
}

void map_draw_gdk(map_t* pMap, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags)
{
	TIMER_BEGIN(maptimer, "BEGIN RENDER MAP (gdk)");

	GdkGC* pGC = pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)];

	// 1. Save values (so we can restore them)
	GdkGCValues gcValues;
	gdk_gc_get_values(pGC, &gcValues);

	// 2. Drawing
	if(nDrawFlags & DRAWFLAG_GEOMETRY) {
		gint i;

		// 2.1. Draw Background
//		map_draw_gdk_background(pMap, pPixmap);

		// 2.2. Draw layer list in reverse order (painter's algorithm: http://en.wikipedia.org/wiki/Painter's_algorithm )
		for(i=pMap->pLayersArray->len-1 ; i>=0 ; i--) {
			maplayer_t* pLayer = g_ptr_array_index(pMap->pLayersArray, i);

			if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_FILL) {
				map_draw_gdk_layer_fill(pMap, pPixmap,	pRenderMetrics,
										 pLayer->paStylesAtZoomLevels[pRenderMetrics->nZoomLevel-1]);		// style
			}
			else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_LINES) {
				map_draw_gdk_layer_lines(pMap, pPixmap,	pRenderMetrics,
										 pMap->apLayerData[pLayer->nDataSource]->pRoadsArray,				// data
										 pLayer->paStylesAtZoomLevels[pRenderMetrics->nZoomLevel-1]);		// style
			}
			else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_POLYGONS) {
				map_draw_gdk_layer_polygons(pMap, pPixmap, pRenderMetrics,
											pMap->apLayerData[pLayer->nDataSource]->pRoadsArray,          // data
											pLayer->paStylesAtZoomLevels[pRenderMetrics->nZoomLevel-1]); 	// style
			}
		}

		map_draw_gdk_tracks(pMap, pPixmap, pRenderMetrics);
		map_draw_gdk_locations(pMap, pPixmap, pRenderMetrics);
	}

	// 3. Labels
	// ...GDK just shouldn't attempt text. :)

	// 4. Restore values
	gdk_gc_set_values(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], &gcValues, GDK_GC_FOREGROUND | GDK_GC_BACKGROUND | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE | GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE);
	TIMER_END(maptimer, "END RENDER MAP (gdk)");
}

// static void map_draw_gdk_background(map_t* pMap, GdkPixmap* pPixmap)
// {
//     GdkColor clr;
//     clr.red = 236/255.0 * 65535;
//     clr.green = 230/255.0 * 65535;
//     clr.blue = 230/255.0 * 65535;
//     gdk_gc_set_rgb_fg_color(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], &clr);
//
//     gdk_draw_rectangle(pPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
//             TRUE, 0,0, pMap->MapDimensions.uWidth, pMap->MapDimensions.uHeight);
// }

static void map_draw_gdk_layer_polygons(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle)
{
	mappoint_t* pPoint;
	road_t* pRoad;
	gint iString;
	gint iPoint;

	if(pLayerStyle->clrPrimary.fAlpha == 0.0) return;	// invisible?  (not that we respect it in gdk drawing anyway)

	map_draw_gdk_set_color(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], &(pLayerStyle->clrPrimary));

	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
		gdouble fMinLat = MAX_LATITUDE;
		gdouble fMaxLon = MIN_LONGITUDE;
		gdouble fMinLon = MAX_LONGITUDE;

		if(pRoad->pPointsArray->len >= 2) {
			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			if(pRoad->pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
				//g_warning("not drawing line with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
				continue;
			}

			for(iPoint=0 ; iPoint<pRoad->pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pRoad->pPointsArray, iPoint);

				// find extents
				fMaxLat = max(pPoint->fLatitude,fMaxLat);
				fMinLat = min(pPoint->fLatitude,fMinLat);
				fMaxLon = max(pPoint->fLongitude,fMaxLon);
				fMinLon = min(pPoint->fLongitude,fMinLon);

				aPoints[iPoint].x = pLayerStyle->nPixelOffsetX + (gint)SCALE_X(pRenderMetrics, pPoint->fLongitude);
				aPoints[iPoint].y = pLayerStyle->nPixelOffsetY + (gint)SCALE_Y(pRenderMetrics, pPoint->fLatitude);
			}

			// rectangle overlap test
			if(fMaxLat < pRenderMetrics->rWorldBoundingBox.A.fLatitude
			   || fMaxLon < pRenderMetrics->rWorldBoundingBox.A.fLongitude
			   || fMinLat > pRenderMetrics->rWorldBoundingBox.B.fLatitude
			   || fMinLon > pRenderMetrics->rWorldBoundingBox.B.fLongitude)
			{
			    continue;	// not visible
			}
			gdk_draw_polygon(pPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
				TRUE, aPoints, pRoad->pPointsArray->len);
   		}
	}
}

// useful for filling the screen with a color.  not much else.
static void map_draw_gdk_layer_fill(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, maplayerstyle_t* pLayerStyle)
{
	map_draw_gdk_set_color(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], &(pLayerStyle->clrPrimary));
	gdk_draw_rectangle(pPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
			TRUE, 0,0, pMap->MapDimensions.uWidth, pMap->MapDimensions.uHeight);
}

static void map_draw_gdk_layer_lines(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle)
{
	road_t* pRoad;
	mappoint_t* pPoint;
	gint iString;
	gint iPoint;

	if(pLayerStyle->fLineWidth <= 0.0) return;			// Don't draw invisible lines
	if(pLayerStyle->clrPrimary.fAlpha == 0.0) return;	// invisible?  (not that we respect it in gdk drawing anyway)

	// Translate generic cap style into GDK constant
	gint nCapStyle;
	if(pLayerStyle->nCapStyle == MAP_CAP_STYLE_ROUND) {
		nCapStyle = GDK_CAP_ROUND;
	}
	else {
		nCapStyle = GDK_CAP_PROJECTING;
	}

	// Convert to integer width.  Ouch!
	gint nLineWidth = (gint)(pLayerStyle->fLineWidth);

	// Use GDK dash style if ANY dash pattern is set
	gint nDashStyle = GDK_LINE_SOLID;
	if(pLayerStyle->pDashStyle != NULL) {
		gdk_gc_set_dashes(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
				0, /* offset to start at */
				pLayerStyle->pDashStyle->panDashList,
				pLayerStyle->pDashStyle->nDashCount);
		
		nDashStyle = GDK_LINE_ON_OFF_DASH;
		// further set line attributes below...
	}

	// Set line style
	gdk_gc_set_line_attributes(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)],
			   nLineWidth, nDashStyle, nCapStyle, GDK_JOIN_ROUND);

	map_draw_gdk_set_color(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], &(pLayerStyle->clrPrimary));

	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		if(pRoad->pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
			//g_warning("not drawing line with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
			continue;
		}

		gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
		gdouble fMinLat = MAX_LATITUDE;
		gdouble fMaxLon = MIN_LONGITUDE;
		gdouble fMinLon = MAX_LONGITUDE;

		if(pRoad->pPointsArray->len >= 2) {
			// Copy all points into this array.  Yuuup this is slow. :)
			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			for(iPoint=0 ; iPoint<pRoad->pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pRoad->pPointsArray, iPoint);

				// find extents
				fMaxLat = max(pPoint->fLatitude,fMaxLat);
				fMinLat = min(pPoint->fLatitude,fMinLat);
				fMaxLon = max(pPoint->fLongitude,fMaxLon);
				fMinLon = min(pPoint->fLongitude,fMinLon);

				aPoints[iPoint].x = pLayerStyle->nPixelOffsetX + (gint)SCALE_X(pRenderMetrics, pPoint->fLongitude);
				aPoints[iPoint].y = pLayerStyle->nPixelOffsetY + (gint)SCALE_Y(pRenderMetrics, pPoint->fLatitude);
			}

			// basic rectangle overlap test
			// XXX: not quite right. the points that make up a road may be offscreen,
			// but a thick road should still be visible
			if(fMaxLat < pRenderMetrics->rWorldBoundingBox.A.fLatitude
			   || fMaxLon < pRenderMetrics->rWorldBoundingBox.A.fLongitude
			   || fMinLat > pRenderMetrics->rWorldBoundingBox.B.fLatitude
			   || fMinLon > pRenderMetrics->rWorldBoundingBox.B.fLongitude)
			{
			    continue;	// not visible
			}

			gdk_draw_lines(pPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], aPoints, pRoad->pPointsArray->len);
   		}
	}
}

static void map_draw_gdk_tracks(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics)
{
	gint i;
	for(i=0 ; i<pMap->pTracksArray->len ; i++) {
		gint hTrack = g_array_index(pMap->pTracksArray, gint, i);

		GdkColor clr;
		clr.red = (gint)(0.5 * 65535.0);
		clr.green = (gint)(0.5 * 65535.0);
		clr.blue = (gint)(1.0 * 65535.0);
		gdk_gc_set_rgb_fg_color(pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], &clr);

		pointstring_t* pPointString = track_get_pointstring(hTrack);
		if(pPointString == NULL) continue;

		if(pPointString->pPointsArray->len >= 2) {

			if(pPointString->pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
				//g_warning("not drawing track with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
				continue;
			}

			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
			gdouble fMinLat = MAX_LATITUDE;
			gdouble fMaxLon = MIN_LONGITUDE;
			gdouble fMinLon = MAX_LONGITUDE;

			gint iPoint;
			for(iPoint=0 ; iPoint<pPointString->pPointsArray->len ; iPoint++) {
				mappoint_t* pPoint = g_ptr_array_index(pPointString->pPointsArray, iPoint);

				// find extents
				fMaxLat = max(pPoint->fLatitude,fMaxLat);
				fMinLat = min(pPoint->fLatitude,fMinLat);
				fMaxLon = max(pPoint->fLongitude,fMaxLon);
				fMinLon = min(pPoint->fLongitude,fMinLon);

				aPoints[iPoint].x = (gint)SCALE_X(pRenderMetrics, pPoint->fLongitude);
				aPoints[iPoint].y = (gint)SCALE_Y(pRenderMetrics, pPoint->fLatitude);
			}

			// rectangle overlap test
			if(fMaxLat < pRenderMetrics->rWorldBoundingBox.A.fLatitude
			   || fMaxLon < pRenderMetrics->rWorldBoundingBox.A.fLongitude
			   || fMinLat > pRenderMetrics->rWorldBoundingBox.B.fLatitude
			   || fMinLon > pRenderMetrics->rWorldBoundingBox.B.fLongitude)
			{
			    continue;	// not visible
			}

			gdk_draw_lines(pPixmap, pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)], aPoints, pPointString->pPointsArray->len);
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
		pLocationsArray = g_hash_table_lookup(pMap->pLocationArrayHashTable, &(pLocationSet->nID));
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
		if(pLocation->Coordinates.fLatitude < pRenderMetrics->rWorldBoundingBox.A.fLatitude
		   || pLocation->Coordinates.fLongitude < pRenderMetrics->rWorldBoundingBox.A.fLongitude
		   || pLocation->Coordinates.fLatitude > pRenderMetrics->rWorldBoundingBox.B.fLatitude
		   || pLocation->Coordinates.fLongitude > pRenderMetrics->rWorldBoundingBox.B.fLongitude)
		{
		    continue;   // not visible
		}

		gint nX = (gint)SCALE_X(pRenderMetrics, pLocation->Coordinates.fLongitude);
		gint nY = (gint)SCALE_Y(pRenderMetrics, pLocation->Coordinates.fLatitude);

//		g_print("drawing at %d,%d\n", nX,nY);
		
		GdkGC* pGC = pMap->pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->pTargetWidget)];

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
