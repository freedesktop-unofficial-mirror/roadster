/***************************************************************************
 *            import_tiger.c
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

// See TGR2003.PDF page 208 for county list
#include <stdlib.h>			// for strtod

#include <string.h>

#include "main.h"
#include "util.h"
#include "import_tiger.h"
#include "importwindow.h"
#include "road.h"

#ifdef USE_GNOME_VFS
#include <gnome-vfs-2.0/libgnomevfs/gnome-vfs.h>
#endif

#define TIGER_RT1_LINE_LENGTH 				(230)
#define TIGER_RT2_LINE_LENGTH				(210)
#define TIGER_RT7_LINE_LENGTH				(76)
#define TIGER_RT8_LINE_LENGTH				(38)
#define TIGER_RTc_LINE_LENGTH				(124)
#define TIGER_RTi_LINE_LENGTH				(129)

#define LINE_LENGTH_MAX						(2000)	// mostly (only?) for the .MET file

#define TIGER_TLID_LENGTH					(10)
#define TIGER_POLYID_LENGTH					(10)
#define TIGER_LANDID_LENGTH					(10)
#define TIGER_ZEROCELL_LENGTH 				(10)

//#define BUFFER_SIZE 						(10000)

#define ROWS_PER_PULSE						(18000)		// call import_progress_pulse() after this many rows parsed
#define CALLBACKS_PER_PULSE					(30)		// call " after this many iterations over hash tables (writing to DB)

typedef struct {
	gchar* m_pszCode;
	gchar* m_pszName;
} state_t;

extern state_t g_aStates[79];

gint g_nStateID = 0;		// this is set during import process

typedef enum {
	IMPORT_RECORD_OK,
	IMPORT_RECORD_ERROR,
	IMPORT_RECORD_DONE
} EImportLineResult;

#define TIGER_CHAIN_NAME_LEN	(30)
typedef struct tiger_record_rt1
{
	gint m_nTLID;		// index- TLID links a complete chain together
	gint m_nRecordType;
	mappoint_t m_PointA;
	mappoint_t m_PointB;
	gint m_nAddressLeftStart;
	gint m_nAddressLeftEnd;
	gint m_nAddressRightStart;
	gint m_nAddressRightEnd;

	gint m_nZIPCodeLeft;
	gint m_nZIPCodeRight;

	char m_achName[TIGER_CHAIN_NAME_LEN + 1];
	gint m_nRoadNameSuffixID;

	gint m_nFIPS55Left;
	gint m_nFIPS55Right;

	gint m_nCountyIDLeft;	// if left and right are in diff counties, we've found a boundary line!
	gint m_nCountyIDRight;
} tiger_record_rt1_t;

#define TIGER_RT2_MAX_POINTS (10)
typedef struct tiger_record_rt2
{
	gint m_nTLID;		// index- TLID links a complete chain together

	GPtrArray* m_pPointsArray;
} tiger_record_rt2_t;

#define TIGER_LANDMARK_NAME_LEN (30)
typedef struct tiger_record_rt7
{
	gint m_nLANDID;		// index- a unique landmark ID #
	gint m_nRecordType;
	char m_achName[TIGER_LANDMARK_NAME_LEN + 1];	// note the +1!!
	mappoint_t m_Point;
} tiger_record_rt7_t;

typedef struct tiger_record_rt8
{
	gint m_nPOLYID;		// index (for each polygon, we'll get the
	gint m_nLANDID;		// FK to table 7
} tiger_record_rt8_t;

typedef struct tiger_rt1_link
{
	gint m_nTLID;
	gint m_nPointATZID;	// the unique # for the rt1's PointA, usefull for stiching chains together
	gint m_nPointBTZID;	// the unique # for the rt1's PointB
} tiger_rt1_link_t;

typedef struct tiger_import_process {
	gchar* m_pszFileDescription;

	GHashTable* m_pTableRT1;
	GHashTable* m_pTableRT2;
	GHashTable* m_pTableRT7;
	GHashTable* m_pTableRT8;
	GHashTable* m_pTableRTi;
	GHashTable* m_pTableRTc;

	GPtrArray* m_pBoundaryRT1s;
} tiger_import_process_t;

typedef struct tiger_record_rti
{
	// store a list of TLIDs for a polygonID
	gint m_nPOLYID;	// index
	GPtrArray* m_pRT1LinksArray;
} tiger_record_rti_t;

#define TIGER_CITY_NAME_LEN 	(60)
#define TIGER_FIPS55_LEN		(5)
typedef struct tiger_record_rtc
{
	// store a list of city names
	gint m_nFIPS55;	// index
	char m_achName[TIGER_CITY_NAME_LEN + 1];	// note the +1!!
	gint m_nCityID;					// a database ID, stored here after it is inserted
} tiger_record_rtc_t;


static gboolean import_tiger_read_lat(gint8* pBuffer, gdouble* pValue)
{
	// 0,1,2,3
	// - 1 2 . 1 2 3 4 5 6
	char buffer[11];
	memcpy(&buffer[0], &pBuffer[0], 3);	// copy first 3 bytes
	buffer[3] = '.';					// and 1
	memcpy(&buffer[4], &pBuffer[3], 6);	// copy next 6
	buffer[10] = '\0';

	char* p = buffer;
	gdouble fVal = strtod(buffer, &p);
	if(p == buffer) {
		g_warning("strtod('%s') resulted in an error\n", buffer);
		return FALSE;
	}
	*pValue = fVal;
	return TRUE;
}

static gboolean import_tiger_read_lon(char* pBuffer, gdouble* pValue)
{
	char buffer[12];
	memcpy(&buffer[0], &pBuffer[0], 4);	// copy first 4 bytes (yes, this is different than lat, TIGER!!)
	buffer[4] = '.';
	memcpy(&buffer[5], &pBuffer[4], 6);
	buffer[11] = '\0';

	char* p = buffer;
	gdouble fVal = strtod(buffer, &p);
	if(p == buffer) {
		g_warning("strtod('%s') resulted in an error\n", buffer);
		return FALSE;
	}
	if(fVal > 180.0 || fVal < -180.0) {
		g_warning("bad longitude fVal (%f) from string (%s)\n", fVal, buffer);
	}
	*pValue = fVal;
	return TRUE;
}


static gboolean import_tiger_read_int(gint8* pBuffer, gint nLen, gint32* pValue)
{
	gint8 buffer[11];
	g_assert(nLen <= 10);
	memcpy(buffer, pBuffer, nLen);
	buffer[nLen] = '\0';
	gint32 nVal = atoi(buffer);
	*pValue = nVal;
	return TRUE;
}

static gboolean import_tiger_read_address(gint8* pBuffer, gint nLen, gint32* pValue)
{
	gint8 buffer[15];
	g_assert(nLen <= 14);
	memcpy(buffer, pBuffer, nLen);
	buffer[nLen] = '\0';
	gint32 nVal = atoi(buffer);
	*pValue = nVal;
	return TRUE;
}

static gboolean import_tiger_read_string(char* pBuffer, gint nLen, char* pValue)
{
	g_assert(pBuffer != NULL);
	g_assert(nLen > 0);
	g_assert(pValue != NULL);
	
	// NOTE: pValue must point to space enough for nLen+1 ... potential bug here!! :O

	gint i;
	gint nLength=0;
	for(i=0 ; i<nLen ; i++) {
		if(pBuffer[i] != ' ' && pBuffer[i] != '\0') {
			nLength = (i+1);
		}
	}
	memcpy(pValue, pBuffer, nLength);
	pValue[nLength] = '\0';
//	snprintf(pValue, nLength+1, "%s", pBuffer);
	return TRUE;

	memcpy(pValue, pBuffer, nLen);
	pValue[nLen] = '\0';

	// trim whitespace
	nLen--;
	gint8* p = &pValue[nLen];	// last non-null char
	while(nLen >= 0 && *p == ' ') {
		*p = '\0';
		p--;
		nLen--;
	}
	return TRUE;
}

static gboolean import_tiger_read_layer_type(gint8* pBuffer, gint* pValue)
{
	//g_print("%c%c%c\n", *(pBuffer), *(pBuffer+1), *(pBuffer+2));
	gchar chFeatureClass 	= *(pBuffer+0);
	gint8 chCode 			= *(pBuffer+1);
	gint8 chSubCode 		= *(pBuffer+2);

	// See TGR2003.PDF pages 81-96 for full list of feature classes/codes
	if(chFeatureClass == 'A') {
		if(chCode == '1') {	// primary road with "Limited access" (few connecting roads)
			/* A19, A29, A39 are bridges */
			*pValue = MAP_OBJECT_TYPE_MINORHIGHWAY;
			return TRUE;
		}
		else if(chCode == '2') {	// "Primary Road without Limited Access"
			*pValue = MAP_OBJECT_TYPE_MAJORROAD;
			return TRUE;
		}
		else if(chCode == '3') {
			*pValue = MAP_OBJECT_TYPE_MAJORROAD;
			return TRUE;
		}
		else if(chCode == '4') {	// "Local, Neighborhood, Rural" Roads
			*pValue = MAP_OBJECT_TYPE_MINORROAD;
			return TRUE;
		}
		else if(chCode == '5') {	// dirt roads
			//*pValue = MAP_OBJECT_TYPE_TRAIL;
			return FALSE;
		}
		else if(chCode == '6') {
			if(chSubCode == '1') {
				g_print("found code A61: cul-de-sac!\n");
			}
			else if(chSubCode == '3') {
				*pValue = MAP_OBJECT_TYPE_MINORHIGHWAY_RAMP;			
				return TRUE;
			}
			else if(chSubCode == '5') {
				//*pValue = MAP_OBJECT_TYPE_FERRY_ROUTE;	// where a boat carrying cars goes
				return FALSE;
			}
			else if(chSubCode == '7') {
				g_print("found code A67: toll booth!\n");
			}
		}
		else if(chCode == '7') {	// "other"
			//~ if(chSubCode == '1') {
				//~ *pValue = MAP_OBJECT_TYPE_PEDESTRIAN_PATH;	// bike/walking path			
				//~ return TRUE;
			//~ }
			//~ else if(chSubCode == '2') {
				//~ *pValue = MAP_OBJECT_TYPE_PEDESTRIAN_PATH;	// stairway for pedestrians...
				//~ return TRUE;
			//~ }
			//~ else if(chSubCode == '4') {
				//~ *pValue = MAP_OBJECT_TYPE_DRIVEWAY;			// private access roads
				//~ return TRUE;
			//~ }
		}
	}
	else if(chFeatureClass == 'B') {		// Railroads
		*pValue = MAP_OBJECT_TYPE_RAILROAD;
		return TRUE;
	}
	else if(chFeatureClass == 'D') {		// Landmarks
		//~ if(chCode == '5' && chSubCode == '1') {
			//~ *pValue = MAP_OBJECT_TYPE_AIRPORT;
			//~ return TRUE;
		//~ }
		//~ else
		/*
		 D8* open spaces
		 D81 golf courses
		 D82 Cemetery
		 D83 National Park
		 D84 National forest
		 D85 State or local park or forest
		*/
		if(chCode == '8') {
			*pValue = MAP_OBJECT_TYPE_PARK;
			return TRUE;
		}
		else {
			*pValue = MAP_OBJECT_TYPE_MISC_AREA;
			return TRUE;
		}

		// TODO: Add 'misc areas' to get all this stuff?
		/*
		D21 Apartment building
		D24 Marina
		D27 Hotel, motel, resort, spa, hostel, YMCA, YWCA
		D28 Campground
		D31 Hospital
		D36 Jail
		D37 Fed. Penitentiary / state prison
		
		D4* educational
		D43 schools/university
		D44 religious church/synagogue
		
		D5* transportation
		D51 airport
		D52 train station
		D53 bus terminal
		D54 marine terminal

		*/
	}
	else if(chFeatureClass == 'E') {		// topographic
		/*
		E2*
		E23 island
		E25 marsh
		*/
//		g_print("found topographic (E%c%c)\n", chCode, chSubCode);
	}
	else if(chFeatureClass == 'H') { 	// water

		if(chCode == '0') {
			// generic unknown water ...
			// this includes charles river (cambridge/boston ma)
			// but they are badly formed for some reason?
//			*pValue = MAP_OBJECT_TYPE_LAKE;
//			return TRUE;
//			return FALSE;
			if(chSubCode == '1') {
				// these need to be stitched by lat/lon
				//*pValue = MAP_OBJECT_TYPE_LAKE;	// shoreline of perennial water feature
				//return TRUE;
			}
		}
		else if(chCode == '1') { 	// streams
			*pValue = MAP_OBJECT_TYPE_RIVER;
			return TRUE;
		}
		else if(chCode == '3') { 	// lakes
			*pValue = MAP_OBJECT_TYPE_LAKE;
			return TRUE;
		}
		else if(chCode == '4') { 	// reservoirs
			*pValue = MAP_OBJECT_TYPE_LAKE;
			return TRUE;
		}
		else if(chCode == '5') { 	// ocean
			if(chSubCode == '1') {
				*pValue = MAP_OBJECT_TYPE_LAKE;	// bay, estuary, gulf or sound
				return TRUE;				
			}
			else {
//				*pValue = MAP_OBJECT_TYPE_LAKE;
//				return TRUE;
				//~ *pValue = MAP_OBJECT_TYPE_OCEAN;
				//~ return TRUE;
			}
		}
		else if(chCode == '6') { 	// water-filled gravel pit
			g_print("found code H6: water filled gravel pit!\n");
			//~ *pValue = MAP_OBJECT_TYPE_LAKE;
			//~ return TRUE;
		}
	}
	//~ *pValue = MAP_OBJECT_TYPE_TRAIL;
	//~ return TRUE;
	*pValue = MAP_OBJECT_TYPE_NONE;
	return FALSE;
}

