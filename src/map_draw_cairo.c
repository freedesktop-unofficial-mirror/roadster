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

#define ENABLE_LABEL_LIMIT_TO_ROAD				// take road line length into account when drawing labels!
#define	ACCEPTABLE_LINE_LABEL_OVERDRAW_IN_PIXELS (18)	// XXX: make this a run-time variable
#define ENABLE_DRAW_MAP_SCALE
//#define ENABLE_MAP_DROP_SHADOW

//#define ENABLE_HACK_AROUND_CAIRO_LINE_CAP_BUG	// enable to ensure roads have rounded caps if the style dictates
												// supposedly fixed as of 1.0 but I haven't tested yet

#define ROAD_FONT	"Free Sans" //Bitstream Vera Sans"
#define AREA_FONT	"Free Sans" // "Bitstream Vera Sans"

#define MIN_AREA_LABEL_LINE_LENGTH	(4)	// in chars
#define MAX_AREA_LABEL_LINE_LENGTH	(8)

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <math.h>
#include <string.h>

#include "main.h"
#include "map.h"
#include "map_math.h"
#include "mainwindow.h"
#include "util.h"
#include "road.h"
#include "map_style.h"
#include "location.h"
#include "locationset.h"
#include "scenemanager.h"
#include "util.h"

// Draw whole layers
static void map_draw_cairo_layer_polygons(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle);
static void map_draw_cairo_layer_lines(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle);
static void map_draw_cairo_layer_road_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle);
static void map_draw_cairo_layer_polygon_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle);

// Draw a single line/polygon/point
static void map_draw_cairo_background(map_t* pMap, cairo_t *pCairo);
static void map_draw_cairo_layer_points(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray);
//static void map_draw_cairo_locationset(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics, locationset_t* pLocationSet, GPtrArray* pLocationsArray);
//static void map_draw_cairo_locationselection(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray);

static void map_draw_cairo_layer_fill(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, maplayerstyle_t* pLayerStyle);

// Draw labels for a single line/polygon
static void map_draw_cairo_road_label(map_t* pMap, cairo_t *pCairo, maplayerstyle_t* pLayerStyle, rendermetrics_t* pRenderMetrics, GArray* pMapPointsArray, const gchar* pszLabel);
static void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, maplayerstyle_t* pLayerStyle, rendermetrics_t* pRenderMetrics, GArray* pMapPointsArray, maprect_t* pBoundingRect, const gchar* pszLabel);

// Draw map extras
static void map_draw_cairo_map_scale(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics);
static void map_draw_cairo_message(map_t* pMap, cairo_t *pCairo, gchar* pszMessage);

static struct { gint nX,nY; } g_aHaloOffsets[] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};	// larger
//static struct { gint nX,nY; } g_aHaloOffsets[] = {{1,1},{1,-1},{-1,1},{-1,-1}};	// smaller

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

void map_draw_cairo_set_rgb(cairo_t* pCairo, color_t* pColor)
{
	cairo_set_source_rgba(pCairo, pColor->fRed, pColor->fGreen, pColor->fBlue, 1.0);
}

void map_draw_cairo_set_rgba(cairo_t* pCairo, color_t* pColor)
{
	cairo_set_source_rgba(pCairo, pColor->fRed, pColor->fGreen, pColor->fBlue, pColor->fAlpha);
}

void map_draw_cairo(map_t* pMap, GPtrArray* pTiles, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags)
{
	// 1. Set draw target to X Drawable
	Display* dpy;
	Drawable drawable;
	dpy = gdk_x11_drawable_get_xdisplay(pPixmap);
	Visual *visual = DefaultVisual (dpy, DefaultScreen (dpy));
	gint width, height;
	drawable = gdk_x11_drawable_get_xid(pPixmap);

	gdk_drawable_get_size (pPixmap, &width, &height);
	cairo_surface_t *pSurface = cairo_xlib_surface_create (dpy, drawable, visual, width, height);
	cairo_t* pCairo = cairo_create (pSurface);

//    cairo_set_antialias(pCairo, CAIRO_ANTIALIAS_GRAY);	// CAIRO_ANTIALIAS_DEFAULT, CAIRO_ANTIALIAS_NONE, CAIRO_ANTIALIAS_GRAY, CAIRO_ANTIALIAS_SUBPIXEL

#ifdef ENABLE_DRAW_MAP_SCALE

#	define MAP_SCALE_WIDTH		(55)	// these are just estimates
#	define MAP_SCALE_HEIGHT 	(35)

	GdkRectangle rect = {0,height-MAP_SCALE_HEIGHT, MAP_SCALE_WIDTH,height};
	scenemanager_claim_rectangle(pMap->pSceneManager, &rect);
#endif

	// 2. Rendering
	TIMER_BEGIN(maptimer, "BEGIN RENDER MAP (cairo)");
	cairo_save(pCairo);

	// 2.1. Settings for all rendering
	cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);

	// 2.2. Render Layers
	if(pMap->pLayersArray->len == 0) {
		map_draw_cairo_message(pMap, pCairo, "The style XML file couldn't be loaded");
	}
	else {
		gint i;
		for(i=pMap->pLayersArray->len-1 ; i>=0 ; i--) {
			maplayer_t* pLayer = g_ptr_array_index(pMap->pLayersArray, i);

			gint nStyleZoomLevel = g_sZoomLevels[pRenderMetrics->nZoomLevel-1].nStyleZoomLevel;

			if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_LINES) {
				if(nDrawFlags & DRAWFLAG_GEOMETRY) {
					gint iTile;
					for(iTile=0 ; iTile < pTiles->len ; iTile++) {
						maptile_t* pTile = g_ptr_array_index(pTiles, iTile);
						map_draw_cairo_layer_lines(pMap, pCairo, pRenderMetrics,
												 pTile->apMapObjectArrays[pLayer->nDataSource],               // data
												 pLayer->paStylesAtZoomLevels[nStyleZoomLevel-1]);       // style
					}
				}
			}
			else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_POLYGONS) {
				if(nDrawFlags & DRAWFLAG_GEOMETRY) {
					gint iTile;
					for(iTile=0 ; iTile < pTiles->len ; iTile++) {
						maptile_t* pTile = g_ptr_array_index(pTiles, iTile);
						map_draw_cairo_layer_polygons(pMap, pCairo, pRenderMetrics,
												 pTile->apMapObjectArrays[pLayer->nDataSource],               // data
												 pLayer->paStylesAtZoomLevels[nStyleZoomLevel-1]);       // style
					}
				}
			}
			else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_LINE_LABELS) {
				if(nDrawFlags & DRAWFLAG_LABELS) {
					gint iTile;
					for(iTile=0 ; iTile < pTiles->len ; iTile++) {
						maptile_t* pTile = g_ptr_array_index(pTiles, iTile);
						map_draw_cairo_layer_road_labels(pMap, pCairo, pRenderMetrics,
														 pTile->apMapObjectArrays[pLayer->nDataSource],               // data
														 pLayer->paStylesAtZoomLevels[nStyleZoomLevel-1]);
					}
				}
			}
			else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_POLYGON_LABELS) {
				if(nDrawFlags & DRAWFLAG_LABELS) {
					gint iTile;
					for(iTile=0 ; iTile < pTiles->len ; iTile++) {
						maptile_t* pTile = g_ptr_array_index(pTiles, iTile);
						map_draw_cairo_layer_polygon_labels(pMap, pCairo, pRenderMetrics,
															pTile->apMapObjectArrays[pLayer->nDataSource],               // data
															pLayer->paStylesAtZoomLevels[nStyleZoomLevel-1]);
					}
				}
			}
			else if(pLayer->nDrawType == MAP_LAYER_RENDERTYPE_FILL) {
				if(nDrawFlags & DRAWFLAG_GEOMETRY) {
					map_draw_cairo_layer_fill(pMap, pCairo, pRenderMetrics, pLayer->paStylesAtZoomLevels[nStyleZoomLevel-1]);       // style
				}
			}
		}
	}

