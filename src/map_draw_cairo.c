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

//#define ENABLE_TIMING

#define ENABLE_ROUND_DOWN_TEXT_ANGLES			// draw all text at multiples of X radians to be more cache-friendly
#define 	ROUND_DOWN_TEXT_ANGLE	(100.0)		// 10.0 to keep one decimal place or 100.0 to keep two
#define ENABLE_LABEL_LIMIT_TO_ROAD				// don't draw labels if they would be longer than the road
#define ENABLE_HACK_AROUND_CAIRO_LINE_CAP_BUG	// enable to ensure roads have rounded caps if the style dictates

#define	ACCEPTABLE_LINE_LABEL_OVERDRAW_IN_PIXELS_SQUARED (38*38)

#define ROAD_FONT	"Bitstream Vera Sans"
#define AREA_FONT	"Bitstream Vera Sans"

#include <gdk/gdkx.h>
#include <cairo.h>
#include <cairo-xlib.h>
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
#include "location.h"
#include "locationset.h"
#include "scenemanager.h"

// Draw whole layers
static void map_draw_cairo_layer_polygons(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_layer_roads(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_layer_road_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_layer_polygon_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
//static void map_draw_cairo_locations(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics);

// Draw a single line/polygon/point
static void map_draw_cairo_background(map_t* pMap, cairo_t *pCairo);
static void map_draw_cairo_layer_points(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray);
//static void map_draw_cairo_locationset(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics, locationset_t* pLocationSet, GPtrArray* pLocationsArray);
static void map_draw_cairo_locationselection(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray);

// Draw a single line/polygon label
static void map_draw_cairo_road_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, GPtrArray* pPointsArray, gdouble fLineWidth, const gchar* pszLabel);
static void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, GPtrArray* pPointsArray, const gchar* pszLabel);

// static void map_draw_cairo_crosshair(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics);

void map_draw_text_underline(cairo_t* pCairo, gdouble fLabelWidth)
{
#define UNDERLINE_RELIEF	(2.0)

	cairo_set_line_width(pCairo, 1.0);

	cairo_rel_move_to(pCairo, 2, UNDERLINE_RELIEF);
	cairo_rel_line_to(pCairo, fLabelWidth, 0.0);
	cairo_stroke(pCairo);

	// undo moves for underline
	cairo_rel_move_to(pCairo, -(fLabelWidth+2), -UNDERLINE_RELIEF);
}

void map_draw_cairo_set_rgba(cairo_t* pCairo, color_t* pColor)
{
	cairo_set_source_rgba(pCairo, pColor->m_fRed, pColor->m_fGreen, pColor->m_fBlue, pColor->m_fAlpha);
}



