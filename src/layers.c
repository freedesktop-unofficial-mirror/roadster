/***************************************************************************
 *            layers.c
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

#include <cairo.h>
#include "../include/layers.h"

gdouble pDash_Solid[] = {10};
gdouble pDash_10_10[] = {1, 14};

dashstyle_t g_aDashStyles[NUM_DASH_STYLES] = {
	{NULL, 0},
	{pDash_10_10, 2},
};

layer_t g_aLayers[NUM_LAYERS + 1] = {
/* 0 */	{0, "UNUSED LAYER",
			{{
			/* line widths @ zooms	  	  						 color 									  dash, join, cap	*/
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0/255.0, 0/255.0, 0/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0/255.0, 0/255.0, 0/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,0,0,0,0}, /* font size */
 			 {0,0,0,0,0,0,0,0,0,0},	/* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 1 */	{LAYER_MINORSTREET, "Minor Roads",
			{{
			/* line widths @ zooms	  	  						 color 									  dash, join, cap	*/
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 6.0, 12.0, 18.0,36.0}, {159/255.0, 149/255.0, 127/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_ROUND},
			{{0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 4.5, 10.5, 16.5,34.5}, {255/255.0, 251/255.0, 255/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_ROUND}
			}},

			{{0,0,0,0,0,0,0,8,14,25}, /* font size */
	 		 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 2 */	{LAYER_MAJORSTREET, "Major Roads",
			{{
			{{0.0, 0.0, 2.0, 3.0, 5.0, 10.0, 14.0, 17.0, 24.0,32.0}, {173/255.0, 162/255.0, 140/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 1.8, 2.5, 4.3, 9.0,  13.0, 15.5, 22.0,30.0}, {255/255.0, 251/255.0, 115/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},

			{{0,0,0,0,0,6,8,10,16,26}, /* font size */
			 {0,0,0,0,0,1,1,1,1,1}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 3 */	{LAYER_MINORHIGHWAY, "Minor Highways",
			{{
			{{0.0, 0.0, 4.0, 5.0, 8.0, 9.0, 17.0,22.0,28.0,36.0}, {173/255.0, 162/255.0, 140/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 3.5, 4.1, 6.8, 7.5, 15.0,20.0,26.0,34.0}, {255/255.0, 202/255.0, 58/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,8,8,10,18}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 4 */	{LAYER_MINORHIGHWAY_RAMP, "Minor Highway Ramps",
			{{
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 5.0,8.0,10.0}, {173/255.0, 142/255.0, 33/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.5, 3.5,6.0,8.0}, {247/255.0, 223/255.0, 74/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,8,8,10,18}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 5 */	{LAYER_MAJORHIGHWAY, "Major Highways",
			{{
			{{0.0, 0.0, 4.0, 10.0, 12.0, 16.0, 22.0,28.0,34.0,38.0}, {173/255.0, 162/255.0, 140/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 2.0, 8.5,  10.0, 14.0, 20.0,26.0,32.0,36.0}, {239/255.0, 190/255.0, 33/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,8,8,10,18}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 6 */	{LAYER_MAJORHIGHWAY_RAMP, "Major Highway Ramps",
			{{
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 7.0,16.0}, {173/255.0, 162/255.0, 140/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 5.5,14.0}, {0/255.0, 190/255.0, 0/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},

			{{0,0,0,0,0,0,0,0,0,0}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},
			
/* 7 */	{LAYER_RAILROAD, "Railroads",
			{{
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.6, 0.6, 1.2}, {80/255.0, 80/255.0, 110/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 3.0, 4.0, 7.0}, {80/255.0, 80/255.0, 110/255.0, 1.00}, 1, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,8,10,12,12}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,1,1,1,1}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 8 */	{LAYER_PARK, "Parks",
			{{
			{{0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, {173/255.0, 202/255.0, 148/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {135/255.0, 172/255.0, 126/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,0,6,8,10}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 9 */{LAYER_RIVER, "Rivers",
			{{
			{{0.0, 0.0, 0.0, 0.0, 1.0, 4.0, 6.0, 10.0, 14.0, 18.0}, {148/255.0, 178/255.0, 197/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0/255.0, 0/255.0, 0/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,0,6,8,10}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 10 */{LAYER_LAKE, "Lakes",
			{{
			{{0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, {148/255.0, 178/255.0, 197/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0/255.0, 0/255.0, 0/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,0,6,8,10}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},

/* 11 */{LAYER_MISC_AREA, "Misc Areas",
			{{
			{{0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}, {155/255.0, 155/255.0, 155/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT},
			{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0/255.0, 0/255.0, 0/255.0, 1.00}, 0, CAIRO_LINE_JOIN_MITER, CAIRO_LINE_CAP_BUTT}
			}},
			
			{{0,0,0,0,0,0,0,0,0,0}, /* font size */
			 {0,0,0,0,0,0,0,0,0,0}, /* bold */
			 {0,0,0,0,0,0,0,0,0,0}, /* halo */
			 {0,0,0,0}},
			NULL},
};

void layers_init()
{
	gint i;
	for(i=LAYER_FIRST ; i<=LAYER_LAST ; i++) {
		geometryset_new(&(g_aLayers[i].m_pGeometrySet));
	}
}

void layers_clear()
{
	gint i;
	for(i=LAYER_FIRST ; i<=LAYER_LAST ; i++) {
		geometryset_clear(g_aLayers[i].m_pGeometrySet);
	}
}