#ifdef ENABLE_MAP_DROP_SHADOW
#	define SHADOW_WIDTH	(6)
	cairo_set_source_rgba(pCairo, 0,0,0,0.1);
	cairo_rectangle(pCairo, 0, 0, pMap->MapDimensions.uWidth, SHADOW_WIDTH);
	cairo_rectangle(pCairo, 0, SHADOW_WIDTH, SHADOW_WIDTH, pMap->MapDimensions.uHeight-SHADOW_WIDTH);
	cairo_fill(pCairo);
#endif

	//map_draw_cairo_locations(pMap, pCairo, pRenderMetrics);	done with GDK
	//map_draw_cairo_locationselection(pMap, pCairo, pRenderMetrics, pMap->pLocationSelectionArray);
#ifdef ENABLE_DRAW_MAP_SCALE
	map_draw_cairo_map_scale(pMap, pCairo, pRenderMetrics);
#endif

	// 4. Cleanup
	cairo_restore(pCairo);
	TIMER_END(maptimer, "END RENDER MAP (cairo)");
}

// ==============================================
// Begin map_draw_cairo_* functions
// ==============================================

// useful for filling the screen with a color.  not much else.
void map_draw_cairo_layer_fill(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, maplayerstyle_t* pLayerStyle)
{
	if(pLayerStyle->pGlyphFill != NULL) {
	}
	else {
		// Simple color fill
		map_draw_cairo_set_rgb(pCairo, &(pLayerStyle->clrPrimary));
	}

	cairo_rectangle(pCairo, 0,0,pMap->MapDimensions.uWidth, pMap->MapDimensions.uHeight);
	cairo_fill(pCairo);

	if(pLayerStyle->pGlyphFill != NULL) {
		// Restore fill style
	}
}

// When there are no layers defined, we call this to show an error message in-window
void map_draw_cairo_message(map_t* pMap, cairo_t *pCairo, gchar* pszMessage)
{
	cairo_save(pCairo);
		// background color
		cairo_set_source_rgb(pCairo, 0.9, 0.9, 0.9);
		cairo_rectangle(pCairo, 0, 0, pMap->MapDimensions.uWidth, pMap->MapDimensions.uHeight);
		cairo_fill(pCairo);
		
		cairo_set_source_rgb(pCairo, 0.1, 0.1, 0.1);
		cairo_set_font_size(pCairo, 17);
		cairo_text_extents_t extents;
		cairo_text_extents(pCairo, pszMessage, &extents);

		// Draw centered
		cairo_move_to(pCairo, (pMap->MapDimensions.uWidth/2) - (extents.width/2), (pMap->MapDimensions.uHeight/2) + (extents.height/2));
		cairo_show_text(pCairo, pszMessage);
	cairo_restore(pCairo);
}

//
// Draw a whole layer of line labels
//
void map_draw_cairo_layer_road_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle)
{
	gint i;

	if(pLayerStyle->fFontSize == 0) return;

	gchar* pszFontFamily = ROAD_FONT;   // XXX: remove hardcoded font

	// set font for whole layer
	cairo_save(pCairo);
	cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, pLayerStyle->bFontBold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(pCairo, pLayerStyle->fFontSize);

	for(i=0 ; i<pRoadsArray->len ; i++) {
		road_t* pRoad = g_ptr_array_index(pRoadsArray, i);

		if(pRoad->pszName[0] == '\0') {
			continue;
		}

		if(!map_rects_overlap(&(pRoad->rWorldBoundingBox), &(pRenderMetrics->rWorldBoundingBox))) {
			continue;
		}

		map_draw_cairo_road_label(pMap, pCairo, pLayerStyle, pRenderMetrics, pRoad->pMapPointsArray, pRoad->pszName);
	}
	cairo_restore(pCairo);
}

//
// Draw a whole layer of polygon labels
//
void map_draw_cairo_layer_polygon_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle)
{
	if(pLayerStyle->fFontSize == 0) return;

	gchar* pszFontFamily = AREA_FONT;	// XXX: remove hardcoded font

	// set font for whole layer
	cairo_save(pCairo);
	cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, pLayerStyle->bFontBold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(pCairo, pLayerStyle->fFontSize);

	map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrPrimary));

	gint i;
	for(i=0 ; i<pRoadsArray->len ; i++) {
		road_t* pRoad = g_ptr_array_index(pRoadsArray, i);
		if(pRoad->pszName[0] == '\0') {
			continue;
		}

		if(!map_rects_overlap(&(pRoad->rWorldBoundingBox), &(pRenderMetrics->rWorldBoundingBox))) {
			continue;
		}

		map_draw_cairo_polygon_label(pMap, pCairo, pLayerStyle, pRenderMetrics, pRoad->pMapPointsArray, &(pRoad->rWorldBoundingBox), pRoad->pszName);
	}
	cairo_restore(pCairo);
}

//
// Draw a whole layer of lines
//
void map_draw_cairo_layer_lines(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle)
{
	mappoint_t* pPoint;
	road_t* pRoad;
	gint iString;
	gint iPoint;

	if(pLayerStyle->fLineWidth <= 0.0) return;	// Don't draw invisible lines
	if(pLayerStyle->clrPrimary.fAlpha == 0.0) return;

	cairo_save(pCairo);

	// Raise the tolerance way up for thin lines
	gint nCapStyle = pLayerStyle->nCapStyle;

	gdouble fTolerance;
	if(pLayerStyle->fLineWidth >= 12.0) {	// huge line, low tolerance
		fTolerance = 0.40;
	}
	else if(pLayerStyle->fLineWidth >= 6.0) {	// medium line, medium tolerance
		fTolerance = 0.8;
	}
	else {
		if(nCapStyle == CAIRO_LINE_CAP_ROUND) {
			//g_print("forcing round->square cap style\n");
			//nCapStyle = CAIRO_LINE_CAP_SQUARE;
		}

		if(pLayerStyle->fLineWidth >= 3.0) {
			fTolerance = 1.2;
		}
		else {  // smaller...
			fTolerance = 10;
		}
	}
	cairo_set_tolerance(pCairo, fTolerance);
	cairo_set_line_join(pCairo, pLayerStyle->nJoinStyle);
	cairo_set_line_cap(pCairo, nCapStyle);
	if(pLayerStyle->pDashStyle != NULL) {
		cairo_set_dash(pCairo, pLayerStyle->pDashStyle->pafDashList, pLayerStyle->pDashStyle->nDashCount, 0.0);
	}

	// Set layer attributes	
	map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrPrimary));
	cairo_set_line_width(pCairo, pLayerStyle->fLineWidth);

	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		EOverlapType eOverlapType = map_rect_a_overlap_type_with_rect_b(&(pRoad->rWorldBoundingBox), &(pRenderMetrics->rWorldBoundingBox));
		if(eOverlapType == OVERLAP_NONE) {
			continue;
		}

		if(pRoad->pMapPointsArray->len < 2) {
			continue;
		}

		pPoint = &g_array_index(pRoad->pMapPointsArray, mappoint_t, 0);

		// go to index 0
		cairo_move_to(pCairo, 
					  pLayerStyle->nPixelOffsetX + SCALE_X(pRenderMetrics, pPoint->fLongitude), 
					  pLayerStyle->nPixelOffsetY + SCALE_Y(pRenderMetrics, pPoint->fLatitude));

		// start at index 1 (0 was used above)
		for(iPoint=1 ; iPoint<pRoad->pMapPointsArray->len ; iPoint++) {
			pPoint = &g_array_index(pRoad->pMapPointsArray, mappoint_t, iPoint);//~ g_print("  point (%.05f,%.05f)\n", ScaleX(pPoint->fLongitude), ScaleY(pPoint->fLatitude));
			cairo_line_to(pCairo, 
						  pLayerStyle->nPixelOffsetX + SCALE_X(pRenderMetrics, pPoint->fLongitude), 
						  pLayerStyle->nPixelOffsetY + SCALE_Y(pRenderMetrics, pPoint->fLatitude));
		}