void map_draw_cairo(map_t* pMap, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags)
{
	// 1. Set draw target to X Drawable
	Display* dpy;
	Drawable drawable;
	dpy = gdk_x11_drawable_get_xdisplay(pPixmap);
	Visual *visual = DefaultVisual (dpy, DefaultScreen (dpy));
	gint width, height;
	drawable = gdk_x11_drawable_get_xid(pPixmap);

	gdk_drawable_get_size (pPixmap, &width, &height);
	cairo_surface_t *pSurface = cairo_xlib_surface_create (dpy,
							       drawable,
							       visual,
							       width,
							       height);
	cairo_t* pCairo = cairo_create (pSurface);

	// 2. Rendering
	TIMER_BEGIN(maptimer, "BEGIN RENDER MAP (cairo)");
	cairo_save(pCairo);

	// 2.1. Settings for all rendering
	cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);

	// 2.2. Draw Background
	if(nDrawFlags & DRAWFLAG_GEOMETRY) {
		map_draw_cairo_background(pMap, pCairo);
	}

	// 2.3. Render Layers
	gint i;
	for(i=0 ; i<NUM_ELEMS(layerdraworder) ; i++) {
		gint nLayer = layerdraworder[i].nLayer;
		gint nSubLayer = layerdraworder[i].nSubLayer;
		gint nRenderType = layerdraworder[i].eSubLayerRenderType;

		if(nRenderType == SUBLAYER_RENDERTYPE_LINES) {
			if(nDrawFlags & DRAWFLAG_GEOMETRY) {
				map_draw_cairo_layer_roads(pMap, pCairo,
						pRenderMetrics,
				/* geometry */ 	pMap->m_apLayerData[nLayer]->m_pRoadsArray,
				/* style */ 	&(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
		else if(nRenderType == SUBLAYER_RENDERTYPE_POLYGONS) {
			if(nDrawFlags & DRAWFLAG_GEOMETRY) {
				map_draw_cairo_layer_polygons(pMap, pCairo,
						pRenderMetrics,
				/* geometry */ 	pMap->m_apLayerData[nLayer]->m_pRoadsArray,
				/* style */ 	&(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
		else if(nRenderType == SUBLAYER_RENDERTYPE_LINE_LABELS) {
			if(nDrawFlags & DRAWFLAG_LABELS) {
				map_draw_cairo_layer_road_labels(pMap, pCairo,
						pRenderMetrics,
				/* geometry */  pMap->m_apLayerData[nLayer]->m_pRoadsArray,
				/* style */     &(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
		else if(nRenderType == SUBLAYER_RENDERTYPE_POLYGON_LABELS) {
			if(nDrawFlags & DRAWFLAG_LABELS) {
				map_draw_cairo_layer_polygon_labels(pMap, pCairo,
						pRenderMetrics,
				/* geometry */  pMap->m_apLayerData[nLayer]->m_pRoadsArray,
				/* style */     &(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
	}

//	map_draw_cairo_locations(pMap, pCairo, pRenderMetrics);
	map_draw_cairo_locationselection(pMap, pCairo, pRenderMetrics, pMap->m_pLocationSelectionArray);

	// 4. Cleanup
	cairo_restore(pCairo);
	TIMER_END(maptimer, "END RENDER MAP (cairo)");
}

// ==============================================
// Begin map_draw_cairo_* functions
// ==============================================

// Background
static void map_draw_cairo_background(map_t* pMap, cairo_t *pCairo)
{
	// XXX: Don't hardcode background color
	cairo_save(pCairo);
		cairo_set_source_rgb (pCairo, 247/255.0, 235/255.0, 230/255.0);
		cairo_rectangle(pCairo, 0, 0, pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight);
		cairo_fill(pCairo);
	cairo_restore(pCairo);
}

//
// Draw a whole layer of line labels
//
void map_draw_cairo_layer_road_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	gint i;
	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth == 0) return;

	gdouble fFontSize = pLabelStyle->m_afFontSizeAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
	if(fFontSize == 0) return;

	gchar* pszFontFamily = ROAD_FONT;	// XXX: remove hardcoded font

	// set font for whole layer
	cairo_save(pCairo);
	cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, pLabelStyle->m_abBoldAtZoomLevel[pRenderMetrics->m_nZoomLevel-1] ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(pCairo, fFontSize);

	for(i=0 ; i<pRoadsArray->len ; i++) {
		road_t* pRoad = g_ptr_array_index(pRoadsArray, i);
		if(pRoad->m_pszName[0] != '\0') {
			map_draw_cairo_road_label(pMap, pCairo, pLabelStyle, pRenderMetrics, pRoad->m_pPointsArray, fLineWidth, pRoad->m_pszName);
		}
	}
	cairo_restore(pCairo);
}

//
// Draw a whole layer of polygon labels
//
void map_draw_cairo_layer_polygon_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	gdouble fFontSize = pLabelStyle->m_afFontSizeAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
	if(fFontSize == 0) return;

	gchar* pszFontFamily = AREA_FONT;	// XXX: remove hardcoded font

	// set font for whole layer
	cairo_save(pCairo);
	cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, pLabelStyle->m_abBoldAtZoomLevel[pRenderMetrics->m_nZoomLevel-1] ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(pCairo, fFontSize);

	cairo_set_source_rgb(pCairo, pLabelStyle->m_clrColor.m_fRed, pLabelStyle->m_clrColor.m_fGreen, pLabelStyle->m_clrColor.m_fBlue);

	gint i;
	for(i=0 ; i<pRoadsArray->len ; i++) {
		road_t* pRoad = g_ptr_array_index(pRoadsArray, i);
		if(pRoad->m_pszName[0] != '\0') {
			map_draw_cairo_polygon_label(pMap, pCairo, pLabelStyle, pRenderMetrics, pRoad->m_pPointsArray, pRoad->m_pszName);
		}
	}
	cairo_restore(pCairo);
}

//
// Draw a whole layer of lines
//
void map_draw_cairo_layer_roads(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	mappoint_t* pPoint;
	road_t* pRoad;
	gint iString;
	gint iPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth <= 0.0) return;	// Don't draw invisible lines
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;

	cairo_save(pCairo);

	// Raise the tolerance way up for thin lines
	gint nCapStyle = pSubLayerStyle->m_nCapStyle;

	gdouble fTolerance;
	if(fLineWidth >= 12.0) {	// huge line, low tolerance
		fTolerance = 0.40;
	}
	else if(fLineWidth >= 6.0) {	// medium line, medium tolerance
		fTolerance = 0.8;
	}
	else {
		if(nCapStyle == CAIRO_LINE_CAP_ROUND) {
			//g_print("forcing round->square cap style\n");
			//nCapStyle = CAIRO_LINE_CAP_SQUARE;
		}

		if(fLineWidth >= 3.0) {
			fTolerance = 1.2;
		}
		else {  // smaller...
			fTolerance = 10;
		}
	}
	cairo_set_tolerance(pCairo, fTolerance);
	cairo_set_line_join(pCairo, pSubLayerStyle->m_nJoinStyle);
	cairo_set_line_cap(pCairo, nCapStyle);
	if(g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nDashCount > 1) {
		cairo_set_dash(pCairo, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_pafDashList, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nDashCount, 0.0);
	}

	// Set layer attributes	
	map_draw_cairo_set_rgba(pCairo, &(pSubLayerStyle->m_clrColor));
	cairo_set_line_width(pCairo, fLineWidth);

	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		if(pRoad->m_pPointsArray->len >= 2) {
			pPoint = g_ptr_array_index(pRoad->m_pPointsArray, 0);

			// go to index 0
			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

			// start at index 1 (0 was used above)
			for(iPoint=1 ; iPoint<pRoad->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pRoad->m_pPointsArray, iPoint);//~ g_print("  point (%.05f,%.05f)\n", ScaleX(pPoint->m_fLongitude), ScaleY(pPoint->m_fLatitude));
				cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
			}
#ifdef ENABLE_HACK_AROUND_CAIRO_LINE_CAP_BUG
			cairo_stroke(pCairo);	// this is wrong place for it (see below)
#endif
   		}
	}

#ifndef ENABLE_HACK_AROUND_CAIRO_LINE_CAP_BUG
	// this is correct place to stroke, but we can't do this until Cairo fixes this bug:
	// http://cairographics.org/samples/xxx_multi_segment_caps.html
	cairo_stroke(pCairo);
#endif

	cairo_restore(pCairo);
}

void map_draw_cairo_layer_polygons(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	road_t* pRoad;
	mappoint_t* pPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth == 0.0) return;	// Don't both drawing with line width 0
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?

	// Set layer attributes	
	map_draw_cairo_set_rgba(pCairo, &(pSubLayerStyle->m_clrColor));
	cairo_set_line_width(pCairo, fLineWidth);
	cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_EVEN_ODD);

	cairo_set_line_join(pCairo, pSubLayerStyle->m_nJoinStyle);
	cairo_set_line_cap(pCairo, pSubLayerStyle->m_nCapStyle);	/* CAIRO_LINE_CAP_BUTT, CAIRO_LINE_CAP_ROUND, CAIRO_LINE_CAP_CAP */
//	cairo_set_dash(pCairo, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_pfList, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nCount, 0.0);

	gint iString;
	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		if(pRoad->m_pPointsArray->len >= 3) {
			pPoint = g_ptr_array_index(pRoad->m_pPointsArray, 0);

			// move to index 0
			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

			// start at index 1 (0 was used above)
			gint iPoint;
			for(iPoint=1 ; iPoint<pRoad->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pRoad->m_pPointsArray, iPoint);				
				cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
			}
		}
		else {
//			g_print("OOPS!  A linestring with <3 points (%d)\n", pPointString->m_pPointsArray->len);
		}
	}
	cairo_fill(pCairo);
}

void map_draw_cairo_layer_points(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray)
{
/*
	gdouble fRadius = map_degrees_to_pixels(pMap, 0.0007, map_get_zoomlevel(pMap));
	gboolean bAddition = FALSE;

	cairo_save(pCairo);
		cairo_set_source_rgb(pCairo, 123.0/255.0, 48.0/255.0, 1.0);
		cairo_set_alpha(pCairo, 0.3);

		gint iLocation;
		for(iLocation=0 ; iLocation<pLocationsArray->len ; iLocation++) {
			location_t* pLocation = g_ptr_array_index(pLocationsArray, iLocation);

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude), fRadius, 0, 2*M_PI);
	
			if(bAddition) {
				// fill each circle as we create them so they double-up on coverage
				cairo_fill(pCairo);
			}
		}
		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
		if(!bAddition) cairo_fill(pCairo);

		// point centers
		cairo_set_source_rgb(pCairo, 128.0/255.0, 128.0/255.0, 128.0/255.0);
		cairo_set_alpha(pCairo, 1.0);
		fRadius = 2;
		for(iLocation=0 ; iLocation<pLocationsArray->len ; iLocation++) {
			location_t* pLocation = g_ptr_array_index(pLocationsArray, iLocation);

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude), fRadius, 0, 2*M_PI);

			cairo_fill(pCairo);
		}
		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
//		cairo_fill(pCairo);

	cairo_restore(pCairo);
*/
}

<<<<<<< map_draw_cairo.c
#define ROAD_MAX_SEGMENTS 		(100)
=======
//void map_draw_cairo_locations(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics)
//{
/*
	location_t loc;
	location_t* pLoc = &loc;

	// XXX:	debug
	pLoc->m_pszName = "2020 Massachusetts Ave., Cambridge, MA, 02140";
	pLoc->m_Coordinates.m_fLongitude = 0;
	pLoc->m_Coordinates.m_fLatitude = 0;

	//
	// Draw
	//
	cairo_save(pCairo);

	gdouble fX = (gdouble)(gint)SCALE_X(pRenderMetrics, pLoc->m_Coordinates.m_fLongitude);
	gdouble fY = (gdouble)(gint)SCALE_Y(pRenderMetrics, pLoc->m_Coordinates.m_fLatitude);

#define BIP (3)
#define BAP (1.5)

	cairo_set_source_rgb(pCairo, 20/255.0, 20/255.0, 20/255.0);
	cairo_set_line_width(pCairo, 2.5);
	cairo_set_alpha(pCairo, 1.0);

	cairo_rectangle(pCairo, fX - BIP, fY - BIP, BIP*2, BIP*2);
	cairo_stroke(pCairo);

	cairo_rectangle(pCairo, fX - BAP, fY - BAP, BAP*2, BAP*2);
	cairo_fill(pCairo);
	cairo_restore(pCairo);
*/
//}

#define ROAD_MAX_SEGMENTS 100
#define DRAW_LABEL_BUFFER_LEN	(200)

typedef struct labelposition {
	guint m_nSegments;
	guint m_iIndex;
	gdouble m_fLength;
	gdouble m_fRunTotal;
	gdouble m_fMeanSlope;
	gdouble m_fMeanAbsSlope;
	gdouble m_fScore;
} labelposition_t;

//
// Draw a label along a 2-point line
//
static void map_draw_cairo_road_label_one_segment(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, GPtrArray* pPointsArray, gdouble fLineWidth, const gchar* pszLabel)
{
	// get permission to draw this label
	if(FALSE == scenemanager_can_draw_label_at(pMap->m_pSceneManager, pszLabel, NULL)) {
		return;
	}

	mappoint_t* pMapPoint1 = g_ptr_array_index(pPointsArray, 0);
	mappoint_t* pMapPoint2 = g_ptr_array_index(pPointsArray, 1);

	// swap first and second points such that the line goes left-to-right
	if(pMapPoint2->m_fLongitude < pMapPoint1->m_fLongitude) {
		mappoint_t* pTmp = pMapPoint1; pMapPoint1 = pMapPoint2; pMapPoint2 = pTmp;
	}

	// find extents
	gdouble fMaxLat = max(pMapPoint1->m_fLatitude, pMapPoint2->m_fLatitude);
	gdouble fMinLat = min(pMapPoint1->m_fLatitude, pMapPoint2->m_fLatitude);
	gdouble fMaxLon = max(pMapPoint1->m_fLongitude, pMapPoint2->m_fLongitude);
	gdouble fMinLon = min(pMapPoint1->m_fLongitude, pMapPoint2->m_fLongitude);

	gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
	gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
	gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
	gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

	// rectangle overlap test
	if(fMaxLat < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
	   || fMaxLon < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
	   || fMinLat > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
	   || fMinLon > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
	{
		return;
	}

	gdouble fRise = fY2 - fY1;
	gdouble fRun = fX2 - fX1;
	gdouble fLineLengthSquared = (fRun*fRun) + (fRise*fRise);

	gchar* pszFontFamily = ROAD_FONT;

	cairo_save(pCairo);

	// get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszLabel, &extents);
	gdouble fLabelWidth = extents.width;
	gdouble fFontHeight = extents.height;

//        cairo_font_extents_t font_extents;
//        cairo_current_font_extents(pCairo, &font_extents);
//	gdouble fFontHeight = font_extents.ascent;

	// text too big for line?       XXX: This math is not right but good enough for now ;)
#ifdef ENABLE_LABEL_LIMIT_TO_ROAD
	if((fLabelWidth * fLabelWidth) > (fLineLengthSquared + (ACCEPTABLE_LINE_LABEL_OVERDRAW_IN_PIXELS_SQUARED))) {
		cairo_restore(pCairo);
		return;
	}
#endif

	gdouble fLineLength = sqrt(fLineLengthSquared);	// delay expensive sqrt() until after above test

	gdouble fTotalPadding = fLineLength - fLabelWidth;

	// Normalize (make length = 1.0) by dividing by line length
	// This makes a line with length 1 from the origin (0,0)
	gdouble fNormalizedX = fRun / fLineLength;
	gdouble fNormalizedY = fRise / fLineLength;

	// Swap the X and Y and normalize (make length = 1.0) by dividing by line length
	// This makes a perpendicular line with length 1 from the origin (0,0)
	gdouble fPerpendicularNormalizedX = fRise / fLineLength;
	gdouble fPerpendicularNormalizedY = -(fRun / fLineLength);

	// various places to try
	gdouble afPercentagesOfPadding[] = {0.5, 0.25, 0.75, 0.0, 1.0};
	gint i;
	for(i=0 ; i<NUM_ELEMS(afPercentagesOfPadding) ; i++) {
		// try the next point along the line
		gdouble fFrontPadding = fTotalPadding * afPercentagesOfPadding[i];

		// move it "forward" along the line
		// (front padding) |-text-| (back padding)
		gdouble fDrawX = fX1 + (fNormalizedX * fFrontPadding);
		gdouble fDrawY = fY1 + (fNormalizedY * fFrontPadding);

		// center text "vertically" by translating "down" by half of font height
		fDrawX -= (fPerpendicularNormalizedX * fFontHeight/2);
		fDrawY -= (fPerpendicularNormalizedY * fFontHeight/2);

		//
		// Build a rectangle that surrounds the text to tell the scenemanager where we want to draw
		//
		#define B (3)	// claim an extra border around the text to keep things readable (in pixels)

		GdkPoint aBoundingPolygon[4];
		// 0 is bottom left point
		aBoundingPolygon[0].x = fDrawX - (fPerpendicularNormalizedX * B) - (fNormalizedX * B);
		aBoundingPolygon[0].y = fDrawY - (fPerpendicularNormalizedX * B) - (fNormalizedX * B);
		// 1 is upper left point
		aBoundingPolygon[1].x = fDrawX + (fPerpendicularNormalizedX * (fFontHeight+B)) - (fNormalizedX * B);
		aBoundingPolygon[1].y = fDrawY + (fPerpendicularNormalizedY * (fFontHeight+B)) - (fNormalizedY * B);
		// 2 is upper right point
		aBoundingPolygon[2].x = aBoundingPolygon[1].x + (fNormalizedX * (fLabelWidth+B+B));
		aBoundingPolygon[2].y = aBoundingPolygon[1].y + (fNormalizedY * (fLabelWidth+B+B));
		// 3 is lower right point
		aBoundingPolygon[3].x = fDrawX + (fNormalizedX * (fLabelWidth+B)) - (fPerpendicularNormalizedX * B);
		aBoundingPolygon[3].y = fDrawY + (fNormalizedY * (fLabelWidth+B)) - (fPerpendicularNormalizedY * B);
		
		// Ask whether we can draw here
		if(FALSE == scenemanager_can_draw_polygon(pMap->m_pSceneManager, aBoundingPolygon, 4)) {
			continue;
		}
		
		gdouble fAngleInRadians = atan(fRise / fRun);	// delay expensive atan() until we've found a place we can draw

		if(fRun < 0.0) fAngleInRadians += M_PI;			// XXX: Why do we do this?

#ifdef ENABLE_ROUND_DOWN_TEXT_ANGLES
		fAngleInRadians = floor((fAngleInRadians * ROUND_DOWN_TEXT_ANGLE) + 0.5) / ROUND_DOWN_TEXT_ANGLE;
#endif

		cairo_save(pCairo);								// XXX: do we need this?

		cairo_set_source_rgb(pCairo, 0.0,0.0,0.0);		// XXX: this should be a style setting

		// Draw a "halo" around text if the style calls for it
		gdouble fHaloSize = pLabelStyle->m_afHaloAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
		if(fHaloSize >= 0) {
			// Halo = stroking the text path with a fat white line
			cairo_save(pCairo);
			cairo_move_to(pCairo, fDrawX, fDrawY);
			cairo_rotate(pCairo, fAngleInRadians);
			cairo_text_path(pCairo, pszLabel);
			cairo_set_line_width(pCairo, fHaloSize);
			cairo_set_source_rgb(pCairo, 1.0,1.0,1.0);	// XXX: this should be a style setting
			cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_BEVEL);
			//cairo_set_miter_limit(pCairo, 0.1);
			cairo_stroke(pCairo);
			cairo_restore(pCairo);
		}
		cairo_move_to(pCairo, fDrawX, fDrawY);
		cairo_rotate(pCairo, fAngleInRadians);
		cairo_show_text(pCairo, pszLabel);
		cairo_restore(pCairo);

		// claim the space this took up and the label (so it won't be drawn twice)
		scenemanager_claim_polygon(pMap->m_pSceneManager, aBoundingPolygon, 4);
		scenemanager_claim_label(pMap->m_pSceneManager, pszLabel);

		// success
		break;
	}
	cairo_restore(pCairo);
}


//
// Draw a label along a multi-point line
//
static gint map_draw_cairo_road_label_position_sort(gconstpointer pA, gconstpointer pB)
{
	labelposition_t *pPosA = *(labelposition_t **)pA;
	labelposition_t *pPosB = *(labelposition_t **)pB;

	if(pPosA->m_fScore > pPosB->m_fScore) return -1;
	return 1;
}

static void map_draw_cairo_road_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, GPtrArray* pPointsArray, gdouble fLineWidth, const gchar* pszLabel)
{
	if(pPointsArray->len < 2) return;

	// pass off single segments to a specialized function
	if(pPointsArray->len == 2) {
		map_draw_cairo_road_label_one_segment(pMap, pCairo, pLabelStyle, pRenderMetrics, pPointsArray, fLineWidth, pszLabel);
		return;
	}

	// XXX: Below code is broken.  For now we can only label 2-point roads. :(
	return;

	if(pPointsArray->len > ROAD_MAX_SEGMENTS) {
		g_warning("not drawing label for road '%s' with > %d segments.\n", pszLabel, ROAD_MAX_SEGMENTS);
		return;
	}

//         gdouble fFontSize = pLabelStyle->m_afFontSizeAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
//         if(fFontSize == 0) return;

	// Request permission to draw this label.  This prevents multiple labels too close together.
	// NOTE: Currently no location is used, only allows one of each text string per draw
	if(!scenemanager_can_draw_label_at(pMap->m_pSceneManager, pszLabel, NULL)) {
		return;
	}

	gchar* pszFontFamily = ROAD_FONT;		// XXX: remove hardcoded font

	cairo_save(pCairo);

	// get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszLabel, &extents);

	// now find the ideal location

	// place to store potential label positions
	labelposition_t aPositions[ROAD_MAX_SEGMENTS];
	gdouble aSlopes[ROAD_MAX_SEGMENTS];

	mappoint_t* apPoints[ROAD_MAX_SEGMENTS];
	gint nNumPoints = pPointsArray->len;

	mappoint_t* pMapPoint1;
	mappoint_t* pMapPoint2;

	// load point string into an array
	gint iRead;
	for(iRead=0 ; iRead<nNumPoints ; iRead++) {
		apPoints[iRead] = g_ptr_array_index(pPointsArray, iRead);
	}

	// measure total line length
	gdouble fTotalLineLength = 0.0;
	gint nPositions = 1;
	gint iPoint, iPosition;

	for(iPoint=1 ; iPoint<nNumPoints ; iPoint++) {
		pMapPoint1 = apPoints[iPoint-1];
		pMapPoint2 = apPoints[iPoint];

		gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
		gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
		gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
		gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

		// determine slope of the line
		gdouble fRise = fY2 - fY1;
		gdouble fRun = fX2 - fX1;
		gdouble fSlope = (fRun==0) ? G_MAXDOUBLE : (fRise/fRun);
		gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

		aSlopes[iPoint] = fSlope;
		aPositions[iPoint].m_nSegments = 0;
		aPositions[iPoint].m_iIndex = iPoint;
		aPositions[iPoint].m_fLength = 0.0;
		aPositions[iPoint].m_fRunTotal = 0.0;
		aPositions[iPoint].m_fMeanSlope = 0.0;
		aPositions[iPoint].m_fMeanAbsSlope = 0.0;

		for(iPosition = nPositions ; iPosition <= iPoint ; iPosition++) {
			aPositions[iPosition].m_fLength += fLineLength;
			aPositions[iPosition].m_fRunTotal += fRun;
			aPositions[iPosition].m_nSegments++;
			aPositions[iPosition].m_fMeanSlope += fSlope;
			aPositions[iPosition].m_fMeanAbsSlope += (fSlope<0) ? -fSlope : fSlope;

			if(aPositions[iPosition].m_fLength >= extents.width) nPositions++;
		}

		fTotalLineLength += fLineLength;
	}

	// if label is longer than entire line, we're out of luck
	if(extents.width > fTotalLineLength) {
		cairo_restore(pCairo);
		return;
	}

	//GPtrArray *pPositionsPtrArray = g_ptr_array_new();
	gdouble fMaxScore = 0;
	gint iBestPosition;

	for(iPosition = 1 ; iPosition < nPositions ; iPosition++) {
		// finish calculating mean slope
		aPositions[iPosition].m_fMeanSlope /= aPositions[iPosition].m_nSegments;
		aPositions[iPosition].m_fMeanAbsSlope /= aPositions[iPosition].m_nSegments;
		
		// calculating std dev of slope
		gint iEndPoint = iPosition + aPositions[iPosition].m_nSegments;
		gdouble fDiffSquaredSum = 0.0;

		for(iPoint = iPosition ; iPoint < iEndPoint ; iPoint++) {
			gdouble fDiff = aSlopes[iPoint] - aPositions[iPosition].m_fMeanSlope;
			fDiffSquaredSum += (fDiff*fDiff);
		}

		gdouble fStdDevSlope = sqrt(fDiffSquaredSum / aPositions[iPosition].m_nSegments);

		// calculate a score between 0 (worst) and 1 (best), we want to minimize both the mean and std dev of slope
		aPositions[iPosition].m_fScore = 1.0/((aPositions[iPosition].m_fMeanAbsSlope+1.0)*(fStdDevSlope+1.0));

		// find max only
		if(aPositions[iPosition].m_fScore > fMaxScore) {
			fMaxScore = aPositions[iPosition].m_fScore;
			iBestPosition = iPosition;
		}

		// add position to the ptr array
		//g_ptr_array_add(pPositionsPtrArray, &(aPositions[iPosition]));

		/*
		g_print("%s: [%d] segments = %d, slope = %2.2f, stddev = %2.2f, score = %2.2f\n", pszLabel, iPosition,
			aPositions[iPosition].m_nSegments,
			aPositions[iPosition].m_fMeanAbsSlope,
			fStdDevSlope,
			aPositions[iPosition].m_fScore);
		*/
	}
	
	// sort postions by score
	//if(nPositions > 1)
	//	g_ptr_array_sort(pPositionsPtrArray, map_draw_cairo_road_label_position_sort);

	/*
	for(iPosition = 1 ; iPosition < nPositions ; iPosition++) {
		labelposition_t *pPos = g_ptr_array_index(pPositionsPtrArray, iPosition - 1);

		if(pPos->m_nSegments > 1)
			g_print("%s: [%d] segments = %d, slope = %2.2f, score = %2.2f\n", pszLabel, iPosition,
				pPos->m_nSegments,
				pPos->m_fMeanAbsSlope,
				pPos->m_fScore);
	}
	*/

	cairo_font_extents_t font_extents;
	cairo_current_font_extents(pCairo, &font_extents);

	gint nTotalStringLength = strlen(pszLabel);

	/*
	for(iPosition = 1 ; iPosition < nPositions ; iPosition++) {
		labelposition_t *pPos = g_ptr_array_index(pPositionsPtrArray, iPosition - 1);
		gint iBestPosition = pPos->m_iIndex;
	*/
		//g_print("trying position %d with score %2.2f\n", iBestPosition, pPos->m_fScore);

		gdouble fFrontPadding = 0.0;
		gdouble fFrontPaddingNext = (aPositions[iBestPosition].m_fLength - extents.width) / 2;
		gint iStartPoint;

		iStartPoint = iBestPosition;
		if(aPositions[iBestPosition].m_fRunTotal > 0) {
			iStartPoint = iBestPosition;
		}
		else {
			// road runs backwards, reverse everything
			iStartPoint = nNumPoints - iBestPosition - aPositions[iBestPosition].m_nSegments + 1;
			// reverse the array
			gint iRead,iWrite;
			for(iWrite=0, iRead=nNumPoints-1 ; iRead>= 0 ; iWrite++, iRead--) {
				apPoints[iWrite] = g_ptr_array_index(pPointsArray, iRead);
			}
		}

		gint iEndPoint = iStartPoint + aPositions[iBestPosition].m_nSegments;

		gint nStringStartIndex = 0;

#if 0
		gboolean bGood = TRUE;

		for(iPoint = iStartPoint ; iPoint < iEndPoint ; iPoint++) {
			if(nTotalStringLength == nStringStartIndex) break;	// done

			pMapPoint1 = apPoints[iPoint-1];
			pMapPoint2 = apPoints[iPoint];

			gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
			gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
			gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
			gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

			// determine slope of the line
			gdouble fRise = fY2 - fY1;
			gdouble fRun = fX2 - fX1;

			gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

			fFrontPadding = fFrontPaddingNext;
			fFrontPaddingNext = 0.0;

			// this is probably not needed now that we only loop over line segments that will contain some label
			if(fFrontPadding > fLineLength) {
				fFrontPaddingNext = fFrontPadding - fLineLength;
				continue;
			}

			// do this after the padding calculation to possibly save some CPU cycles
			gdouble fAngleInRadians = atan(fRise / fRun);
			if(fRun < 0.0) fAngleInRadians += M_PI;
#ifdef ENABLE_ROUND_DOWN_TEXT_ANGLES
        fAngleInRadians = floor((fAngleInRadians * ROUND_DOWN_TEXT_ANGLE) + 0.5) / ROUND_DOWN_TEXT_ANGLE;
#endif

			//g_print("(fRise(%f) / fRun(%f)) = %f, atan(fRise / fRun) = %f: ", fRise, fRun, fRise / fRun, fAngleInRadians);
			//g_print("=== NEW SEGMENT, pixel (deltaY=%f, deltaX=%f), line len=%f, (%f,%f)->(%f,%f)\n",fRise, fRun, fLineLength, pMapPoint1->m_fLatitude,pMapPoint1->m_fLongitude,pMapPoint2->m_fLatitude,pMapPoint2->m_fLongitude);
			//g_print("  has screen coords (%f,%f)->(%f,%f)\n", fX1,fY1,fX2,fY2);

			gchar azLabelSegment[DRAW_LABEL_BUFFER_LEN];

			// Figure out how much of the string we can put in this line segment
			gboolean bFoundWorkableStringLength = FALSE;
			gint nWorkableStringLength;
			if(iPoint == (nNumPoints-1)) {
				// last segment, use all of string
				nWorkableStringLength = (nTotalStringLength - nStringStartIndex);

				// copy it in to the temp buffer
				memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
				azLabelSegment[nWorkableStringLength] = '\0';
			}
			else {
				g_assert((nTotalStringLength - nStringStartIndex) > 0);

				for(nWorkableStringLength = (nTotalStringLength - nStringStartIndex) ; nWorkableStringLength >= 1 ; nWorkableStringLength--) {
					//g_print("trying nWorkableStringLength = %d\n", nWorkableStringLength);

					if(nWorkableStringLength >= DRAW_LABEL_BUFFER_LEN) break;

					// copy it in to the temp buffer
					memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
					azLabelSegment[nWorkableStringLength] = '\0';
	
					//g_print("azLabelSegment = %s\n", azLabelSegment);
	
					// measure the label
					cairo_text_extents(pCairo, azLabelSegment, &extents);

					// if we're skipping ahead some (frontpadding), effective line length is smaller, so subtract padding
					if(extents.width <= (fLineLength - fFrontPadding)) {
						//g_print("found length %d for %s\n", nWorkableStringLength, azLabelSegment);	
						bFoundWorkableStringLength = TRUE;

						// if we have 3 pixels, and we use 2, this should be NEGATIVE 1
						// TODO: we should only really do this if the next segment doesn't take a huge bend
						fFrontPaddingNext = extents.width - (fLineLength - fFrontPadding);
						fFrontPaddingNext /= 2;	// no good explanation for this
						break;
					}
				}

				if(!bFoundWorkableStringLength) {
					// write one character and set some frontpadding for the next piece
					g_assert(nWorkableStringLength == 0);
					nWorkableStringLength = 1;

					// give the next segment some padding if we went over on this segment
					fFrontPaddingNext = extents.width - (fLineLength - fFrontPadding);
					//g_print("forcing a character (%s) on small segment, giving next segment front-padding: %f\n", azLabelSegment, fFrontPaddingNext);
				}
			}

			// move the current index up
			nStringStartIndex += nWorkableStringLength;

			// Normalize (make length = 1.0) by dividing by line length
			// This makes a line with length 1 from the origin (0,0)
			gdouble fNormalizedX = fRun / fLineLength;
			gdouble fNormalizedY = fRise / fLineLength;

			// CENTER IT on the line ?
			// (buffer space) |-text-| (buffer space)
			// ======================================
			//gdouble fHalfBufferSpace = ((fLineLength - extents.width)/2);
			gdouble fDrawX = fX1 + (fNormalizedX * fFrontPadding);
			gdouble fDrawY = fY1 + (fNormalizedY * fFrontPadding);

			// NOTE: ***Swap the X and Y*** and normalize (make length = 1.0) by dividing by line length
			// This makes a perpendicular line with length 1 from the origin (0,0)
			gdouble fPerpendicularNormalizedX = fRise / fLineLength;
			gdouble fPerpendicularNormalizedY = -(fRun / fLineLength);

			// we want the normal pointing towards the top of the screen.  that's the negative Y direction.
			//if(fPerpendicularNormalizedY > 0) fPerpendicularNormalizedY = -fPerpendicularNormalizedY;	

			fDrawX -= (fPerpendicularNormalizedX * font_extents.ascent/2);
			fDrawY -= (fPerpendicularNormalizedY * font_extents.ascent/2);

			GdkPoint aBoundingPolygon[4];
			// 0 is bottom left point
			aBoundingPolygon[0].x = fDrawX;	
			aBoundingPolygon[0].y = fDrawY;
			// 1 is upper left point
			aBoundingPolygon[1].x = fDrawX + (fPerpendicularNormalizedX * font_extents.ascent);
			aBoundingPolygon[1].y = fDrawY + (fPerpendicularNormalizedY * font_extents.ascent);
			// 2 is upper right point
			aBoundingPolygon[2].x = aBoundingPolygon[1].x + (fNormalizedX * extents.width);
			aBoundingPolygon[2].y = aBoundingPolygon[1].y + (fNormalizedY * extents.width);
			// 2 is lower right point
			aBoundingPolygon[3].x = fDrawX + (fNormalizedX * extents.width);
			aBoundingPolygon[3].y = fDrawY + (fNormalizedY * extents.width);

			if(FALSE == scenemanager_can_draw_polygon(pMap->m_pSceneManager, aBoundingPolygon, 4)) {
				bGood = FALSE;
				break;
			}

		}

		// try next position
		if(bGood == FALSE)
			continue;
		
		// reset these
		fFrontPadding = 0.0;
		fFrontPaddingNext = (aPositions[iBestPosition].m_fLength - extents.width) / 2;
		nStringStartIndex = 0;
#endif

		// draw it
		for(iPoint = iStartPoint ; iPoint < iEndPoint ; iPoint++) {
			if(nTotalStringLength == nStringStartIndex) break;	// done

			pMapPoint1 = apPoints[iPoint-1];
			pMapPoint2 = apPoints[iPoint];

			gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->m_fLongitude);
			gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->m_fLatitude);
			gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->m_fLongitude);
			gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->m_fLatitude);

			// determine slope of the line
			gdouble fRise = fY2 - fY1;
			gdouble fRun = fX2 - fX1;

			gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

			fFrontPadding = fFrontPaddingNext;
			fFrontPaddingNext = 0.0;

			// do this after the padding calculation to possibly save some CPU cycles
			gdouble fAngleInRadians = atan(fRise / fRun);
			if(fRun < 0.0) fAngleInRadians += M_PI;
#ifdef ENABLE_ROUND_DOWN_TEXT_ANGLES
        fAngleInRadians = floor((fAngleInRadians * ROUND_DOWN_TEXT_ANGLE) + 0.5) / ROUND_DOWN_TEXT_ANGLE;
#endif

			//g_print("(fRise(%f) / fRun(%f)) = %f, atan(fRise / fRun) = %f: ", fRise, fRun, fRise / fRun, fAngleInRadians);
			//g_print("=== NEW SEGMENT, pixel (deltaY=%f, deltaX=%f), line len=%f, (%f,%f)->(%f,%f)\n",fRise, fRun, fLineLength, pMapPoint1->m_fLatitude,pMapPoint1->m_fLongitude,pMapPoint2->m_fLatitude,pMapPoint2->m_fLongitude);
			//g_print("  has screen coords (%f,%f)->(%f,%f)\n", fX1,fY1,fX2,fY2);

			gchar azLabelSegment[DRAW_LABEL_BUFFER_LEN];

			// Figure out how much of the string we can put in this line segment
			gboolean bFoundWorkableStringLength = FALSE;
			gint nWorkableStringLength;
			if(iPoint == (nNumPoints-1)) {
				// last segment, use all of string
				nWorkableStringLength = (nTotalStringLength - nStringStartIndex);

				// copy it in to the temp buffer
				memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
				azLabelSegment[nWorkableStringLength] = '\0';
			}
			else {
				g_assert((nTotalStringLength - nStringStartIndex) > 0);

				for(nWorkableStringLength = (nTotalStringLength - nStringStartIndex) ; nWorkableStringLength >= 1 ; nWorkableStringLength--) {
					//g_print("trying nWorkableStringLength = %d\n", nWorkableStringLength);

					if(nWorkableStringLength >= DRAW_LABEL_BUFFER_LEN) break;

					// copy it in to the temp buffer
					memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
					azLabelSegment[nWorkableStringLength] = '\0';
	
					//g_print("azLabelSegment = %s\n", azLabelSegment);
	
					// measure the label
					cairo_text_extents(pCairo, azLabelSegment, &extents);

					// if we're skipping ahead some (frontpadding), effective line length is smaller, so subtract padding
					if(extents.width <= (fLineLength - fFrontPadding)) {
						//g_print("found length %d for %s\n", nWorkableStringLength, azLabelSegment);	
						bFoundWorkableStringLength = TRUE;

						// if we have 3 pixels, and we use 2, this should be NEGATIVE 1
						// TODO: we should only really do this if the next segment doesn't take a huge bend
						fFrontPaddingNext = extents.width - (fLineLength - fFrontPadding);
						fFrontPaddingNext /= 2;	// no good explanation for this
						break;
					}
				}

				if(!bFoundWorkableStringLength) {
					// write one character and set some frontpadding for the next piece
					g_assert(nWorkableStringLength == 0);
					nWorkableStringLength = 1;

					// give the next segment some padding if we went over on this segment
					fFrontPaddingNext = extents.width - (fLineLength - fFrontPadding);
					//g_print("forcing a character (%s) on small segment, giving next segment front-padding: %f\n", azLabelSegment, fFrontPaddingNext);
				}
			}

			// move the current index up
			nStringStartIndex += nWorkableStringLength;

			// Normalize (make length = 1.0) by dividing by line length
			// This makes a line with length 1 from the origin (0,0)
			gdouble fNormalizedX = fRun / fLineLength;
			gdouble fNormalizedY = fRise / fLineLength;

			// CENTER IT on the line ?
			// (buffer space) |-text-| (buffer space)
			// ======================================
			//gdouble fHalfBufferSpace = ((fLineLength - extents.width)/2);
			gdouble fDrawX = fX1 + (fNormalizedX * fFrontPadding);
			gdouble fDrawY = fY1 + (fNormalizedY * fFrontPadding);

			// NOTE: ***Swap the X and Y*** and normalize (make length = 1.0) by dividing by line length
			// This makes a perpendicular line with length 1 from the origin (0,0)
			gdouble fPerpendicularNormalizedX = fRise / fLineLength;
			gdouble fPerpendicularNormalizedY = -(fRun / fLineLength);

			// we want the normal pointing towards the top of the screen.  that's the negative Y direction.
			//if(fPerpendicularNormalizedY > 0) fPerpendicularNormalizedY = -fPerpendicularNormalizedY;	

			fDrawX -= (fPerpendicularNormalizedX * font_extents.ascent/2);
			fDrawY -= (fPerpendicularNormalizedY * font_extents.ascent/2);

			GdkPoint aBoundingPolygon[4];
			// 0 is bottom left point
			aBoundingPolygon[0].x = fDrawX;	
			aBoundingPolygon[0].y = fDrawY;
			// 1 is upper left point
			aBoundingPolygon[1].x = fDrawX + (fPerpendicularNormalizedX * font_extents.ascent);
			aBoundingPolygon[1].y = fDrawY + (fPerpendicularNormalizedY * font_extents.ascent);
			// 2 is upper right point
			aBoundingPolygon[2].x = aBoundingPolygon[1].x + (fNormalizedX * extents.width);
			aBoundingPolygon[2].y = aBoundingPolygon[1].y + (fNormalizedY * extents.width);
			// 3 is lower right point
			aBoundingPolygon[3].x = fDrawX + (fNormalizedX * extents.width);
			aBoundingPolygon[3].y = fDrawY + (fNormalizedY * extents.width);

			cairo_save(pCairo);
			cairo_set_source_rgb(pCairo, 0.0,0.0,0.0);
			cairo_set_alpha(pCairo, 1.0);

			gdouble fHaloSize = pLabelStyle->m_afHaloAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
			if(fHaloSize >= 0) {
				cairo_save(pCairo);
				cairo_move_to(pCairo, fDrawX, fDrawY);
				cairo_rotate(pCairo, fAngleInRadians);
				cairo_text_path(pCairo, azLabelSegment);
				cairo_set_line_width(pCairo, fHaloSize);
				cairo_set_source_rgb(pCairo, 1.0,1.0,1.0);
				cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_BEVEL);
				//cairo_set_miter_limit(pCairo, 0.1);
				cairo_stroke(pCairo);
				cairo_restore(pCairo);
			}
			cairo_move_to(pCairo, fDrawX, fDrawY);
			cairo_rotate(pCairo, fAngleInRadians);
			cairo_show_text(pCairo, azLabelSegment);
			//cairo_fill(pCairo);
			cairo_restore(pCairo);

			// claim the space this took up
			scenemanager_claim_polygon(pMap->m_pSceneManager, aBoundingPolygon, 4);
		}

		cairo_restore(pCairo);

		// claim the label (so it won't be drawn twice)
		scenemanager_claim_label(pMap->m_pSceneManager, pszLabel);

	/*
		// success
		break;
	}

	g_ptr_array_free(pPositionsPtrArray, FALSE);
	*/
}
/*
//
// Draw a single polygon label
//
void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, GPtrArray* pPointsArray, const gchar* pszLabel)
{
	if(pPointsArray->len < 3) return;

	if(FALSE == scenemanager_can_draw_label_at(pMap->m_pSceneManager, pszLabel, NULL)) {
		return;
	}

	//gdouble fTotalX = 0.0;
	//gdouble fTotalY = 0.0;

	gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
	gdouble fMinLat = MAX_LATITUDE;
	gdouble fMaxLon = MIN_LONGITUDE;
	gdouble fMinLon = MAX_LONGITUDE;

	mappoint_t* pMapPoint;
	gdouble fX;
	gdouble fY;
	gint i;
	for(i=0 ; i<pPointsArray->len ; i++) {
		pMapPoint = g_ptr_array_index(pPointsArray, i);

		// find polygon bounding box for visibility test below
		fMaxLat = max(pMapPoint->m_fLatitude,fMaxLat);
		fMinLat = min(pMapPoint->m_fLatitude,fMinLat);
		fMaxLon = max(pMapPoint->m_fLongitude,fMaxLon);
		fMinLon = min(pMapPoint->m_fLongitude,fMinLon);

		// sum up Xs and Ys (we'll take an average later)
		//fTotalX += SCALE_X(pRenderMetrics, pMapPoint->m_fLongitude);
		//fTotalY += SCALE_Y(pRenderMetrics, pMapPoint->m_fLatitude);
	}

	// rectangle overlap test
	if(fMaxLat < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
	   || fMaxLon < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
	   || fMinLat > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
	   || fMinLon > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
	{
	    return;	// not visible
	}

	gdouble fDrawX = SCALE_X(pRenderMetrics, (fMinLon + fMaxLon) / 2); 	//fMinX + fPolygonWidth/2;	//fTotalX / pPointString->m_pPointsArray->len;
	gdouble fDrawY = SCALE_Y(pRenderMetrics, (fMinLat + fMaxLat) / 2);	//fMinY + fPolygonHeight/2; 	//fTotalY / pPointString->m_pPointsArray->len;

#define MIN_AREA_LABEL_LINE_LENGTH	(4)
#define MAX_AREA_LABEL_LINE_LENGTH	(10)

	gchar** aLines = util_split_words_onto_two_lines(pszLabel, MIN_AREA_LABEL_LINE_LENGTH, MAX_AREA_LABEL_LINE_LENGTH);

	cairo_save(pCairo);

	// Get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, aLines[0], &extents);
	gint nWidth = extents.width;
	gint nHeight = extents.height;

	// add second line if present
	if(aLines[1]) {
		cairo_text_extents(pCairo, aLines[1], &extents);
		nWidth = max(nWidth, extents.width);
		nHeight += extents.height;
	}

	fDrawX -= (extents.width / 2);
	fDrawY += (extents.height / 2);

	// check permission with scenemanager
	GdkRectangle rcLabelOutline;
	rcLabelOutline.x = (gint)fDrawX;
	rcLabelOutline.width = nWidth;
	rcLabelOutline.y = ((gint)fDrawY) - nHeight;
	rcLabelOutline.height = nHeight;
	if(FALSE == scenemanager_can_draw_rectangle(pMap->m_pSceneManager, &rcLabelOutline)) {
		cairo_restore(pCairo);
		g_strfreev(aLines);
		return;
	}
	// claim it!  Now no one else will draw text here.
	scenemanager_claim_rectangle(pMap->m_pSceneManager, &rcLabelOutline);

	gdouble fHaloSize = pLabelStyle->m_afHaloAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
	if(fHaloSize >= 0) {
		cairo_save(pCairo);
			cairo_move_to(pCairo, fDrawX, fDrawY);
			cairo_text_path(pCairo, pszLabel);
			cairo_set_line_width(pCairo, fHaloSize);
			cairo_set_source_rgb (pCairo, 1.0,1.0,1.0);
			cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_BEVEL);
//				cairo_set_miter_limit(pCairo, 0.1);
			cairo_stroke(pCairo);
		cairo_restore(pCairo);
	}
	cairo_move_to(pCairo, fDrawX, fDrawY);
	cairo_show_text(pCairo, pszLabel);
	cairo_restore(pCairo);

	// Tell scenemanager that we've drawn this label
	scenemanager_claim_label(pMap->m_pSceneManager, pszLabel);
	
	g_strfreev(aLines);
}
*/

