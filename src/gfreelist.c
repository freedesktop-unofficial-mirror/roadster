/* GLIB - Library of useful routines for C programming
 *
 * Copyright (C) 2003 Soeren Sandmann (sandmann@daimi.au.dk)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define G_IMPLEMENT_INLINES 1
#define __G_FREELIST_C__

#include "gfreelist.h"

#include <glib.h>

struct _GFreeListBlock
{
  GFreeListBlock *next;
  gint8 *data;
};
     
GFreeList *
g_free_list_new (gsize atom_size,
		 gint n_prealloc)
{
  GFreeList *list;

  g_return_val_if_fail (atom_size > 0, NULL);
  g_return_val_if_fail (n_prealloc > 0, NULL);

  list = g_new0 (GFreeList, 1);

  list->allocated = TRUE;
  list->atom_size = MAX (atom_size, sizeof (GFreeListAtom));
  list->n_prealloc = n_prealloc;
  list->atoms = NULL;
  list->blocks = NULL;
  
  return list;
}

static void
g_free_list_ensure_atoms (GFreeList *list)
{
  if (!list->atoms)
    {
      gint i;
#ifdef DISABLE_MEM_POOLS
      const int n_atoms = 1;
#else
      const int n_atoms = list->n_prealloc;
#endif
      
      GFreeListBlock *block = g_new (GFreeListBlock, 1);
      block->data = g_malloc (list->atom_size * n_atoms);
      block->next = list->blocks;
      list->blocks = block;

      for (i = n_atoms - 1; i >= 0; --i)
	{
	  GFreeListAtom *atom =
	    (GFreeListAtom *)(block->data + i * list->atom_size);

	  atom->next = list->atoms;
	  list->atoms = atom;
	}
    }
}

gpointer
g_free_list_alloc_internal (GFreeList *list)
{
  gpointer result;

  g_return_val_if_fail (list != NULL, NULL);
  
  if (!list->atoms)
    g_free_list_ensure_atoms (list);

  result = list->atoms;
  list->atoms = list->atoms->next;

  return result;
}

void
g_free_list_free_internal (GFreeList *list,
			   gpointer   mem)
{
  GFreeListAtom *atom;

  g_return_if_fail (list != NULL);

  if (!mem)
    return;
  
  atom = mem;
  
  atom->next = list->atoms;
  list->atoms = atom;
}

static void
g_free_list_free_all (GFreeList *list)
{
  GFreeListBlock *block;

  g_return_if_fail (list != NULL);

  block = list->blocks;
  while (block)
    {
      GFreeListBlock *next = block->next;

      g_free (block->data);
      g_free (block);
      
      block = next;
    }
  
  list->atoms = NULL;
  list->blocks = NULL;
}

void
g_free_list_destroy (GFreeList *list)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (list->allocated);

  g_free_list_free_all (list);
  g_free (list);
}
