/***************************************************************************
 *            geometryset.h
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

#ifndef __GEOMETRYSET_H__
#define __GEOMETRYSET_H__

// a geometry set holds all the geometry for a layer
typedef struct geometryset {
	GPtrArray* m_pPointStringsArray;
} geometryset_t;

// holds points that form a road or polygon (park, etc.)
typedef struct {
	gchar* m_pszName;
	GPtrArray* m_pPointsArray;
} pointstring_t;

#include "map.h"

void geometryset_init();
void geometryset_free(geometryset_t* pGeometrySet);
void geometryset_clear(geometryset_t* pGeometrySet);

gboolean geometryset_new(geometryset_t** ppGeometrySet);

gboolean geometryset_util_new_point(mappoint_t** ppPoint);
gboolean geometryset_util_new_pointstring(pointstring_t** ppPointString);
void geometryset_util_free_pointstring(pointstring_t* pPointString);

gboolean geometryset_load_geometry(maprect_t* pRect);

void geometryset_debug_print(geometryset_t* pGeometrySet);

#endif
