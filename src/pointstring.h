/***************************************************************************
 *            pointstring.h
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

#ifndef _POINTSTRING_H_
#define _POINTSTRING_H_

// holds points that form a road or polygon (park, etc.)
typedef struct {
	gchar* m_pszName;
	GPtrArray* m_pPointsArray;
} pointstring_t;

void pointstring_init(void);
gboolean pointstring_alloc(pointstring_t** ppPointString);
void pointstring_free(pointstring_t* pPointString);
void pointstring_append_point(pointstring_t* pPointString, const mappoint_t* pPoint);

#endif
