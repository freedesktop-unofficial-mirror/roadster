/***************************************************************************
 *            history.h
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

#ifndef _HISTORY_H_
#define _HISTORY_H_

#include "map.h"

typedef struct {
	gint nCurrentIndex;
	gint nTotalItems;
	GArray* MapViewArray;
} maphistory_t;

maphistory_t* map_history_new();
void map_history_add(maphistory_t* pHistory, mappoint_t* pPoint, gint nZoomLevel);
gboolean map_history_can_go_forward(maphistory_t* pHistory);
gboolean map_history_can_go_back(maphistory_t* pHistory);
gboolean map_history_go_forward(maphistory_t* pHistory);
gboolean map_history_go_back(maphistory_t* pHistory);

void map_history_get_current(maphistory_t* pHistory, mappoint_t* pReturnPoint, gint* pnReturnZoomLevel);

#endif