static gchar* import_tiger_copy_line(const gchar* pszString)
{
	char azBuffer[LINE_LENGTH_MAX];
	const gchar* pszFrom = pszString;
	gchar* pszTo = azBuffer;

	gint i=0;	// -1 = room for null char
	while(i < (LINE_LENGTH_MAX-1) && *pszFrom != '\0' && *pszFrom != '\n') {
		*pszTo++ = *pszFrom++;
	}
	*pszTo = '\0';
	return g_strdup(azBuffer);
}

// The MET file *is* zero terminated
static gboolean import_tiger_parse_MET(const gchar* pszMET, tiger_import_process_t* pImportProcess)
{
	gboolean bSuccess = FALSE;
	const gchar* p = pszMET;
	while(TRUE) {
		while(*p == ' ') p++;
		if(*p == '\0') break;

		if(g_str_has_prefix(p, "Title: ")) {
			// found title line
			p += 7;	// move past "Title: "
			pImportProcess->m_pszFileDescription = import_tiger_copy_line(p);
			bSuccess = TRUE;
		}
		// move to end of line
		while(*p != '\0' && *p != '\n') p++;
		if(*p == '\n') p++;
	}
	return bSuccess;
}


// See TGR2003.PDF page 186 for field description
static gboolean import_tiger_parse_table_1(gchar* pBuffer, gint nLength, GHashTable* pTable, GPtrArray* pBoundaryRT1s)
{
	gint i;
	for(i=0 ; i<=(nLength-TIGER_RT1_LINE_LENGTH) ; i+=TIGER_RT1_LINE_LENGTH) {
		if((i%ROWS_PER_PULSE) == 0) importwindow_progress_pulse();

		gchar* pLine = &pBuffer[i];

		gint nRecordType;
		import_tiger_read_layer_type(&pLine[56-1], &nRecordType);

		tiger_record_rt1_t* pRecord = g_new0(tiger_record_rt1_t, 1);

		pRecord->m_nRecordType = nRecordType;

		// addresses (these are really not numeric, they are alpha... how to handle this?)
		import_tiger_read_address(&pLine[59-1], 11, &pRecord->m_nAddressLeftStart);
		import_tiger_read_address(&pLine[70-1], 11, &pRecord->m_nAddressLeftEnd);
		import_tiger_read_address(&pLine[81-1], 11, &pRecord->m_nAddressRightStart);
		import_tiger_read_address(&pLine[92-1], 11, &pRecord->m_nAddressRightEnd);

		// columns 107-111 and 112-116 are zip codes
		import_tiger_read_int(&pLine[107-1], 5, &pRecord->m_nZIPCodeLeft);
		import_tiger_read_int(&pLine[112-1], 5, &pRecord->m_nZIPCodeRight);

		// columns 6 to 15 is the TLID -
		import_tiger_read_int(&pLine[6-1], TIGER_TLID_LENGTH, &pRecord->m_nTLID);
		
		// columns 20 to ? is the name
		import_tiger_read_string(&pLine[20-1], TIGER_CHAIN_NAME_LEN, &pRecord->m_achName[0]);

		// columns 141-145 and 146-150 are FIPS55 codes which link this road to a city
		import_tiger_read_int(&pLine[141-1], TIGER_FIPS55_LEN, &pRecord->m_nFIPS55Left);
		import_tiger_read_int(&pLine[146-1], TIGER_FIPS55_LEN, &pRecord->m_nFIPS55Right);

		// Read suffix name and convert it to an integer
		gchar achType[5];
		import_tiger_read_string(&pLine[50-1], 4, &achType[0]);
//		g_print("%30s is type %s\n", pRecord->m_achName, achType);	
		road_suffix_atoi(achType, &pRecord->m_nRoadNameSuffixID);

if(achType[0] != '\0' && pRecord->m_nRoadNameSuffixID == ROAD_SUFFIX_NONE) {
	g_print("type '%s' couldn't be looked up\n", achType);
}
		import_tiger_read_int(&pLine[135-1], 3, &pRecord->m_nCountyIDLeft);
		import_tiger_read_int(&pLine[138-1], 3, &pRecord->m_nCountyIDRight);

		if(pRecord->m_nCountyIDLeft != pRecord->m_nCountyIDRight) {
			g_ptr_array_add(pBoundaryRT1s, pRecord);
	//		g_print("county boundary\n");
		}
		//~ gint nFeatureType;
		//~ import_tiger_read_int(&pLine[50-1], 4, &nFeatureType);
		//~ g_print("name: '%s' (%d)\n", pRecord->m_achName, nFeatureType);

//g_print("for name %s\n", pRecord->m_achName);

 		// lat/lon coordinates...
		import_tiger_read_lon(&pLine[191-1], &pRecord->m_PointA.m_fLongitude);
		import_tiger_read_lat(&pLine[201-1], &pRecord->m_PointA.m_fLatitude);
		import_tiger_read_lon(&pLine[210-1], &pRecord->m_PointB.m_fLongitude);
		import_tiger_read_lat(&pLine[220-1], &pRecord->m_PointB.m_fLatitude);

//g_print("name: %s, (%f,%f) (%f,%f)\n", pRecord->m_achName, pRecord->m_PointA.m_fLongitude, pRecord->m_PointA.m_fLatitude, pRecord->m_PointB.m_fLongitude, pRecord->m_PointB.m_fLatitude);

		// add to table
		g_hash_table_insert(pTable, &pRecord->m_nTLID, pRecord);
	}
	return TRUE;
}

