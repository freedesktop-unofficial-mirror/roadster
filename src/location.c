/***************************************************************************
 *            location.c
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
#include "location.h"

struct {
	GMemChunk* m_pLocationChunkAllocator;
} g_Location;

void location_init()
{
	g_Location.m_pLocationChunkAllocator = g_mem_chunk_new("location chunk allocator",
			sizeof(location_t), 1000, G_ALLOC_AND_FREE);
	g_return_if_fail(g_Location.m_pLocationChunkAllocator != NULL);
}

// get a new point struct from the allocator
gboolean location_new(location_t** ppLocation)
{
	g_return_val_if_fail(ppLocation != NULL, FALSE);
	g_return_val_if_fail(*ppLocation == NULL, FALSE);	// must be a pointer to a NULL pointer
	g_return_val_if_fail(g_Location.m_pLocationChunkAllocator != NULL, FALSE);

	location_t* pNew = g_mem_chunk_alloc0(g_Location.m_pLocationChunkAllocator);
	if(pNew) {
		*ppLocation = pNew;
		return TRUE;
	}
	return FALSE;
}

// return a location_t struct to the allocator
void location_free(location_t* pLocation)
{
	g_return_if_fail(pLocation != NULL);
	g_return_if_fail(g_Location.m_pLocationChunkAllocator != NULL);

	// give back to allocator
	g_mem_chunk_free(g_Location.m_pLocationChunkAllocator, pLocation);
}
