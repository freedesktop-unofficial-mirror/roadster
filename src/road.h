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

typedef struct {
	GArray* pMapPointsArray;

	gchar* pszName;
	gint nAddressLeftStart;
	gint nAddressLeftEnd;
	gint nAddressRightStart;
	gint nAddressRightEnd;
} road_t;

// ESuffixLength
typedef enum {
	ROAD_SUFFIX_LENGTH_SHORT,
	ROAD_SUFFIX_LENGTH_LONG
} ESuffixLength;

enum ERoadNameSuffix {			// these can't change once stored in DB
	ROAD_SUFFIX_FIRST = 0,
	ROAD_SUFFIX_NONE = 0,

	ROAD_SUFFIX_ROAD = 1,
	ROAD_SUFFIX_STREET,
	ROAD_SUFFIX_DRIVE,
	ROAD_SUFFIX_BOULEVARD,	// blvd
	ROAD_SUFFIX_AVENUE,
	ROAD_SUFFIX_CIRCLE,
	ROAD_SUFFIX_SQUARE,
	ROAD_SUFFIX_PATH,
	ROAD_SUFFIX_WAY,
	ROAD_SUFFIX_PLAZA,
	ROAD_SUFFIX_TRAIL,
	ROAD_SUFFIX_LANE,
	ROAD_SUFFIX_CROSSING,
	ROAD_SUFFIX_PLACE,
	ROAD_SUFFIX_COURT,
	ROAD_SUFFIX_TURNPIKE,
	ROAD_SUFFIX_TERRACE,
	ROAD_SUFFIX_ROW,
	ROAD_SUFFIX_PARKWAY,

	ROAD_SUFFIX_BRIDGE,
	ROAD_SUFFIX_HIGHWAY,
	ROAD_SUFFIX_RUN,
	ROAD_SUFFIX_PASS,
	
	ROAD_SUFFIX_FREEWAY,
	ROAD_SUFFIX_ALLEY,
	ROAD_SUFFIX_CRESCENT,
	ROAD_SUFFIX_TUNNEL,
	ROAD_SUFFIX_WALK,
	ROAD_SUFFIX_BRANCE,
	ROAD_SUFFIX_COVE,
	ROAD_SUFFIX_BYPASS,
	ROAD_SUFFIX_LOOP,
	ROAD_SUFFIX_SPUR,
	ROAD_SUFFIX_RAMP,
	ROAD_SUFFIX_PIKE,
	ROAD_SUFFIX_GRADE,
	ROAD_SUFFIX_ROUTE,
	ROAD_SUFFIX_ARC,

	ROAD_SUFFIX_LAST = ROAD_SUFFIX_ARC
};

const gchar* road_suffix_itoa(gint nSuffixID, ESuffixLength eSuffixLength);
gboolean road_suffix_atoi(const gchar* pszSuffix, gint* pReturnSuffixID);

#endif
