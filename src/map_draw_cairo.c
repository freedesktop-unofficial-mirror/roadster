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

#define RENDERING_THREAD_YIELD	// do nothing

#define HACK_AROUND_CAIRO_LINE_CAP_BUG	// enable to ensure roads have rounded caps if the style dictates

#define ROAD_FONT	"Bitstream Vera Sans"
#define AREA_FONT	"Bitstream Vera Sans" // "Bitstream Charter"

#include <gdk/gdkx.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

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

static void map_draw_cairo_layer_polygons(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_layer_lines(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_layer_line_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_layer_polygon_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, pointstring_t* pPointString, const gchar* pszLabel);
static void map_draw_cairo_layer_points(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray);
static void map_draw_cairo_crosshair(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics);
static void map_draw_cairo_background(map_t* pMap, cairo_t *pCairo);

void map_draw_cairo(map_t* pMap, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags)
{
	// 1. Set draw target to X Drawable
	Display* dpy;
	Drawable drawable;
	dpy = gdk_x11_drawable_get_xdisplay(pPixmap);
	drawable = gdk_x11_drawable_get_xid(pPixmap);

	cairo_t* pCairo = cairo_create ();
	cairo_set_target_drawable(pCairo, dpy, drawable);

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
				map_draw_cairo_layer_lines(pMap, pCairo,
						pRenderMetrics,
				/* geometry */ 	pMap->m_apLayerData[nLayer]->m_pPointStringsArray,
				/* style */ 	&(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
		else if(nRenderType == SUBLAYER_RENDERTYPE_POLYGONS) {
			if(nDrawFlags & DRAWFLAG_GEOMETRY) {
				map_draw_cairo_layer_polygons(pMap, pCairo,
						pRenderMetrics,
				/* geometry */ 	pMap->m_apLayerData[nLayer]->m_pPointStringsArray,
				/* style */ 	&(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
		else if(nRenderType == SUBLAYER_RENDERTYPE_LINE_LABELS) {
			if(nDrawFlags & DRAWFLAG_LABELS) {
				map_draw_cairo_layer_line_labels(pMap, pCairo,
						pRenderMetrics,
				/* geometry */  pMap->m_apLayerData[nLayer]->m_pPointStringsArray,
				/* style */     &(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
		else if(nRenderType == SUBLAYER_RENDERTYPE_POLYGON_LABELS) {
			if(nDrawFlags & DRAWFLAG_LABELS) {
				map_draw_cairo_layer_polygon_labels(pMap, pCairo,
						pRenderMetrics,
				/* geometry */  pMap->m_apLayerData[nLayer]->m_pPointStringsArray,
				/* style */     &(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
	}

	// 4. Cleanup
	cairo_restore(pCairo);
	TIMER_END(maptimer, "END RENDER MAP (cairo)");
}

// ==============================================
// Begin map_draw_cairo_* functions
// ==============================================

static void map_draw_cairo_background(map_t* pMap, cairo_t *pCairo)
{
	cairo_save(pCairo);
		cairo_set_rgb_color(pCairo, 247/255.0, 235/255.0, 230/255.0);
		cairo_rectangle(pCairo, 0, 0, pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight);
		cairo_fill(pCairo);
	cairo_restore(pCairo);
}

// EXPERIMENTAL TEXT RENDERING
static void map_draw_cairo_line_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, pointstring_t* pPointString, gdouble fLineWidth, const gchar* pszLabel)
{
	if(pPointString->m_pPointsArray->len < 2) return;

#define ROAD_MAX_SEGMENTS 100
	if(pPointString->m_pPointsArray->len > ROAD_MAX_SEGMENTS) { g_warning("road %s has > %d segments!\n", pszLabel, ROAD_MAX_SEGMENTS); return; }

	gfloat fFontSize = pLabelStyle->m_afFontSizeAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
	if(fFontSize == 0) return;

	if(!scenemanager_can_draw_label(pMap->m_pSceneManager, pszLabel)) {
		//g_print("dup label: %s\n", pszLabel);
		return;
	}

	mappoint_t* apPoints[ROAD_MAX_SEGMENTS];
	gint nNumPoints = pPointString->m_pPointsArray->len;

	// figure out which way the road goes overall, by looking at the first and last points
	mappoint_t* pMapPoint1 = g_ptr_array_index(pPointString->m_pPointsArray, 0);
	mappoint_t* pMapPoint2 = g_ptr_array_index(pPointString->m_pPointsArray, pPointString->m_pPointsArray->len-1);

	// Does it go left-to-right?
	// NOTE: a better test would be to figure out the total length of roadsegment that goes left-to-right
	// and the total length that goes right-to-left, and swap the whole thing if right-to-left wins
	if(pMapPoint1->m_fLongitude < pMapPoint2->m_fLongitude) {
		// YES-- just copy the array
		gint iRead;
		for(iRead=0 ; iRead<pPointString->m_pPointsArray->len ; iRead++) {
			apPoints[iRead] = g_ptr_array_index(pPointString->m_pPointsArray, iRead);
		}
	}
	else {
		// NO-- (right-to-left) so reverse the array
		gint iRead,iWrite;
		for(iWrite=0, iRead=pPointString->m_pPointsArray->len-1 ; iRead>= 0 ; iWrite++, iRead--) {
			apPoints[iWrite] = g_ptr_array_index(pPointString->m_pPointsArray, iRead);
		}
	}

	//
	// Measure total line length	(perhaps this should be passed in)
	//
	gdouble fTotalLineLength = 0.0;
	gint iPoint;
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
		gdouble fLineLength = sqrt((fRun*fRun) + (fRise*fRise));

		fTotalLineLength += fLineLength;
	}

	gchar* pszFontFamily = ROAD_FONT;

	cairo_save(pCairo);
	cairo_select_font(pCairo, pszFontFamily,
						CAIRO_FONT_SLANT_NORMAL,
						pLabelStyle->m_abBoldAtZoomLevel[pRenderMetrics->m_nZoomLevel-1] ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_scale_font(pCairo, fFontSize);	

	// Get total width of string
	cairo_text_extents_t extents;
	cairo_text_extents(pCairo, pszLabel, &extents);
	if(extents.width > fTotalLineLength) {
		cairo_restore(pCairo);
		return;
	}

	cairo_font_extents_t font_extents;
	cairo_current_font_extents(pCairo, &font_extents);

	// CENTER IT on the line ?
	// (padding) |-text-| (padding)
	// ============================
	gdouble fFrontPadding = 0.0;
	gdouble fFrontPaddingNext = (fTotalLineLength - extents.width) / 2;	
	// NOTE: we only worry about padding at the start, the padding at the end should just happen...	

	gint nTotalStringLength = strlen(pszLabel);
	gint nStringStartIndex = 0;

//	g_print("=== NEW STRING: %s (padding %f)\n", pszLabel, fPaddingRemaining);

	for(iPoint=1 ; iPoint<nNumPoints ; iPoint++) {
		RENDERING_THREAD_YIELD;

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

		if(fFrontPadding > fLineLength) {
			fFrontPaddingNext = fFrontPadding - fLineLength;
			continue;
		}

		// do this after the padding calculation to possibly save some CPU cycles
		//~ gdouble fAngleInRadians = atan2(fRise, fRun); // * (M_PI/180.0);

		// this way works too:
		gdouble fAngleInRadians = atan(fRise / fRun);
		if(fRun < 0.0) fAngleInRadians += M_PI;

//		g_print("(fRise(%f) / fRun(%f)) = %f, atan(fRise / fRun) = %f: ", fRise, fRun, fRise / fRun, fAngleInRadians);

//		g_print("=== NEW SEGMENT, pixel (deltaY=%f, deltaX=%f), line len=%f, (%f,%f)->(%f,%f)\n",fRise, fRun, fLineLength, pMapPoint1->m_fLatitude,pMapPoint1->m_fLongitude,pMapPoint2->m_fLatitude,pMapPoint2->m_fLongitude);
//		g_print("  has screen coords (%f,%f)->(%f,%f)\n", fX1,fY1,fX2,fY2);

#define DRAW_LABEL_BUFFER_LEN	(200)
		gchar azLabelSegment[DRAW_LABEL_BUFFER_LEN];

		//
		// Figure out how much of the string we can put in this line segment
		//
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
//				g_print("trying nWorkableStringLength = %d\n", nWorkableStringLength);

				if(nWorkableStringLength >= DRAW_LABEL_BUFFER_LEN) break;

				// copy it in to the temp buffer
				memcpy(azLabelSegment, &pszLabel[nStringStartIndex], nWorkableStringLength);
				azLabelSegment[nWorkableStringLength] = '\0';
	
//				g_print("azLabelSegment = %s\n", azLabelSegment);
	
				// measure the label
				cairo_text_extents(pCairo, azLabelSegment, &extents);

				// if we're skipping ahead some (frontpadding), effective line length is smaller, so subtract padding
				if(extents.width <= (fLineLength - fFrontPadding)) {
//					g_print("found length %d for %s\n", nWorkableStringLength, azLabelSegment);	
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
	//			g_print("forcing a character (%s) on small segment, giving next segment front-padding: %f\n", azLabelSegment, fFrontPaddingNext);
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
//		if(fPerpendicularNormalizedY > 0) fPerpendicularNormalizedY = -fPerpendicularNormalizedY;	

		// text too big to fit?  then move the text "up" above the line
		//~ if(extents.height > (fLineWidth - LABEL_PIXEL_RELIEF_INSIDE_LINE)) {
			//~ // Raise the text "up" (away from center of line) half the width of the line
			//~ // This leaves it resting on the line.  Then add a few pixels of relief.
	
			//~ // NOTE: the point started in the dead-center of the line
			//~ fDrawX += (fPerpendicularNormalizedX * ((fLineWidth / 2) + LABEL_PIXELS_ABOVE_LINE));
			//~ fDrawY += (fPerpendicularNormalizedY * ((fLineWidth / 2) + LABEL_PIXELS_ABOVE_LINE));
		//~ }
		//~ else {
			//~ // just nudge it "down" slightly-- the text shows up "ABOVE" and to the "RIGHT" of the point
//                         fDrawX -= (fPerpendicularNormalizedX * extents.height/2);
//                         fDrawY -= (fPerpendicularNormalizedY * extents.height/2);
			fDrawX -= (fPerpendicularNormalizedX * font_extents.ascent/2);
			fDrawY -= (fPerpendicularNormalizedY * font_extents.ascent/2);

		//~ }

		//
		cairo_save(pCairo);
			cairo_move_to(pCairo, fDrawX, fDrawY);
			cairo_set_rgb_color(pCairo, 0.0,0.0,0.0);
			cairo_set_alpha(pCairo, 1.0);
			cairo_rotate(pCairo, fAngleInRadians);
			//cairo_text_path(pCairo, azLabelSegment);

			gdouble fHaloSize = pLabelStyle->m_afHaloAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
			if(fHaloSize >= 0) {
				cairo_save(pCairo);
					cairo_set_line_width(pCairo, fHaloSize);
					cairo_set_rgb_color(pCairo, 1.0,1.0,1.0);
					cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_BEVEL);
					//cairo_set_miter_limit(pCairo, 0.1);
					cairo_stroke(pCairo);
				cairo_restore(pCairo);
			}
			cairo_show_text(pCairo, azLabelSegment);
			//cairo_fill(pCairo);
		cairo_restore(pCairo);
	}
	cairo_restore(pCairo);

	scenemanager_label_drawn(pMap->m_pSceneManager, pszLabel);
}

void map_draw_cairo_polygon_label(map_t* pMap, cairo_t *pCairo, textlabelstyle_t* pLabelStyle, rendermetrics_t* pRenderMetrics, pointstring_t* pPointString, const gchar* pszLabel)
{
	if(pPointString->m_pPointsArray->len < 3) return;

	gdouble fFontSize = pLabelStyle->m_afFontSizeAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
	if(fFontSize == 0) return;

	gdouble fAlpha = pLabelStyle->m_clrColor.m_fAlpha;
	if(fAlpha == 0.0) return;

	if(!scenemanager_can_draw_label(pMap->m_pSceneManager, pszLabel)) {
		//g_print("dup label: %s\n", pszLabel);
		return;
	}

	gdouble fTotalX = 0.0;
	gdouble fTotalY = 0.0;

	gdouble fMaxX = -G_MAXDOUBLE;	// init to the worst possible value so first point will override
	gdouble fMaxY = -G_MAXDOUBLE;
	gdouble fMinX = G_MAXDOUBLE;
	gdouble fMinY = G_MAXDOUBLE;

	mappoint_t* pMapPoint;
	gdouble fX;
	gdouble fY;
	gint i;
	for(i=0 ; i<pPointString->m_pPointsArray->len ; i++) {
		pMapPoint = g_ptr_array_index(pPointString->m_pPointsArray, i);
		
		fX = SCALE_X(pRenderMetrics, pMapPoint->m_fLongitude);
		fY = SCALE_Y(pRenderMetrics, pMapPoint->m_fLatitude);

		// find extents
		fMaxX = max(fX,fMaxX);
		fMinX = min(fX,fMinX);
		fMaxY = max(fY,fMaxY);
		fMinY = min(fY,fMinY);

		// sum up Xs and Ys (we'll take an average later)
		fTotalX += fX;
		fTotalY += fY;
	}
	RENDERING_THREAD_YIELD;

	//
	gdouble fPolygonHeight = fMaxY - fMinY;
	gdouble fPolygonWidth = fMaxX - fMinX;

	gdouble fDrawX = fTotalX / pPointString->m_pPointsArray->len;
	gdouble fDrawY = fTotalY / pPointString->m_pPointsArray->len;

	gchar* pszFontFamily = AREA_FONT;

	cairo_save(pCairo);

		cairo_select_font(pCairo, pszFontFamily,
							CAIRO_FONT_SLANT_NORMAL,
							pLabelStyle->m_abBoldAtZoomLevel[pRenderMetrics->m_nZoomLevel-1] ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
		cairo_scale_font(pCairo, fFontSize);	

		// Get total width of string
		cairo_text_extents_t extents;
		cairo_text_extents(pCairo, pszLabel, &extents);

		// is the text too big for the polygon?
		if((extents.width > (fPolygonWidth * 1.5)) || (extents.height > (fPolygonHeight * 1.5))) {
			cairo_restore(pCairo);
			return;
		}

		fDrawX -= (extents.width / 2);
		fDrawY += (extents.height / 2);

// g_print("drawing at %f,%f\n", fDrawX, fDrawY);

		cairo_move_to(pCairo, fDrawX, fDrawY);
		cairo_set_rgb_color(pCairo, pLabelStyle->m_clrColor.m_fRed, pLabelStyle->m_clrColor.m_fGreen, pLabelStyle->m_clrColor.m_fBlue);
		cairo_set_alpha(pCairo, fAlpha);
		//cairo_text_path(pCairo, pszLabel);

		gdouble fHaloSize = pLabelStyle->m_afHaloAtZoomLevel[pRenderMetrics->m_nZoomLevel-1];
		if(fHaloSize >= 0) {
			cairo_save(pCairo);
				cairo_set_line_width(pCairo, fHaloSize);
				cairo_set_rgb_color(pCairo, 1.0,1.0,1.0);
				cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_BEVEL);
//				cairo_set_miter_limit(pCairo, 0.1);
				cairo_stroke(pCairo);
			cairo_restore(pCairo);
		}
		cairo_show_text(pCairo, pszLabel);
		//cairo_fill(pCairo);
	cairo_restore(pCairo);

        scenemanager_label_drawn(pMap->m_pSceneManager, pszLabel);
}

#define CROSSHAIR_LINE_RELIEF   (6)
#define CROSSHAIR_LINE_LENGTH   (12)
#define CROSSHAIR_CIRCLE_RADIUS (12)

static void map_draw_cairo_crosshair(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics)
{
	cairo_save(pCairo);
		cairo_set_line_width(pCairo, 1.0);
		cairo_set_rgb_color(pCairo, 0.1, 0.1, 0.1);
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

void map_draw_cairo_layer_points(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pLocationsArray)
{
	gfloat fRadius = map_degrees_to_pixels(pMap, 0.0007, map_get_zoomlevel(pMap));
	gboolean bAddition = FALSE;

	cairo_save(pCairo);
		cairo_set_rgb_color(pCairo, 123.0/255.0, 48.0/255.0, 1.0);
		cairo_set_alpha(pCairo, 0.3);

		gint iLocation;
		for(iLocation=0 ; iLocation<pLocationsArray->len ; iLocation++) {
			RENDERING_THREAD_YIELD;
			
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
		cairo_set_rgb_color(pCairo, 128.0/255.0, 128.0/255.0, 128.0/255.0);
		cairo_set_alpha(pCairo, 1.0);
		fRadius = 2;
		for(iLocation=0 ; iLocation<pLocationsArray->len ; iLocation++) {
			RENDERING_THREAD_YIELD;
			
			location_t* pLocation = g_ptr_array_index(pLocationsArray, iLocation);

			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude));
			cairo_arc(pCairo, SCALE_X(pRenderMetrics, pLocation->m_Coordinates.m_fLongitude), SCALE_Y(pRenderMetrics, pLocation->m_Coordinates.m_fLatitude), fRadius, 0, 2*M_PI);

			cairo_fill(pCairo);
		}
		cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_WINDING);
//		cairo_fill(pCairo);

	cairo_restore(pCairo);
}

void map_draw_cairo_layer_polygons(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	mappoint_t* pPoint;
	pointstring_t* pPointString;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth == 0.0) return;	// Don't both drawing with line width 0
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?
	
	// Set layer attributes	
	cairo_set_rgb_color(pCairo, pSubLayerStyle->m_clrColor.m_fRed, pSubLayerStyle->m_clrColor.m_fGreen, pSubLayerStyle->m_clrColor.m_fBlue);
	cairo_set_alpha(pCairo, pSubLayerStyle->m_clrColor.m_fAlpha);
	cairo_set_line_width(pCairo, fLineWidth);
	cairo_set_fill_rule(pCairo, CAIRO_FILL_RULE_EVEN_ODD);

	cairo_set_line_join(pCairo, pSubLayerStyle->m_nJoinStyle);
	cairo_set_line_cap(pCairo, pSubLayerStyle->m_nCapStyle);	/* CAIRO_LINE_CAP_BUTT, CAIRO_LINE_CAP_ROUND, CAIRO_LINE_CAP_CAP */
//	cairo_set_dash(pCairo, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_pfList, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nCount, 0.0);

	gint iString;
	for(iString=0 ; iString<pPointStringsArray->len ; iString++) {
		RENDERING_THREAD_YIELD;

		pPointString = g_ptr_array_index(pPointStringsArray, iString);

		if(pPointString->m_pPointsArray->len >= 3) {
			pPoint = g_ptr_array_index(pPointString->m_pPointsArray, 0);

			// move to index 0
			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

			// start at index 1 (0 was used above)
			gint iPoint;
			for(iPoint=1 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);				
				cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
			}
			//cairo_close_path(pCairo);
		}
		else {
//			g_print("pPointString->m_pPointsArray->len = %d\n", pPointString->m_pPointsArray->len);
		}

		// TODO: this is debugging of polygons.
//         cairo_save(pCairo);
//         cairo_set_rgb_color(pCairo, 1,0,0);
//
//         if(pPointString->m_pPointsArray->len >= 3) {
//             gdouble fRadius = 2;
//             // start at index 1 (0 was used above)
//             for(iPoint=0 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
//                 pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);
//                 cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
//
//                 color_t clr;
//                 util_random_color(&clr);
//                 cairo_set_rgb_color(pCairo, clr.m_fRed, clr.m_fGreen, clr.m_fBlue);
//
//                 gchar buf[20];
//                 g_snprintf(buf, 20, "%d", iPoint);
//     //~ //              cairo_select_font(pCairo, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
//                 cairo_text_path(pCairo, buf);
//
//                 cairo_arc(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude), fRadius, 0, 360.0  * (M_PI/180.0));
//                 cairo_fill(pCairo);
//                 fRadius += 2;
//             }
//         }
//         cairo_restore(pCairo);
	}
	cairo_fill(pCairo);
}

void map_draw_cairo_layer_lines(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	mappoint_t* pPoint;
	pointstring_t* pPointString;
	gint iString;
	gint iPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth <= 0.0) return;	// Don't draw invisible lines
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?

	cairo_save(pCairo);

	// Raise the tolerance way up for thin lines
	gint nCapStyle = pSubLayerStyle->m_nCapStyle;
	
	// XXX: HACK
//	nCapStyle = CAIRO_LINE_CAP_SQUARE;

	gdouble fTolerance;
	if(fLineWidth > 12.0) {	// huge line, low tolerance
		fTolerance = 0.40;
	}
	else if(fLineWidth >= 6.0) {	// medium line, medium tolerance
		fTolerance = 0.8;
	}
	else {
		if(nCapStyle == CAIRO_LINE_CAP_ROUND) {
			//g_print("forcing round->square cap style\n");
			nCapStyle = CAIRO_LINE_CAP_SQUARE;
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
	cairo_set_line_cap(pCairo, nCapStyle);	/* CAIRO_LINE_CAP_BUTT, CAIRO_LINE_CAP_ROUND, CAIRO_LINE_CAP_CAP */
	if(g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nCount > 1) {
		cairo_set_dash(pCairo, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_pfList, g_aDashStyles[pSubLayerStyle->m_nDashStyle].m_nCount, 0.0);
	}

	// Set layer attributes	
	cairo_set_rgb_color(pCairo, pSubLayerStyle->m_clrColor.m_fRed, pSubLayerStyle->m_clrColor.m_fGreen, pSubLayerStyle->m_clrColor.m_fBlue);
	cairo_set_alpha(pCairo, pSubLayerStyle->m_clrColor.m_fAlpha);
	cairo_set_line_width(pCairo, fLineWidth);

	for(iString=0 ; iString<pPointStringsArray->len ; iString++) {
		RENDERING_THREAD_YIELD;

		pPointString = g_ptr_array_index(pPointStringsArray, iString);

		if(pPointString->m_pPointsArray->len >= 2) {
			pPoint = g_ptr_array_index(pPointString->m_pPointsArray, 0);

			// go to index 0
			cairo_move_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));

			// start at index 1 (0 was used above)
			for(iPoint=1 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);//~ g_print("  point (%.05f,%.05f)\n", ScaleX(pPoint->m_fLongitude), ScaleY(pPoint->m_fLatitude));
				cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
			}
#ifdef HACK_AROUND_CAIRO_LINE_CAP_BUG
			cairo_stroke(pCairo);	// this is wrong place for it (see below)
#endif
   		}
	}

#ifndef HACK_AROUND_CAIRO_LINE_CAP_BUG
	// this is correct place to stroke, but we can't do this until Cairo fixes this bug:
	// http://cairographics.org/samples/xxx_multi_segment_caps.html
	cairo_stroke(pCairo);
#endif

	cairo_restore(pCairo);
}

void map_draw_cairo_layer_line_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	gint i;
	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];

	for(i=0 ; i<pPointStringsArray->len ; i++) {
		RENDERING_THREAD_YIELD;

		pointstring_t* pPointString = g_ptr_array_index(pPointStringsArray, i);
		if(pPointString->m_pszName[0] != '\0') {
			map_draw_cairo_line_label(pMap, pCairo, pLabelStyle, pRenderMetrics, pPointString, fLineWidth, pPointString->m_pszName);
		}
	}
}

void map_draw_cairo_layer_polygon_labels(map_t* pMap, cairo_t* pCairo, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	gint i;
	for(i=0 ; i<pPointStringsArray->len ; i++) {
		RENDERING_THREAD_YIELD;
		
		pointstring_t* pPointString = g_ptr_array_index(pPointStringsArray, i);
		if(pPointString->m_pszName[0] != '\0') {
			map_draw_cairo_polygon_label(pMap, pCairo, pLabelStyle, pRenderMetrics, pPointString, pPointString->m_pszName);
		}
	}
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
			RENDERING_THREAD_YIELD;
			
			pPoint = g_ptr_array_index(pPointString->m_pPointsArray, i);
			cairo_line_to(pCairo, SCALE_X(pRenderMetrics, pPoint->m_fLongitude), SCALE_Y(pRenderMetrics, pPoint->m_fLatitude));
		}

		cairo_set_rgb_color(pCairo, 0.0, 0.0, 0.7);
		cairo_set_alpha(pCairo, 0.6);
		cairo_set_line_width(pCairo, 6);
		cairo_set_line_cap(pCairo, CAIRO_LINE_CAP_ROUND);
		cairo_set_line_join(pCairo, CAIRO_LINE_JOIN_ROUND);
		cairo_set_miter_limit(pCairo, 5);
		cairo_stroke(pCairo);
	}
}

