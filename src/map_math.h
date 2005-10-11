/***************************************************************************
 *            map_math.h
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

#ifndef _MAP_MATH_H_
#define _MAP_MATH_H_

typedef enum {
	OVERLAP_FULL,
	OVERLAP_NONE,
	OVERLAP_PARTIAL
} EOverlapType;

gboolean map_math_screenpoint_in_screenrect(screenpoint_t* pPt, screenrect_t* pRect);
gboolean map_math_maprects_equal(maprect_t* pA, maprect_t* pB);

EOverlapType map_rect_a_overlap_type_with_rect_b(const maprect_t* pA, const maprect_t* pB);
gboolean map_rects_overlap(const maprect_t* p1, const maprect_t* p2);

void map_math_simplify_pointstring(const GArray* pInput, gdouble fTolerance, GArray* pOutput);
gdouble map_math_point_distance_squared_from_line(mappoint_t* pHitPoint, mappoint_t* pPoint1, mappoint_t* pPoint2);

#endif