static gboolean import_tiger_parse_table_2(gint8* pBuffer, gint nLength, GHashTable *pTable)
{
	gint i;
	for(i=0 ; i<=(nLength-TIGER_RT2_LINE_LENGTH) ; i+=TIGER_RT2_LINE_LENGTH) {
		if((i%ROWS_PER_PULSE) == 0) importwindow_progress_pulse();

		gchar* pLine = &pBuffer[i];

		// columns 6 to 15 is the TLID -
		gint nTLID;
		import_tiger_read_int(&pLine[6-1], TIGER_TLID_LENGTH, &nTLID);
		tiger_record_rt2_t* pRecord = g_hash_table_lookup(pTable, &nTLID);
		if(pRecord == NULL) {
			// create new one and add it
			pRecord = g_new0(tiger_record_rt2_t, 1);
			pRecord->m_nTLID = nTLID;
			pRecord->m_pPointsArray = g_ptr_array_new();

			// add to table
			g_hash_table_insert(pTable, &pRecord->m_nTLID, pRecord);
		}
		else {
			// g_print("****** updating existing record %d\n", pRecord->m_nTLID);
		}

		mappoint_t point;
		gint iPoint;
		for(iPoint=0 ; iPoint< TIGER_RT2_MAX_POINTS ; iPoint++) {
			import_tiger_read_lon(&pLine[19-1 + (iPoint * 19)], &point.m_fLongitude);
			import_tiger_read_lat(&pLine[29-1 + (iPoint * 19)], &point.m_fLatitude);
			if(point.m_fLatitude == 0.0 && point.m_fLongitude == 0.0) {
				break;
			}
			mappoint_t* pNewPoint = g_new0(mappoint_t, 1);
			pNewPoint->m_fLatitude = point.m_fLatitude;
			pNewPoint->m_fLongitude = point.m_fLongitude;

			g_ptr_array_add(pRecord->m_pPointsArray, pNewPoint);
		}
	}
	return TRUE;
}

static gboolean import_tiger_parse_table_7(gint8* pBuffer, gint nLength, GHashTable *pTable)
{
	gint i;
	for(i=0 ; i<=(nLength-TIGER_RT7_LINE_LENGTH) ; i+=TIGER_RT7_LINE_LENGTH) {
		if((i%ROWS_PER_PULSE) == 0) importwindow_progress_pulse();

		gchar* pLine = &pBuffer[i];

		tiger_record_rt7_t* pRecord;

		// 22-24 is a CFCC (
		gint nRecordType;

		import_tiger_read_layer_type(&pLine[22-1], &nRecordType);
		pRecord = g_new0(tiger_record_rt7_t, 1);
		pRecord->m_nRecordType = nRecordType;

		// columns 11 to 20 is the TLID -
		import_tiger_read_int(&pLine[11-1], TIGER_LANDID_LENGTH, &pRecord->m_nLANDID);

		import_tiger_read_string(&pLine[25-1], TIGER_LANDMARK_NAME_LEN, &pRecord->m_achName[0]);

		//if(nRecordType == MAP_OBJECT_TYPE_MISC_AREA) {
			// g_print("misc area: %s\n", pRecord->m_achName);
		//}
// g_print("record 7: TypeID=%d LANDID=%d\n", pRecord->m_nRecordType, pRecord->m_nLANDID);
//g_print("name: '%s'\n", pRecord->m_achName);

		// add to table
		g_hash_table_insert(pTable, &pRecord->m_nLANDID, pRecord);
	}
	return TRUE;
}

