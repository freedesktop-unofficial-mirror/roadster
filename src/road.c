/***************************************************************************
 *            road.c
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
#include "road.h"
#include "util.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define	ROAD_SUFFIX_LIST_FILE_NAME ("road-suffix-list.txt")
#define ROAD_SUFFIX_LIST_MIN_WORDS (2)

struct {
	GArray* pRoadNameSuffixArray;	// an array of null-terminated vectors (an array of gchar* with the last being NULL)
} g_Road = {0};

void road_init()
{
	gchar* pszPath = g_strdup_printf(PACKAGE_SOURCE_DIR"/data/%s", ROAD_SUFFIX_LIST_FILE_NAME);
	if(util_load_array_of_string_vectors(pszPath, &(g_Road.pRoadNameSuffixArray), ROAD_SUFFIX_LIST_MIN_WORDS) == FALSE) {
		g_free(pszPath);
		pszPath = g_strdup_printf(PACKAGE_DATA_DIR"/data/%s", ROAD_SUFFIX_LIST_FILE_NAME);
		if(util_load_array_of_string_vectors(pszPath, &(g_Road.pRoadNameSuffixArray), ROAD_SUFFIX_LIST_MIN_WORDS) == FALSE) {
			g_error("Unable to load %s", ROAD_SUFFIX_LIST_FILE_NAME);
		}
	}
	g_free(pszPath);
}

// ========================================================
//	Road Direction / Suffix conversions
// ========================================================

const gchar* road_suffix_itoa(gint nSuffixID, ESuffixLength eSuffixLength)
{
	if(nSuffixID < g_Road.pRoadNameSuffixArray->len) {
		//g_debug("looking up suffixID %d", nSuffixID);
		gchar** apszWords = g_array_index(g_Road.pRoadNameSuffixArray, gchar**, nSuffixID);
		g_assert(eSuffixLength == 0 || eSuffixLength == 1);	// we know there are at least 2 words in the array (see call to util_load_name_list)
		return apszWords[eSuffixLength];
	}
	else {
		return "";
	}
}

gboolean road_suffix_atoi(const gchar* pszSuffix, gint* pReturnSuffixID)
{
	g_assert(pszSuffix != NULL);
	g_assert(pReturnSuffixID != NULL);

	gint i;
	for(i=0 ; i<g_Road.pRoadNameSuffixArray->len ; i++) {
		gchar** apszWords = g_array_index(g_Road.pRoadNameSuffixArray, gchar**, i);

		// Does this list of words contain the string?
		if(util_find_string_in_string_vector(pszSuffix, apszWords, NULL)) {
			*pReturnSuffixID = i;
			return TRUE;
		}
	}
	return FALSE;
}