#ifdef ENABLE_HACK_AROUND_CAIRO_LINE_CAP_BUG
		cairo_stroke(pCairo);	// this is wrong place for it (see below)
#endif
	}

#ifndef ENABLE_HACK_AROUND_CAIRO_LINE_CAP_BUG
	// this is correct place to stroke, but we can't do this until Cairo fixes this bug:
	// http://cairographics.org/samples/xxx_multi_segment_caps.html
	cairo_stroke(pCairo);
#endif

	cairo_restore(pCairo);
}

void map_draw_cairo_polygon(cairo_t* pCairo, const rendermetrics_t* pRenderMetrics, const GArray* pMapPointsArray)
{
	// move to index 0
	mappoint_t* pPoint = &g_array_index(pMapPointsArray, mappoint_t, 0);
	cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->fLongitude), SCALE_Y(pRenderMetrics, pPoint->fLatitude));

	// start at index 1 (0 was used above)
	gint iPoint;
	for(iPoint=1 ; iPoint<pMapPointsArray->len ; iPoint++) {
		pPoint = &g_array_index(pMapPointsArray, mappoint_t, iPoint);				
		cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->fLongitude), SCALE_Y(pRenderMetrics, pPoint->fLatitude));
	}
}

void map_draw_cairo_layer_polygons(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pRoadsArray, maplayerstyle_t* pLayerStyle)
{
	road_t* pRoad;

	if(pLayerStyle->clrPrimary.fAlpha == 0.0) return;

	// Set layer attributes	
	map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrPrimary));
	cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_set_line_join(pCairo, pLayerStyle->nJoinStyle);

	gint iString;
	for(iString=0 ; iString<pRoadsArray->len ; iString++) {
		pRoad = g_ptr_array_index(pRoadsArray, iString);

		EOverlapType eOverlapType = map_rect_a_overlap_type_with_rect_b(&(pRoad->rWorldBoundingBox), &(pRenderMetrics->rWorldBoundingBox));
		if(eOverlapType == OVERLAP_NONE) {
//			g_print("OOPS!  A linestring with <3 points (%d)\n", pPointString->pPointsArray->len);
			continue;
		}

		if(pRoad->pMapPointsArray->len < 3) {
			continue;
		}

		if(eOverlapType == OVERLAP_PARTIAL) {
			// draw clipped
			GArray* pClipped = g_array_sized_new(FALSE, FALSE, sizeof(mappoint_t), pRoad->pMapPointsArray->len + 20);	// it's rarely more than a few extra points
			map_math_clip_pointstring_to_worldrect(pRoad->pMapPointsArray, &(pRenderMetrics->rWorldBoundingBox), pClipped);
			map_draw_cairo_polygon(pCairo, pRenderMetrics, pClipped);
			g_array_free(pClipped, TRUE);
		}
		else {
			map_draw_cairo_polygon(pCairo, pRenderMetrics, pRoad->pMapPointsArray);
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

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->Coordinates.fLongitude), SCALE_Y(pRenderMetrics, pLocation->Coordinates.fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->Coordinates.fLongitude), SCALE_Y(pRenderMetrics, pLocation->Coordinates.fLatitude), fRadius, 0, 2*M_PI);
	
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

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->Coordinates.fLongitude), SCALE_Y(pRenderMetrics, pLocation->Coordinates.fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->Coordinates.fLongitude), SCALE_Y(pRenderMetrics, pLocation->Coordinates.fLatitude), fRadius, 0, 2*M_PI);

			cairo_fill(pCairo);
		}
		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
//		cairo_fill(pCairo);

	cairo_restore(pCairo);
*/
}

#define ROAD_MAX_SEGMENTS 		(200)
#define DRAW_LABEL_BUFFER_LEN	(200)

typedef struct labelposition {
	guint nSegments;
	guint iIndex;
	gdouble fLength;
	gdouble fRunTotal;
	gdouble fMeanSlope;
	gdouble fMeanAbsSlope;
	gdouble fScore;
} labelposition_t;

//
// Draw a label along a 2-point line
//
static void map_draw_cairo_road_label_one_segment(map_t* pMap, cairo_t *pCairo, maplayerstyle_t* pLayerStyle, rendermetrics_t* pRenderMetrics, GArray* pMapPointsArray, const gchar* pszLabel)
{
	// get permission to draw this label
	if(FALSE == scenemanager_can_draw_label_at(pMap->pSceneManager, pszLabel, NULL, SCENEMANAGER_FLAG_PARTLY_ON_SCREEN)) {
		return;
	}

	mappoint_t* pMapPoint1 = &g_array_index(pMapPointsArray, mappoint_t, 0);
	mappoint_t* pMapPoint2 = &g_array_index(pMapPointsArray, mappoint_t, 1);

	// swap first and second points such that the line goes left-to-right
	if(pMapPoint2->fLongitude < pMapPoint1->fLongitude) {
		mappoint_t* pTmp = pMapPoint1; pMapPoint1 = pMapPoint2; pMapPoint2 = pTmp;
	}

	gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->fLongitude);
	gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->fLatitude);
	gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->fLongitude);
	gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->fLatitude);

	gdouble fRise = fY2 - fY1;
	gdouble fRun = fX2 - fX1;
	gdouble fLineLengthSquared = (fRun*fRun) + (fRise*fRise);

	//gchar* pszFontFamily = ROAD_FONT;

	cairo_save(pCairo);

	// get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszLabel, &extents);
	gdouble fLabelWidth = extents.width;
	gdouble fFontHeight = -extents.y_bearing;

	gdouble fLineLength = sqrt(fLineLengthSquared);

#ifdef ENABLE_LABEL_LIMIT_TO_ROAD
	// text too big for line?
	if(fLabelWidth > (fLineLength + ACCEPTABLE_LINE_LABEL_OVERDRAW_IN_PIXELS)) {
		cairo_restore(pCairo);
		return;
	}