static gboolean import_tiger_parse_table_8(gint8* pBuffer, gint nLength, GHashTable *pTable)
{
	gint i;
	for(i=0 ; i<=(nLength-TIGER_RT8_LINE_LENGTH) ; i+=TIGER_RT8_LINE_LENGTH) {
		if((i%ROWS_PER_PULSE) == 0) importwindow_progress_pulse();

		gchar* pLine = &pBuffer[i];

		tiger_record_rt8_t* pRecord;
		pRecord = g_new0(tiger_record_rt8_t, 1);

		// columns 16 to 25 is the POLYGON ID -
		import_tiger_read_int(&pLine[16-1], TIGER_POLYID_LENGTH, &pRecord->m_nPOLYID);

		// columns 26 to 35 is the LANDMARK ID -
		import_tiger_read_int(&pLine[26-1], TIGER_LANDID_LENGTH, &pRecord->m_nLANDID);

// g_print("record 8: POLYID=%d LANDID=%d\n", pRecord->m_nPOLYID, pRecord->m_nLANDID);

		// add to table
		g_hash_table_insert(pTable, &pRecord->m_nPOLYID, pRecord);
	}
	return TRUE;
}

static gboolean import_tiger_parse_table_c(gint8* pBuffer, gint nLength, GHashTable *pTable)
{
	gint i;
	for(i=0 ; i<=(nLength-TIGER_RTc_LINE_LENGTH) ; i+=TIGER_RTc_LINE_LENGTH) {
		if((i%ROWS_PER_PULSE) == 0) importwindow_progress_pulse();

		gchar* pLine = &pBuffer[i];

		// We only want Entity Type M (??)
		char chEntityType = pLine[25-1];
		if(chEntityType != 'M') continue;

		tiger_record_rtc_t* pRecord;
		pRecord = g_new0(tiger_record_rtc_t, 1);

		// columns 15 to 19 is the FIPS number (links roads to cities)
		import_tiger_read_int(&pLine[15-1], TIGER_FIPS55_LEN, &pRecord->m_nFIPS55);
		import_tiger_read_string(&pLine[63-1], TIGER_CITY_NAME_LEN, &pRecord->m_achName[0]);
		
g_print("record c: FIPS55=%d NAME=%s\n", pRecord->m_nFIPS55, pRecord->m_achName);

		// add to table
		g_hash_table_insert(pTable, &pRecord->m_nFIPS55, pRecord);
	}
	return TRUE;
}


static gboolean import_tiger_parse_table_i(gint8* pBuffer, gint nLength, GHashTable *pTable)
{
	//
	// Gather RTi records (chainID,TZID-A,TZID-B) and index them by POLYGON ID in the given hash table
	//
	gint i;
	for(i=0 ; i<=(nLength-TIGER_RTi_LINE_LENGTH) ; i+=TIGER_RTi_LINE_LENGTH) {
		gchar* pLine = &pBuffer[i];

		gint nTLID;
		gint nLeftPolygonID;
		gint nRightPolygonID;
		tiger_record_rti_t* pRecord;

		// 11-20 is the TLID
		import_tiger_read_int(&pLine[11-1], TIGER_TLID_LENGTH, &nTLID);
		
		// 46-55 is left polygon id
		import_tiger_read_int(&pLine[46-1], TIGER_POLYID_LENGTH, &nLeftPolygonID);
		// 61-70 is right polygon id
		import_tiger_read_int(&pLine[61-1], TIGER_POLYID_LENGTH, &nRightPolygonID);

		// 21-30 is zero-cell for start point
		gint nZeroCellA;
		import_tiger_read_int(&pLine[21-1], TIGER_ZEROCELL_LENGTH, &nZeroCellA);
		// 31-40 is zero-cell for end point
		gint nZeroCellB;
		import_tiger_read_int(&pLine[31-1], TIGER_ZEROCELL_LENGTH, &nZeroCellB);

//         if(nZeroCellA == nZeroCellB) {
//             // we can't link this with anything..?
//             //g_print("nZeroCellA == nZeroCellB\n");
//             continue;
//         }

		if(nLeftPolygonID != 0) {
			// is there an existing RTi for this POLYID?
			pRecord = g_hash_table_lookup(pTable, &nLeftPolygonID);	// RTi is indexed by polygon ID
			if(pRecord == NULL) {
				// create new RTi record and add the link to it
				pRecord = g_new0(tiger_record_rti_t, 1);
				pRecord->m_nPOLYID = nLeftPolygonID;
				pRecord->m_pRT1LinksArray = g_ptr_array_new();

				// add new RTi record to the RTi hash table (indexed by POLYID)
				g_hash_table_insert(pTable, &pRecord->m_nPOLYID, pRecord);

				//g_print("(L)added TLID %d to new polygon %d ", nTLID, pRecord->m_nPOLYID);
			}
			else {
				//g_print("(L)added TLID %d to existing polygon %d ", nTLID, pRecord->m_nPOLYID);
			}
			tiger_rt1_link_t* pNewRT1Link = g_new0(tiger_rt1_link_t, 1);
			pNewRT1Link->m_nTLID = nTLID;
			pNewRT1Link->m_nPointATZID = nZeroCellA;
			pNewRT1Link->m_nPointBTZID = nZeroCellB;
			g_ptr_array_add(pRecord->m_pRT1LinksArray, pNewRT1Link);
		}
		if(nRightPolygonID != 0) {
			pRecord = g_hash_table_lookup(pTable, &nRightPolygonID);
			if(pRecord == NULL) {
				// create new one and add it
				pRecord = g_new0(tiger_record_rti_t, 1);
				pRecord->m_nPOLYID = nRightPolygonID;
				pRecord->m_pRT1LinksArray = g_ptr_array_new();
				
				// add new RTi record to the RTi hash table (indexed by POLYID)
				g_hash_table_insert(pTable, &pRecord->m_nPOLYID, pRecord);
				//g_print("(R)adding TLID %d to new polygon %d ", nTLID, pRecord->m_nPOLYID);
			}
			else {
				//g_print("(R)adding TLID %d to existing polygon %d ", nTLID, pRecord->m_nPOLYID);
			}
			tiger_rt1_link_t* pNewRT1Link = g_new0(tiger_rt1_link_t, 1);
			pNewRT1Link->m_nTLID = nTLID;
			pNewRT1Link->m_nPointATZID = nZeroCellA;
			pNewRT1Link->m_nPointBTZID = nZeroCellB;
			g_ptr_array_add(pRecord->m_pRT1LinksArray, pNewRT1Link);
		}
	}
	return TRUE;
}

