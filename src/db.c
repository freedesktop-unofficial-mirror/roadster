/***************************************************************************
 *            db.c
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

#include <mysql.h>

#if HAVE_MYSQL_EMBED
# include <mysql_embed.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include "db.h"
#include "mainwindow.h"
#include "geometryset.h"
#include "util.h"
#include "layers.h"
#include "locationset.h"

/*
Notes on the database:
	- MySQL uses an R-TREE to find the data we want, this is fast

	For a query that matches 690 of 85840 rows:
	- The query takes about 0.03 seconds
	- Clearing out previous query's data takes about 0.001 seconds
	- Retrieving and storing rows takes about 0.07 seconds
	- Rendering takes 0.5 seconds
	 Total: 0.6 seconds

	getting just ID but not storing it for 85840 rows: 1.089513
	getting geography data but not storing it for 85840 rows: 4.006307
	getting and storing: 4.200560
*/
#define MYSQL_RESULT_SUCCESS  	(0)		// for clearer code

#define MAX_SQLBUFFER_LEN		(132000)	// must be big for lists of coordinates
#define COORD_LIST_MAX 			(128000)

// #define EMBEDDED_DATABASE_NAME	("embedded")

// mysql_use_result - 	less client memory, ties up the server (other clients can't do updates)
//						better for embedded or local servers
// mysql_store_result - more client memory, gets all results right away and frees up server
//						better for remote servers
#define MYSQL_GET_RESULT(x)		mysql_use_result((x))

db_connection_t* g_pDB = NULL;