#endif


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
	for(i=0 ; i<G_N_ELEMENTS(afPercentagesOfPadding) ; i++) {
		// try the next point along the line
		gdouble fFrontPadding = fTotalPadding * afPercentagesOfPadding[i];

		// move it "forward" along the line
		// (front padding) |-text-| (back padding)
		gdouble fDrawX = fX1 + (fNormalizedX * fFrontPadding);
		gdouble fDrawY = fY1 + (fNormalizedY * fFrontPadding);

		// center text "vertically" by translating "down" by half of font height
		fDrawX -= (fPerpendicularNormalizedX * (fFontHeight/2));
		fDrawY -= (fPerpendicularNormalizedY * (fFontHeight/2));

		//
		// Build a rectangle that surrounds the text to tell the scenemanager where we want to draw
		//
		#define BRDR (3)	// claim an extra border around the text to keep things readable (in pixels)

		GdkPoint aBoundingPolygon[4];
		// 0 is bottom left point
		aBoundingPolygon[0].x = fDrawX - (fPerpendicularNormalizedX * BRDR) - (fNormalizedX * BRDR);
		aBoundingPolygon[0].y = fDrawY - (fPerpendicularNormalizedY * BRDR) - (fNormalizedX * BRDR);
		// 1 is upper left point
		aBoundingPolygon[1].x = fDrawX + (fPerpendicularNormalizedX * (fFontHeight+BRDR)) - (fNormalizedX * BRDR);
		aBoundingPolygon[1].y = fDrawY + (fPerpendicularNormalizedY * (fFontHeight+BRDR)) - (fNormalizedY * BRDR);
		// 2 is upper right point
		aBoundingPolygon[2].x = aBoundingPolygon[1].x + (fNormalizedX * (fLabelWidth+BRDR+BRDR));
		aBoundingPolygon[2].y = aBoundingPolygon[1].y + (fNormalizedY * (fLabelWidth+BRDR+BRDR));
		// 3 is lower right point
		aBoundingPolygon[3].x = fDrawX + (fNormalizedX * (fLabelWidth+BRDR)) - (fPerpendicularNormalizedX * BRDR);
		aBoundingPolygon[3].y = fDrawY + (fNormalizedY * (fLabelWidth+BRDR)) - (fPerpendicularNormalizedY * BRDR);
		
		// Ask whether we can draw here
		if(FALSE == scenemanager_can_draw_polygon(pMap->pSceneManager, aBoundingPolygon, 4, SCENEMANAGER_FLAG_PARTLY_ON_SCREEN)) {
			continue;
		}
		
		gdouble fAngleInRadians = atan(fRise / fRun);	// delay expensive atan() until we've found a place we can draw

		if(fRun < 0.0) fAngleInRadians += M_PI;			// XXX: Why do we do this?

#ifdef ENABLE_ROUND_DOWN_TEXT_ANGLES
		fAngleInRadians = floor((fAngleInRadians * ROUND_DOWN_TEXT_ANGLE) + 0.5) / ROUND_DOWN_TEXT_ANGLE;
#endif

		cairo_save(pCairo);								// XXX: do we need this?


		// Draw a "halo" around text if the style calls for it
		if(pLayerStyle->fHaloSize >= 0) {
			// Halo = stroking the text path with a fat white line
			map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrHalo));

			gint iHaloOffsets;

			for(iHaloOffsets = 0 ; iHaloOffsets < G_N_ELEMENTS(g_aHaloOffsets); iHaloOffsets++) {
				cairo_save(pCairo);
				cairo_move_to(pCairo, fDrawX + g_aHaloOffsets[iHaloOffsets].nX, fDrawY + g_aHaloOffsets[iHaloOffsets].nY);
				cairo_rotate(pCairo, fAngleInRadians);
				cairo_show_text(pCairo, pszLabel);
				cairo_restore(pCairo);
			}
		}
		map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrPrimary));
		cairo_move_to(pCairo, fDrawX, fDrawY);
		cairo_rotate(pCairo, fAngleInRadians);
		cairo_show_text(pCairo, pszLabel);
		cairo_restore(pCairo);

		// claim the space this took up and the label (so it won't be drawn twice)
		scenemanager_claim_polygon(pMap->pSceneManager, aBoundingPolygon, 4);
		scenemanager_claim_label(pMap->pSceneManager, pszLabel);

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

	if(pPosA->fScore > pPosB->fScore) return -1;
	return 1;
}

