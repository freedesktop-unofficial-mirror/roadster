/***************************************************************************
 *            location.h
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

#ifndef _LOCATION_H
#define _LOCATION_H

#include "map.h"

G_BEGIN_DECLS

#define LOCATION_ATTRIBUTE_ID_NAME	(1)		// "name" must be #1 in the DB
#define LOCATION_ATTRIBUTE_ID_ADDRESS	(2)		// "address" must be #2 in the DB
#define LOCATION_ATTRIBUTE_ID________	(3)		// "" must be #3 in the DB

// a single location (eg. "Someday Cafe").  this is all that's needed for drawing lots of points (with mouse-over name).
typedef struct {
	gint m_nID;
	gchar* m_pszName;
	mappoint_t m_Coordinates;
} location_t;

typedef struct {
	gchar* m_pszName;
	gchar* m_pszValue;
	gint m_nValueID;
} locationattribute_t;

void location_init();
gboolean location_alloc(location_t** ppLocation);
void location_free(location_t* pLocation);
gboolean location_insert(gint nLocationSetID, mappoint_t* pPoint, gint* pnReturnID);
gboolean location_insert_attribute(gint nLocationID, gint nAttributeID, const gchar* pszValue, gint* pnReturnID);
gboolean location_load(gint nLocationID, mappoint_t* pReturnCoordinates, gint* pnReturnLocationSetID);
void location_load_attributes(gint nLocationID, GPtrArray* pAttributeArray);

G_END_DECLS

#endif /* _LOCATION_H */
