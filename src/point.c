/***************************************************************************
 *            point.c
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

#include <gtk/gtk.h>
#include "map.h"
#include "point.h"

#define USE_GFREELIST

#ifdef USE_GFREELIST
#include "gfreelist.h"
GFreeList* g_pPointFreeList;
#else
GMemChunk* g_pPointChunkAllocator;		// chunk allocators to be shared by all geometrysets
#endif

void point_init(void)
{
#ifdef USE_GFREELIST
	g_pPointFreeList = g_free_list_new(sizeof(mappoint_t), 1000);
#else
	// create memory allocators
	g_pPointChunkAllocator = g_mem_chunk_new("ROADSTER points",
			sizeof(mappoint_t), 1000, G_ALLOC_AND_FREE);
#endif
}

/*******************************************************
** point alloc/free
*******************************************************/
gboolean point_alloc(mappoint_t** ppPoint)
{
	g_return_val_if_fail(ppPoint != NULL, FALSE);
	g_return_val_if_fail(*ppPoint == NULL, FALSE);	// must be a pointer to a NULL pointer

	// get a new point struct from the allocator
#ifdef USE_GFREELIST
	mappoint_t* pNew = g_free_list_alloc(g_pPointFreeList);
	memset(pNew, 0, sizeof(mappoint_t));
#else
	mappoint_t* pNew = g_mem_chunk_alloc0(g_pPointChunkAllocator);
#endif
	if(pNew) {
		*ppPoint = pNew;
		return TRUE;
	}
	return FALSE;
}

void point_free(mappoint_t* pPoint)
{
	g_return_if_fail(pPoint != NULL);

	// give back to allocator
#ifdef USE_GFREELIST
	g_free_list_free(g_pPointFreeList, pPoint);
#else
	g_mem_chunk_free(g_pPointChunkAllocator, pPoint);
#endif
}

