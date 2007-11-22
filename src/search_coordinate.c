/***************************************************************************
 *            search_coordinate.c
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

#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>

#include "main.h"
#include "map.h"
#include "util.h"
#include "search_coordinate.h"
#include "glyph.h"
#include "searchwindow.h"		// for defines about glyph size

#define COORDINATE_RESULT_SUGGESTED_ZOOMLEVEL		(37)		// no idea what to use for this :)

#define FORMAT_COORDINATE_RESULT 	("<b>Lat:</b> %10.5f\n<b>Lon:</b> %10.5f")

static GList *search_coordinate_add_result(const mappoint_t* pPoint, GList *ret);

//
// globals
//
static glyph_t* g_SearchResultTypeCoordinateGlyph = NULL;

//
// Public API
//

GList *search_coordinate_execute(const gchar* pszSentence)
{
	//mappoint_t ptResult = {0};

	// Goals:
	// Make sure never to match something that could likely be a road or POI search!
	// That said, be _as flexible as possible_ with input formats.

	// Here are some examples from the web:
	//  32°	57' N 	 85°	 57' W
	//  37 degrees N 123 degrees W
	//  40 02 40
	//  43° 28' 07" N
	//  49:30.0-123:30.0
	//  49.5000-123.5000
	//  h&lat=32.11611&lon=-110.94109&alt=

	// And our own:
	//  Lat: 1.0, Lon: -1.0

	// And of course, don't match things out of valid range (map.h has constants: MIN_LATITUDE, MAX_LATITUDE, MIN_LONGITUDE, MAX_LONGITUDE)

	// Call only if we have a likely match!
	//search_coordinate_add_result(&ptResult);

	return NULL;
}

//
// Private
//
static GList *search_coordinate_add_result(const mappoint_t* pPoint, GList *ret)
{
	if (!g_SearchResultTypeCoordinateGlyph)
		g_SearchResultTypeCoordinateGlyph = glyph_load_at_size("search-result-type-coordinate",
		                                                       SEARCHWINDOW_SEARCH_RESULT_GLYPH_WIDTH,
		                                                       SEARCHWINDOW_SEARCH_RESULT_GLYPH_HEIGHT);

	gchar* pszBuffer = g_strdup_printf(FORMAT_COORDINATE_RESULT, pPoint->fLatitude, pPoint->fLongitude);

	mappoint_t *point = g_new0(mappoint_t, 1);
	*point = *pPoint;

	struct search_result *hit = g_new0(struct search_result, 1);
	*hit = (struct search_result) {
		.type       = SEARCH_RESULT_TYPE_COORDINATE,
		.text       = pszBuffer,
		.glyph      = g_SearchResultTypeCoordinateGlyph,
		.point      = point,
		.zoom_level = COORDINATE_RESULT_SUGGESTED_ZOOMLEVEL,
	};

	ret = g_list_append(ret, hit);

	return ret;
}
