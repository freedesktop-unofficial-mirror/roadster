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

static void map_draw_gdk_layer_polygons(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_gdk_layer_lines(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle);
static void map_draw_gdk_background(map_t* pMap, GdkPixmap* pPixmap);

void map_draw_gdk(map_t* pMap, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags)
{
	TIMER_BEGIN(maptimer, "BEGIN RENDER MAP (gdk)");

	// 1. Setup (save values so we can restore them)
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
		for(i=0 ; i<NUM_ELEMS(layerdraworder) ; i++) {
			gint nLayer = layerdraworder[i].nLayer;
			gint nSubLayer = layerdraworder[i].nSubLayer;
	
			if(layerdraworder[i].eSubLayerRenderType == SUBLAYER_RENDERTYPE_LINES) {
				map_draw_gdk_layer_lines(pMap, pPixmap,
						pRenderMetrics,
				/* geometry */ 	pMap->m_apLayerData[nLayer]->m_pPointStringsArray,
				/* style */ 	&(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
			else if(layerdraworder[i].eSubLayerRenderType == SUBLAYER_RENDERTYPE_POLYGONS) {
				map_draw_gdk_layer_polygons(pMap, pPixmap,
						pRenderMetrics,
				/* geometry */ 	pMap->m_apLayerData[nLayer]->m_pPointStringsArray,
				/* style */ 	&(g_aLayers[nLayer]->m_Style.m_aSubLayers[nSubLayer]),
						&(g_aLayers[nLayer]->m_TextLabelStyle));
			}
		}
	}

	// 3. Labels
	// ...GDK just shouldn't attempt text. :)

	// 4. Cleanup
	gdk_gc_set_values(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &gcValues, GDK_GC_FOREGROUND | GDK_GC_BACKGROUND | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE | GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE);
	TIMER_END(maptimer, "END RENDER MAP (gdk)");
}

static void map_draw_gdk_layer_polygons(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	mappoint_t* pPoint;
	pointstring_t* pPointString;
	gint iString;
	gint iPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth <= 0.0) return;	// Don't draw invisible lines
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?  (not that we respect it in gdk drawing anyway)

	// Raise the tolerance way up for thin lines
	gint nCapStyle = pSubLayerStyle->m_nCapStyle;

	// Set line style
	gdk_gc_set_line_attributes(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			   ((gint)fLineWidth), GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);

	GdkColor clr;
	clr.red = pSubLayerStyle->m_clrColor.m_fRed * 65535;
	clr.green = pSubLayerStyle->m_clrColor.m_fGreen * 65535;
	clr.blue = pSubLayerStyle->m_clrColor.m_fBlue * 65535;
	gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);

	for(iString=0 ; iString<pPointStringsArray->len ; iString++) {
		pPointString = g_ptr_array_index(pPointStringsArray, iString);

		if(pPointString->m_pPointsArray->len >= 2) {
			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			if(pPointString->m_pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
				g_warning("not drawing line with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
				continue;
			}

			// start at index 1 (0 was used above)
			for(iPoint=0 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);
				
				aPoints[iPoint].x = SCALE_X(pRenderMetrics, pPoint->m_fLongitude);
				aPoints[iPoint].y = SCALE_Y(pRenderMetrics, pPoint->m_fLatitude);
			}
			gdk_draw_polygon(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
				TRUE, aPoints, pPointString->m_pPointsArray->len);
   		}
	}
}

static void map_draw_gdk_layer_lines(map_t* pMap, GdkPixmap* pPixmap, rendermetrics_t* pRenderMetrics, GPtrArray* pPointStringsArray, sublayerstyle_t* pSubLayerStyle, textlabelstyle_t* pLabelStyle)
{
	mappoint_t* pPoint;
	pointstring_t* pPointString;
	gint iString;
	gint iPoint;

	gdouble fLineWidth = pSubLayerStyle->m_afLineWidths[pRenderMetrics->m_nZoomLevel-1];
	if(fLineWidth <= 0.0) return;	// Don't draw invisible lines
	if(pSubLayerStyle->m_clrColor.m_fAlpha == 0.0) return;	// invisible?  (not that we respect it in gdk drawing anyway)

	// Use GDK dash style if ANY dash pattern is set
	gint nDashStyle = GDK_LINE_SOLID;
	if(pSubLayerStyle->m_nDashStyle != 0) {
		nDashStyle = GDK_LINE_ON_OFF_DASH;

		// gdk_gc_set_dashes
	}

	// XXX: Don't use round at low zoom levels
	// gint nCapStyle = pSubLayerStyle->m_nCapStyle;
	gint nCapStyle = GDK_CAP_ROUND;
	if(fLineWidth < 8) {
		nCapStyle = GDK_CAP_PROJECTING;
	}

	// Set line style
	gdk_gc_set_line_attributes(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			   ((gint)fLineWidth), nDashStyle, nCapStyle, GDK_JOIN_MITER);

	GdkColor clr;
	clr.red = pSubLayerStyle->m_clrColor.m_fRed * 65535;
	clr.green = pSubLayerStyle->m_clrColor.m_fGreen * 65535;
	clr.blue = pSubLayerStyle->m_clrColor.m_fBlue * 65535;
	gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);

	for(iString=0 ; iString<pPointStringsArray->len ; iString++) {
		pPointString = g_ptr_array_index(pPointStringsArray, iString);

		if(pPointString->m_pPointsArray->len >= 2) {
			GdkPoint aPoints[MAX_GDK_LINE_SEGMENTS];

			if(pPointString->m_pPointsArray->len > MAX_GDK_LINE_SEGMENTS) {
				g_warning("not drawing line with > %d segments\n", MAX_GDK_LINE_SEGMENTS);
				continue;
			}

			// start at index 1 (0 was used above)
			for(iPoint=0 ; iPoint<pPointString->m_pPointsArray->len ; iPoint++) {
				pPoint = g_ptr_array_index(pPointString->m_pPointsArray, iPoint);
				
				aPoints[iPoint].x = SCALE_X(pRenderMetrics, pPoint->m_fLongitude);
				aPoints[iPoint].y = SCALE_Y(pRenderMetrics, pPoint->m_fLatitude);
			}
			gdk_draw_lines(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
				aPoints, pPointString->m_pPointsArray->len);
   		}
	}
}

static void map_draw_gdk_background(map_t* pMap, GdkPixmap* pPixmap)
{
	GdkColor clr;
	clr.red = 247/255.0 * 65535;
	clr.green = 235/255.0 * 65535;
	clr.blue = 230/255.0 * 65535;
	gdk_gc_set_rgb_fg_color(pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)], &clr);
	
	gdk_draw_rectangle(pPixmap, pMap->m_pTargetWidget->style->fg_gc[GTK_WIDGET_STATE(pMap->m_pTargetWidget)],
			TRUE, 0,0, pMap->m_MapDimensions.m_uWidth, pMap->m_MapDimensions.m_uHeight);
}
