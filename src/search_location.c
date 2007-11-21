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

#include "main.h"
#include "db.h"
#include "util.h"
#include "location.h"
#include "locationset.h"
#include "search.h"
#include "searchwindow.h"
#include "search_location.h"

#define MAX_QUERY					(4000)
#define SEARCH_RESULT_COUNT_LIMIT	(100)
#define LOCATION_RESULT_SUGGESTED_ZOOMLEVEL	(4)

//typedef struct {
//	mappoint_t ptCenter;
//	gdouble fRadiusInDegrees;
//	gint nLocationSetID;
//	gchar* pszCleanedSentence;
//	gchar** aWords;
//	gint nWordCount;
//} locationsearch_t;

void search_location_on_words(gchar** aWords, gint nWordCount);
void search_location_filter_result(gint nLocationID, gint nLocationSetID, const gchar* pszName, const gchar* pszAddress, const mappoint_t* pCoordinates);


void search_location_execute(const gchar* pszSentence)
{
	// Create an array of the words
	gchar** aaWords = g_strsplit(pszSentence, " ", 0);        // " " = delimeters, 0 = no max #
	gint nWords = g_strv_length(aaWords);
	search_location_on_words(aaWords, nWords);
	g_strfreev(aaWords);    // free entire array of strings
}

/*
We require ALL words that the user typed show up in 

Our SQL statement for finding matching POI works like this:

(1) Get a list of LocationIDs that contain each word (use DISTINCT so even if the word shows up in multiple Values, we only put the LocationID in the list once).
(2) Combine the lists together and count how many lists each LocationID is in (MatchedWords).  This will be 0 -> nWordCount
(3) Filter on MatchedWords.  If we use nWordCount, all words are required.  We could also use nWordCount-1, or always use 1 and ORDER BY MatchedWords DESC.
(4) Finally join the Location table to get needed Location details.

Our finished SQL statement will look something like this:

SELECT Location.ID, ASBINARY(Location.Coordinates) 
FROM 
(
  SELECT LocationID, COUNT(*) as MatchedWords FROM ( 
    SELECT DISTINCT LocationID FROM LocationAttributeValue WHERE MATCH(Value) AGAINST ('coffee' IN BOOLEAN MODE) 
    UNION ALL 
    SELECT DISTINCT LocationID FROM LocationAttributeValue WHERE MATCH(Value) AGAINST ('food' IN BOOLEAN MODE) 
    UNION ALL 
    SELECT DISTINCT LocationID FROM LocationAttributeValue WHERE MATCH(Value) AGAINST ('wifi' IN BOOLEAN MODE) 
  ) AS PossibleMatches
  GROUP BY LocationID			# 
  HAVING MatchedWords = 3		# use the number of words here to require a full match
) AS Matches, Location
WHERE
  Matches.LocationID = Location.ID	# join our list of Matches to Location to get needed data
*/

void search_location_on_words(gchar** aWords, gint nWordCount)
{
	if(nWordCount == 0) return;

	gchar* pszInnerSelects = g_strdup("");
	gint i;
	for(i=0 ; i<nWordCount ; i++) {
		
		gchar* pszExcludeAddressFieldClause;

		// Search the 'Address' attribute?
		if(nWordCount >= 2) {
			// Yes, allow it
			pszExcludeAddressFieldClause = g_strdup("");
		}
		else {
			pszExcludeAddressFieldClause = g_strdup_printf(" AND AttributeNameID != %d", LOCATION_ATTRIBUTE_ID_ADDRESS);
		}

		// add a join
		gchar* pszNewSelect = g_strdup_printf(
			" %s SELECT DISTINCT LocationID"	// the DISTINCT means a word showing up in 10 places for a POI will only count as 1 towards MatchedWords below
			" FROM LocationAttributeValue WHERE (MATCH(Value) AGAINST ('%s' IN BOOLEAN MODE)%s)",	// note %s at the end
				(i>0) ? "UNION ALL" : "",	// add "UNION ALL" between SELECTs
				aWords[i],
				pszExcludeAddressFieldClause);

		// out with the old, in with the new.  yes, it's slow, but not in user-time. :)
		gchar* pszTmp = g_strconcat(pszInnerSelects, pszNewSelect, NULL);
		g_free(pszInnerSelects);
		g_free(pszNewSelect);
		g_free(pszExcludeAddressFieldClause);
		pszInnerSelects = pszTmp;
	}

	gchar* pszSQL = g_strdup_printf(
		"SELECT Location.ID, Location.LocationSetID, LocationAttributeValue_Name.Value AS Name, LocationAttributeValue_Address.Value AS Address, AsBinary(Location.Coordinates)"
		" FROM ("
		  "SELECT LocationID, COUNT(*) AS MatchedWords FROM ("
		    "%s"
		  ") AS PossibleMatches" // unused alias
		 " GROUP BY LocationID"
		 " HAVING MatchedWords = %d"
		 ") AS Matches, Location"

		// Get the two values we need: Name and Address
		" LEFT JOIN LocationAttributeValue AS LocationAttributeValue_Name ON (Location.ID=LocationAttributeValue_Name.LocationID AND LocationAttributeValue_Name.AttributeNameID=%d)"
		" LEFT JOIN LocationAttributeValue AS LocationAttributeValue_Address ON (Location.ID=LocationAttributeValue_Address.LocationID AND LocationAttributeValue_Address.AttributeNameID=%d)"

		" WHERE Matches.LocationID = Location.ID",
			pszInnerSelects,
			nWordCount,
			LOCATION_ATTRIBUTE_ID_NAME,
			LOCATION_ATTRIBUTE_ID_ADDRESS
		);

	//g_print("SQL: %s\n", pszSQL);

	g_free(pszInnerSelects);

	db_resultset_t* pResultSet;
	gboolean bQueryResult = db_query(pszSQL, &pResultSet);
	g_free(pszSQL);

	if(bQueryResult) {
		if(pResultSet != NULL) {
			db_row_t aRow;

			g_assert(pResultSet);

			// get result rows!
			gint nCount = 0;		
			while((aRow = db_fetch_row(pResultSet))) {
				nCount++;
				if(nCount <= SEARCH_RESULT_COUNT_LIMIT) {
					mappoint_t pt;
					gint nLocationID = atoi(aRow[0]);
					gint nLocationSetID = atoi(aRow[1]);
					gchar* pszLocationName = aRow[2];
					gchar* pszLocationAddress = aRow[3];
					db_parse_wkb_point(aRow[4], &pt);	// Parse coordinates

					search_location_filter_result(nLocationID, nLocationSetID, pszLocationName, pszLocationAddress, &pt);
				}
			}
			db_free_result(pResultSet);
			//g_print("%d location results\n", nCount);
		}
		else {
			g_print("no location search results\n");
		}
	}
	else {
		g_print("search failed\n");
	}
}