static void map_draw_cairo_road_label(map_t* pMap, cairo_t *pCairo, maplayerstyle_t* pLayerStyle, rendermetrics_t* pRenderMetrics, GArray* pMapPointsArray, const gchar* pszLabel)
{
	if(pMapPointsArray->len < 2) return;

	// pass off single segments to a specialized function
	if(pMapPointsArray->len == 2) {
		map_draw_cairo_road_label_one_segment(pMap, pCairo, pLayerStyle, pRenderMetrics, pMapPointsArray, pszLabel);
		return;
	}

	if(pMapPointsArray->len > ROAD_MAX_SEGMENTS) {
		g_warning("not drawing label for road '%s' with > %d segments.\n", pszLabel, ROAD_MAX_SEGMENTS);
		return;
	}

	// Request permission to draw this label.  This prevents multiple labels too close together.
	// NOTE: Currently no location is used, only allows one of each text string per draw
	if(!scenemanager_can_draw_label_at(pMap->pSceneManager, pszLabel, NULL, SCENEMANAGER_FLAG_PARTLY_ON_SCREEN)) {
		return;
	}

	//gchar* pszFontFamily = ROAD_FONT;		// XXX: remove hardcoded font

	cairo_save(pCairo);

	// get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszLabel, &extents);

	// now find the ideal location

	// place to store potential label positions
	labelposition_t aPositions[ROAD_MAX_SEGMENTS];
	gdouble aSlopes[ROAD_MAX_SEGMENTS];

	mappoint_t* apPoints[ROAD_MAX_SEGMENTS];
	gint nNumPoints = pMapPointsArray->len;

	mappoint_t* pMapPoint1;
	mappoint_t* pMapPoint2;

	// load point string into an array
	gint iRead;
	for(iRead=0 ; iRead<nNumPoints ; iRead++) {
		apPoints[iRead] = &g_array_index(pMapPointsArray, mappoint_t, iRead);
	}

	// measure total line length
	gdouble fTotalLineLength = 0.0;
	gint nPositions = 1;
	gint iPoint, iPosition;

	for(iPoint=1 ; iPoint<nNumPoints ; iPoint++) {
		pMapPoint1 = apPoints[iPoint-1];
		pMapPoint2 = apPoints[iPoint];

		gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->fLongitude);
		gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->fLatitude);
		gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->fLongitude);
		gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->fLatitude);

		// determine slope of the line
		gdouble fRise = fY2 - fY1;
		gdouble fRun = fX2 - fX1;
		gdouble fSlope = (fRun==0) ? G_MAXDOUBLE : (fRise/fRun);
		gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

		aSlopes[iPoint] = fSlope;
		aPositions[iPoint].nSegments = 0;
		aPositions[iPoint].iIndex = iPoint;
		aPositions[iPoint].fLength = 0.0;
		aPositions[iPoint].fRunTotal = 0.0;
		aPositions[iPoint].fMeanSlope = 0.0;
		aPositions[iPoint].fMeanAbsSlope = 0.0;

		for(iPosition = nPositions ; iPosition <= iPoint ; iPosition++) {
			aPositions[iPosition].fLength += fLineLength;
			aPositions[iPosition].fRunTotal += fRun;
			aPositions[iPosition].nSegments++;
			aPositions[iPosition].fMeanSlope += fSlope;
			aPositions[iPosition].fMeanAbsSlope += (fSlope<0) ? -fSlope : fSlope;

			if(aPositions[iPosition].fLength >= extents.width) nPositions++;
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
	gint iBestPosition = -1;

	for(iPosition = 1 ; iPosition < nPositions ; iPosition++) {
		// finish calculating mean slope
		aPositions[iPosition].fMeanSlope /= aPositions[iPosition].nSegments;
		aPositions[iPosition].fMeanAbsSlope /= aPositions[iPosition].nSegments;
		
		// calculating std dev of slope
		gint iEndPoint = iPosition + aPositions[iPosition].nSegments;
		gdouble fDiffSquaredSum = 0.0;

		for(iPoint = iPosition ; iPoint < iEndPoint ; iPoint++) {
			gdouble fDiff = aSlopes[iPoint] - aPositions[iPosition].fMeanSlope;
			fDiffSquaredSum += (fDiff*fDiff);
		}

		gdouble fStdDevSlope = sqrt(fDiffSquaredSum / aPositions[iPosition].nSegments);

		// calculate a score between 0 (worst) and 1 (best), we want to minimize both the mean and std dev of slope
		aPositions[iPosition].fScore = 1.0/((aPositions[iPosition].fMeanAbsSlope+1.0)*(fStdDevSlope+1.0));

		// find max only
		if(aPositions[iPosition].fScore > fMaxScore) {
			fMaxScore = aPositions[iPosition].fScore;
			iBestPosition = iPosition;
		}

		// add position to the ptr array
		//g_ptr_array_add(pPositionsPtrArray, &(aPositions[iPosition]));

//         g_print("%s: [%d] segments = %d, slope = %2.2f, stddev = %2.2f, score = %2.2f\n", pszLabel, iPosition,
//             aPositions[iPosition].nSegments,
//             aPositions[iPosition].fMeanAbsSlope,
//             fStdDevSlope,
//             aPositions[iPosition].fScore);
	}
	
	// sort postions by score
	//if(nPositions > 1)
	//	g_ptr_array_sort(pPositionsPtrArray, map_draw_cairo_road_label_position_sort);

//     for(iPosition = 1 ; iPosition < nPositions ; iPosition++) {
//         labelposition_t *pPos = g_ptr_array_index(pPositionsPtrArray, iPosition - 1);
//
//         if(pPos->nSegments > 1)
//             g_print("%s: [%d] segments = %d, slope = %2.2f, score = %2.2f\n", pszLabel, iPosition,
//                 pPos->nSegments,
//                 pPos->fMeanAbsSlope,
//                 pPos->fScore);
//     }

	if(iBestPosition == -1) {
		cairo_restore(pCairo);
		return;
	}

	cairo_font_extents_t font_extents;
	cairo_font_extents(pCairo, &font_extents);

	gint nTotalStringLength = strlen(pszLabel);

//     for(iPosition = 1 ; iPosition < nPositions ; iPosition++) {
//         labelposition_t *pPos = g_ptr_array_index(pPositionsPtrArray, iPosition - 1);
//         gint iBestPosition = pPos->iIndex;
		//g_print("trying position %d with score %2.2f\n", iBestPosition, pPos->fScore);

		gdouble fFrontPadding = 0.0;
		gdouble fFrontPaddingNext = (aPositions[iBestPosition].fLength - extents.width) / 2;
		gint iStartPoint;

		iStartPoint = iBestPosition;
		if(aPositions[iBestPosition].fRunTotal > 0) {
			iStartPoint = iBestPosition;
		}
		else {
			// road runs backwards, reverse everything
			iStartPoint = nNumPoints - iBestPosition - aPositions[iBestPosition].nSegments + 1;
			// reverse the array
			gint iRead,iWrite;
			for(iWrite=0, iRead=nNumPoints-1 ; iRead>= 0 ; iWrite++, iRead--) {
				apPoints[iWrite] = &g_array_index(pMapPointsArray, mappoint_t, iRead);
			}
		}

		gint iEndPoint = iStartPoint + aPositions[iBestPosition].nSegments;

		gint nStringStartIndex = 0;

#if 0
/*	XXX: why is this commented out??

		gboolean bGood = TRUE;

		for(iPoint = iStartPoint ; iPoint < iEndPoint ; iPoint++) {
			if(nTotalStringLength == nStringStartIndex) break;	// done

			pMapPoint1 = apPoints[iPoint-1];
			pMapPoint2 = apPoints[iPoint];

			gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->fLongitude);
			gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->fLatitude);
			gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->fLongitude);
			gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->fLatitude);

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
			if(fRun < 0.0) fAngleInRadians += PI;
#ifdef ENABLE_ROUND_DOWN_TEXT_ANGLES
        fAngleInRadians = floor((fAngleInRadians * ROUND_DOWN_TEXT_ANGLE) + 0.5) / ROUND_DOWN_TEXT_ANGLE;
#endif

			//g_print("(fRise(%f) / fRun(%f)) = %f, atan(fRise / fRun) = %f: ", fRise, fRun, fRise / fRun, fAngleInRadians);
			//g_print("=== NEW SEGMENT, pixel (deltaY=%f, deltaX=%f), line len=%f, (%f,%f)->(%f,%f)\n",fRise, fRun, fLineLength, pMapPoint1->fLatitude,pMapPoint1->fLongitude,pMapPoint2->fLatitude,pMapPoint2->fLongitude);
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

			if(FALSE == scenemanager_can_draw_polygon(pMap->pSceneManager, aBoundingPolygon, 4, SCENEMANAGER_FLAG_PARTLY_ON_SCREEN)) {
				bGood = FALSE;
				break;
			}

		}

		// try next position
		if(bGood == FALSE)
			continue;
		
		// reset these
		fFrontPadding = 0.0;
		fFrontPaddingNext = (aPositions[iBestPosition].fLength - extents.width) / 2;
		nStringStartIndex = 0;
*/
#endif

		// draw it
		for(iPoint = iStartPoint ; iPoint < iEndPoint ; iPoint++) {
			if(nTotalStringLength == nStringStartIndex) break;	// done

			pMapPoint1 = apPoints[iPoint-1];
			pMapPoint2 = apPoints[iPoint];

			gdouble fX1 = SCALE_X(pRenderMetrics, pMapPoint1->fLongitude);
			gdouble fY1 = SCALE_Y(pRenderMetrics, pMapPoint1->fLatitude);
			gdouble fX2 = SCALE_X(pRenderMetrics, pMapPoint2->fLongitude);
			gdouble fY2 = SCALE_Y(pRenderMetrics, pMapPoint2->fLatitude);

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
			//g_print("=== NEW SEGMENT, pixel (deltaY=%f, deltaX=%f), line len=%f, (%f,%f)->(%f,%f)\n",fRise, fRun, fLineLength, pMapPoint1->fLatitude,pMapPoint1->fLongitude,pMapPoint2->fLatitude,pMapPoint2->fLongitude);
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
			cairo_set_source_rgba(pCairo, 0.0,0.0,0.0,1.0);

			// Draw a "halo" around text if the style calls for it
			// XXX: This is WRONG, we need to draw halos for all segments FIRST, then draw all text
			if(pLayerStyle->fHaloSize >= 0) {
				// Halo = stroking the text path with a fat white line
				map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrHalo));

				gint iHaloOffsets;

				for(iHaloOffsets = 0 ; iHaloOffsets < G_N_ELEMENTS(g_aHaloOffsets); iHaloOffsets++) {
					cairo_save(pCairo);
					cairo_move_to(pCairo, fDrawX + g_aHaloOffsets[iHaloOffsets].nX, fDrawY + g_aHaloOffsets[iHaloOffsets].nY);
					cairo_rotate(pCairo, fAngleInRadians);
					cairo_show_text(pCairo, azLabelSegment);
					cairo_restore(pCairo);
				}
			}
			map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrPrimary));
			cairo_move_to(pCairo, fDrawX, fDrawY);
			cairo_rotate(pCairo, fAngleInRadians);
			cairo_show_text(pCairo, azLabelSegment);
			cairo_restore(pCairo);

			// claim the space this took up
			scenemanager_claim_polygon(pMap->pSceneManager, aBoundingPolygon, 4);
		}
		cairo_restore(pCairo);

		// claim the label (so it won't be drawn twice)
		scenemanager_claim_label(pMap->pSceneManager, pszLabel);

//         // success
//         break;
//     }
//
//     g_ptr_array_free(pPositionsPtrArray, FALSE);
}


