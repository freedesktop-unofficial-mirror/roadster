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

GMemChunk* g_pPointChunkAllocator;		// chunk allocators to be shared by all geometrysets

void point_init(void)
{
	// create memory allocators
	g_pPointChunkAllocator = g_mem_chunk_new("ROADSTER points",
			sizeof(mappoint_t), 1000, G_ALLOC_AND_FREE);
	g_return_if_fail(g_pPointChunkAllocator != NULL);
}

/*******************************************************
** point alloc/free
*******************************************************/
gboolean point_alloc(mappoint_t** ppPoint)
{
	g_return_val_if_fail(ppPoint != NULL, FALSE);
	g_return_val_if_fail(*ppPoint == NULL, FALSE);	// must be a pointer to a NULL pointer

	// get a new point struct from the allocator
	mappoint_t* pNew = g_mem_chunk_alloc0(g_pPointChunkAllocator);
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
	g_mem_chunk_free(g_pPointChunkAllocator, pPoint);
}

