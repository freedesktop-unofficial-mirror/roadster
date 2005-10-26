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

struct {
	gchar* pszLong;
	gchar* pszShort;
} g_RoadNameSuffix[] = {
	{"",""},
	{"Road", "Rd"},
	{"Street", "St"},
	{"Drive", "Dr"},
	{"Boulevard", "Bvd"},
	{"Avenue", "Ave"},
	{"Circle", "Crl"},
	{"Square", "Sq"},
	{"Path", "Pth"},
	{"Way", "Wy"},
	{"Plaza", "Plz"},
	{"Trail", "Trl"},
	{"Lane", "Ln"},
	{"Crossing", "Xing"},
	{"Place", "Pl"},
	{"Court", "Ct"},
	{"Turnpike", "Tpke"},
	{"Terrace", "Ter"},
	{"Row", "Row"},
	{"Parkway", "Pky"},
	
	{"Bridge", "Brg"},
	{"Highway", "Hwy"},
	{"Run", "Run"},
	{"Pass", "Pass"},
	
	{"Freeway", "Fwy"},
	{"Alley", "Aly"},
	{"Crescent", "Cres"},
	{"Tunnel", "Tunl"},
	{"Walk", "Walk"},
	{"Terrace", "Trce"},
	{"Branch", "Br"},
	{"Cove", "Cv"},
	{"Bypass", "Byp"},
	{"Loop", "Loop"},
	{"Spur", "Spur"},
	{"Ramp", "Ramp"},
	{"Pike", "Pike"},
	{"Grade", "Grd"},
	{"Route", "Rte"},
	{"Arc", "Arc"},
};

struct {
	gchar* pszName;
	gint nID;
} g_RoadNameSuffixLookup[] = {
	{"Rd", ROAD_SUFFIX_ROAD},
	{"Road", ROAD_SUFFIX_ROAD},

	{"St", ROAD_SUFFIX_STREET},
	{"Street", ROAD_SUFFIX_STREET},

	{"Dr", ROAD_SUFFIX_DRIVE},
	{"Drive", ROAD_SUFFIX_DRIVE},

	{"Blv", ROAD_SUFFIX_BOULEVARD},
	{"Blvd", ROAD_SUFFIX_BOULEVARD},
	{"Boulevard", ROAD_SUFFIX_BOULEVARD},

	{"Av", ROAD_SUFFIX_AVENUE},
	{"Ave", ROAD_SUFFIX_AVENUE},
	{"Avenue", ROAD_SUFFIX_AVENUE},
	
	{"Cir", ROAD_SUFFIX_CIRCLE},
	{"Crl", ROAD_SUFFIX_CIRCLE},
	{"Circle", ROAD_SUFFIX_CIRCLE},

	{"Sq", ROAD_SUFFIX_SQUARE},
	{"Square", ROAD_SUFFIX_SQUARE},	

	{"Pl", ROAD_SUFFIX_PLACE},
	{"Place", ROAD_SUFFIX_PLACE},
	
	{"Xing", ROAD_SUFFIX_CROSSING},
	{"Crossing", ROAD_SUFFIX_CROSSING},

	{"Ct", ROAD_SUFFIX_COURT},
	{"Court", ROAD_SUFFIX_COURT},

	{"Tpke", ROAD_SUFFIX_TURNPIKE},
	{"Turnpike", ROAD_SUFFIX_TURNPIKE},

	{"Ter", ROAD_SUFFIX_TERRACE},
	{"Terrace", ROAD_SUFFIX_TERRACE},
	
	{"Row", ROAD_SUFFIX_ROW},

	{"Pth", ROAD_SUFFIX_PATH},
	{"Path", ROAD_SUFFIX_PATH},	

	{"Wy", ROAD_SUFFIX_WAY},
	{"Way", ROAD_SUFFIX_WAY},	

	{"Plz", ROAD_SUFFIX_PLAZA},
	{"Plaza", ROAD_SUFFIX_PLAZA},	

	{"Trl", ROAD_SUFFIX_TRAIL},
	{"Trail", ROAD_SUFFIX_TRAIL},	
	
	{"Ln", ROAD_SUFFIX_LANE},
	{"Lane", ROAD_SUFFIX_LANE},	
	
	{"Pky", ROAD_SUFFIX_PARKWAY},
	{"Parkway", ROAD_SUFFIX_PARKWAY},

	{"Brg", ROAD_SUFFIX_BRIDGE},
	{"Bridge", ROAD_SUFFIX_BRIDGE},

	{"Hwy", ROAD_SUFFIX_HIGHWAY},
	{"Highway", ROAD_SUFFIX_HIGHWAY},

	{"Run", ROAD_SUFFIX_RUN},

	{"Pass", ROAD_SUFFIX_PASS},

	{"Freeway", ROAD_SUFFIX_FREEWAY},
	{"Fwy", ROAD_SUFFIX_FREEWAY},
	
	{"Alley", ROAD_SUFFIX_ALLEY},
	{"Aly", ROAD_SUFFIX_ALLEY},
	
	{"Crescent", ROAD_SUFFIX_CRESCENT},
	{"Cres", ROAD_SUFFIX_CRESCENT},
	
	{"Tunnel", ROAD_SUFFIX_TUNNEL},
	{"Tunl", ROAD_SUFFIX_TUNNEL},
	
	{"Walk", ROAD_SUFFIX_WALK},

	{"Branch", ROAD_SUFFIX_BRANCE},
	{"Br", ROAD_SUFFIX_BRANCE},
	
	{"Cove", ROAD_SUFFIX_COVE},
	{"Cv", ROAD_SUFFIX_COVE},
	
	{"Bypass", ROAD_SUFFIX_BYPASS},
	{"Byp", ROAD_SUFFIX_BYPASS},
	
	{"Loop", ROAD_SUFFIX_LOOP},
	
	{"Spur", ROAD_SUFFIX_SPUR},
	
	{"Ramp", ROAD_SUFFIX_RAMP},
	
	{"Pike", ROAD_SUFFIX_PIKE},
	
	{"Grade", ROAD_SUFFIX_GRADE},
	{"Grd", ROAD_SUFFIX_GRADE},
	
	{"Route", ROAD_SUFFIX_ROUTE},
	{"Rte", ROAD_SUFFIX_ROUTE},
	
	{"Arc", ROAD_SUFFIX_ARC},
};


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