//
// Callbacks
//
static void callback_save_rt1_chains(gpointer key, gpointer value, gpointer user_data)
{
	static int nCallCount=0; nCallCount++;
	if((nCallCount%CALLBACKS_PER_PULSE) == 0) importwindow_progress_pulse();
	
	GPtrArray* pTempPointsArray = g_ptr_array_new();
	tiger_import_process_t* pImportProcess = (tiger_import_process_t*)user_data;
	tiger_record_rt1_t* pRecordRT1 = (tiger_record_rt1_t*)value;
	// lookup table2 record by TLID
	tiger_record_rt2_t* pRecordRT2 = g_hash_table_lookup(pImportProcess->m_pTableRT2, &pRecordRT1->m_nTLID);

	// add RT1's point A, (optionally) add all points from RT2, then add RT1's point B
	g_ptr_array_add(pTempPointsArray, &pRecordRT1->m_PointA);
	if(pRecordRT2 != NULL) {
		// append all points from pRecordRT2->m_pPointsArray to pTempPointsArray
		gint i;
		for(i=0 ; i<pRecordRT2->m_pPointsArray->len ; i++) {
			g_ptr_array_add(pTempPointsArray, g_ptr_array_index(pRecordRT2->m_pPointsArray, i));		
		}
	}
	g_ptr_array_add(pTempPointsArray, &pRecordRT1->m_PointB);

	//
	// Change rivers into lakes if they are circular (why doesn't this work here?)
	//
//     if(pRecordRT1->m_nRecordType == MAP_OBJECT_TYPE_RIVER) {
//         if(((gint)(pRecordRT1->m_PointA.m_fLongitude * 1000.0)) == ((gint)(pRecordRT1->m_PointB.m_fLongitude * 1000.0)) &&
//            ((gint)(pRecordRT1->m_PointA.m_fLatitude * 1000.0)) == ((gint)(pRecordRT1->m_PointB.m_fLatitude * 1000.0)))
//         {
//             if(pRecordRT1->m_PointA.m_fLongitude != pRecordRT1->m_PointB.m_fLongitude) {
//                 g_print("OOPS: %20.20f != %20.20f\n", pRecordRT1->m_PointA.m_fLongitude, pRecordRT1->m_PointB.m_fLongitude);
//             }
//             if(pRecordRT1->m_PointA.m_fLatitude != pRecordRT1->m_PointB.m_fLatitude) {
//                 g_print("OOPS: %20.20f != %20.20f\n", pRecordRT1->m_PointA.m_fLatitude, pRecordRT1->m_PointB.m_fLatitude);
//             }
//             g_print("converting circular river to lake: %s\n", pRecordRT1->m_achName);
//             pRecordRT1->m_nRecordType = MAP_OBJECT_TYPE_LAKE;
//         }
//         else {
// //          g_print("NOT converting river: %s (%f != %f)(%f != %f)\n", pRecordRT1->m_achName, pRecordRT1->m_PointA.m_fLongitude, pRecordRT1->m_PointB.m_fLongitude, pRecordRT1->m_PointA.m_fLatitude, pRecordRT1->m_PointB.m_fLatitude);
//         }
//     }

	// use RT1's FIPS code to lookup related RTc record, which contains a CityID
	gint nCityLeftID=0;
	gint nCityRightID=0;

	tiger_record_rtc_t* pRecordRTc;

	// lookup left CityID, if the FIPS is valid
	if(pRecordRT1->m_nFIPS55Left != 0) {
		pRecordRTc = g_hash_table_lookup(pImportProcess->m_pTableRTc, &pRecordRT1->m_nFIPS55Left);
		if(pRecordRTc) {
			nCityLeftID = pRecordRTc->m_nCityID;
		}
		else {
			g_warning("couldn't lookup CityID by FIPS %d for road %s\n", pRecordRT1->m_nFIPS55Left, pRecordRT1->m_achName);
		}
	}

	// lookup right CityID, if the FIPS is valid
	if(pRecordRT1->m_nFIPS55Right != 0) {
		pRecordRTc = g_hash_table_lookup(pImportProcess->m_pTableRTc, &pRecordRT1->m_nFIPS55Right);
		if(pRecordRTc) {
			nCityRightID = pRecordRTc->m_nCityID;
		}
		else {
			g_warning("couldn't lookup city ID by FIPS %d for road %s\n", pRecordRT1->m_nFIPS55Right, pRecordRT1->m_achName);
		}
	}

	// insert, then free temp array
	if(pRecordRT1->m_nRecordType != MAP_OBJECT_TYPE_NONE) {
		gchar azZIPCodeLeft[6];
		g_snprintf(azZIPCodeLeft, 6, "%05d", pRecordRT1->m_nZIPCodeLeft);
		gchar azZIPCodeRight[6];
		g_snprintf(azZIPCodeRight, 6, "%05d", pRecordRT1->m_nZIPCodeRight);

		gint nRoadNameID = 0;
		if(pRecordRT1->m_achName[0] != '\0') {
			//printf("inserting road name %s\n", pRecordRT1->m_achName);
			db_insert_roadname(pRecordRT1->m_achName, pRecordRT1->m_nRoadNameSuffixID, &nRoadNameID);
		}

		gint nRoadID;
		db_insert_road(nRoadNameID,
			pRecordRT1->m_nRecordType,
			pRecordRT1->m_nAddressLeftStart,
			pRecordRT1->m_nAddressLeftEnd,
			pRecordRT1->m_nAddressRightStart,
			pRecordRT1->m_nAddressRightEnd,
			nCityLeftID, nCityRightID,
			azZIPCodeLeft, azZIPCodeRight,
			pTempPointsArray, &nRoadID);
	}
	g_ptr_array_free(pTempPointsArray, TRUE);
}

typedef enum {
	ORDER_FORWARD,
	ORDER_BACKWARD
} EOrder;

static void tiger_util_add_RT1_points_to_array(tiger_import_process_t* pImportProcess, gint nTLID, GPtrArray* pPointsArray, EOrder eOrder)
{
	g_assert(pImportProcess != NULL);
	g_assert(pImportProcess->m_pTableRT1 != NULL);
	g_assert(pImportProcess->m_pTableRT2 != NULL);

	// lookup table1 record by TLID
	tiger_record_rt1_t* pRecordRT1 = g_hash_table_lookup(pImportProcess->m_pTableRT1, &nTLID);
	if(pRecordRT1 == NULL) return;

	// lookup table2 record by TLID
	tiger_record_rt2_t* pRecordRT2 = g_hash_table_lookup(pImportProcess->m_pTableRT2, &pRecordRT1->m_nTLID);

	if(eOrder == ORDER_FORWARD) {
		g_ptr_array_add(pPointsArray, &pRecordRT1->m_PointA);
		if(pRecordRT2 != NULL) {
			// append all points from pRecordRT2->m_pPointsArray
			gint i;
			for(i=0 ; i<pRecordRT2->m_pPointsArray->len ; i++) {
				g_ptr_array_add(pPointsArray, g_ptr_array_index(pRecordRT2->m_pPointsArray, i));
			}
		}
		g_ptr_array_add(pPointsArray, &pRecordRT1->m_PointB);
	} else {
		g_ptr_array_add(pPointsArray, &pRecordRT1->m_PointB);
		if(pRecordRT2 != NULL) {
			// append all points from pRecordRT2->m_pPointsArray in REVERSE order
			gint i;
			for(i=pRecordRT2->m_pPointsArray->len-1 ; i>=0 ; i--) {
				g_ptr_array_add(pPointsArray, g_ptr_array_index(pRecordRT2->m_pPointsArray, i));		
			}
		}
		g_ptr_array_add(pPointsArray, &pRecordRT1->m_PointA);
	}
}

