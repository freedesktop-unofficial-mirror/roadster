/***************************************************************************
 *            layers.h
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

#ifndef _LAYERS_H_
#define _LAYERS_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LAYER_NONE				(0)

#define LAYER_MINORSTREET			(1)
#define LAYER_MAJORSTREET			(2)
#define LAYER_MINORHIGHWAY			(3)
#define LAYER_MINORHIGHWAY_RAMP			(4)
#define LAYER_MAJORHIGHWAY			(5)	// used?
#define LAYER_MAJORHIGHWAY_RAMP			(6)	// used?
#define LAYER_RAILROAD				(7)
#define LAYER_PARK				(8)
#define LAYER_RIVER				(9)
#define LAYER_LAKE				(10)
#define LAYER_MISC_AREA				(11)

#define NUM_LAYERS 				(11)

#define LAYER_FIRST				(1)
#define LAYER_LAST				(11)

#include "map.h"

typedef struct color {
	gfloat m_fRed;
	gfloat m_fGreen;
	gfloat m_fBlue;
	gfloat m_fAlpha;
} color_t;

#define NUM_DASH_STYLES (2)

typedef struct dashstyle {
	gdouble* m_pfList;
	gint m_nCount;
} dashstyle_t;
dashstyle_t g_aDashStyles[NUM_DASH_STYLES];

typedef struct sublayerstyle {
	gdouble m_afLineWidths[MAX_ZOOM_LEVEL];
	color_t m_clrColor;
	gint m_nDashStyle;
	gint m_nJoinStyle;
	gint m_nCapStyle;
} sublayerstyle_t;

typedef struct textlabelstyle {
	gdouble m_afFontSizeAtZoomLevel[MAX_ZOOM_LEVEL];
	gint m_abBoldAtZoomLevel[MAX_ZOOM_LEVEL];	// 0s or 1s
	gint m_afHaloAtZoomLevel[MAX_ZOOM_LEVEL];	// stroke width
	color_t m_clrColor;
	// font family...
} textlabelstyle_t;

// defines the look of a layer
typedef struct layerstyle {
	sublayerstyle_t m_aSubLayers[2];
} layerstyle_t;

typedef struct layer {
	gint nLayerIndex;
	gchar* m_pszName;
	layerstyle_t m_Style;
	textlabelstyle_t m_TextLabelStyle;
} layer_t;

extern layer_t g_aLayers[NUM_LAYERS+1];

#ifdef __cplusplus
}
#endif

#endif /* _LAYERS_H_ */
