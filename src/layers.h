/***************************************************************************
 *            layers.h
 *
 *  Copyright 2005
 *  Ian McIntosh <ian_mcintosh@linuxadvocate.org>
 *  Nathan Fredrickson <nathan@silverorange.com>
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

G_BEGIN_DECLS

#include "map.h"

typedef struct color {
	gfloat m_fRed;
	gfloat m_fGreen;
	gfloat m_fBlue;
	gfloat m_fAlpha;
} color_t;

#define NUM_DASH_STYLES (2)

typedef struct dashstyle {
	gdouble* m_pafDashList;	// the dashes, as floating point (for Cairo)
	gint8* m_panDashList;	// the dashes, as integers (for GDK)
	gint m_nDashCount;
} dashstyle_t;

// defines the look of a layer
typedef struct layerstyle {
	color_t m_clrPrimary;	// Color used for polygon fill or line stroke
	gdouble m_fLineWidth;

	gint m_nJoinStyle;
	gint m_nCapStyle;
	gint m_nDashStyle;

	// XXX: switch to this:
	//dashstyle_t m_pDashStyle;	// can be NULL

	// Used just for text
	gdouble m_fFontSize;
	gboolean m_bFontBold;
	gdouble m_fHaloSize;	// actually a stroke width
	color_t m_clrHalo;
} layerstyle_t;

typedef struct layer {
	gint m_nDataSource;		// which data to use (lakes, roads...)
	gint m_nDrawType;		// as lines, polygons, etc.

	// A layer has a style for each zoomlevel
	layerstyle_t* m_paStylesAtZoomLevels[ NUM_ZOOM_LEVELS ];
} layer_t;

//extern layer_t * g_aLayers[NUM_LAYERS+1];
extern GPtrArray* g_pLayersArray;
extern dashstyle_t g_aDashStyles[NUM_DASH_STYLES];

void layers_init(void);
void layers_deinit(void);
void layers_reload(void);

G_END_DECLS

#endif /* _LAYERS_H_ */
