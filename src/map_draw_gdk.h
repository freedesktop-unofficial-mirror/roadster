/***************************************************************************
 *            map_draw_gdk.h
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

#ifndef _MAP_DRAW_GDK_H_
#define _MAP_DRAW_GDK_H_

#include <gdk/gdk.h>

void map_draw_gdk(map_t* pMap, GPtrArray* pTiles, rendermetrics_t* pRenderMetrics, GdkPixmap* pPixmap, gint nDrawFlags);
void map_draw_gdk_xor_rect(map_t* pMap, GdkDrawable* pTargetDrawable, screenrect_t* pRect);

#endif