gboolean db_query(const gchar* pszSQL, db_resultset_t** ppResultSet)
{
	g_assert(pszSQL != NULL);
	if(g_pDB == NULL) return FALSE;
	
//g_print("doing? A - %s\n", pszSQL);
	if(mysql_query(g_pDB->m_pMySQLConnection, pszSQL) != MYSQL_RESULT_SUCCESS) {
//g_print("doing? Aaa\n");
		g_warning("db_query: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), pszSQL);
		return FALSE;
	}
//g_print("doing? B\n");

	// get result?
	if(ppResultSet != NULL) {
		*ppResultSet = (db_resultset_t*)MYSQL_GET_RESULT(g_pDB->m_pMySQLConnection);	
	}
	return TRUE;
}

static gboolean db_insert(const gchar* pszSQL, gint* pnReturnRowsInserted)
{
	g_assert(pszSQL != NULL);
	if(g_pDB == NULL) return FALSE;
	
	if(mysql_query(g_pDB->m_pMySQLConnection, pszSQL) != MYSQL_RESULT_SUCCESS) {
		g_warning("db_query: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), pszSQL);
		return FALSE;
	}

	my_ulonglong uCount = mysql_affected_rows(g_pDB->m_pMySQLConnection);
	if(uCount > 0) {
		if(pnReturnRowsInserted != NULL) {
			*pnReturnRowsInserted = uCount;
		}
		return TRUE;
	}
	return FALSE;
}

db_row_t db_fetch_row(db_resultset_t* pResultSet)
{
	return (db_row_t)mysql_fetch_row((MYSQL_RES*)pResultSet);
}

void db_free_result(db_resultset_t* pResultSet)
{
	mysql_free_result((MYSQL_RES*)pResultSet);
}

gint db_get_last_insert_id()
{
	if(g_pDB == NULL) return 0;
	return mysql_insert_id(g_pDB->m_pMySQLConnection);
}

/******************************************************
** database utility functions
******************************************************/

static gboolean db_is_connected(void)
{
	// 'mysql_ping' will also attempt a re-connect if necessary
	return (g_pDB != NULL && mysql_ping(g_pDB->m_pMySQLConnection) == MYSQL_RESULT_SUCCESS);
}

// gets a descriptive string about the connection.  (do not free it.)
const gchar* db_get_connection_info()
{
	if(g_pDB == NULL || g_pDB->m_pMySQLConnection == NULL) {
		return "Not connected";
	}

	return mysql_get_host_info(g_pDB->m_pMySQLConnection);
}

/* String Escaping */
gchar* db_make_escaped_string(const gchar* pszString)
{
	if(!db_is_connected()) return NULL;

//	g_assert_not_reached();	// doesn't do escaping..?
	gint nLength = (strlen(pszString)*2) + 1;
//	g_print("length: %s (%d)\n", pszString, nLength);
	
	gchar* pszNew = g_malloc(nLength);
	mysql_real_escape_string(g_pDB->m_pMySQLConnection, pszNew, pszString, strlen(pszString));

//	g_print("huh: %s\n", pszNew);
	return pszNew;
}

void db_free_escaped_string(gchar* pszString)
{
	g_free(pszString);
}

static guint db_count_table_rows(const gchar* pszTable)
{
	if(!db_is_connected()) return 0;

	MYSQL_RES* pResultSet;
	MYSQL_ROW aRow;
	gchar azQuery[MAX_SQLBUFFER_LEN];
	guint uRows = 0;
	
	// count rows
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "SELECT COUNT(*) FROM %s;", pszTable);
	if(mysql_query(g_pDB->m_pMySQLConnection, azQuery) != MYSQL_RESULT_SUCCESS) {
		g_message("db_count_table_rows query failed: %s\n", mysql_error(g_pDB->m_pMySQLConnection));
		return 0;
	}
	if((pResultSet = MYSQL_GET_RESULT(g_pDB->m_pMySQLConnection)) != NULL) {
		if((aRow = mysql_fetch_row(pResultSet)) != NULL) {
			uRows = atoi(aRow[0]);
		}
		mysql_free_result(pResultSet);
	}
	return uRows;
}

gboolean db_is_empty()
{
	return 	(db_count_table_rows(DB_ROADS_TABLENAME) == 0);
}

/******************************************************
** Init and deinit of database module
******************************************************/

// call once on program start-up
void db_init()
{
#if HAVE_MYSQL_EMBED
	// Initialize the embedded server
	// NOTE: if not linked with libmysqld, this call will do nothing (but will succeed)
 	if(mysql_server_init(0, NULL, NULL) != 0) {
		g_print("mysql_server_init failed\n");
		return;
	}
#endif
}

// call once on program shut-down
void db_deinit()
{
#if HAVE_MYSQL_EMBED
	// Close embedded server if present
	mysql_server_end();
#endif
}

/******************************************************
** Connection creation and destruction
******************************************************/

// initiate a new connection to server
gboolean db_connect(const gchar* pzHost, const gchar* pzUserName, const gchar* pzPassword, const gchar* pzDatabase)
{
	// create a MySQL connection context
	MYSQL *pMySQLConnection = mysql_init(NULL);
	g_return_val_if_fail(pMySQLConnection != NULL, FALSE);

	// attempt a MySQL connection
	if(mysql_real_connect(pMySQLConnection, pzHost, pzUserName, pzPassword, pzDatabase, 0, NULL, 0) == FALSE) {
		g_warning("mysql_real_connect failed: %s\n", mysql_error(pMySQLConnection));
		return FALSE;
	}

	// on success, alloc our connection struct and fill it
	db_connection_t* pNewConnection = g_new0(db_connection_t, 1);
	pNewConnection->m_pMySQLConnection = pMySQLConnection;
	pNewConnection->m_pzHost = g_strdup(pzHost);
	pNewConnection->m_pzUserName = g_strdup(pzUserName);
	pNewConnection->m_pzPassword = g_strdup(pzPassword);
	pNewConnection->m_pzDatabase = g_strdup(pzDatabase);

	g_assert(g_pDB == NULL);
	g_pDB = pNewConnection;

	// just in case (this could mess with multi-user databases)
	return TRUE;
}

#if 0
static void db_disconnect(void)
{
	g_return_if_fail(g_pDB != NULL);

	// close database and free all strings
	mysql_close(g_pDB->m_pMySQLConnection);
	g_free(g_pDB->m_pzHost);
	g_free(g_pDB->m_pzUserName);
	g_free(g_pDB->m_pzPassword);
	g_free(g_pDB->m_pzDatabase);

	// free structure itself
	g_free(g_pDB);
	g_pDB = NULL;
}
#endif

/******************************************************
** data inserting
******************************************************/
gboolean db_insert_road(gint nLayerType, gint nAddressLeftStart, gint nAddressLeftEnd, gint nAddressRightStart, gint nAddressRightEnd, GPtrArray* pPointsArray, gint* pReturnID)
{
	g_assert(pReturnID != NULL);
	if(!db_is_connected()) return FALSE;
	if(pPointsArray->len == 0) return TRUE; 	// skip 0-length

	gchar azCoordinateList[COORD_LIST_MAX] = {0};
	gint nCount = 0;
	gint i;
	for(i=0 ; i < pPointsArray->len ;i++) {
		mappoint_t* pPoint = g_ptr_array_index(pPointsArray, i);

		gchar azNewest[40];
		if(nCount > 0) g_snprintf(azNewest, 40, ",%f %f", pPoint->m_fLatitude, pPoint->m_fLongitude);
		else g_snprintf(azNewest, 40, "%f %f", pPoint->m_fLatitude, pPoint->m_fLongitude);

		g_strlcat(azCoordinateList, azNewest, COORD_LIST_MAX);
		nCount++;
	}

	gchar azQuery[MAX_SQLBUFFER_LEN];
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN,
		"INSERT INTO %s SET TypeID=%d, Coordinates=GeometryFromText('LINESTRING(%s)')"
		", AddressLeftStart=%d, AddressLeftEnd=%d, AddressRightStart=%d, AddressRightEnd=%d",
		DB_ROADS_TABLENAME,
		nLayerType,
		azCoordinateList,
		nAddressLeftStart, nAddressLeftEnd, nAddressRightStart, nAddressRightEnd);

	if(MYSQL_RESULT_SUCCESS != mysql_query(g_pDB->m_pMySQLConnection, azQuery)) {
		g_warning("db_insert_road failed: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), azQuery);
		return FALSE;
	}
	// return the new ID
	*pReturnID = mysql_insert_id(g_pDB->m_pMySQLConnection);
	return TRUE;
}

static gboolean db_roadname_get_id(const gchar* pszName, gint nSuffixID, gint* pnReturnID)
{
	gint nReturnID = 0;

	// create SQL for selecting RoadName.ID
	gchar* pszSafeName = db_make_escaped_string(pszName);
	gchar* pszSQL = g_strdup_printf("SELECT RoadName.ID FROM RoadName WHERE RoadName.Name='%s' AND RoadName.SuffixID=%d;", pszSafeName, nSuffixID);
	db_free_escaped_string(pszSafeName);

	// try query
	db_resultset_t* pResultSet = NULL;
	db_row_t aRow;
	db_query(pszSQL, &pResultSet);
	g_free(pszSQL);
	// get result?
	if(pResultSet) {
		if((aRow = db_fetch_row(pResultSet)) != NULL) {
			nReturnID = atoi(aRow[0]);
		}
		db_free_result(pResultSet);
	
		if(nReturnID != 0) {
			*pnReturnID = nReturnID;
			return TRUE;
		}
	}
	return FALSE;
}

gboolean db_insert_roadname(gint nRoadID, const gchar* pszName, gint nSuffixID)
{
	gint nRoadNameID = 0;
	// Step 1. Insert into RoadName
	if(db_roadname_get_id(pszName, nSuffixID, &nRoadNameID) == FALSE) {
		gchar* pszSafeName = db_make_escaped_string(pszName);
		gchar* pszSQL = g_strdup_printf("INSERT INTO RoadName SET Name='%s', SuffixID=%d", pszSafeName, nSuffixID);
		db_free_escaped_string(pszSafeName);

		if(db_insert(pszSQL, NULL)) {
			nRoadNameID = db_get_last_insert_id();
		}
		g_free(pszSQL);
	}

	// Step 2. Insert connector into Road_RoadName
	if(nRoadNameID != 0) {
		gchar* pszSQL = g_strdup_printf("INSERT INTO Road_RoadName SET RoadID='%d', RoadNameID='%d'",
			nRoadID, nRoadNameID);
		if(db_insert(pszSQL, NULL)) {
			g_free(pszSQL);
			return TRUE;
		}
		else {
			g_free(pszSQL);
			return FALSE;
		}
	}
	return FALSE;
}


/******************************************************
** data loading
******************************************************/

/*
gboolean db_load_geometry(db_connection_t* pConnection, maprect_t* pRect, layer_t* pLayers, gint nNumLayers) //, geometryset_t* pGeometrySet)
{
	TIMER_BEGIN(mytimer, "BEGIN DB LOAD");

//	g_return_val_if_fail(pGeometrySet != NULL, FALSE);
	gint nZoomLevel = map_get_zoomlevel();
	
	if(!db_is_connected(pConnection)) return FALSE;

	gchar* pszTable = DB_ROADS_TABLENAME; 	// use a hardcoded table name for now
	
	// HACKY: make a list of layer IDs "2,3,5,6"
	gchar azLayerNumberList[200] = {0};
	gint nActiveLayerCount = 0;
	gint i;
	for(i=LAYER_FIRST ; i <= LAYER_LAST ;i++) {
		if(g_aLayers[i].m_Style.m_aSubLayers[0].m_afLineWidths[nZoomLevel-1] != 0.0 ||
		   g_aLayers[i].m_Style.m_aSubLayers[1].m_afLineWidths[nZoomLevel-1] != 0.0)
		{
			gchar azLayerNumber[10];
			if(nActiveLayerCount > 0) g_snprintf(azLayerNumber, 10, ",%d", i);
			else g_snprintf(azLayerNumber, 10, "%d", i);
			g_strlcat(azLayerNumberList, azLayerNumber, 200);
			nActiveLayerCount++;
		}
	}
	if(nActiveLayerCount == 0) {
		g_print("no visible layers!\n");
		layers_clear();
		return TRUE;
	}

	// generate SQL
	gchar azQuery[MAX_SQLBUFFER_LEN];
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN,
		"SELECT ID, TypeID, AsText(Coordinates) FROM %s WHERE"
		" TypeID IN (%s) AND" //
		" MBRIntersects(GeomFromText('Polygon((%f %f,%f %f,%f %f,%f %f,%f %f))'), Coordinates)",
		pszTable,
		azLayerNumberList,
		//~ (nZoomLevel >= 9) ? 2 : 0,
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude, 	// upper left
		pRect->m_A.m_fLatitude, pRect->m_B.m_fLongitude, 	// upper right
		pRect->m_B.m_fLatitude, pRect->m_B.m_fLongitude, 	// bottom right
		pRect->m_B.m_fLatitude, pRect->m_A.m_fLongitude, 	// bottom left
		pRect->m_A.m_fLatitude, pRect->m_A.m_fLongitude		// upper left again
		);
	TIMER_SHOW(mytimer, "after SQL generation");
g_print("sql: %s\n", azQuery);
	mysql_query(pConnection->m_pMySQLConnection, azQuery);
	TIMER_SHOW(mytimer, "after query");

	MYSQL_RES* pResultSet = MYSQL_GET_RESULT(pConnection->m_pMySQLConnection);
	guint32 uRowCount = 0;

	MYSQL_ROW aRow;
	if(pResultSet) {
		// HACK: empty out old data, since we don't know how to merge yet
		layers_clear();
		TIMER_SHOW(mytimer, "after clear layers");

		while((aRow = mysql_fetch_row(pResultSet))) {
			uRowCount++;

			// aRow[0] is ID
			// aRow[1] is TypeID
			// aRow[2] is Coordinates in mysql's text format
			//g_print("data: %s, %s, %s\n", aRow[0], aRow[1], aRow[2]);

			gint nTypeID = atoi(aRow[1]);
			if(nTypeID < LAYER_FIRST || nTypeID > LAYER_LAST) {
				g_warning("geometry record '%s' has bad type '%s'\n", aRow[0], aRow[1]);
				continue;
			}

			//~ // add it to layer
			geometryset_add_from_mysql_geometry(pLayers[nTypeID].m_pGeometrySet, aRow[2]);
		} // end while loop on rows
		g_print(" -- got %d rows\n", uRowCount);
		TIMER_SHOW(mytimer, "after rows retrieved");

		mysql_free_result(pResultSet);
		TIMER_SHOW(mytimer, "after free results");
		TIMER_END(mytimer, "END DB LOAD");

		return TRUE;
	}
	else {
		g_print(" no rows\n");
		return FALSE;
	}
}
*/

void db_parse_point(const gchar* pszText, mappoint_t* pPoint)
{
	const gchar* p;

	p = pszText;

	if(g_str_has_prefix(p, "POINT")) {
		// format is "POINT(1.2345 -5.4321)"

		p += (6); 	// move past "POINT("
		pPoint->m_fLatitude = g_ascii_strtod(p, (gchar**)&p);

		// space between coordinates
		g_return_if_fail(*p == ' ');
		p++;

		pPoint->m_fLongitude = g_ascii_strtod(p, (gchar**)&p);
		g_return_if_fail(*p == ')');
		g_return_if_fail(*(p+1) == '\0');
	}
	else {
		g_assert_not_reached();
	}	
}

void db_parse_pointstring(const gchar* pszText, pointstring_t* pPointString, gboolean (*callback_get_point)(mappoint_t**))
{
	// parse string and add points to the string
	const gchar* p = pszText;
	if(g_str_has_prefix(p, "LINESTRING")) {
		// format is "LINESTRING(1.2345 5.4321, 10 10, 20 20)"
		mappoint_t* pPoint;

		p += (11); 	// move past "LINESTRING("

		while(TRUE) {
			// g_print("reading a point...\n");
			pPoint = NULL;
			if(!callback_get_point(&pPoint)) return;

			// read in latitude
			pPoint->m_fLatitude = g_ascii_strtod(p, (gchar**)&p);

			// space between coordinates
			g_return_if_fail(*p == ' ');
			p++;

			// read in longitude
			pPoint->m_fLongitude = g_ascii_strtod(p, (gchar**)&p);
			
			// g_print("got (%f,%f)\n", pPoint->m_fLatitude, pPoint->m_fLongitude);
			
			// add point to the list
			g_ptr_array_add(pPointString->m_pPointsArray, pPoint);

			if(*p == ',') {
				p++;

				// after this, we're ready to read in next point, so loop...
				// g_print("looping for another!...\n");
			}
			else {
				break;
			}
		}
	}
	else {
		g_assert_not_reached();
	}
}

//
// WordHash functionality
//
//~ #define DB_WORDHASH_TABLENAME 	"WordHash"

//~ gint32 db_wordhash_insert(db_connection_t* pConnection, const gchar* pszWord)
//~ {
	//~ gchar azQuery[MAX_SQLBUFFER_LEN];
	//~ int nResult;

	//~ g_snprintf(azQuery, MAX_SQLBUFFER_LEN,
		//~ "INSERT INTO %s SET ID=NULL, Value='%s';",
		//~ DB_WORDHASH_TABLENAME, pszWord);

	//~ if((nResult = mysql_query(pConnection->m_pMySQLConnection, azQuery)) != MYSQL_RESULT_SUCCESS) {
		//~ g_message("db_word_to_int failed to insert: %s\n", mysql_error(pConnection->m_pMySQLConnection));
		//~ return 0;
	//~ }
	//~ return mysql_insert_id(pConnection->m_pMySQLConnection);
//~ }

//~ gint32 db_wordhash_lookup(db_connection_t* pConnection, const gchar* pszWord)
//~ {
	//~ MYSQL_RES* pResultSet;
	//~ MYSQL_ROW aRow;
	//~ gint nWordNumber = 0;
	//~ int nResult;
	//~ gchar azQuery[MAX_SQLBUFFER_LEN];

	//~ if(!db_is_connected(pConnection)) return FALSE;

	//~ // generate SQL
	//~ g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "SELECT ID FROM %s WHERE Value='%s';", DB_WORDHASH_TABLENAME, pszWord);

	//~ // get row
	//~ if((nResult = mysql_query(pConnection->m_pMySQLConnection, azQuery)) != MYSQL_RESULT_SUCCESS) {
		//~ g_message("db_word_to_int failed to select: %s\n", mysql_error(pConnection->m_pMySQLConnection));
		//~ return 0;
	//~ }
	//~ if((pResultSet = MYSQL_GET_RESULT(pConnection->m_pMySQLConnection)) != NULL) {
		//~ if((aRow = mysql_fetch_row(pResultSet)) != NULL) {
			//~ nWordNumber = atoi(aRow[0]);
		//~ }
		//~ mysql_free_result(pResultSet);
	//~ }

	//~ // Successful?  Return it.
	//~ if(nWordNumber != 0) return nWordNumber;
	//~ else return 0;
//~ }

//=======================================================
// table creation functions
//=======================================================

//~ TABLES:
//~ Geometry (lines [roads, rail, rivers], polygons [parks, cemetaries, ponds])
//~ Points (points)
//~ Paths (lines)

//~ CREATE TABLE Points (
	//~ ID INT8 UNSIGNED NOT NULL AUTO_INCREMENT,
	//~ SetID INT4 UNSIGNED NOT NULL,

	//~ Coordinates POINT NOT NULL,
	//~ PRIMARY KEY(ID),
	//~ SPATIAL INDEX(Coordinates)
//~ );

// TODO: make sure dataset name is clean
//~ gboolean db_create_points_table(db_connection_t* pConnection, const gchar* szDatasetName)
//~ {
	//~ if(!db_is_connected(pConnection)) return FALSE;

	//~ // generate SQL
	//~ gchar azQuery[MAX_SQLBUFFER_LEN];
	//~ g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "CREATE TABLE points_%s (ID INT8 UNSIGNED NOT NULL AUTO_INCREMENT, Coordinates POINT NOT NULL, PRIMARY KEY(ID, SPATIAL INDEX(Coordinates));",
		//~ szDatasetName);

	//~ // execute and return status
	//~ return (mysql_query(pConnection->m_pMySQLConnection, azQuery) == MYSQL_RESULT_SUCCESS);
//~ }

//~ GList db_get_table_list(db_connection_t* pConnection)
//~ {
	//~ if(!db_is_connected(pConnection)) return FALSE;

	//~ // generate SQL
	//~ gchar azQuery[MAX_SQLBUFFER_LEN];
	//~ g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "SHOW TABLES;");

	//~ MYSQL_RES* pResultSet = mysql_list_tables(pConnection->m_pMySQLConnection, "points_%");

	//~ MYSQL_ROW aRow;
	//~ if(pResultSet) {
		//~ while((aRow = mysql_fetch_row(pResultSet))) {
			//~ g_print("table: %s\n", aRow[0]);
		//~ }		
		//~ mysql_free_result(pResultSet);
		//~ return TRUE;
	//~ }
	//~ else {
		//~ return FALSE;
	//~ }
//~ }

void db_create_tables()
{
	db_query("CREATE DATABASE IF NOT EXISTS roadster;", NULL);

	db_query("USE roadster;", NULL);

	// Road
	db_query("CREATE TABLE IF NOT EXISTS Road("
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" TypeID INT1 UNSIGNED NOT NULL,"
		" AddressLeftStart INT2 UNSIGNED NOT NULL,"
		" AddressLeftEnd INT2 UNSIGNED NOT NULL,"
		" AddressRightStart INT2 UNSIGNED NOT NULL,"
		" AddressRightEnd INT2 UNSIGNED NOT NULL,"
		" Coordinates point NOT NULL,"
		" PRIMARY KEY (ID),"
" INDEX(TypeID),"
		" INDEX(AddressLeftStart, AddressLeftEnd),"
		" INDEX(AddressRightStart, AddressRightEnd),"
		" SPATIAL KEY (Coordinates));", NULL);

	// RoadName
	db_query("CREATE TABLE IF NOT EXISTS RoadName("
		" ID INT4 UNSIGNED NOT NULL auto_increment,"
		" Name VARCHAR(30) NOT NULL,"
		" SuffixID INT1 UNSIGNED NOT NULL,"
		" PRIMARY KEY (ID),"
		" UNIQUE KEY (Name, SuffixID));", NULL);

	// Road_RoadName
	db_query("CREATE TABLE IF NOT EXISTS Road_RoadName("
		" RoadID INT4 UNSIGNED NOT NULL,"
		" RoadNameID INT4 UNSIGNED NOT NULL,"
		" PRIMARY KEY (RoadID, RoadNameID),"
		" INDEX(RoadNameID));", NULL);

	// Location
	db_query("CREATE TABLE IF NOT EXISTS Location("
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" LocationSetID INT4 NOT NULL,"
		" Coordinates point NOT NULL,"
		" PRIMARY KEY (ID),"
		" INDEX(LocationSetID),"
		" SPATIAL KEY (Coordinates));", NULL);

	// Location Attribute Name
	db_query("CREATE TABLE IF NOT EXISTS LocationAttributeName("
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" Name VARCHAR(30) NOT NULL,"
		" PRIMARY KEY (ID),"
		" INDEX (Name));", NULL);

	// Location Attribute Value
	db_query("CREATE TABLE IF NOT EXISTS LocationAttributeValue("
		// a unique ID for the value
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		// which location this value applies to
		" LocationID INT4 UNSIGNED NOT NULL,"
		// type 'name' of this name=value pair
		" AttributeNameID INT4 UNSIGNED NOT NULL,"
		// the actual value, a text blob
		" Value TEXT NOT NULL,"
		" PRIMARY KEY (ID),"	// for fast deletes (needed only if POIs can have multiple values per name)
		" INDEX (LocationID),"	// for searching values for a given POI
		" FULLTEXT(Value));", NULL);

	// Location Set
	db_query("CREATE TABLE IF NOT EXISTS LocationSet("
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" Name VARCHAR(60) NOT NULL,"
		" PRIMARY KEY (ID));", NULL);
}

void db_enable_keys(void)
{
	db_query("ALTER TABLE Road ENABLE KEYS", NULL);
	db_query("ALTER TABLE RoadName ENABLE KEYS", NULL);
	db_query("ALTER TABLE Road_RoadName ENABLE KEYS", NULL);
}

void db_disable_keys(void)
{
	db_query("ALTER TABLE Road DISABLE KEYS", NULL);
	db_query("ALTER TABLE RoadName DISABLE KEYS", NULL);
	db_query("ALTER TABLE Road_RoadName DISABLE KEYS", NULL);
}

/*
TABLES
======

*/