void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, GPtrArray* pPointsArray, const gchar* pszLabel)
{
	if(pPointsArray->len < 3) return;

	if(FALSE == scenemanager_can_draw_label_at(pMap->m_pSceneManager, pszLabel, NULL)) {
		return;
	}

	//gdouble fTotalX = 0.0;
	//gdouble fTotalY = 0.0;

	gdouble fMaxLat = MIN_LATITUDE;	// init to the worst possible value so first point will override
	gdouble fMinLat = MAX_LATITUDE;
	gdouble fMaxLon = MIN_LONGITUDE;
	gdouble fMinLon = MAX_LONGITUDE;

	mappoint_t* pMapPoint;
	gdouble fX;
	gdouble fY;
	gint i;
	for(i=0 ; i<pPointsArray->len ; i++) {
		pMapPoint = g_ptr_array_index(pPointsArray, i);

		// find polygon bounding box for visibility test below
		fMaxLat = max(pMapPoint->m_fLatitude,fMaxLat);
		fMinLat = min(pMapPoint->m_fLatitude,fMinLat);
		fMaxLon = max(pMapPoint->m_fLongitude,fMaxLon);
		fMinLon = min(pMapPoint->m_fLongitude,fMinLon);

		// sum up Xs and Ys (we'll take an average later)
		//fTotalX += SCALE_X(pRenderMetrics, pMapPoint->m_fLongitude);
		//fTotalY += SCALE_Y(pRenderMetrics, pMapPoint->m_fLatitude);
	}

	// rectangle overlap test
	if(fMaxLat < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
	   || fMaxLon < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
	   || fMinLat > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
	   || fMinLon > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
	{
	    return;	// not visible
	}

	gdouble fDrawX = SCALE_X(pRenderMetrics, (fMinLon + fMaxLon) / 2); 	//fMinX + fPolygonWidth/2;	//fTotalX / pPointString->m_pPointsArray->len;
	gdouble fDrawY = SCALE_Y(pRenderMetrics, (fMinLat + fMaxLat) / 2);	//fMinY + fPolygonHeight/2; 	//fTotalY / pPointString->m_pPointsArray->len;

	cairo_save(pCairo);

		// Get total width of string
		cairo_text_extents_t extents;
		cairo_text_extents(pCairo, pszLabel, &extents);

		// is the text too big for the polygon?
//                 if((extents.width > (fPolygonWidth * 1.5)) || (extents.height > (fPolygonHeight * 1.5))) {
//                         cairo_restore(pCairo);
//                         return;
//                 }

		fDrawX -= (extents.width / 2);
		fDrawY += (extents.height / 2);

		// check permission with scenemanager
		GdkRectangle rcLabelOutline;
		rcLabelOutline.x = (gint)fDrawX;
		rcLabelOutline.width = extents.width;
		rcLabelOutline.y = ((gint)fDrawY) - extents.height;
		rcLabelOutline.height = extents.height;
		if(FALSE == scenemanager_can_draw_rectangle(pMap->m_pSceneManager, &rcLabelOutline)) {
			cairo_restore(pCairo);
			return;
		}
		// claim it!  Now no one else will draw text here.
		scenemanager_claim_rectangle(pMap->m_pSceneManager, &rcLabelOutline);

		gdouble fHaloSize = pLabelStyle->m_afHaloAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
		if(fHaloSize >= 0) {
			cairo_save(pCairo);
				cairo_move_to(pCairo, fDrawX, fDrawY);
				cairo_text_path(pCairo, pszLabel);
				cairo_set_line_width(pCairo, fHaloSize);
				cairo_set_source_rgb (pCairo, 1.0,1.0,1.0);
				cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_BEVEL);
//				cairo_set_miter_limit(pCairo, 0.1);
				cairo_stroke(pCairo);
			cairo_restore(pCairo);
		}
		cairo_move_to(pCairo, fDrawX, fDrawY);
		cairo_show_text(pCairo, pszLabel);
	cairo_restore(pCairo);

	// Tell scenemanager that we've drawn this label
	scenemanager_claim_label(pMap->m_pSceneManager, pszLabel);
}

