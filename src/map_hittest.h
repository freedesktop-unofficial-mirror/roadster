/***************************************************************************
 *            map_hittest.h
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

#ifndef _MAP_HITTEST_H_
#define _MAP_HITTEST_H_

#include "map.h"

typedef enum {
	MAP_HITTYPE_LOCATION,
	MAP_HITTYPE_ROAD,
	
	// the following all use LocationSelectionHit in the union below
	MAP_HITTYPE_LOCATIONSELECTION,	// hit somewhere on a locationselection graphic (info balloon)
	MAP_HITTYPE_LOCATIONSELECTION_CLOSE,	// hit locationselection graphic close graphic (info balloon [X])
	MAP_HITTYPE_LOCATIONSELECTION_EDIT,	// hit locationselection graphic edit graphic (info balloon "edit")

	MAP_HITTYPE_URL,
} EMapHitType;

typedef struct {
	EMapHitType eHitType;
	gchar* pszText;
	union {
		struct {
			gint nLocationID;
			mappoint_t Coordinates;
		} LocationHit;

		struct {
			gint nRoadID;
			mappoint_t ClosestPoint;
		} RoadHit;

		struct {
			gint nLocationID;
		} LocationSelectionHit;

		struct {
			gchar* pszURL;
		} URLHit;
	};
} maphit_t;

gboolean map_hittest(map_t* pMap, mappoint_t* pMapPoint, maphit_t** ppReturnStruct);
void map_hittest_maphit_free(map_t* pMap, maphit_t* pHitStruct);

#endif
