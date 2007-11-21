/***************************************************************************
 *            road.h
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

#ifndef _ROAD_H_
#define _ROAD_H_

#include "map.h"

typedef struct {
	GArray* pMapPointsArray;

	gchar* pszName;
	gint nAddressLeftStart;
	gint nAddressLeftEnd;
	gint nAddressRightStart;
	gint nAddressRightEnd;

	maprect_t rWorldBoundingBox;
} road_t;

// ESuffixLength
typedef enum {
	ROAD_SUFFIX_LENGTH_LONG,
	ROAD_SUFFIX_LENGTH_SHORT
} ESuffixLength;

#define ROAD_SUFFIX_NONE (0)

const gchar* road_suffix_itoa(gint nSuffixID, ESuffixLength eSuffixLength);
gboolean road_suffix_atoi(const gchar* pszSuffix, gint* pReturnSuffixID);

#endif
