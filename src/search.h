/***************************************************************************
 *            search.h
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

#ifndef _SEARCH_H
#define _SEARCH_H

#include "glyph.h"
#include "map.h"

typedef enum {
	SEARCH_RESULT_TYPE_COORDINATE,	// in order of importance (for search results)
	SEARCH_RESULT_TYPE_COUNTRY,
	SEARCH_RESULT_TYPE_STATE,
	SEARCH_RESULT_TYPE_CITY,
	SEARCH_RESULT_TYPE_LOCATION,
	SEARCH_RESULT_TYPE_ROAD,

	NUM_SEARCH_RESULT_TYPES
} ESearchResultType;

G_BEGIN_DECLS

struct search_result {
	ESearchResultType type;
	char *text;
	glyph_t *glyph;
	mappoint_t *point;
	int zoom_level;
};

void search_clean_string(gchar* p);
void search_all(const gchar* pszSentence);

gboolean search_address_number_atoi(const gchar* pszText, gint* pnReturn);

G_END_DECLS

#endif /* _SEARCH_H */
