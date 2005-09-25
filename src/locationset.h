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

// typedef struct locationsetstyle {
//     // icon?
//     // color?
//     int __unused;
// } locationsetstyle_t;

// a set of locations (eg. "Coffee Shops")
typedef struct locationset {
	gint nID;
	gchar* pszName;
	gchar* pszIconName;
	gint nLocationCount;
	gboolean bVisible;		// user has chosen to view these
//	locationsetstyle_t Style;
} locationset_t;

void locationset_init(void);
void locationset_load_locationsets(void);
const GPtrArray* locationset_get_array(void);
gboolean locationset_find_by_id(gint nLocationSetID, locationset_t** ppLocationSet);
gboolean locationset_insert(const gchar* pszName, gint* pnReturnID);
void locationset_set_visible(locationset_t* pLocationSet, gboolean bVisible);
gboolean locationset_is_visible(locationset_t* pLocationSet);

G_END_DECLS

#endif /* _LOCATIONSET_H */