static void callback_save_rtc_cities(gpointer key, gpointer value, gpointer user_data)
{
	tiger_record_rtc_t* pRecordRTc = (tiger_record_rtc_t*)value;
	g_assert(pRecordRTc != NULL);

	gint nCityID = 0;
	if(!db_insert_city(pRecordRTc->m_achName, g_nStateID, &nCityID)) {
		g_warning("insert city %s failed\n", pRecordRTc->m_achName);
	}
	pRecordRTc->m_nCityID = nCityID;
}

static void callback_save_rti_polygons(gpointer key, gpointer value, gpointer user_data)
{
	static int nCallCount=0; nCallCount++;
	if((nCallCount%CALLBACKS_PER_PULSE) == 0) importwindow_progress_pulse();

	tiger_import_process_t* pImportProcess = (tiger_import_process_t*)user_data;
	g_assert(pImportProcess != NULL);

	//
	// pRecordRTi has an array of RT1 chains that make up this polygon.
	// our job here is to stitch them together.
	//
	tiger_record_rti_t* pRecordRTi = (tiger_record_rti_t*)value;
	g_assert(pRecordRTi != NULL);

	// lookup table8 (polygon-landmark link) record by POLYID
	tiger_record_rt8_t* pRecordRT8 = g_hash_table_lookup(pImportProcess->m_pTableRT8, &pRecordRTi->m_nPOLYID);
	if(pRecordRT8 == NULL) return;	// allowed to be null(?)

	// lookup table7 (landmark) record by LANDID
	tiger_record_rt7_t* pRecordRT7 = g_hash_table_lookup(pImportProcess->m_pTableRT7, &pRecordRT8->m_nLANDID);
	if(pRecordRT7 == NULL) return;	// allowed to be null(?)

	// now we have landmark data (name, type)

	g_assert(pRecordRTi->m_pRT1LinksArray != NULL);
	g_assert(pRecordRTi->m_pRT1LinksArray->len >= 1);

	GPtrArray* pTempPointsArray = NULL;
	// create a temp array to hold the points for this polygon (in order)
	g_assert(pTempPointsArray == NULL);
	pTempPointsArray = g_ptr_array_new();

	// start with the RT1Link at index 0 (and remove it)
	tiger_rt1_link_t* pCurrentRT1Link = g_ptr_array_index(pRecordRTi->m_pRT1LinksArray, 0);
	g_ptr_array_remove_index(pRecordRTi->m_pRT1LinksArray, 0);	// TODO: should maybe choose the last one instead? :)  easier to remove and arbitrary anyway!

	// we'll use the first RT1 in forward order, that is A->B...
	tiger_util_add_RT1_points_to_array(pImportProcess, pCurrentRT1Link->m_nTLID,
		pTempPointsArray, ORDER_FORWARD);
	// ...so B is the last TZID for now.
	gint nLastTZID = pCurrentRT1Link->m_nPointBTZID;

	while(TRUE) {
		if(pRecordRTi->m_pRT1LinksArray->len == 0) break;

		// Loop through the RT1Links and try to find the next RT1 by matching TZID fields.
		// NOTE: This is just like dominos!  Only instead of matching white dots we're
		// matching TZIDs.  And like a domino, an RT1 chain can be in the wrong order!
		// In that case we have to add the points from the RT1 in *reverse* order.

		gboolean bFound = FALSE;
		gint iRT1Link;
		for(iRT1Link=0 ; iRT1Link < pRecordRTi->m_pRT1LinksArray->len ; iRT1Link++) {
			tiger_rt1_link_t* pNextRT1Link = g_ptr_array_index(pRecordRTi->m_pRT1LinksArray, iRT1Link);
			
			if(nLastTZID == pNextRT1Link->m_nPointATZID) {
				// add pNextRT1Link's points in order (A->B)
				// this (pNextRT1Link) RT1Link becomes the new "current"
				// remove it from the array!
				g_ptr_array_remove_index(pRecordRTi->m_pRT1LinksArray, iRT1Link);
				iRT1Link--;	// undo the next ++ in the for loop

				// we're done forever with the old 'current'
				// (the RT1 it links to has already had its points copied to the list!)
				g_free(pCurrentRT1Link);
				pCurrentRT1Link = pNextRT1Link;

				// add this new RT1's points
				tiger_util_add_RT1_points_to_array(pImportProcess, pCurrentRT1Link->m_nTLID,
					pTempPointsArray, ORDER_FORWARD);

				nLastTZID = pCurrentRT1Link->m_nPointBTZID;	// Note: point *B* of this RT1Link
				bFound = TRUE;
				break;
			}
			else if(nLastTZID == pNextRT1Link->m_nPointBTZID) {
				// add pNextRT1Link's points in REVERSE order (B->A)
				// (otherwise same as above)

				g_ptr_array_remove_index(pRecordRTi->m_pRT1LinksArray, iRT1Link);
				iRT1Link--;	// undo the next ++ in the for loop

				g_free(pCurrentRT1Link);
				pCurrentRT1Link = pNextRT1Link;

				// add this new RT1's points
				tiger_util_add_RT1_points_to_array(pImportProcess, pCurrentRT1Link->m_nTLID,
					pTempPointsArray, ORDER_BACKWARD);

				nLastTZID = pCurrentRT1Link->m_nPointATZID;	// Note: point *A* of this RT1Link
				bFound = TRUE;
				break;
			}
		}
		if(bFound == FALSE) {
			// no next domino-match
			break;	// inner while() loop
		}
		// else loop and attempt to find next RT1 whose points we should append
	}
	g_assert(pCurrentRT1Link != NULL);
	g_free(pCurrentRT1Link);

	// save this polygon
	if(pTempPointsArray->len > 3) {	// takes 3 to make a polygon
		mappoint_t* p1 = g_ptr_array_index(pTempPointsArray, 0);
		mappoint_t* p2 = g_ptr_array_index(pTempPointsArray, pTempPointsArray->len-1);

		if(p1->m_fLatitude != p2->m_fLatitude || p1->m_fLongitude != p2->m_fLongitude) {
			g_print("Found a polygon that doesn't loop %s\n", pRecordRT7->m_achName);
		}

		// XXX: looking up a city for a polygon?  unimplemented.
		gint nCityLeftID = 0;
		gchar* pszZIPCodeLeft = "";
		gint nCityRightID = 0;
		gchar* pszZIPCodeRight = "";

		// insert record
		if(pRecordRT7->m_nRecordType != MAP_OBJECT_TYPE_NONE) {
			gint nRoadNameID = 0;
			if(pRecordRT7->m_achName[0] != '\0') {
				//g_printf("inserting area name %s\n", pRecordRT7->m_achName);
				db_insert_roadname(pRecordRT7->m_achName, 0, &nRoadNameID);
			}

			gint nRoadID;
			db_insert_road(
				nRoadNameID,
				pRecordRT7->m_nRecordType,
				0,0,0,0,
				nCityLeftID, nCityRightID,
				pszZIPCodeLeft, pszZIPCodeRight,
				pTempPointsArray, &nRoadID);

		}
	}
	g_ptr_array_free(pTempPointsArray, TRUE);

	// we SHOULD have used all RT1 links up!
	if(pRecordRTi->m_pRT1LinksArray->len > 0) {
		//g_warning("RT1 Links remain:\n");
		while(pRecordRTi->m_pRT1LinksArray->len > 0) {
			tiger_rt1_link_t* pTemp = g_ptr_array_remove_index(pRecordRTi->m_pRT1LinksArray, 0);
			//g_print("  (A-TZID:%d B-TZID:%d)\n", pTemp->m_nPointATZID, pTemp->m_nPointBTZID);
			g_free( pTemp );
		}
	}
}