void search_location_filter_result(gint nLocationID, gint nLocationSetID, const gchar* pszName, const gchar* pszAddress, const mappoint_t* pCoordinates)
{
	gchar* pszResultText = g_strdup_printf("<b>%s</b>%s%s", pszName,
					       (pszAddress == NULL || pszAddress[0] == '\0') ? "" : "\n",
					       (pszAddress == NULL || pszAddress[0] == '\0') ? "" : pszAddress);

	glyph_t* pGlyph = NULL;

	locationset_t* pLocationSet = NULL;
	if(locationset_find_by_id(nLocationSetID, &pLocationSet)) {
		pGlyph = pLocationSet->pGlyph;
	}
	searchwindow_add_result(SEARCH_RESULT_TYPE_LOCATION, pszResultText, pGlyph, pCoordinates, LOCATION_RESULT_SUGGESTED_ZOOMLEVEL);

	g_free(pszResultText);
}

// 

/*
void search_location_on_locationsearch_struct(locationsearch_t* pLocationSearch)
{
	// location matching
	gchar* pszCoordinatesMatch = NULL;
	if(TRUE) {
		mappoint_t ptCenter;
		map_get_centerpoint(&ptCenter);

		gdouble fDegrees = pLocationSearch->fRadiusInDegrees;
		pszCoordinatesMatch = g_strdup_printf(
			" AND MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)",
			ptCenter.fLatitude + fDegrees, ptCenter.fLongitude - fDegrees, 	// upper left
			ptCenter.fLatitude + fDegrees, ptCenter.fLongitude + fDegrees, 	// upper right
			ptCenter.fLatitude - fDegrees, ptCenter.fLongitude + fDegrees, 	// bottom right
			ptCenter.fLatitude - fDegrees, ptCenter.fLongitude - fDegrees,	// bottom left
			ptCenter.fLatitude + fDegrees, ptCenter.fLongitude - fDegrees);	// upper left again
	} else {
		pszCoordinatesMatch = g_strdup("");
	}

	// location set matching
	gchar* pszLocationSetMatch = NULL;
	if(pLocationSearch->nLocationSetID != 0) {
		pszLocationSetMatch = g_strdup_printf(" AND LocationSetID=%d", pLocationSearch->nLocationSetID);		
	}
	else {
		pszLocationSetMatch = g_strdup("");
	}

	// attribute value matching
	gchar* pszAttributeNameJoin = NULL;
	gchar* pszAttributeNameMatch = NULL;
	if(pLocationSearch->pszCleanedSentence[0] != '\0') {
		pszAttributeNameJoin = g_strdup_printf("LEFT JOIN LocationAttributeValue ON (LocationAttributeValue.LocationID=Location.ID AND MATCH(LocationAttributeValue.Value) AGAINST ('%s' IN BOOLEAN MODE))", pLocationSearch->pszCleanedSentence);		
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
	searchwindow_add_result(SEARCH_RESULT_TYPE_LOCATION, 0, p, &pt);
	g_free(p);
}
*/
