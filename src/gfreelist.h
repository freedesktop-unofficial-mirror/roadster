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

#ifndef __G_FREELIST_H__
#define __G_FREELIST_H__

#include <glib/gtypes.h>
#include <glib/gutils.h>

G_BEGIN_DECLS

typedef struct _GFreeList GFreeList;
typedef struct _GFreeListAtom GFreeListAtom;
typedef struct _GFreeListBlock GFreeListBlock;

struct _GFreeListAtom
{
  GFreeListAtom *next;    
};

struct _GFreeList
{
  /*< private >*/
  gboolean		allocated;
  gsize			atom_size;
  gint			n_prealloc;
  GFreeListAtom *	atoms;
  GFreeListBlock *	blocks;
};

#define G_FREE_LIST_NAME(name)						\
  g__ ## name ## __free_list

#define G_FREE_LIST_EXTERN(name)					\
  extern GFreeList G_FREE_LIST_NAME (name)

/* Defining free lists */
#define G_FREE_LIST_DEFINE(name, atom_size, n_prealloc)			\
  GFreeList G_FREE_LIST_NAME (name) =					\
  {									\
    FALSE,					/* allocated */		\
    MAX (atom_size, sizeof (GFreeListAtom)),	/* atom_size */		\
    n_prealloc,					/* n_prealloc */	\
    NULL,					/* atoms */		\
    NULL					/* blocks */		\
  }

#define G_FREE_LIST_DEFINE_STATIC(name, atom_size, n_prealloc)		\
  static G_FREE_LIST_DEFINE (name, atom_size, n_prealloc)

/* Macros to allocate and free atoms */
#define G_FREE_LIST_ALLOC(name)						\
  g_free_list_alloc (&G_FREE_LIST_NAME (name))

#define G_FREE_LIST_FREE(name, mem)					\
  g_free_list_free (&G_FREE_LIST_NAME (name), mem)


/* functions */
GFreeList *   g_free_list_new        (gsize      atom_size,
				      gint       n_prealloc);
void          g_free_list_destroy    (GFreeList *free_list);

G_INLINE_FUNC
gpointer      g_free_list_alloc      (GFreeList *list);

G_INLINE_FUNC
void          g_free_list_free       (GFreeList *list,
				      gpointer   mem);

/* internal functions */
gpointer g_free_list_alloc_internal (GFreeList *flist);
void     g_free_list_free_internal  (GFreeList *flist,
				     gpointer   mem);

/* inline functions */

#if defined (G_CAN_INLINE) || defined (__G_FREELIST_C__)

G_INLINE_FUNC gpointer
g_free_list_alloc (GFreeList *flist)
{
  GFreeListAtom *result = flist->atoms;

  if (G_LIKELY (result))
    flist->atoms = result->next;
  else
    result = g_free_list_alloc_internal (flist);

  return result;
}

G_INLINE_FUNC void
g_free_list_free (GFreeList *flist,
		  gpointer   mem)
{
  ((GFreeListAtom *)mem)->next = flist->atoms;
  flist->atoms = mem;
}

#endif /* G_CAN_INLINE || __G_FREELIST_C__ */

G_END_DECLS

#endif /* __G_FREELIST_H__ */