//
//
//
static gboolean import_tiger_from_directory(const gchar* pszDirectoryPath, gint nTigerSetNumber);
static gboolean import_tiger_from_buffers(gint8* pBufferMET, gint nLengthMET, gint8* pBufferRT1, gint nLengthRT1, gint8* pBufferRT2, gint nLengthRT2,	gint8* pBufferRT7, gint nLengthRT7,	gint8* pBufferRT8, gint nLengthRT8, gint8* pBufferRTc, gint nLengthRTc, gint8* pBufferRTi, gint nLengthRTi);

gboolean import_tiger_from_uri(const gchar* pszURI, gint nTigerSetNumber)
{
#ifdef USE_GNOME_VFS
	//g_print("pszURI = %s\n", pszURI);

	importwindow_progress_pulse();

	// convert from "file:///path/to/file" to "/path/to/file" (only works on local files)
	gchar* pszLocalFilePath = gnome_vfs_get_local_path_from_uri(pszURI);
	if(pszLocalFilePath == NULL) {
		g_warning("import_tiger_from_uri: gnome_vfs_get_local_path_from_uri failed (not local?)\n");
		return FALSE;
	}
	
	//
	// Make a temporary directory for ZIP file contents
	//
	gchar* pszTempDir = g_strdup_printf("%s/roadster", g_get_tmp_dir());
	gnome_vfs_make_directory(pszTempDir, 0700);

	//
	// Create unzip command line
	//
	gchar* pszCommandLine = g_strdup_printf("unzip -qq -o -j %s -d %s -x *RT4 *RT5 *RT6 *RTA *RTE *RTH *RTP *RTR *RTT *RTS *RTZ", pszLocalFilePath, pszTempDir);
	// NOTE: to use other TIGER file types, remove them from the 'exclude' list (-x)
	// -qq = be very quiet (no output)
	// -o = overwrite files without prompting
	// -j = don't create directories (shouldn't be any)
	// -d = next argument is directory to extract to
	// -x = Exclude list follows

	g_free(pszLocalFilePath); pszLocalFilePath = NULL;

	// run unzip
	gint nExitStatus=0;
	gboolean bUnzipOK = g_spawn_command_line_sync(pszCommandLine,
                                             NULL, //gchar **standard_output,
                                             NULL, //gchar **standard_error,
                                             &nExitStatus, //gint *exit_status,
                                             NULL); //GError **error);

	g_free(pszCommandLine);	pszCommandLine = NULL;

	if(!bUnzipOK || nExitStatus != 0) {
		g_free(pszTempDir);

		g_warning("error involking unzip command (exit status = %d)\n", nExitStatus);
		return FALSE;
	}

	gboolean bSuccess = import_tiger_from_directory(pszTempDir, nTigerSetNumber);

	g_print("success = %d\n", bSuccess?1:0);
	
	g_free(pszTempDir);
	return bSuccess;
#endif
}

static gboolean import_tiger_from_directory(const gchar* pszDirectoryPath, gint nTigerSetNumber)
{
#ifdef USE_GNOME_VFS
	g_print("import_tiger_from_directory\n");

	gchar* pszFilePath;

	gchar* apszExtensions[7] = {"MET", "RT1", "RT2", "RT7", "RT8", "RTC", "RTI"};	
	gint8* apBuffers[G_N_ELEMENTS(apszExtensions)] = {0};
	gint nSizes[G_N_ELEMENTS(apszExtensions)] = {0};

	// open, read, and delete (unlink) each file
	gint i;
	gboolean bSuccess = TRUE;
	for(i=0 ; i<G_N_ELEMENTS(apszExtensions) ; i++) {
		pszFilePath = g_strdup_printf("file://%s/TGR%05d.%s", pszDirectoryPath, nTigerSetNumber, apszExtensions[i]);
		if(GNOME_VFS_OK != gnome_vfs_read_entire_file(pszFilePath, &nSizes[i], (char**)&apBuffers[i])) {
			bSuccess = FALSE;
		}
		gnome_vfs_unlink(pszFilePath);
		g_free(pszFilePath);
	}

	// did we read all files?
	if(bSuccess) {
		gint nStateID = (nTigerSetNumber / 1000);	// int division (eg. turn 25017 into 25)
		if(nStateID < G_N_ELEMENTS(g_aStates)) {
			gint nCountryID = 1;	// USA is #1 *gag*
			db_insert_state(g_aStates[nStateID].m_pszName, g_aStates[nStateID].m_pszCode, nCountryID, &g_nStateID);
		}

		g_assert(G_N_ELEMENTS(apszExtensions) == 7);
		bSuccess = import_tiger_from_buffers(apBuffers[0], nSizes[0], apBuffers[1], nSizes[1], apBuffers[2], nSizes[2], apBuffers[3], nSizes[3], apBuffers[4], nSizes[4], apBuffers[5], nSizes[5], apBuffers[6], nSizes[6]);
	}
	for(i=0 ; i<G_N_ELEMENTS(apszExtensions) ; i++) {
		g_free(apBuffers[i]); // can be null
	}
	return bSuccess;
#endif
}

