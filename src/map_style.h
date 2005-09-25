/***************************************************************************
 *            map_style.h
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

#ifndef _MAP_STYLE_H_
#define _MAP_STYLE_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#include "main.h"
#include "map.h"

typedef struct dashstyle {
	gdouble* pafDashList;	// the dashes, as an array of gdouble's (for Cairo)
	gint8* panDashList;	// the dashes, as an array of gint8's (for GDK)
	gint nDashCount;		// how many
} dashstyle_t;

// defines the look of a layer
typedef struct layerstyle {
	color_t clrPrimary;	// Color used for polygon fill or line stroke
	gdouble fLineWidth;

	gint nJoinStyle;
	gint nCapStyle;
	
	dashstyle_t* pDashStyle;

	// XXX: switch to this:
	//dashstyle_t pDashStyle;	// can be NULL

	// Used just for text
	gdouble fFontSize;
	gboolean bFontBold;
	gdouble fHaloSize;	// actually a stroke width
	color_t clrHalo;
	gint nPixelOffsetX;
	gint nPixelOffsetY;
} maplayerstyle_t;

typedef struct layer {
	gint nDataSource;		// which data to use (lakes, roads...)
	gint nDrawType;		// as lines, polygons, etc.

	// A layer has a style for each zoomlevel
	maplayerstyle_t* paStylesAtZoomLevels[ NUM_ZOOM_LEVELS ];
} maplayer_t;

//extern layer_t * g_aLayers[NUM_LAYERS+1];
//extern GPtrArray* g_pLayersArray;

void map_style_init(void);
void map_style_deinit(void);

void map_style_load(map_t* pMap, const gchar* pszFileName);
void map_style_reload(map_t* pMap, const gchar* pszFileName);

G_END_DECLS

#endif /* _MAP_STYLE_H_ */
