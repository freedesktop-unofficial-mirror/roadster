/***************************************************************************
 *            search_road.c
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

#include "db.h"
#include "util.h"
//#include "geometryset.h"
#include "pointstring.h"
#include "point.h"
#include "searchwindow.h"
#include "search.h"
#include "search_road.h"

typedef struct {
	gint m_nNumber;			// house number	eg. 51
	gchar* m_pszRoadName;	// road name eg. "Washington"
	gint m_nCityID;			//
	gint m_nStateID;
	gint m_pszZIPCode;
	gint m_nSuffixID;		// a number representing eg. Ave
} roadsearch_t;

#define ROADSEARCH_NUMBER_NONE		(-1)
#define SEARCH_RESULT_COUNT_LIMIT	(200)		// how many rows to get from DB
#define MAX_QUERY 	(4000)

// if glib < 2.6
#if ((GLIB_MAJOR_VERSION < 2) || ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION < 6)))
gint g_strv_length(const gchar** a)
{
	gint nCount=0;
	const gchar** pp = a;
	while(*pp != NULL) {
		nCount++;
		pp++;
	}
	return nCount;
}
#endif

gchar* g_strjoinv_limit(const gchar* separator, gchar** a, gint iFirst, gint iLast)
{
	g_assert(iFirst <= iLast);
	g_assert(iLast < g_strv_length(a));

	gchar* pszSave;

	// replace first unwanted string with NULL (save old value)
	pszSave = a[iLast+1];
	a[iLast+1] = NULL;

	// use built-in function for joining
	gchar* pszReturn = g_strjoinv(separator, &a[iFirst]);

	// restore old value
	a[iLast+1] = pszSave;
	return pszReturn;
}

gboolean search_address_match_zipcode(const gchar* pszWord)
{
	// very US-centric right now
	gint nLen = strlen(pszWord);
	if(nLen < 4 || nLen > 5) return FALSE;

	gint i;
	for(i=0 ; i<nLen ; i++) {
		// XXX: is g_ascii_isalnum the right function? we should be testing for numbers only
		if(!g_ascii_isdigit(pszWord[i])) {
			return FALSE;
		}
	}
	return TRUE;
}

// prototypes

void search_road_on_cleaned_sentence(const gchar* pszCleanedSentence);
void search_road_on_words(gchar** aWords, gint nWordCount);
void search_road_on_roadsearch_struct(const roadsearch_t* pRoadSearch);
void search_road_filter_result(const gchar* pszRoadName, gint nRoadNumber, gint nRoadSuffixID, gint nAddressLeftStart, gint nAddressLeftEnd, gint nAddressRightStart, gint nAddressRightEnd, const gchar* pszCityNameLeft, const gchar* pszCityNameRight, const gchar* pszStateNameLeft, const gchar* pszStateNameRight, const gchar* pszZIPLeft, const gchar* pszZIPRight, pointstring_t* pPointString);

// functions

void search_road_execute(const gchar* pszSentence)
{
	if(pszSentence[0] == 0) {
		return;	// ignore empty searches ?
	}

	TIMER_BEGIN(search, "SEARCH BEGIN");

	// copy sentence and clean it
	gchar* pszCleanedSentence = g_strdup(pszSentence);
	search_clean_string(pszCleanedSentence);
	search_road_on_cleaned_sentence(pszCleanedSentence);
	g_free(pszCleanedSentence);

	TIMER_END(search, "SEARCH END");
}

void search_road_on_cleaned_sentence(const gchar* pszCleanedSentence)
{
	// Create an array of the words
	gchar** aWords = g_strsplit(pszCleanedSentence," ", 0);	// " " = delimeters, 0 = no max #

	gint nWordCount = g_strv_length(aWords);

	if(nWordCount > 0) {
		search_road_on_words(aWords, nWordCount);
	}

	// cleanup
	g_strfreev(aWords);	// free the array of strings	
}

void search_road_on_words(gchar** aWords, gint nWordCount)
{
	g_assert(nWordCount > 0);
	roadsearch_t roadsearch = {0};

	// index of first and last words of road name (the bit in the middle)
	gint iFirst = 0;
	gint iLast = nWordCount-1;

	// Start stripping off words as we identify roadsearch_t structure members
	gint nRemainingWordCount = nWordCount;

	// Claim house number if present
	roadsearch.m_nNumber = ROADSEARCH_NUMBER_NONE;
	if(nRemainingWordCount >= 2) {
		if(search_address_number_atoi(aWords[iFirst], &roadsearch.m_nNumber)) {
			iFirst++;	// first word is TAKEN :)
			nRemainingWordCount--;
		}
	}

	// Claim zip code, if present
	if(search_address_match_zipcode(aWords[iLast])) {
		g_print("matched ZIP %s\n", aWords[iLast]);
		roadsearch.m_pszZIPCode = aWords[iLast];

		iLast--;	// last word taken
		nRemainingWordCount--;
	}

	if(nRemainingWordCount == 0) {
		// do a zip code search and return
		g_print("TODO: zip code search\n");
		return;
	}

	// See if we can match a state name   XXX: We need to match multi-word state names
	gboolean bGotStateName = FALSE;
	if(nRemainingWordCount >= 3) {
		// try two-word state name
		gchar* pszStateName = g_strjoinv_limit(" ", aWords, iLast-1, iLast);
		g_print("trying two-word state name '%s'\n", pszStateName);

		if(db_state_get_id(pszStateName, &roadsearch.m_nStateID)) {
			g_print("matched state name!\n");
			iLast -= 2;	// last TWO words taken
			nRemainingWordCount -= 2;
		}
		g_free(pszStateName);
	}
	// try a one-word state name
	if(bGotStateName == FALSE && nRemainingWordCount >= 2) {
		g_print("trying one-word state name '%s'\n", aWords[iLast]);
		if(db_state_get_id(aWords[iLast], &roadsearch.m_nStateID)) {
			g_print("matched state name!\n");
			iLast--;	// last word taken
			nRemainingWordCount--;
		}
	}

	// try to match city name
	gint nCityNameLength;
	for(nCityNameLength = 5 ; nCityNameLength >= 1 ; nCityNameLength--) {
		if(nRemainingWordCount > nCityNameLength) {
			gchar* pszCityName = g_strjoinv_limit(" ", aWords, iLast - (nCityNameLength-1), iLast);

			if(db_city_get_id(pszCityName, roadsearch.m_nStateID, &roadsearch.m_nCityID)) {
				iLast -= nCityNameLength;	// several words taken :)
				nRemainingWordCount -= nCityNameLength;

				// success
				g_free(pszCityName);
				break;
			}
			else {
				// failure
				g_free(pszCityName);
				continue;
			}
		}
	}

	// road name suffix (eg. "ave")
	if(nRemainingWordCount >= 2) {
		gint nSuffixID;

		if(map_road_suffix_atoi(aWords[iLast], &nSuffixID)) {
			// matched
			roadsearch.m_nSuffixID = nSuffixID;
			iLast--;
			nRemainingWordCount--;
		}
	}

	if(nRemainingWordCount > 0) {
		roadsearch.m_pszRoadName = g_strjoinv_limit(" ", aWords, iFirst, iLast);
		search_road_on_roadsearch_struct(&roadsearch);
	}
	else {
		// oops, no street name
		g_print("no street name found in search\n");
	}
	g_free(roadsearch.m_pszRoadName);
}

void search_road_on_roadsearch_struct(const roadsearch_t* pRoadSearch)
{
	//
	// Assemble the various optional clauses for the SQL statement
	//
	gchar* pszAddressClause;
	if(pRoadSearch->m_nNumber != ROADSEARCH_NUMBER_NONE) {
		pszAddressClause = g_strdup_printf(
			" AND ("
			"(%d BETWEEN Road.AddressLeftStart AND Road.AddressLeftEnd)"
			" OR (%d BETWEEN Road.AddressLeftEnd AND Road.AddressLeftStart)"
			" OR (%d BETWEEN Road.AddressRightStart AND Road.AddressRightEnd)"
			" OR (%d BETWEEN Road.AddressRightEnd AND Road.AddressRightStart)"
			")", pRoadSearch->m_nNumber, pRoadSearch->m_nNumber,
				 pRoadSearch->m_nNumber, pRoadSearch->m_nNumber);
	}
	else {
		pszAddressClause = g_strdup("");
	}

	gchar* pszSuffixClause;
	if(pRoadSearch->m_nSuffixID != ROAD_SUFFIX_NONE) {
		pszSuffixClause = g_strdup_printf(
			" AND (RoadName.SuffixID = %d)",
			pRoadSearch->m_nSuffixID);
	}
	else {
		pszSuffixClause = g_strdup("");
	}

	gchar* pszZIPClause;
	if(pRoadSearch->m_pszZIPCode != NULL) {
		pszZIPClause = g_strdup_printf(" AND (Road.ZIPCodeLeft='%s' OR Road.ZIPCodeRight='%s')", pRoadSearch->m_pszZIPCode, pRoadSearch->m_pszZIPCode);
	}
	else {
		pszZIPClause = g_strdup("");
	}

	gchar* pszCityClause;
	if(pRoadSearch->m_nCityID != 0) {
		pszCityClause = g_strdup_printf(" AND (Road.CityLeftID=%d OR Road.CityRightID=%d)", pRoadSearch->m_nCityID, pRoadSearch->m_nCityID);
	}
	else {
		pszCityClause = g_strdup("");
	}

	gchar* pszStateClause;
	if(pRoadSearch->m_nStateID != 0) {
		pszStateClause = g_strdup_printf(" AND (CityLeft.StateID=%d OR CityRight.StateID=%d)", pRoadSearch->m_nStateID, pRoadSearch->m_nStateID);
	}
	else {
		pszStateClause = g_strdup("");
	}

	// if doing a road search, only show 1 hit per road?
	//~ gchar* pszGroupClause;
	//~ if(pRoadSearch->m_nSuffixID == ROAD_SUFFIX_NONE) {
		//~ pszGroupClause = g_strdup("GROUP BY (RoadName.ID, RoadName.SuffixID)");
	//~ }
	//~ else {
		//~ pszGroupClause = g_strdup("");
	//~ }

	gchar azQuery[MAX_QUERY];
	gchar* pszSafeRoadName = db_make_escaped_string(pRoadSearch->m_pszRoadName);
	g_print("pRoadSearch->m_pszRoadName = %s, pszSafeRoadName = %s\n", pRoadSearch->m_pszRoadName, pszSafeRoadName);

	g_snprintf(azQuery, MAX_QUERY,
		"SELECT Road.ID, RoadName.Name, RoadName.SuffixID, AsText(Road.Coordinates), Road.AddressLeftStart, Road.AddressLeftEnd, Road.AddressRightStart, Road.AddressRightEnd, CityLeft.Name, CityRight.Name"
		", StateLeft.Code, StateRight.Code, Road.ZIPCodeLeft, Road.ZIPCodeRight"
		" FROM RoadName"
		" LEFT JOIN Road_RoadName ON (RoadName.ID=Road_RoadName.RoadNameID)"
 	    " LEFT JOIN Road ON (Road_RoadName.RoadID=Road.ID%s)"					// address # clause
		// left side
	    " LEFT JOIN City AS CityLeft ON (Road.CityLeftID=CityLeft.ID)"
		" LEFT JOIN State AS StateLeft ON (CityLeft.StateID=StateLeft.ID)"
		// right side
		" LEFT JOIN City AS CityRight ON (Road.CityRightID=CityRight.ID)"
		" LEFT JOIN State AS StateRight ON (CityRight.StateID=StateRight.ID)"
		" WHERE RoadName.Name LIKE '%s%%'"
//		" WHERE RoadName.Name='%s'"
		" AND Road.ID IS NOT NULL"	// don't include rows where the Road didn't match
		// begin clauses
		"%s"
		"%s"
  	    "%s"
 	    "%s"
//		" ORDER BY RoadName.Name"
		" LIMIT %d;",
			   pszAddressClause,
			   pszSafeRoadName,

			   // clauses
			   pszSuffixClause,
			   pszZIPClause,
			   pszCityClause,
			   pszStateClause,
			SEARCH_RESULT_COUNT_LIMIT + 1);
	
	// free strings
	db_free_escaped_string(pszSafeRoadName);
	g_free(pszAddressClause);
	g_free(pszSuffixClause);
	g_free(pszZIPClause);
	g_free(pszCityClause);
	g_free(pszStateClause);

//	g_strlcpy(azQuery, , MAX_QUERY);
	g_print("SQL: %s\n", azQuery);

	db_resultset_t* pResultSet;
	if(db_query(azQuery, &pResultSet)) {
		db_row_t aRow;

		// get result rows!
		gint nCount = 0;		
		while((aRow = mysql_fetch_row(pResultSet))) {
			// [0] Road.ID
			// [1] RoadName.Name
			// [2] RoadName.SuffixID
			// [3] AsText(Road.Coordinates),
			// [4] Road.AddressLeftStart,
			// [5] Road.AddressLeftEnd
			// [6] Road.AddressRightStart
			// [7] Road.AddressRightEnd,
			// [8] CityLeft.Name,
			// [9] CityRight.Name

			// [10] StateLeft.Name,
			// [11] StateRight.Name

			// [12] ZIPLeft
			// [13] ZIPRight

			nCount++;
			if(nCount <= SEARCH_RESULT_COUNT_LIMIT) {
				pointstring_t* pPointString = NULL;
				pointstring_alloc(&pPointString);
				db_parse_pointstring(aRow[3], pPointString, point_alloc);

//	g_print("raw: %s\n", aRow[3]);
				search_road_filter_result(aRow[1], pRoadSearch->m_nNumber, atoi(aRow[2]), atoi(aRow[4]), atoi(aRow[5]), atoi(aRow[6]), atoi(aRow[7]), aRow[8], aRow[9], aRow[10], aRow[11], aRow[12], aRow[13], pPointString);
//	g_print("%03d: Road.ID='%s' RoadName.Name='%s', Suffix=%s, L:%s-%s, R:%s-%s\n", nCount, aRow[0], aRow[1], aRow[3], aRow[4], aRow[5], aRow[6], aRow[7]);
				pointstring_free(pPointString);
			}
		}
		db_free_result(pResultSet);

		if(nCount == 0) {
			g_print("no address search results\n");
		}
	}
	else {
		g_print("search failed\n");
	}
}


static gfloat point_calc_distance(mappoint_t* pA, mappoint_t* pB)
{
	// determine slope of the line
	gdouble fRise = pB->m_fLatitude - pA->m_fLatitude;
	gdouble fRun = pB->m_fLongitude - pA->m_fLongitude;

	return sqrt((fRun*fRun) + (fRise*fRise));
}

typedef enum {
	ROADSIDE_CENTER,
	ROADSIDE_LEFT,
	ROADSIDE_RIGHT,
} ERoadSide;

#define HIGHLIGHT_DISTANCE_FROM_ROAD (0.00012)		// this seems like a good amount...

static void pointstring_walk_percentage(pointstring_t* pPointString, gdouble fPercent, ERoadSide eRoadSide, mappoint_t* pReturnPoint)
{
	gint i;
	if(pPointString->m_pPointsArray->len < 2) {
		g_assert_not_reached();
	}

	//
	// count total distance
	//
	gfloat fTotalDistance = 0.0;
	mappoint_t* pPointA = g_ptr_array_index(pPointString->m_pPointsArray, 0);
	mappoint_t* pPointB;
	for(i=1 ; i<pPointString->m_pPointsArray->len ; i++) {
		pPointB = g_ptr_array_index(pPointString->m_pPointsArray, 1);

		fTotalDistance += point_calc_distance(pPointA, pPointB);
		
		pPointA = pPointB;
	}

	gfloat fTargetDistance = (fTotalDistance * fPercent);
	gfloat fRemainingDistance = fTargetDistance;

	pPointA = g_ptr_array_index(pPointString->m_pPointsArray, 0);
	for(i=1 ; i<pPointString->m_pPointsArray->len ; i++) {
		pPointB = g_ptr_array_index(pPointString->m_pPointsArray, 1);

		gfloat fLineSegmentDistance = point_calc_distance(pPointA, pPointB);
		if(fRemainingDistance <= fLineSegmentDistance || (i == pPointString->m_pPointsArray->len-1)) {
			// this is the line segment we are looking for.

			gfloat fPercentOfLine = (fRemainingDistance / fLineSegmentDistance);
			if(fPercentOfLine > 1.0) fPercentOfLine = 1.0;

			gfloat fRise = (pPointB->m_fLatitude - pPointA->m_fLatitude);
			gfloat fRun = (pPointB->m_fLongitude - pPointA->m_fLongitude);

			// Calculate the a point on the center of the line segment
			// the right percent along the road
			pReturnPoint->m_fLongitude = pPointA->m_fLongitude + (fRun * fPercentOfLine);
			pReturnPoint->m_fLatitude = pPointA->m_fLatitude + (fRise * fPercentOfLine);

			gdouble fPerpendicularNormalizedX = -fRise / fLineSegmentDistance;
			gdouble fPerpendicularNormalizedY = fRun / fLineSegmentDistance;

			if(eRoadSide == ROADSIDE_CENTER) {
				// do nothing, we're already at line center
			}
			else if(eRoadSide == ROADSIDE_LEFT) {
// g_print("MOVING LEFT\n");
				pReturnPoint->m_fLongitude += fPerpendicularNormalizedX * HIGHLIGHT_DISTANCE_FROM_ROAD;
				pReturnPoint->m_fLatitude += fPerpendicularNormalizedY * HIGHLIGHT_DISTANCE_FROM_ROAD;
			}
			else {
// g_print("MOVING RIGHT\n");
				pReturnPoint->m_fLongitude -= fPerpendicularNormalizedX * HIGHLIGHT_DISTANCE_FROM_ROAD;
				pReturnPoint->m_fLatitude -= fPerpendicularNormalizedY * HIGHLIGHT_DISTANCE_FROM_ROAD;
			}
			return;
		}
		fRemainingDistance -= fLineSegmentDistance;
		pPointA = pPointB;
	}
	g_assert_not_reached();
}

#if ROADSTER_DEAD_CODE
static gint min4(gint a, gint b, gint c, gint d)
{
	gint x = min(a,b);
	gint y = min(c,d);
	return min(x,y);
}

static gint max4(gint a, gint b, gint c, gint d)
{
	gint x = max(a,b);
	gint y = max(c,d);
	return max(x,y);
}
#endif /* ROADSTER_DEAD_CODE */