/*
static void map_draw_cairo_locations(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics)
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
			map_draw_cairo_locationset(pMap, pCairo, pRenderMetrics, pLocationSet, pLocationsArray);
		}
		else {
			// none to draw
		}
	}
}
*/

static void map_draw_cairo_locationselection_outline(cairo_t *pCairo, gdouble fPointX, gdouble fPointY, const screenrect_t* pRect, screenrect_t* pCloseRect)
{
	cairo_save(pCairo);

	gdouble fCorner = 30.0;
	gdouble fArrowWidth = 30.0; 

	gdouble fBoxX = pRect->m_A.m_nX + 0.5;
	gdouble fBoxY = pRect->m_A.m_nY + 0.5;
	gdouble fBoxWidth = pRect->m_B.m_nX - pRect->m_A.m_nX;
	gdouble fBoxHeight = pRect->m_B.m_nY - pRect->m_A.m_nY;

	typedef enum {
		SIDE_BOTTOM = 1,
		SIDE_TOP,
		SIDE_LEFT,
		SIDE_RIGHT
	} ESide;

	ESide eArrowSide = SIDE_BOTTOM;

	// =======================================================================================
	// BEGIN drawing balloon

#define	 CCPOE	(4.0)	// # pixels to move the control points past the rectangle's corner points
						// a higher number means less curve.  (what does it stand for?  Who cares?  It's short!)

	// move to spot just below top left curve
	cairo_move_to(pCairo, fBoxX, (fBoxY + fCorner));

	// top left curver
	cairo_curve_to(pCairo, 	(fBoxX), (fBoxY - CCPOE),
				(fBoxX - CCPOE), (fBoxY),
				(fBoxX + fCorner), (fBoxY));

	// top line
	cairo_line_to(pCairo, (fBoxX + fBoxWidth) - fCorner, fBoxY);

	// top right corner
	cairo_curve_to(pCairo,	(fBoxX + fBoxWidth + CCPOE), (fBoxY),
				(fBoxX + fBoxWidth), (fBoxY - CCPOE),
				(fBoxX + fBoxWidth), (fBoxY + fCorner));

	// right line
	cairo_line_to(pCairo, (fBoxX + fBoxWidth), (fBoxY + fBoxHeight) - fCorner);

	// bottom right corner
	cairo_curve_to(pCairo,  (fBoxX + fBoxWidth), (fBoxY + fBoxHeight + CCPOE),
				(fBoxX + fBoxWidth + CCPOE), (fBoxY + fBoxHeight), 
				(fBoxX + fBoxWidth) - fCorner, (fBoxY + fBoxHeight));

	// bottom line (drawing right to left)
	if(eArrowSide == SIDE_BOTTOM) {
		// right side of arrow
		cairo_line_to(pCairo, (fBoxX + fBoxWidth/2) + fArrowWidth, (fBoxY + fBoxHeight));

		// point of array
		cairo_line_to(pCairo, fPointX, fPointY);

		// left side of arrow
		cairo_line_to(pCairo, (fBoxX + fBoxWidth/2) - 0, (fBoxY + fBoxHeight));

		// right side of bottom left corner
		cairo_line_to(pCairo, (fBoxX + fCorner), (fBoxY + fBoxHeight));
	}
	else {
		cairo_line_to(pCairo, (fBoxX + fCorner), (fBoxY + fBoxHeight));
	}

	// bottom left corner
	cairo_curve_to(pCairo,  (fBoxX - CCPOE), (fBoxY + fBoxHeight),
				(fBoxX), (fBoxY + fBoxHeight + CCPOE),
				(fBoxX), (fBoxY + fBoxHeight) - fCorner);

	// left line, headed up
	cairo_line_to(pCairo, (fBoxX), (fBoxY + fCorner));

	// DONE drawing balloon
	// =======================================================================================

	// fill then stroke
	cairo_set_source_rgba(pCairo, 1.0, 1.0, 1.0, 1.0);
	cairo_fill_preserve(pCairo);

	cairo_set_source_rgb(pCairo, 0.0, 0.0, 0.0);
	cairo_set_line_width(pCairo, 1.0);
	cairo_stroke(pCairo);

	// BEGIN [X]
	gdouble fCloseRectRelief = 8.0;		// Distance from edge
	gdouble fCloseRectWidth = 13.0;

	cairo_set_line_width(pCairo, 1.0);
	cairo_set_source_rgb(pCairo, 0.5,0.5,0.5);

	cairo_move_to(pCairo, (fBoxX + fBoxWidth) - fCloseRectRelief, fBoxY + fCloseRectRelief);
	cairo_rel_line_to(pCairo, 0.0, fCloseRectWidth);
	cairo_rel_line_to(pCairo, -fCloseRectWidth, 0.0);
	cairo_rel_line_to(pCairo, 0.0, -fCloseRectWidth);
	cairo_rel_line_to(pCairo, fCloseRectWidth, 0.0);
	cairo_stroke(pCairo);

	pCloseRect->m_A.m_nX = (gint)((fBoxX + fBoxWidth) - fCloseRectRelief) - fCloseRectWidth;
	pCloseRect->m_A.m_nY = (gint)fBoxY + fCloseRectRelief;
	pCloseRect->m_B.m_nX = (gint)((fBoxX + fBoxWidth) - fCloseRectRelief);
	pCloseRect->m_B.m_nY = (gint)fBoxY + fCloseRectRelief + fCloseRectWidth;

	// draw the X itself
	cairo_set_line_width(pCairo, 2.0);
	cairo_move_to(pCairo, (((fBoxX + fBoxWidth) - fCloseRectRelief) - fCloseRectWidth) + 3, fBoxY + fCloseRectRelief + 3);
	cairo_rel_line_to(pCairo, fCloseRectWidth - 6, fCloseRectWidth - 6);

	cairo_move_to(pCairo, (((fBoxX + fBoxWidth) - fCloseRectRelief) - fCloseRectWidth) + 3, (fBoxY + fCloseRectRelief + fCloseRectWidth) - 3);
	cairo_rel_line_to(pCairo, (fCloseRectWidth - 6), -(fCloseRectWidth - 6));
	cairo_stroke(pCairo);
	// END [X]

	cairo_restore(pCairo);
}

