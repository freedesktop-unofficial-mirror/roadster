/***************************************************************************
 *            locationset.h
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
 
#ifndef _LOCATIONSET_H
#define _LOCATIONSET_H

G_BEGIN_DECLS

#include "map.h"
	
typedef struct locationsetstyle {
	// icon?
	int a;
} locationsetstyle_t;

// a single location (eg. "Someday Cafe")
typedef struct location {
	gint m_nID;
	gchar* m_pszName;
	mappoint_t m_Coordinates;
} location_t;

// a set of locations (eg. "Coffee Shops")
typedef struct locationset {
	gint m_nID;
	gchar* m_pszName;

	locationsetstyle_t m_Style;

	GPtrArray* m_pLocationsArray;
} locationset_t;

void locationset_init(void);
void locationset_load_locationsets(void);
gboolean locationset_load_locations(maprect_t* pRect);
gboolean locationset_add_location(gint nLocationSetID, mappoint_t* pPoint, gint* pReturnID);
const GPtrArray* locationset_get_set_array(void);

G_END_DECLS

#endif /* _LOCATIONSET_H */
