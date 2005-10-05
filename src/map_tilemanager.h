/***************************************************************************
 *            map_tilemanager.h
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

#ifndef _MAP_TILEMANAGER_H_
#define _MAP_TILEMANAGER_H_

#include <gtk/gtk.h>

typedef struct {
	GPtrArray* apTileCachedArrays[4];	// MAP_NUM_LEVELS_OF_DETAIL
} maptilemanager_t;

#include "map.h"

typedef struct {
	maprect_t rcWorldBoundingBox;
	GPtrArray* apMapObjectArrays[ MAP_NUM_OBJECT_TYPES + 1 ];
} maptile_t;

maptilemanager_t* map_tilemanager_new();

// returns GArray containing maptile_t types 
GPtrArray* map_tilemanager_load_tiles_for_worldrect(maptilemanager_t* pTileManager, maprect_t* pWorldRect, gint nLOD);

#endif