static void map_draw_cairo_locationselection(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray)
{
	cairo_save(pCairo);

	gint i;
	for(i=0 ; i<pLocationSelectionArray->len ; i++) {
		locationselection_t* pLocationSelection = g_ptr_array_index(pLocationSelectionArray, i);

		pLocationSelection->m_nNumURLs=0;

		// bounding box test
		if(pLocationSelection->m_Coordinates.m_fLatitude < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLatitude
		   || pLocationSelection->m_Coordinates.m_fLongitude < pRenderMetrics->m_rWorldBoundingBox.m_A.m_fLongitude
		   || pLocationSelection->m_Coordinates.m_fLatitude > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLatitude
		   || pLocationSelection->m_Coordinates.m_fLongitude > pRenderMetrics->m_rWorldBoundingBox.m_B.m_fLongitude)
		{
			pLocationSelection->m_bVisible = FALSE;
			continue;   // not visible
		}
		pLocationSelection->m_bVisible = TRUE;

		gdouble fX = SCALE_X(pRenderMetrics, pLocationSelection->m_Coordinates.m_fLongitude);
		fX = floor(fX) + 0.5;
		gdouble fY = SCALE_Y(pRenderMetrics, pLocationSelection->m_Coordinates.m_fLatitude);
		fY = floor(fY) + 0.5;

		gdouble fBoxHeight = 105.0;
		gdouble fBoxWidth = 200.0;

		gdouble fOffsetX = -(fBoxWidth/3);
		gdouble fOffsetY = -40.0;

		// determine top left corner of box
		gdouble fBoxX = fX + fOffsetX;
		gdouble fBoxY = (fY - fBoxHeight) + fOffsetY;

		pLocationSelection->m_InfoBoxRect.m_A.m_nX = (gint)fBoxX;
		pLocationSelection->m_InfoBoxRect.m_A.m_nY = (gint)fBoxY;
		pLocationSelection->m_InfoBoxRect.m_B.m_nX = (gint)(fBoxX + fBoxWidth);
		pLocationSelection->m_InfoBoxRect.m_B.m_nY = (gint)(fBoxY + fBoxHeight);

		map_draw_cairo_locationselection_outline(pCairo, fX, fY, &(pLocationSelection->m_InfoBoxRect), &(pLocationSelection->m_InfoBoxCloseRect));
/*
		gchar* pszFontFamily = ROAD_FONT;	// XXX: remove hardcoded font
		gint fTitleFontSize = 15.0;
		gint fBodyFontSize = 13.0;

		// BEGIN text
		// shrink box a little
		fBoxX += 8.0;
		fBoxY += 10.0;
		fBoxWidth -= 16.0;
		fBoxHeight -= 20.0;

#define LINE_RELIEF (4.0)	// space between lines in pixels
#define BODY_RELIEF (6.0)
#define LINK_HORIZONTAL_RELIEF (10)

		const gchar* pszString;
		cairo_text_extents_t extents;
		cairo_select_font_face (pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(pCairo, fTitleFontSize);

		// draw name
		pszString = map_location_selection_get_attribute(pMap, pLocationSelection, "name");
		if(pszString == NULL) pszString = "";	// unnamed POI?!
		cairo_text_extents(pCairo, pszString, &extents);
		gdouble fLabelWidth = extents.width;
		gdouble fFontHeight = extents.height;
		cairo_set_source_rgb(pCairo, 0.1,0.1,0.1);	// normal text color

		gdouble fTextOffsetX = fBoxX;
		gdouble fTextOffsetY = fBoxY + fFontHeight;

		cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
		cairo_show_text(pCairo, pszString);

		// draw address, line 1
		pszString = map_location_selection_get_attribute(pMap, pLocationSelection, "address");
		if(pszString != NULL) {
			gchar** aLines = g_strsplit(pszString,"\n", 0);	// "\n" = delimeters, 0 = no max #

			gchar* pszLine;
			if(aLines[0]) {
				pszLine = aLines[0];
				cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
				cairo_set_font_size(pCairo, fBodyFontSize);
				cairo_text_extents(pCairo, pszLine, &extents);
				fLabelWidth = extents.width;
				fFontHeight = extents.height;

				fTextOffsetY += (BODY_RELIEF + fFontHeight);
				cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
				cairo_show_text(pCairo, pszLine);

				if(aLines[1]) {
					//g_print("line 2\n");
					// draw address, line 2
					pszLine = aLines[1];

					fTextOffsetY += (LINE_RELIEF + fFontHeight);
					cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
					cairo_show_text(pCairo, pszLine);
				}
			}
			g_strfreev(aLines);	// free the array of strings	
		}

		// draw phone number
		pszString = "(617) 576-1369";
		cairo_text_extents(pCairo, pszString, &extents);
		fLabelWidth = extents.width;
		fFontHeight = extents.height;

		fTextOffsetY += (LINE_RELIEF + fFontHeight);
		cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
		cairo_set_source_rgb(pCairo, 0.1,0.1,0.1);	// normal text color
		cairo_show_text(pCairo, pszString);
		// draw underline
//                 cairo_text_extents(pCairo, pszString, &extents);
//                 fLabelWidth = extents.width;
//                 fFontHeight = extents.height;
//                 map_draw_text_underline(pCairo, fLabelWidth);

		// draw website link
		fTextOffsetX = fBoxX;
		fTextOffsetY = fBoxY + fBoxHeight;
		pszString = "1369coffeehouse.com";
		cairo_text_extents(pCairo, pszString, &extents);
		fLabelWidth = extents.width;
		fFontHeight = extents.height;
		cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
		cairo_set_source_rgb(pCairo, 0.1,0.1,0.4);	// remote link color
		map_draw_text_underline(pCairo, fLabelWidth);
		cairo_show_text(pCairo, pszString);

		screenrect_t* pRect = &(pLocationSelection->m_aURLs[pLocationSelection->m_nNumURLs].m_Rect);
		pRect->m_A.m_nX = fTextOffsetX;
		pRect->m_A.m_nY = fTextOffsetY - fFontHeight;
		pRect->m_B.m_nX = fTextOffsetX + fLabelWidth;
		pRect->m_B.m_nY = fTextOffsetY;
		pLocationSelection->m_aURLs[pLocationSelection->m_nNumURLs].m_pszURL = g_strdup(pszString);
		pLocationSelection->m_nNumURLs++;

		// draw 'edit' link
		pszString = "edit";
		cairo_set_source_rgb(pCairo, 0.1,0.4,0.1);	// local link color
		cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(pCairo, fBodyFontSize);
		cairo_text_extents(pCairo, pszString, &extents);
		fLabelWidth = extents.width;
		fFontHeight = extents.height;

		// bottom edge, right justified
		fTextOffsetX = (fBoxX + fBoxWidth) - fLabelWidth;
		fTextOffsetY = (fBoxY + fBoxHeight);

		cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
		cairo_show_text(pCairo, pszString);
		map_draw_text_underline(pCairo, fLabelWidth);
		
		pRect = &(pLocationSelection->m_EditRect);
		pRect->m_A.m_nX = fTextOffsetX;
		pRect->m_A.m_nY = fTextOffsetY - fFontHeight;
		pRect->m_B.m_nX = fTextOffsetX + fLabelWidth;
		pRect->m_B.m_nY = fTextOffsetY;
*/
	}
	cairo_restore(pCairo);
}

