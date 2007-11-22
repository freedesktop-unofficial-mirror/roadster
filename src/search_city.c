/***************************************************************************
 *            search_city.c
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "searchwindow.h"
#include "search.h"
#include "db.h"
#include "util.h"

#define CITY_RESULT_SUGGESTED_ZOOMLEVEL		(3)

void search_city_on_words(gchar** aWords, gint nWordCount);

static glyph_t* g_SearchResultTypeCityGlyph = NULL;


void search_city_execute(const gchar* pszSentence)
{
	// Create an array of the words
	gchar** aWords = g_strsplit(pszSentence," ", 0);	// " " = delimeters, 0 = no max #
	gint nWordCount = g_strv_length(aWords);

	if(nWordCount > 0) {
		search_city_on_words(aWords, nWordCount);
	}

	// cleanup
	g_strfreev(aWords);	// free the array of strings	
}

void search_city_on_words(gchar** aWords, gint nWordCount)
{
	g_assert(nWordCount > 0);

	gint nStateID = 0;

	if (!g_SearchResultTypeCityGlyph)
		g_SearchResultTypeCityGlyph = glyph_load_at_size("search-result-type-city",
		                                                 SEARCHWINDOW_SEARCH_RESULT_GLYPH_WIDTH,
		                                                 SEARCHWINDOW_SEARCH_RESULT_GLYPH_HEIGHT);

	// index of first and last words of city name
	gint iFirst = 0;
	gint iLast = nWordCount-1;

	// Start stripping off words as we identify roadsearch_t structure members
	gint nRemainingWordCount = nWordCount;

	// Match state
	gboolean bGotStateName = FALSE;
	if(nRemainingWordCount >= 3) {
		// try two-word state name
		gchar* pszStateName = util_g_strjoinv_limit(" ", aWords, iLast-1, iLast);
		//g_print("trying two-word state name '%s'\n", pszStateName);

		if(db_state_get_id(pszStateName, &nStateID)) {
			//g_print("matched state name!\n");
			iLast -= 2;	// last TWO words taken
			nRemainingWordCount -= 2;
		}
		g_free(pszStateName);
	}

	// try a one-word state name
	if(bGotStateName == FALSE && nRemainingWordCount >= 2) {
		//g_print("trying one-word state name '%s'\n", aWords[iLast]);
		if(db_state_get_id(aWords[iLast], &nStateID)) {
			//g_print("matched state name!\n");
			iLast--;	// last word taken
			nRemainingWordCount--;
		}
	}

	// If we got a StateID, create a bit of SQL to filter on state
	gchar* pszStateClause;
	if(nStateID != 0) {
		pszStateClause = g_strdup_printf(" AND StateID=%d", nStateID);
	}
	else {
		pszStateClause = g_strdup("");
	}

	// Use all remaining words as city name
	gchar* pszCityName = util_g_strjoinv_limit(" ", aWords, iFirst, iLast);

	// Make it safe for DB
	gchar* pszSafeCityName = db_make_escaped_string(pszCityName);

	gchar* pszQuery = g_strdup_printf(
						"SELECT City.Name, State.Name, State.CountryID"
						" FROM City"
						" LEFT JOIN State ON City.StateID = State.ID"
						" WHERE City.Name='%s'"
						" %s",
						pszSafeCityName,
						pszStateClause);

	g_free(pszCityName);
	db_free_escaped_string(pszSafeCityName);
	g_free(pszStateClause);

	//g_print("query: %s\n", pszQuery);

	db_resultset_t* pResultSet;
	gint nCount = 0;		
	if(db_query(pszQuery, &pResultSet)) {
		db_row_t aRow;

		// get result rows!
		while((aRow = db_fetch_row(pResultSet))) {
			// [0] City.Name
			// [1] State.Name
			// [2] State.CountryID
			nCount++;

			//gint nCountryID = atoi(aRow[2]);

			mappoint_t point = {0,0};

			gchar* pszResultText = g_strdup_printf("<b>%s,\n%s</b>", aRow[0], aRow[1]);	// XXX: add country?
			searchwindow_add_result(SEARCH_RESULT_TYPE_CITY, pszResultText, g_SearchResultTypeCityGlyph, &point, CITY_RESULT_SUGGESTED_ZOOMLEVEL);
			g_free(pszResultText);
		}
		db_free_result(pResultSet);
	}
	//g_print("%d city results\n", nCount);
	g_free(pszQuery);
}