//
// Draw a single polygon label
//
void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, maplayerstyle_t* pLayerStyle, rendermetrics_t* pRenderMetrics, GArray* pMapPointsArray, maprect_t* pBoundingRect, const gchar* pszLabel)
{
	// XXX: update to use bounding box instead of (removed) fMaxLon etc.
//     if(pMapPointsArray->len < 3) return;
//
//     if(FALSE == scenemanager_can_draw_label_at(pMap->pSceneManager, pszLabel, NULL, SCENEMANAGER_FLAG_PARTLY_ON_SCREEN)) {
//         return;
//     }
//
//     gdouble fAreaOriginX = SCALE_X(pRenderMetrics, fMinLon);
//     gdouble fAreaOriginY = SCALE_Y(pRenderMetrics, fMaxLat);
//     //g_print("Area origin (%f,%f)\n", fAreaOriginX, fAreaOriginY);
//
//     gdouble fAreaWidth = SCALE_X(pRenderMetrics, fMaxLon) - SCALE_X(pRenderMetrics, fMinLon);
//     gdouble fAreaHeight = SCALE_Y(pRenderMetrics, fMinLat) - SCALE_Y(pRenderMetrics, fMaxLat);
//     //g_print("Area size (%f,%f)\n", fAreaWidth, fAreaHeight);
//
//     gchar** aLines = util_split_words_onto_two_lines(pszLabel, MIN_AREA_LABEL_LINE_LENGTH, MAX_AREA_LABEL_LINE_LENGTH);
//     gint nLineCount = g_strv_length(aLines);    // could be one or two, unless we change above function
//     gint* anWidths = g_new0(gint, nLineCount);
//
//     gdouble fLabelBoxWidth = 0.0;
//     gdouble fLabelBoxHeight = 0.0;
//
//     cairo_text_extents_t extents;
//     for(i=0 ; i<nLineCount ; i++) {
//         cairo_text_extents(pCairo, aLines[i], &extents);
//         anWidths[i] = extents.width;
//         fLabelBoxWidth = max(fLabelBoxWidth, (gdouble)extents.width);   // as wide as the widest line
//         fLabelBoxHeight += (gdouble)extents.height;                     // total height of all lines
//     }
//
//     gdouble fOneLineHeight = fLabelBoxHeight / nLineCount;
//
//     gdouble fHorizontalPadding = (fAreaWidth - fLabelBoxWidth);
//     gdouble fVerticalPadding = (fAreaHeight - fLabelBoxHeight);
//     //g_print("padding (%f,%f)\n", fHorizontalPadding, fVerticalPadding);
//
//     // various places to try...
//     struct { gdouble fX,fY; } afPercentagesOfPadding[] = {
//         {0.50, 0.50},
//
//         // first the close-to-center positions (.25 and .75)
//         {0.50, 0.25}, {0.50, 0.75}, // dodge up/down
//         {0.25, 0.50}, {0.75, 0.50}, // dodge left/right middle
//         {0.25, 0.25}, {0.75, 0.25}, // upper left/right
//         {0.25, 0.75}, {0.75, 0.75}, // lower left/right
//
//         // now the far-from-center positions (0.0 and 1.0)
// //      {0.50, 0.00}, {0.50, 1.00}, // dodge up/down
// //      {0.00, 0.50}, {1.00, 0.50}, // dodge left/right middle
// //      {0.00, 0.00}, {1.00, 0.00}, // upper left/right
// //      {0.00, 1.00}, {1.00, 1.00}, // lower left/right
//     };
//
//     //gboolean bSuccess = FALSE;
//     gint iSlot;
//     for(iSlot=0 ; iSlot<G_N_ELEMENTS(afPercentagesOfPadding) ; iSlot++) {
//         gdouble fDrawBoxCornerX = fAreaOriginX + (fHorizontalPadding * afPercentagesOfPadding[iSlot].fX);
//         gdouble fDrawBoxCornerY = fAreaOriginY + (fVerticalPadding * afPercentagesOfPadding[iSlot].fY);
//
//         GdkRectangle rcLabelOutline;
//         rcLabelOutline.x = (gint)(fDrawBoxCornerX);
//         rcLabelOutline.width = (gint)(fLabelBoxWidth);
//         rcLabelOutline.y = (gint)(fDrawBoxCornerY);
//         rcLabelOutline.height = (gint)(fLabelBoxHeight);
//         if(FALSE == scenemanager_can_draw_rectangle(pMap->pSceneManager, &rcLabelOutline, SCENEMANAGER_FLAG_FULLY_ON_SCREEN)) {
//             continue;
//         }
//
//         // passed test!  claim this label and rectangle area
//         scenemanager_claim_label(pMap->pSceneManager, pszLabel);
//         scenemanager_claim_rectangle(pMap->pSceneManager, &rcLabelOutline);
//
//         //
//         // Draw Halo for all lines, if style dictates.
//         //
//         if(pLayerStyle->fHaloSize >= 0) {
//             cairo_save(pCairo);
//
//             map_draw_cairo_set_rgba(pCairo, &(pLayerStyle->clrHalo));
//
//             for(i=0 ; i<nLineCount ; i++) {
//                 gint iHaloOffsets;
//                 for(iHaloOffsets = 0 ; iHaloOffsets < G_N_ELEMENTS(g_aHaloOffsets); iHaloOffsets++) {
//                     cairo_move_to(pCairo,
//                                   g_aHaloOffsets[iHaloOffsets].nX + fDrawBoxCornerX + ((fLabelBoxWidth - anWidths[i])/2),
//                                   g_aHaloOffsets[iHaloOffsets].nY + fDrawBoxCornerY + ((i+1) * fOneLineHeight));
//                     cairo_show_text(pCairo, aLines[i]);
//                 }
//             }
//             cairo_restore(pCairo);
//         }
//
//         //
//         // Draw text for all lines.
//         //
//         for(i=0 ; i<nLineCount ; i++) {
//             // Get total width of string
//             cairo_move_to(pCairo,
//                           fDrawBoxCornerX + ((fLabelBoxWidth - anWidths[i])/2),
//                           fDrawBoxCornerY + ((i+1) * fOneLineHeight));
//             cairo_show_text(pCairo, aLines[i]);
//         }
//         break;
//     }
//
//         // draw it
// //      cairo_save(pCairo);
//
// //      cairo_restore(pCairo);
//     g_strfreev(aLines);
//     g_free(anWidths);
}