#ifdef ROADSTER_DEAD_CODE
/*
<<<<<<< map_draw_cairo.c
=======
//
// Draw a crosshair
//
#define CROSSHAIR_LINE_RELIEF   (6)
#define CROSSHAIR_LINE_LENGTH   (12)
#define CROSSHAIR_CIRCLE_RADIUS (12)

static void map_draw_cairo_crosshair(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics)
{
	cairo_save(pCairo);
		cairo_set_line_width(pCairo, 1.0);
		cairo_set_source_rgb(pCairo, 0.1, 0.1, 0.1);
		cairo_set_alpha(pCairo, 1.0);
		
		// left line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) - (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH), (pRenderMetrics->m_nWindowHeight/2));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) - (CROSSHAIR_LINE_RELIEF), (pRenderMetrics->m_nWindowHeight/2));
		// right line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) + (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH), (pRenderMetrics->m_nWindowHeight/2));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2) + (CROSSHAIR_LINE_RELIEF), (pRenderMetrics->m_nWindowHeight/2));
		// top line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) - (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) - (CROSSHAIR_LINE_RELIEF));
		// bottom line
		cairo_move_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) + (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH));
		cairo_line_to(pCairo, (pRenderMetrics->m_nWindowWidth/2), (pRenderMetrics->m_nWindowHeight/2) + (CROSSHAIR_LINE_RELIEF));
		cairo_stroke(pCairo);
		
		cairo_arc(pCairo, pRenderMetrics->m_nWindowWidth/2, pRenderMetrics->m_nWindowHeight/2, CROSSHAIR_CIRCLE_RADIUS, 0, 2*M_PI);
		cairo_stroke(pCairo);
	cairo_restore(pCairo);
}

void map_draw_cairo_locations(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics)
{
	location_t loc;
	location_t* pLoc = &loc;

	// XXX:	debug
	pLoc->m_pszName = "some address., Cambridge, MA, 02140";
	pLoc->m_Coordinates.m_fLongitude = 0;
	pLoc->m_Coordinates.m_fLatitude = 0;

	gchar* pszFontFamily = ROAD_FONT;
	gdouble fFontSize = 14.0;

	//
	// Draw
	//
	cairo_save(pCairo);

	cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(pCairo, fFontSize);	

	// measure the label
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pLoc->m_pszName, &extents);
	gdouble fLabelWidth = extents.width;
	gdouble fLabelHeight = extents.height;

	gdouble fX = SCALE_X(pRenderMetrics, pLoc->m_Coordinates.m_fLongitude);
	gdouble fY = SCALE_Y(pRenderMetrics, pLoc->m_Coordinates.m_fLatitude);

#define LOCATION_LABEL_CELL_PADDING	(4)

	cairo_rectangle(pCairo, fX - LOCATION_LABEL_CELL_PADDING, fY-(fLabelHeight/2)-(LOCATION_LABEL_CELL_PADDING), fLabelWidth + (LOCATION_LABEL_CELL_PADDING*2), fLabelHeight + (LOCATION_LABEL_CELL_PADDING*2));
	cairo_save(pCairo);
		cairo_set_source_rgb(pCairo, 247/255.0, 247/255.0, 247/255.0);
		cairo_set_alpha(pCairo, 0.9);

		cairo_fill(pCairo);
	cairo_restore(pCairo);

	cairo_set_source_rgb(pCairo, 232/255.0, 166/255.0, 0/255.0);
	cairo_set_alpha(pCairo, 1.0);
	cairo_set_line_width(pCairo, 0.9);
	cairo_stroke(pCairo);
	
	cairo_set_source_rgb(pCairo, 0/255.0, 0/255.0, 0/255.0);
	cairo_move_to(pCairo, fX, fY+(fLabelHeight/2));
	cairo_show_text(pCairo, pLoc->m_pszName);

	cairo_restore(pCairo);
}

void map_draw_cairo_gps_trail(map_t* pMap, cairo_t* pCairo, pointstring_t* pPointString)
{
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	if(pPointString->m_pPointsArray->len > 2) {
		mappoint_t* pPoint = g_ptr_array_index(pPointString->m_pPointsArray, 0);
		
		// move to index 0
		cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

		gint i;
		for(i=1 ; i<pPointString->m_pPointsArray->len ; i++) {
			pPoint = g_ptr_array_index(pPointString->m_pPointsArray, i);
			cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
		}

		cairo_set_source_rgb(pCairo, 0.0, 0.0, 0.7);
		cairo_set_alpha(pCairo, 0.6);
		cairo_set_line_width(pCairo, 6);
		cairo_set_line_cap(pCairo, CAIRO_LINE_CAP_ROUND);
		cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_ROUND);
		cairo_set_miter_limit(pCairo, 5);
		cairo_stroke(pCairo);
	}
}
*/
#endif