//
// XXX: the SQL doesn't require all fields be set for THE SAME SIDE
// 		do we need to filter out records where each side matches some of the criteria but not all?
//
#define BUFFER_SIZE 200
void search_road_filter_result(
		const gchar* pszRoadName, gint nRoadNumber, gint nRoadSuffixID,
		gint nAddressLeftStart, gint nAddressLeftEnd,
		gint nAddressRightStart, gint nAddressRightEnd,
		const gchar* pszCityNameLeft, const gchar* pszCityNameRight,
		const gchar* pszStateNameLeft, const gchar* pszStateNameRight,
		const gchar* pszZIPLeft, const gchar* pszZIPRight,
		
		pointstring_t* pPointString)
{
	gint nRoadID = 0;
	gchar azBuffer[BUFFER_SIZE];

	mappoint_t ptAddress = {0};

//     pszStateNameLeft = "(st)";
//     pszStateNameRight = "(st)";

	// set City, State, Zip text to be used for each side of the road "City, State, ZIP"
	gchar* pszCSZLeft = g_strdup_printf("%s%s%s%s%s",
										(pszCityNameLeft != NULL) ? pszCityNameLeft : "",
										(pszStateNameLeft != NULL) ? ", " : "",
										(pszStateNameLeft != NULL) ? pszStateNameLeft : "",
										(strcmp(pszZIPLeft, "00000") == 0) ? "" : ", ",
										(strcmp(pszZIPLeft, "00000") == 0) ? "" : pszZIPLeft);
	gchar* pszCSZRight = g_strdup_printf("%s%s%s%s%s",
										 (pszCityNameRight != NULL) ? pszCityNameRight : "",
										 (pszStateNameRight != NULL) ? ", " : "",
										 (pszStateNameRight != NULL) ? pszStateNameRight : "",
										 (strcmp(pszZIPRight, "00000") == 0) ? "" : ", ",
										 (strcmp(pszZIPRight, "00000") == 0) ? "" : pszZIPRight);
	
	// consider the longer of the two to be better-- (for joined results)
//	gchar* pszCSZBetter = (strlen(pszCSZLeft) > strlen(pszCSZRight)) ? pszCSZLeft : pszCSZRight;

	if(nRoadNumber == ROADSEARCH_NUMBER_NONE) {
		// Right in the center
		pointstring_walk_percentage(pPointString, 0.5, ROADSIDE_CENTER, &ptAddress);

//         gint nStart = min4(nAddressLeftStart, nAddressLeftEnd, nAddressRightStart, nAddressRigtEnd);
//         gint nEnd = min4(nAddressLeftStart, nAddressLeftEnd, nAddressRightStart, nAddressRigtEnd);

		if(nAddressRightStart == 0 && nAddressRightEnd == 0) {
			// show no numbers if they're both 0
			g_snprintf(azBuffer, BUFFER_SIZE, "%s %s\n%s",
					   pszRoadName,
					   map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG),
					   pszCSZRight);
		}
		else if(nAddressRightStart < nAddressRightEnd) {
			g_snprintf(azBuffer, BUFFER_SIZE, "%d-%d %s %s\n%s", nAddressRightStart, nAddressRightEnd, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZRight);
		}
		else {
			// reverse start/end for the dear user :)
			g_snprintf(azBuffer, BUFFER_SIZE, "%d-%d %s %s\n%s", nAddressRightEnd, nAddressRightStart, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZRight);
		}
		searchwindow_add_result(nRoadID, azBuffer, &ptAddress);

		// do left side, same as right side (see above)
		if(nAddressLeftStart == 0 && nAddressLeftEnd == 0) {
			g_snprintf(azBuffer, BUFFER_SIZE, "%s %s\n%s", pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZLeft);
		}
		else if(nAddressLeftStart < nAddressLeftEnd) {
			g_snprintf(azBuffer, BUFFER_SIZE, "%d-%d %s %s\n%s", nAddressLeftStart, nAddressLeftEnd, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZLeft);
		}
		else {
			// swap address to keep smaller number to the left
			g_snprintf(azBuffer, BUFFER_SIZE, "%d-%d %s %s\n%s", nAddressLeftEnd, nAddressLeftStart, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZLeft);
		}
		searchwindow_add_result(nRoadID, azBuffer, &ptAddress);		
	}
	else {	// else the search had a road number
		// NOTE: we have to filter out results like "97-157" when searching for "124" because it's
		// on the wrong side of the road.
//g_snprintf(azBuffer, BUFFER_SIZE, "%d %s %s", nRoadNumber, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG));

		// check left side of street
		// NOTE: if search was for an even, at least one (and hopefully both) of the range should be even
		if((is_even(nRoadNumber) && (is_even(nAddressLeftStart) || is_even(nAddressLeftEnd))) ||
		   (is_odd(nRoadNumber) && (is_odd(nAddressLeftStart) || is_odd(nAddressLeftEnd))))
		{
			// check range, and the range reversed
			if(nRoadNumber >= nAddressLeftStart && nRoadNumber <= nAddressLeftEnd) {
				// MATCH: left side forward
				gint nRange = (nAddressLeftEnd - nAddressLeftStart);
				if(nRange == 0) {
					// just use road center...?
					pointstring_walk_percentage(pPointString, 0.5, ROADSIDE_LEFT, &ptAddress);
				}
				else {
					gfloat fPercent = (gfloat)(nRoadNumber - nAddressLeftStart) / (gfloat)nRange;
					pointstring_walk_percentage(pPointString, fPercent, ROADSIDE_LEFT, &ptAddress);
				}
				g_snprintf(azBuffer, BUFFER_SIZE, "%d %s %s\n%s", nRoadNumber, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZLeft);
				searchwindow_add_result(nRoadID, azBuffer, &ptAddress);				
			}
			else if(nRoadNumber >= nAddressLeftEnd && nRoadNumber <= nAddressLeftStart) {
				// MATCH: left side backwards
				gint nRange = (nAddressLeftStart - nAddressLeftEnd);
				if(nRange == 0) {
					// just use road center...?
					pointstring_walk_percentage(pPointString, 0.5, ROADSIDE_RIGHT, &ptAddress);
				}
				else {
					gfloat fPercent = (gfloat)(nRoadNumber - nAddressLeftEnd) / (gfloat)nRange;

					// flip percent (23 becomes 77, etc.)
					pointstring_walk_percentage(pPointString, (100.0 - fPercent), ROADSIDE_RIGHT, &ptAddress);
				}
				g_snprintf(azBuffer, BUFFER_SIZE, "%d %s %s\n%s", nRoadNumber, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZLeft);
				searchwindow_add_result(nRoadID, azBuffer, &ptAddress);
			}
		}

		// check right side of street
		if((is_even(nRoadNumber) && (is_even(nAddressRightStart) || is_even(nAddressRightEnd))) ||
		   (is_odd(nRoadNumber) && (is_odd(nAddressRightStart) || is_odd(nAddressRightEnd))))
		{
			// check range, and the range reversed
			if(nRoadNumber >= nAddressRightStart && nRoadNumber <= nAddressRightEnd) {
				// MATCH: right side forward
				gint nRange = (nAddressRightEnd - nAddressRightStart);
				if(nRange == 0) {
					// just use road center...?
					pointstring_walk_percentage(pPointString, 0.5, ROADSIDE_RIGHT, &ptAddress);
				}
				else {
					gfloat fPercent = (gfloat)(nRoadNumber - nAddressRightStart) / (gfloat)nRange;
					pointstring_walk_percentage(pPointString, fPercent, ROADSIDE_RIGHT, &ptAddress);
				}
				g_snprintf(azBuffer, BUFFER_SIZE, "%d %s %s\n%s", nRoadNumber, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZRight);
				searchwindow_add_result(nRoadID, azBuffer, &ptAddress);				
			}
			else if(nRoadNumber >= nAddressRightEnd && nRoadNumber <= nAddressRightStart) {
				// MATCH: right side backwards
				gint nRange = (nAddressRightStart - nAddressRightEnd);
				if(nRange == 0) {
					// just use road center...?
					pointstring_walk_percentage(pPointString, 0.5, ROADSIDE_LEFT, &ptAddress);
				}
				else {
					gfloat fPercent = (gfloat)(nRoadNumber - nAddressRightEnd) / (gfloat)nRange;

					// flip percent (23 becomes 77, etc.)
					pointstring_walk_percentage(pPointString, (100.0 - fPercent), ROADSIDE_LEFT, &ptAddress);
				}
				g_snprintf(azBuffer, BUFFER_SIZE, "%d %s %s\n%s", nRoadNumber, pszRoadName, map_road_suffix_itoa(nRoadSuffixID, SUFFIX_LENGTH_LONG), pszCSZRight);
				searchwindow_add_result(nRoadID, azBuffer, &ptAddress);				
			}
		}
	}
	g_free(pszCSZLeft);
	g_free(pszCSZRight);
}