static gboolean import_tiger_from_buffers(
	gint8* pBufferMET, gint nLengthMET,
	gint8* pBufferRT1, gint nLengthRT1,
	gint8* pBufferRT2, gint nLengthRT2,
	gint8* pBufferRT7, gint nLengthRT7,
	gint8* pBufferRT8, gint nLengthRT8,
	gint8* pBufferRTc, gint nLengthRTc,
	gint8* pBufferRTi, gint nLengthRTi)
{
	//	g_hash_table_lookup
//	g_assert(func_progress_callback != NULL);

	tiger_import_process_t importProcess = {0};

	// Parse each file
	importwindow_log_append(".");
	importwindow_progress_pulse();	

	g_print("parsing MET\n");
	gchar* pszZeroTerminatedBufferMET = g_malloc(nLengthMET + 1);
	memcpy(pszZeroTerminatedBufferMET, pBufferMET, nLengthMET);
	pszZeroTerminatedBufferMET[nLengthMET] = '\0';
		import_tiger_parse_MET(pszZeroTerminatedBufferMET, &importProcess);
	g_free(pszZeroTerminatedBufferMET);
	g_print("MET Title: %s\n", importProcess.m_pszFileDescription );

	importwindow_log_append(".");
	importwindow_progress_pulse();

	// a list of RT1 records that make up the boundary of this county
	importProcess.m_pBoundaryRT1s = g_ptr_array_new();

	g_print("parsing RT1\n");
	importProcess.m_pTableRT1 = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	import_tiger_parse_table_1(pBufferRT1, nLengthRT1, importProcess.m_pTableRT1, importProcess.m_pBoundaryRT1s);
	g_print("RT1: %d records\n", g_hash_table_size(importProcess.m_pTableRT1));

	importwindow_log_append(".");
	importwindow_progress_pulse();

	g_print("parsing RT2\n");
	importProcess.m_pTableRT2 = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	import_tiger_parse_table_2(pBufferRT2, nLengthRT2, importProcess.m_pTableRT2);
	g_print("RT2: %d records\n", g_hash_table_size(importProcess.m_pTableRT2));

	importwindow_log_append(".");
	importwindow_progress_pulse();

	g_print("parsing RT7\n");
	importProcess.m_pTableRT7 = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	import_tiger_parse_table_7(pBufferRT7, nLengthRT7, importProcess.m_pTableRT7);
	g_print("RT7: %d records\n", g_hash_table_size(importProcess.m_pTableRT7));

	importwindow_log_append(".");
	importwindow_progress_pulse();

	g_print("parsing RT8\n");
	importProcess.m_pTableRT8 = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	import_tiger_parse_table_8(pBufferRT8, nLengthRT8, importProcess.m_pTableRT8);
	g_print("RT8: %d records\n", g_hash_table_size(importProcess.m_pTableRT8));

	importwindow_log_append(".");
	importwindow_progress_pulse();

	g_print("parsing RTc\n");
	importProcess.m_pTableRTc = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	import_tiger_parse_table_c(pBufferRTc, nLengthRTc, importProcess.m_pTableRTc);
	g_print("RTc: %d records\n", g_hash_table_size(importProcess.m_pTableRTc));

	g_print("parsing RTi\n");
	importProcess.m_pTableRTi = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);
	import_tiger_parse_table_i(pBufferRTi, nLengthRTi, importProcess.m_pTableRTi);
	g_print("RTi: %d records\n", g_hash_table_size(importProcess.m_pTableRTi));

	importwindow_log_append(".");
	importwindow_progress_pulse();

	//
	// Insert cities first
	//
	g_print("iterating over RTc cities...\n");
	g_hash_table_foreach(importProcess.m_pTableRTc, callback_save_rtc_cities, &importProcess);
	g_print("done.\n");
	
	importwindow_log_append(".");
	importwindow_progress_pulse();

	//
	// Stitch and insert polygons
	//
	g_print("iterating over RTi polygons...\n");
	g_hash_table_foreach(importProcess.m_pTableRTi, callback_save_rti_polygons, &importProcess);
	g_print("done.\n");

	importwindow_log_append(".");
	importwindow_progress_pulse();

	//
	// Roads
	//
	g_print("iterating over RT1 chains...\n");
	g_hash_table_foreach(importProcess.m_pTableRT1, callback_save_rt1_chains, &importProcess);
	g_print("done.\n");

	importwindow_log_append(".");
	importwindow_progress_pulse();
g_print("cleaning up\n");
	//
	// free up the importprocess structure
	//
	g_hash_table_destroy(importProcess.m_pTableRT1);
	g_hash_table_destroy(importProcess.m_pTableRT2);
	g_hash_table_destroy(importProcess.m_pTableRT7);
	g_hash_table_destroy(importProcess.m_pTableRT8);
	g_hash_table_destroy(importProcess.m_pTableRTc);
	// XXX: this call sometimes segfaults:
	g_warning("leaking some memory due to unsolved bug in import.  just restart roadster after/between imports ;)\n");
	//g_hash_table_destroy(importProcess.m_pTableRTi);
	g_free(importProcess.m_pszFileDescription);

	return TRUE;
}

#ifdef ROADSTER_DEAD_CODE
static void debug_print_string(char* str, gint len)
{
	char* p = str;
	while(len > 0) {
		g_print("(%c %02d)", *p, *p);
		p++;
		len--;
		if(*p == '\0') break;
	}
	g_print("\n");
}
#endif

state_t g_aStates[79] = {
			{"", 	""},	// NOTE: these are in the proper order to line up with the TIGER data's FIPS code.  don't mess with them. :)
/* 1 */		{"AL", 	"Alabama"},
/* 2 */		{"AK", 	"Alaska"},
/* 3 */		{"", 	""},	// unused
/* 4 */		{"AZ", 	"Arizona"},
/* 5 */		{"AR", 	"Arkansas"},
/* 6 */		{"CA", 	"California"},
/* 7 */		{"", 	""},
/* 8 */		{"CO", 	"Colorado"},
/* 9 */		{"CT", 	"Connecticut"},
/* 10 */	{"DE", 	"Delaware"},
/* 11 */	{"DC", 	"District of Columbia"},
/* 12 */	{"FL", 	"Florida"},
/* 13 */	{"GA", 	"Georgia"},
/* 14 */	{"", 	""},
/* 15 */	{"HI", 	"Hawaii"},
/* 16 */	{"ID", 	"Idaho"},
/* 17 */	{"IL", 	"Illinois"},
/* 18 */	{"IN", 	"Indiana"},
/* 19 */	{"IA", 	"Iowa"},
/* 20 */	{"KS", 	"Kansas"},
/* 21 */	{"KY", 	"Kentucky"},
/* 22 */	{"LA", 	"Louisiana"},
/* 23 */	{"ME", 	"Maine"},
/* 24 */	{"MD", 	"Maryland"},
/* 25 */	{"MA", 	"Massachusetts"},
/* 26 */	{"MI", 	"Michigan"},
/* 27 */	{"MN", 	"Minnesota"},
/* 28 */	{"MS", 	"Mississippi"},
/* 29 */	{"MO", 	"Missouri"},
/* 30 */	{"MT", 	"Montana"},
/* 31 */	{"NE", 	"Nebraska"},
/* 32 */	{"NV", 	"Nevade"},
/* 33 */	{"NH", 	"New Hampshire"},
/* 34 */	{"NJ", 	"New Jersey"},
/* 35 */	{"NM", 	"New Mexico"},
/* 36 */	{"NY", 	"New York"},
/* 37 */	{"NC", 	"North Carolina"},
/* 38 */	{"ND", 	"North Dakota"},
/* 39 */	{"OH", 	"Ohio"},
/* 40 */	{"OK", 	"Oklahoma"},
/* 41 */	{"OR", 	"Oregon"},
/* 42 */	{"PA", 	"Pennsylvania"},
/* 43 */	{"", 	""},
/* 44 */	{"RI", 	"Rhode Island"},
/* 45 */	{"SC", 	"South Carolina"},
/* 46 */	{"SD", 	"South Dakota"},
/* 47 */	{"TN", 	"Tennessee"},
/* 48 */	{"TX", 	"Texas"},
/* 49 */	{"UT", 	"Utah"},
/* 50 */	{"VT", 	"Vermont"},
/* 51 */	{"VA", 	"Virginia"},
/* 52 */	{"", 	""},
/* 53 */	{"WA", 	"Washington"},
/* 54 */	{"WV", 	"West Virginia"},
/* 55 */	{"WI", 	"Wisconsin"},
/* 56 */	{"WY", 	"Wyoming"},
/* 57-59 */	{"", 	""},{"", 	""},{"", 	""},
/* 60 */	{"AS", 	"American Samoa"},
/* 61-65 */	{"", 	""},{"", 	""},{"", 	""},{"", 	""},{"", 	""},
/* 66 */	{"GU", 	"Guam"},
/* 67-68 */	{"", 	""}, {"", 	""},
/* 69 */	{"MP", 	"Northern Mariana Islands"},
/* 70-71 */	{"", 	""},{"", 	""},
/* 72 */	{"PR", 	"Puerto Rico"},
/* 73-77 */	{"", 	""},{"", 	""},{"", 	""},{"", 	""},{"", 	""},
/* 78 */	{"VI", 	"Virgin Islands"},
};
