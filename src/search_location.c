/***************************************************************************
 *            search_location.c
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
#include "db.h"
#include "util.h"
#include "search.h"
#include "searchwindow.h"
#include "search_location.h"

#define MAX_QUERY					(4000)
#define SEARCH_RESULT_COUNT_LIMIT	(100)

typedef struct {
	mappoint_t m_ptCenter;
	gdouble m_fRadiusInDegrees;
	gint m_nLocationSetID;
	gchar* m_pszCleanedSentence;
	gchar** m_aWords;
	gint m_nWordCount;
} locationsearch_t;

void search_location_on_cleaned_sentence(locationsearch_t* pLocationSearch);
void search_location_on_locationsearch_struct(locationsearch_t* pLocationSearch);
void search_location_filter_result(gint nLocationID);

void search_location_execute(const gchar* pszSentence, gint nLocationSetID, gfloat fDistance, gint nDistanceUnit)
{
	return;
/*
	g_print("pszSentence = %s, nLocationSetID = %d, fDistance = %f, nDistanceUnit=%d\n", pszSentence, nLocationSetID, fDistance, nDistanceUnit);

	TIMER_BEGIN(search, "\n\n****************************\nSEARCH BEGIN");

	locationsearch_t locationsearch = { {0}, 0};
	locationsearch.m_nLocationSetID = nLocationSetID;
	locationsearch.m_fRadiusInDegrees = map_distance_in_units_to_degrees(fDistance, nDistanceUnit);

	// copy sentence and clean it
	locationsearch.m_pszCleanedSentence = g_strdup(pszSentence);
	search_clean_string(locationsearch.m_pszCleanedSentence);
	search_location_on_cleaned_sentence(&locationsearch);
	g_free(locationsearch.m_pszCleanedSentence);

	TIMER_END(search, "SEARCH END");
*/
}

/*
void search_location_on_cleaned_sentence(locationsearch_t* pLocationSearch)
{
	// Create an array of the words
	pLocationSearch->m_aWords = g_strsplit(pLocationSearch->m_pszCleanedSentence," ", 0);	// " " = delimeters, 0 = no max #
	pLocationSearch->m_nWordCount = g_strv_length(pLocationSearch->m_aWords);

	search_location_on_locationsearch_struct(pLocationSearch);

	// cleanup
	g_strfreev(pLocationSearch->m_aWords);	// free the array of strings		
}

void search_location_on_locationsearch_struct(locationsearch_t* pLocationSearch)
{
	// location matching
	gchar* pszCoordinatesMatch = NULL;
	if(TRUE) {
		mappoint_t ptCenter;
		map_get_centerpoint(&ptCenter);

		gdouble fDegrees = pLocationSearch->m_fRadiusInDegrees;
		pszCoordinatesMatch = g_strdup_printf(
			" AND MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)",
			ptCenter.m_fLatitude + fDegrees, ptCenter.m_fLongitude - fDegrees, 	// upper left
			ptCenter.m_fLatitude + fDegrees, ptCenter.m_fLongitude + fDegrees, 	// upper right
			ptCenter.m_fLatitude - fDegrees, ptCenter.m_fLongitude + fDegrees, 	// bottom right
			ptCenter.m_fLatitude - fDegrees, ptCenter.m_fLongitude - fDegrees,	// bottom left
			ptCenter.m_fLatitude + fDegrees, ptCenter.m_fLongitude - fDegrees);	// upper left again
	} else {
		pszCoordinatesMatch = g_strdup("");
	}

	// location set matching
	gchar* pszLocationSetMatch = NULL;
	if(pLocationSearch->m_nLocationSetID != 0) {
		pszLocationSetMatch = g_strdup_printf(" AND LocationSetID=%d", pLocationSearch->m_nLocationSetID);		
	}
	else {
		pszLocationSetMatch = g_strdup("");
	}

	// attribute value matching
	gchar* pszAttributeNameJoin = NULL;
	gchar* pszAttributeNameMatch = NULL;
	if(pLocationSearch->m_pszCleanedSentence[0] != '\0') {
		pszAttributeNameJoin = g_strdup_printf("LEFT JOIN LocationAttributeValue ON (LocationAttributeValue.LocationID=Location.ID AND MATCH(LocationAttributeValue.Value) AGAINST ('%s' IN BOOLEAN MODE))", pLocationSearch->m_pszCleanedSentence);		
		pszAttributeNameMatch = g_strdup_printf("AND LocationAttributeValue.ID IS NOT NULL");
	}
	else {
		pszAttributeNameJoin = g_strdup("");
		pszAttributeNameMatch = g_strdup("");
	}
	
	// build the query
	gchar azQuery[MAX_QUERY];
	g_snprintf(azQuery, MAX_QUERY,
		"SELECT Location.ID"
		" FROM Location"
		" %s"	// pszAttributeNameJoin
		" WHERE TRUE"
		" %s"	// pszCoordinatesMatch
		" %s"	// pszLocationSetMatch
		" %s"	// pszAttributeNameMatch
		//" ORDER BY RoadName.Name"
		" LIMIT %d;",
			pszAttributeNameJoin,
			pszCoordinatesMatch,
			pszLocationSetMatch,
			pszAttributeNameMatch,
			SEARCH_RESULT_COUNT_LIMIT + 1);

	// free temp strings
	g_free(pszCoordinatesMatch);
	g_free(pszLocationSetMatch);

	g_print("SQL: %s\n", azQuery);

	db_resultset_t* pResultSet;
	if(db_query(azQuery, &pResultSet)) {
		db_row_t aRow;

		// get result rows!
		gint nCount = 0;		
		while((aRow = mysql_fetch_row(pResultSet))) {
			nCount++;
			if(nCount <= SEARCH_RESULT_COUNT_LIMIT) {
				search_location_filter_result(atoi(aRow[0]));
			}
		}
		db_free_result(pResultSet);

		if(nCount == 0) {
			g_print("no location search results\n");
		}
	}
	else {
		g_print("search failed\n");
	}
}

void search_location_filter_result(gint nLocationID)
{
	g_print("result: %d\n", nLocationID);
	gchar* p = g_strdup_printf("<span size='larger'><b>Happy Garden</b></span>\n145 Main St.\nCambridge, MA 02141\n617-555-1021");
	mappoint_t pt = {0,0};
	searchwindow_add_result(0, p, &pt);
	g_free(p);
}
*/
