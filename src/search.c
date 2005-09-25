/***************************************************************************
 *            search.c
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

#include "main.h"
#include "util.h"
#include "search.h"
#include "search_road.h"
#include "search_location.h"
#include "search_city.h"
#include "glyph.h"

#define SEARCH_RESULT_TYPE_GLYPH_WIDTH		32
#define SEARCH_RESULT_TYPE_GLYPH_HEIGHT		32

struct {
	glyph_t* apSearchResultTypeGlyphs[ NUM_SEARCH_RESULT_TYPES ];	// don't store pixbufs, store some custom glyph type
} g_Search = {0};

// functions

void search_init()
{
	g_assert(NUM_SEARCH_RESULT_TYPES == 5);		// don't forget to add more here...

	g_Search.apSearchResultTypeGlyphs[SEARCH_RESULT_TYPE_ROAD] = glyph_load_at_size("search-result-type-road", SEARCH_RESULT_TYPE_GLYPH_WIDTH, SEARCH_RESULT_TYPE_GLYPH_HEIGHT);
	g_Search.apSearchResultTypeGlyphs[SEARCH_RESULT_TYPE_CITY] = glyph_load_at_size("search-result-type-city", SEARCH_RESULT_TYPE_GLYPH_WIDTH, SEARCH_RESULT_TYPE_GLYPH_HEIGHT);
	g_Search.apSearchResultTypeGlyphs[SEARCH_RESULT_TYPE_STATE] = glyph_load_at_size("search-result-type-state", SEARCH_RESULT_TYPE_GLYPH_WIDTH, SEARCH_RESULT_TYPE_GLYPH_HEIGHT);
	g_Search.apSearchResultTypeGlyphs[SEARCH_RESULT_TYPE_COUNTRY] = glyph_load_at_size("search-result-type-country", SEARCH_RESULT_TYPE_GLYPH_WIDTH, SEARCH_RESULT_TYPE_GLYPH_HEIGHT);
	g_Search.apSearchResultTypeGlyphs[SEARCH_RESULT_TYPE_LOCATION] = glyph_load_at_size("search-result-type-location", SEARCH_RESULT_TYPE_GLYPH_WIDTH, SEARCH_RESULT_TYPE_GLYPH_HEIGHT);
}

glyph_t* search_glyph_for_search_result_type(ESearchResultType eType)
{
	g_assert(eType >= 0);
	g_assert(eType < NUM_SEARCH_RESULT_TYPES);
	g_assert(g_Search.apSearchResultTypeGlyphs[eType] != NULL);

	return g_Search.apSearchResultTypeGlyphs[eType];
}

// functions common to all searches

void search_clean_string(gchar* p)
{
	g_assert(p != NULL);

	gchar* pReader = p;
	gchar* pWriter = p;

	// skip white
	while(g_ascii_isspace(*pReader)) {
		pReader++;
	}

	// remove double spaces
	while(*pReader != '\0') {
		if(g_ascii_isspace(*pReader)) {
			if(g_ascii_isspace(*(pReader+1)) || *(pReader+1) == '\0') {
				// don't copy this character (space) if the next one is a space
				// or if it's the last character
			}
			else {
				// yes, copy this space
				*pWriter = ' ';	// this also turns newlines etc. into spaces
				pWriter++;
			}
		}
		else if(g_ascii_isalnum(*pReader)) {	// copy alphanumeric only
			// yes, copy this character
			*pWriter = *pReader;
			pWriter++;
		}
		pReader++;
	}
	*pWriter = '\0';
}

gboolean search_address_number_atoi(const gchar* pszText, gint* pnReturn)
{
	g_assert(pszText != NULL);
	g_assert(pnReturn != NULL);

	const gchar* pReader = pszText;

	gint nNumber = 0;

	while(*pReader != '\0') {
		if(g_ascii_isdigit(*pReader)) {
			nNumber *= 10;
			nNumber += g_ascii_digit_value(*pReader);
		}
		else {
			return FALSE;
		}
		pReader++;
	}
	*pnReturn = nNumber;
	return TRUE;
}

void search_all(const gchar* pszSentence)
{
	if(pszSentence[0] == 0) {
		return;	// no results...
	}

	TIMER_BEGIN(search, "BEGIN SearchAll");
	
	gchar* pszCleanedSentence = g_strdup(pszSentence);
	search_clean_string(pszCleanedSentence);

	// Search each object type
	search_city_execute(pszCleanedSentence);
	search_location_execute(pszCleanedSentence);
	search_road_execute(pszCleanedSentence);
	
	TIMER_END(search, "END SearchAll");

	g_free(pszCleanedSentence);
}