static void map_draw_cairo_map_scale(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics)
{
	zoomlevel_t* pZoomLevel = &g_sZoomLevels[pRenderMetrics->nZoomLevel-1];

	// Imperial
	gdouble fImperialLength = 0;
	gchar* pszImperialLabel = NULL;
	if(pZoomLevel->eScaleImperialUnit == UNIT_FEET) {
		fImperialLength = (WORLD_FEET_TO_DEGREES(pZoomLevel->nScaleImperialNumber) / pRenderMetrics->fScreenLongitude) * (gdouble)pRenderMetrics->nWindowWidth;
		pszImperialLabel = g_strdup_printf("%d ft", pZoomLevel->nScaleImperialNumber);
	}
	else {
		fImperialLength = (WORLD_MILES_TO_DEGREES(pZoomLevel->nScaleImperialNumber) / pRenderMetrics->fScreenLongitude) * (gdouble)pRenderMetrics->nWindowWidth;
		pszImperialLabel = g_strdup_printf("%d mi", pZoomLevel->nScaleImperialNumber);
	}
	fImperialLength = floor(fImperialLength);

	// Metric
	gdouble fMetricLength = 0;
	gchar* pszMetricLabel = NULL;
	if(pZoomLevel->eScaleMetricUnit == UNIT_METERS) {
		fMetricLength = (WORLD_METERS_TO_DEGREES(pZoomLevel->nScaleMetricNumber) / pRenderMetrics->fScreenLongitude) * (gdouble)pRenderMetrics->nWindowWidth;
		pszMetricLabel = g_strdup_printf("%d m", pZoomLevel->nScaleMetricNumber);
	}
	else {
		fMetricLength = (WORLD_KILOMETERS_TO_DEGREES(pZoomLevel->nScaleMetricNumber) * (gdouble)pRenderMetrics->nWindowWidth) / pRenderMetrics->fScreenLongitude;
		pszMetricLabel = g_strdup_printf("%d km", pZoomLevel->nScaleMetricNumber);
	}
	fMetricLength = floor(fMetricLength);

	// XXX: Choose this based on where the scale should be on screen
	gdouble fLeftCenterPointX = 10;
	gdouble fLeftCenterPointY = ((gdouble)pRenderMetrics->nWindowHeight) - 20;

#define MAP_SCALE_TICK_SIZE (10)		// size of tick mark extending up from the length line
#define MAP_SCALE_FONT_NAME ("Bitstream Vera Sans")
#define MAP_SCALE_FONT_SIZE (12.0)

	cairo_save(pCairo);

	// Draw vertical line on left
	cairo_move_to(pCairo, fLeftCenterPointX, fLeftCenterPointY-MAP_SCALE_TICK_SIZE);
	cairo_rel_line_to(pCairo, 0, MAP_SCALE_TICK_SIZE * 2);

	// Draw horizontal line
	cairo_move_to(pCairo, fLeftCenterPointX, fLeftCenterPointY);
	cairo_rel_line_to(pCairo, max(fMetricLength, fImperialLength), 0);

	// Draw tick going up for imperial
	cairo_move_to(pCairo, fLeftCenterPointX + fImperialLength, fLeftCenterPointY);
	cairo_rel_line_to(pCairo, 0, -MAP_SCALE_TICK_SIZE);

	// Draw tick going down for metric
	cairo_move_to(pCairo, fLeftCenterPointX + fMetricLength, fLeftCenterPointY);
	cairo_rel_line_to(pCairo, 0, MAP_SCALE_TICK_SIZE);

	// Stroke once in white then again slightly thinner in black
	cairo_set_line_cap(pCairo, CAIRO_LINE_CAP_SQUARE); 
	cairo_set_source_rgba(pCairo, 1, 1, 1, 1);
	cairo_set_line_width(pCairo, 4);
	cairo_stroke_preserve(pCairo);

	cairo_set_source_rgba(pCairo, 0.2, 0.2, 0.2, 1);
	cairo_set_line_width(pCairo, 2);
	cairo_stroke(pCairo);


	cairo_select_font_face(pCairo, MAP_SCALE_FONT_NAME, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(pCairo, MAP_SCALE_FONT_SIZE);

	cairo_move_to(pCairo, fLeftCenterPointX + 3.0, fLeftCenterPointY - 4.0);
	cairo_show_text(pCairo, pszImperialLabel);

	// get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszMetricLabel, &extents);
	gdouble fFontHeight = -extents.y_bearing;

	cairo_move_to(pCairo, fLeftCenterPointX + 3.0, fLeftCenterPointY + fFontHeight + 3.0);
	cairo_show_text(pCairo, pszMetricLabel);

	cairo_restore(pCairo);

	g_free(pszImperialLabel);
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
		pLocationsArray = g_hash_table_lookup(pMap->pLocationArrayHashTable, &(pLocationSet->nID));
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

#if 0	// GGGGGGGGGGGGGGGG
/*
static void map_draw_cairo_locationselection_outline(cairo_t *pCairo, gdouble fPointX, gdouble fPointY, const screenrect_t* pRect, screenrect_t* pCloseRect)
{
	cairo_save(pCairo);

	gdouble fCorner = 30.0;
	gdouble fArrowWidth = 30.0; 

	gdouble fBoxX = pRect->A.nX + 0.5;
	gdouble fBoxY = pRect->A.nY + 0.5;
	gdouble fBoxWidth = pRect->B.nX - pRect->A.nX;
	gdouble fBoxHeight = pRect->B.nY - pRect->A.nY;

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

	pCloseRect->A.nX = (gint)((fBoxX + fBoxWidth) - fCloseRectRelief) - fCloseRectWidth;
	pCloseRect->A.nY = (gint)fBoxY + fCloseRectRelief;
	pCloseRect->B.nX = (gint)((fBoxX + fBoxWidth) - fCloseRectRelief);
	pCloseRect->B.nY = (gint)fBoxY + fCloseRectRelief + fCloseRectWidth;

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
*/
#endif

#if 0	// GGGGGGGGGGGGGGGG
/*
static void map_draw_cairo_locationselection(map_t* pMap, cairo_t *pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationSelectionArray)
{
	cairo_save(pCairo);

	gint i;
	for(i=0 ; i<pLocationSelectionArray->len ; i++) {
		locationselection_t* pLocationSelection = g_ptr_array_index(pLocationSelectionArray, i);

		pLocationSelection->nNumURLs=0;

		// bounding box test
		if(pLocationSelection->Coordinates.fLatitude < pRenderMetrics->rWorldBoundingBox.A.fLatitude
		   || pLocationSelection->Coordinates.fLongitude < pRenderMetrics->rWorldBoundingBox.A.fLongitude
		   || pLocationSelection->Coordinates.fLatitude > pRenderMetrics->rWorldBoundingBox.B.fLatitude
		   || pLocationSelection->Coordinates.fLongitude > pRenderMetrics->rWorldBoundingBox.B.fLongitude)
		{
			pLocationSelection->bVisible = FALSE;
			continue;   // not visible
		}
		pLocationSelection->bVisible = TRUE;

		gdouble fX = SCALE_X(pRenderMetrics, pLocationSelection->Coordinates.fLongitude);
		fX = floor(fX) + 0.5;
		gdouble fY = SCALE_Y(pRenderMetrics, pLocationSelection->Coordinates.fLatitude);
		fY = floor(fY) + 0.5;

		gdouble fBoxHeight = 105.0;
		gdouble fBoxWidth = 200.0;

		gdouble fOffsetX = -(fBoxWidth/3);
		gdouble fOffsetY = -40.0;

		// determine top left corner of box
		gdouble fBoxX = fX + fOffsetX;
		gdouble fBoxY = (fY - fBoxHeight) + fOffsetY;

		pLocationSelection->InfoBoxRect.A.nX = (gint)fBoxX;
		pLocationSelection->InfoBoxRect.A.nY = (gint)fBoxY;
		pLocationSelection->InfoBoxRect.B.nX = (gint)(fBoxX + fBoxWidth);
		pLocationSelection->InfoBoxRect.B.nY = (gint)(fBoxY + fBoxHeight);

		map_draw_cairo_locationselection_outline(pCairo, fX, fY, &(pLocationSelection->InfoBoxRect), &(pLocationSelection->InfoBoxCloseRect));

//         gchar* pszFontFamily = ROAD_FONT;   // XXX: remove hardcoded font
//         gint fTitleFontSize = 15.0;
//         gint fBodyFontSize = 13.0;
//
//         // BEGIN text
//         // shrink box a little
//         fBoxX += 8.0;
//         fBoxY += 10.0;
//         fBoxWidth -= 16.0;
//         fBoxHeight -= 20.0;
//
// #define LINE_RELIEF (4.0)   // space between lines in pixels
// #define BODY_RELIEF (6.0)
// #define LINK_HORIZONTAL_RELIEF (10)
//
//         const gchar* pszString;
//         cairo_text_extents_t extents;
//         cairo_select_font_face (pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
//         cairo_set_font_size(pCairo, fTitleFontSize);
//
//         // draw name
//         pszString = map_location_selection_get_attribute(pMap, pLocationSelection, "name");
//         if(pszString == NULL) pszString = "";   // unnamed POI?!
//         cairo_text_extents(pCairo, pszString, &extents);
//         gdouble fLabelWidth = extents.width;
//         gdouble fFontHeight = extents.height;
//         cairo_set_source_rgb(pCairo, 0.1,0.1,0.1);  // normal text color
//
//         gdouble fTextOffsetX = fBoxX;
//         gdouble fTextOffsetY = fBoxY + fFontHeight;
//
//         cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
//         cairo_show_text(pCairo, pszString);
//
//         // draw address, line 1
//         pszString = map_location_selection_get_attribute(pMap, pLocationSelection, "address");
//         if(pszString != NULL) {
//             gchar** aLines = g_strsplit(pszString,"\n", 0); // "\n" = delimeters, 0 = no max #
//
//             gchar* pszLine;
//             if(aLines[0]) {
//                 pszLine = aLines[0];
//                 cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
//                 cairo_set_font_size(pCairo, fBodyFontSize);
//                 cairo_text_extents(pCairo, pszLine, &extents);
//                 fLabelWidth = extents.width;
//                 fFontHeight = extents.height;
//
//                 fTextOffsetY += (BODY_RELIEF + fFontHeight);
//                 cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
//                 cairo_show_text(pCairo, pszLine);
//
//                 if(aLines[1]) {
//                     //g_print("line 2\n");
//                     // draw address, line 2
//                     pszLine = aLines[1];
//
//                     fTextOffsetY += (LINE_RELIEF + fFontHeight);
//                     cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
//                     cairo_show_text(pCairo, pszLine);
//                 }
//             }
//             g_strfreev(aLines); // free the array of strings
//         }
//
//         // draw phone number
//         pszString = "(617) 576-1369";
//         cairo_text_extents(pCairo, pszString, &extents);
//         fLabelWidth = extents.width;
//         fFontHeight = extents.height;
//
//         fTextOffsetY += (LINE_RELIEF + fFontHeight);
//         cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
//         cairo_set_source_rgb(pCairo, 0.1,0.1,0.1);  // normal text color
//         cairo_show_text(pCairo, pszString);
//         // draw underline
// //                 cairo_text_extents(pCairo, pszString, &extents);
// //                 fLabelWidth = extents.width;
// //                 fFontHeight = extents.height;
// //                 map_draw_text_underline(pCairo, fLabelWidth);
//
//         // draw website link
//         fTextOffsetX = fBoxX;
//         fTextOffsetY = fBoxY + fBoxHeight;
//         pszString = "1369coffeehouse.com";
//         cairo_text_extents(pCairo, pszString, &extents);
//         fLabelWidth = extents.width;
//         fFontHeight = extents.height;
//         cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
//         cairo_set_source_rgb(pCairo, 0.1,0.1,0.4);  // remote link color
//         map_draw_text_underline(pCairo, fLabelWidth);
//         cairo_show_text(pCairo, pszString);
//
//         screenrect_t* pRect = &(pLocationSelection->aURLs[pLocationSelection->nNumURLs].Rect);
//         pRect->A.nX = fTextOffsetX;
//         pRect->A.nY = fTextOffsetY - fFontHeight;
//         pRect->B.nX = fTextOffsetX + fLabelWidth;
//         pRect->B.nY = fTextOffsetY;
//         pLocationSelection->aURLs[pLocationSelection->nNumURLs].pszURL = g_strdup(pszString);
//         pLocationSelection->nNumURLs++;
//
//         // draw 'edit' link
//         pszString = "edit";
//         cairo_set_source_rgb(pCairo, 0.1,0.4,0.1);  // local link color
//         cairo_select_font_face(pCairo, pszFontFamily, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
//         cairo_set_font_size(pCairo, fBodyFontSize);
//         cairo_text_extents(pCairo, pszString, &extents);
//         fLabelWidth = extents.width;
//         fFontHeight = extents.height;
//
//         // bottom edge, right justified
//         fTextOffsetX = (fBoxX + fBoxWidth) - fLabelWidth;
//         fTextOffsetY = (fBoxY + fBoxHeight);
//
//         cairo_move_to(pCairo, fTextOffsetX, fTextOffsetY);
//         cairo_show_text(pCairo, pszString);
//         map_draw_text_underline(pCairo, fLabelWidth);
//
//         pRect = &(pLocationSelection->EditRect);
//         pRect->A.nX = fTextOffsetX;
//         pRect->A.nY = fTextOffsetY - fFontHeight;
//         pRect->B.nX = fTextOffsetX + fLabelWidth;
//         pRect->B.nY = fTextOffsetY;
	}
	cairo_restore(pCairo);
}
*/
#endif

#ifdef ROADSTER_DEAD_CODE
/*
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
		cairo_move_to(pCairo, (pRenderMetrics->nWindowWidth/2) - (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH), (pRenderMetrics->nWindowHeight/2));
		cairo_line_to(pCairo, (pRenderMetrics->nWindowWidth/2) - (CROSSHAIR_LINE_RELIEF), (pRenderMetrics->nWindowHeight/2));
		// right line
		cairo_move_to(pCairo, (pRenderMetrics->nWindowWidth/2) + (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH), (pRenderMetrics->nWindowHeight/2));
		cairo_line_to(pCairo, (pRenderMetrics->nWindowWidth/2) + (CROSSHAIR_LINE_RELIEF), (pRenderMetrics->nWindowHeight/2));
		// top line
		cairo_move_to(pCairo, (pRenderMetrics->nWindowWidth/2), (pRenderMetrics->nWindowHeight/2) - (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH));
		cairo_line_to(pCairo, (pRenderMetrics->nWindowWidth/2), (pRenderMetrics->nWindowHeight/2) - (CROSSHAIR_LINE_RELIEF));
		// bottom line
		cairo_move_to(pCairo, (pRenderMetrics->nWindowWidth/2), (pRenderMetrics->nWindowHeight/2) + (CROSSHAIR_LINE_RELIEF + CROSSHAIR_LINE_LENGTH));
		cairo_line_to(pCairo, (pRenderMetrics->nWindowWidth/2), (pRenderMetrics->nWindowHeight/2) + (CROSSHAIR_LINE_RELIEF));
		cairo_stroke(pCairo);
		
		cairo_arc(pCairo, pRenderMetrics->nWindowWidth/2, pRenderMetrics->nWindowHeight/2, CROSSHAIR_CIRCLE_RADIUS, 0, 2*M_PI);
		cairo_stroke(pCairo);
	cairo_restore(pCairo);
}

void map_draw_cairo_locations(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics)
{
	location_t loc;
	location_t* pLoc = &loc;

	// XXX:	debug
	pLoc->pszName = "some address., Cambridge, MA, 02140";
	pLoc->Coordinates.fLongitude = 0;
	pLoc->Coordinates.fLatitude = 0;

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
	cairo_text_extents(pCairo, pLoc->pszName, &extents);
	gdouble fLabelWidth = extents.width;
	gdouble fLabelHeight = extents.height;

	gdouble fX = SCALE_X(pRenderMetrics, pLoc->Coordinates.fLongitude);
	gdouble fY = SCALE_Y(pRenderMetrics, pLoc->Coordinates.fLatitude);

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
	cairo_show_text(pCairo, pLoc->pszName);

	cairo_restore(pCairo);
}

void map_draw_cairo_gps_trail(map_t* pMap, cairo_t* pCairo, pointstring_t* pPointString)
{
	rendermetrics_t renderMetrics = {0};
	map_get_render_metrics(pMap, &renderMetrics);
	rendermetrics_t* pRenderMetrics = &renderMetrics;

	if(pPointString->pPointsArray->len > 2) {
		mappoint_t* pPoint = g_ptr_array_index(pPointString->pPointsArray, 0);
		
		// move to index 0
		cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->fLongitude), SCALE_Y(pRenderMetrics, pPoint->fLatitude));

		gint i;
		for(i=1 ; i<pPointString->pPointsArray->len ; i++) {
			pPoint = g_ptr_array_index(pPointString->pPointsArray, i);
			cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->fLongitude), SCALE_Y(pRenderMetrics, pPoint->fLatitude));
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
