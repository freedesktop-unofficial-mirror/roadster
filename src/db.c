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

#define USE_GNOME_VFS

#include <mysql.h>

#define HAVE_MYSQL_EMBED

#ifdef HAVE_MYSQL_EMBED
# include <mysql_embed.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#ifdef USE_GNOME_VFS
#include <gnome-vfs-2.0/libgnomevfs/gnome-vfs.h>
#endif

#include "db.h"
#include "mainwindow.h"
#include "util.h"
#include "layers.h"
#include "locationset.h"

#define MYSQL_RESULT_SUCCESS  	(0)		// for clearer code

#define MAX_SQLBUFFER_LEN		(132000)	// must be big for lists of coordinates
#define COORD_LIST_MAX 			(128000)

// mysql_use_result - 	less client memory, ties up the server (other clients can't do updates)
//						better for embedded or local servers
// mysql_store_result - more client memory, gets all results right away and frees up server
//						better for remote servers
#define MYSQL_GET_RESULT(x)		mysql_store_result((x))

db_connection_t* g_pDB = NULL;


/******************************************************
** Init and deinit of database module
******************************************************/

// call once on program start-up
void db_init()
{
//	g_pDBMutex = g_mutex_new();

#ifdef HAVE_MYSQL_EMBED
	gchar* pszDataDir = g_strdup_printf("%s/.roadster/data", g_get_home_dir());
	gchar* pszSetDataDirCommand = g_strdup_printf("--datadir=%s", pszDataDir);
	gchar* pszSetQueryCacheSize = g_strdup_printf("--query-cache-size=%dMB", 40);
	gchar* pszKeyBufferSize	= g_strdup_printf("--key-buffer-size=%dMB", 32);

#ifdef USE_GNOME_VFS
	// Create directory if it doesn't exist
	if(GNOME_VFS_OK != gnome_vfs_make_directory(pszDataDir, 0700)) {
		// no big deal, probably already exists (should we check?)
	}
#endif

	gchar* apszServerOptions[] = {
		"",	// program name -- unused
		"--skip-innodb",	// don't bother with table types we don't use
		"--skip-bdb",
                "--query-cache-type=1",		// enable query cache (for map tiles)
                pszSetQueryCacheSize,	//

		pszKeyBufferSize,
		"--ft-min-word-len=2",		// 2 chars should count as a word for fulltext indexes
		pszSetDataDirCommand
	};

	// Initialize the embedded server
	// NOTE: if not linked with libmysqld, this call will do nothing (but will succeed)
 	if(mysql_server_init(NUM_ELEMS(apszServerOptions), apszServerOptions, NULL) != 0) {
		return;
	}
	g_free(pszDataDir);
	g_free(pszSetDataDirCommand);
	g_free(pszSetQueryCacheSize);
	g_free(pszKeyBufferSize);
#endif
}

// call once on program shut-down
void db_deinit()
{
#ifdef HAVE_MYSQL_EMBED
	// Close embedded server if present
	mysql_server_end();
#endif
}

gboolean db_query(const gchar* pszSQL, db_resultset_t** ppResultSet)
{
	g_assert(pszSQL != NULL);
	if(g_pDB == NULL) return FALSE;

	gint nResult = mysql_query(g_pDB->m_pMySQLConnection, pszSQL);
	if(nResult != MYSQL_RESULT_SUCCESS) {
		g_warning("db_query: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), pszSQL);
		return FALSE;
	}

	// get result?
	if(ppResultSet != NULL) {
		*ppResultSet = (db_resultset_t*)MYSQL_GET_RESULT(g_pDB->m_pMySQLConnection);
	}
	return TRUE;
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
//	db_enable_keys(); // just in case

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


/******************************************************
** database utility functions
******************************************************/

// call db_free_escaped_string() on returned string
gchar* db_make_escaped_string(const gchar* pszString)
{
	// make given string safe for inclusion in a SQL string
	if(!db_is_connected()) return g_strdup("");

	gint nLength = (strlen(pszString)*2) + 1;
	gchar* pszNew = g_malloc(nLength);
	mysql_real_escape_string(g_pDB->m_pMySQLConnection, pszNew, pszString, strlen(pszString));

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
	return (db_count_table_rows(DB_ROADS_TABLENAME) == 0);
}

/******************************************************
** data inserting
******************************************************/

static gboolean db_insert(const gchar* pszSQL, gint* pnReturnRowsInserted)
{
	g_assert(pszSQL != NULL);
	if(g_pDB == NULL) return FALSE;

	if(mysql_query(g_pDB->m_pMySQLConnection, pszSQL) != MYSQL_RESULT_SUCCESS) {
		//g_warning("db_query: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), pszSQL);
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

gboolean db_insert_road(gint nRoadNameID, gint nLayerType, gint nAddressLeftStart, gint nAddressLeftEnd, gint nAddressRightStart, gint nAddressRightEnd, gint nCityLeftID, gint nCityRightID, const gchar* pszZIPCodeLeft, const gchar* pszZIPCodeRight, GPtrArray* pPointsArray, gint* pReturnID)
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
		"INSERT INTO %s SET RoadNameID=%d, TypeID=%d, Coordinates=GeometryFromText('LINESTRING(%s)')"
		", AddressLeftStart=%d, AddressLeftEnd=%d, AddressRightStart=%d, AddressRightEnd=%d"
		", CityLeftID=%d, CityRightID=%d"
		", ZIPCodeLeft='%s', ZIPCodeRight='%s'",
		DB_ROADS_TABLENAME, nRoadNameID, nLayerType, azCoordinateList,
	    nAddressLeftStart, nAddressLeftEnd, nAddressRightStart, nAddressRightEnd,
		nCityLeftID, nCityRightID,
		pszZIPCodeLeft, pszZIPCodeRight);

	if(MYSQL_RESULT_SUCCESS != mysql_query(g_pDB->m_pMySQLConnection, azQuery)) {
		g_warning("db_insert_road failed: %s (SQL: %s)\n", mysql_error(g_pDB->m_pMySQLConnection), azQuery);
		return FALSE;
	}
	// return the new ID
	*pReturnID = mysql_insert_id(g_pDB->m_pMySQLConnection);
	return TRUE;
}

/******************************************************
**
******************************************************/

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

gboolean db_insert_roadname(const gchar* pszName, gint nSuffixID, gint* pnReturnID)
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
	
	if(nRoadNameID != 0) {
		if(pnReturnID != NULL) {
			*pnReturnID = nRoadNameID;
		}
		return TRUE;
	}
	return FALSE;
}

//
// insert / select city
//

// lookup numerical ID of a city by name
gboolean db_city_get_id(const gchar* pszName, gint nStateID, gint* pnReturnID)
{
	g_assert(pnReturnID != NULL);

	gint nReturnID = 0;
	// create SQL for selecting City.ID
	gchar* pszSafeName = db_make_escaped_string(pszName);
	gchar* pszSQL = g_strdup_printf("SELECT City.ID FROM City WHERE City.Name='%s' AND City.StateID=%d;", pszSafeName, nStateID);
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

gboolean db_insert_city(const gchar* pszName, gint nStateID, gint* pnReturnCityID)
{
	gint nCityID = 0;

	// Step 1. Insert into RoadName
	if(db_city_get_id(pszName, nStateID, &nCityID) == FALSE) {
		gchar* pszSafeName = db_make_escaped_string(pszName);
		gchar* pszSQL = g_strdup_printf("INSERT INTO City SET Name='%s', StateID=%d", pszSafeName, nStateID);
		db_free_escaped_string(pszSafeName);

		if(db_insert(pszSQL, NULL)) {
			*pnReturnCityID = db_get_last_insert_id();
		}
		g_free(pszSQL);
	}
	else {
		// already exists, use the existing one.
		*pnReturnCityID = nCityID;
	}
	return TRUE;
}


//
// insert / select state
//
// lookup numerical ID of a state by name
gboolean db_state_get_id(const gchar* pszName, gint* pnReturnID)
{
	gint nReturnID = 0;

	// create SQL for selecting City.ID
	gchar* pszSafeName = db_make_escaped_string(pszName);
	gchar* pszSQL = g_strdup_printf("SELECT State.ID FROM State WHERE State.Name='%s' OR State.Code='%s';", pszSafeName, pszSafeName);
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

gboolean db_insert_state(const gchar* pszName, const gchar* pszCode, gint nCountryID, gint* pnReturnStateID)
{
	gint nStateID = 0;

	// Step 1. Insert into RoadName
	if(db_state_get_id(pszName, &nStateID) == FALSE) {
		gchar* pszSafeName = db_make_escaped_string(pszName);
		gchar* pszSafeCode = db_make_escaped_string(pszCode);
		gchar* pszSQL = g_strdup_printf("INSERT INTO State SET Name='%s', Code='%s', CountryID=%d", pszSafeName, pszSafeCode, nCountryID);
		db_free_escaped_string(pszSafeName);
		db_free_escaped_string(pszSafeCode);

		if(db_insert(pszSQL, NULL)) {
			*pnReturnStateID = db_get_last_insert_id();
		}
		g_free(pszSQL);
	}
	else {
		// already exists, use the existing one.
		*pnReturnStateID = nStateID;
	}
	return TRUE;
}

#define WKB_POINT                  1	// only two we care about
#define WKB_LINESTRING             2

void db_parse_wkb_pointstring(const gint8* data, pointstring_t* pPointString, gboolean (*callback_get_point)(mappoint_t**))
{
	g_assert(sizeof(double) == 8);	// mysql gives us 8 bytes per point

	gint nByteOrder = *data++;	// first byte tells us the byte order
	g_assert(nByteOrder == 1);

	gint nGeometryType = *((gint32*)data)++;
	g_assert(nGeometryType == WKB_LINESTRING);

	gint nNumPoints = *((gint32*)data)++;	// NOTE for later: this field doesn't exist for type POINT

	while(nNumPoints > 0) {
		mappoint_t* pPoint = NULL;
		if(!callback_get_point(&pPoint)) return;

		pPoint->m_fLatitude = *((double*)data)++;
		pPoint->m_fLongitude = *((double*)data)++;

		g_ptr_array_add(pPointString->m_pPointsArray, pPoint);

		nNumPoints--;
	}
}

void db_create_tables()
{
	db_query("CREATE DATABASE IF NOT EXISTS roadster;", NULL);
	db_query("USE roadster;", NULL);

	// Road
	db_query("CREATE TABLE IF NOT EXISTS Road("
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" TypeID INT1 UNSIGNED NOT NULL,"

		" RoadNameID INT4 UNSIGNED NOT NULL,"

		" AddressLeftStart INT2 UNSIGNED NOT NULL,"
		" AddressLeftEnd INT2 UNSIGNED NOT NULL,"
		" AddressRightStart INT2 UNSIGNED NOT NULL,"
		" AddressRightEnd INT2 UNSIGNED NOT NULL,"
		
		" CityLeftID INT4 UNSIGNED NOT NULL,"
		" CityRightID INT4 UNSIGNED NOT NULL,"
		
		" ZIPCodeLeft CHAR(6) NOT NULL,"
		" ZIPCodeRight CHAR(6) NOT NULL,"

		" Coordinates point NOT NULL,"

	    // lots of indexes:
		" PRIMARY KEY (ID),"
		" INDEX(TypeID),"
		" INDEX(RoadNameID),"	// to get roads when we've matched a RoadName
		" INDEX(AddressLeftStart, AddressLeftEnd),"
		" INDEX(AddressRightStart, AddressRightEnd),"
		" SPATIAL KEY (Coordinates));", NULL);

	// RoadName
	db_query("CREATE TABLE IF NOT EXISTS RoadName("
		" ID INT4 UNSIGNED NOT NULL auto_increment,"
		" Name VARCHAR(30) NOT NULL,"
		" SuffixID INT1 UNSIGNED NOT NULL,"
		" PRIMARY KEY (ID),"
		" UNIQUE KEY (Name(15), SuffixID));", NULL);

	// Road_RoadName
//         db_query("CREATE TABLE IF NOT EXISTS Road_RoadName("
//                 " RoadID INT4 UNSIGNED NOT NULL,"
//                 " RoadNameID INT4 UNSIGNED NOT NULL,"
//
//                 " PRIMARY KEY (RoadID, RoadNameID),"    // allows search on (RoadID,RoadName) and just (RoadID)
//                 " INDEX(RoadNameID));", NULL);          // allows search the other way, going from a Name to a RoadID

	// City
	db_query("CREATE TABLE IF NOT EXISTS City("
		// a unique ID for the value
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" StateID INT4 UNSIGNED NOT NULL,"
		" Name CHAR(60) NOT NULL,"
		" PRIMARY KEY (ID),"
		" INDEX (StateID),"	// for finding all cities by state (needed?)
		" INDEX (Name(15)));"	// only index the first X chars of name (who types more than that?) (are city names ever 60 chars anyway??  TIGER think so)
	    ,NULL);

	// State
	db_query("CREATE TABLE IF NOT EXISTS State("
		// a unique ID for the value
		" ID INT4 UNSIGNED NOT NULL AUTO_INCREMENT,"
		" Name CHAR(40) NOT NULL,"
		" Code CHAR(3) NOT NULL,"
		" CountryID INT4 NOT NULL,"
		" PRIMARY KEY (ID),"
		" INDEX (Name(15)));"	// only index the first X chars of name (who types more than that?)
	    ,NULL);

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
		" UNIQUE INDEX (Name));", NULL);

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
		" PRIMARY KEY (ID),"	// for fast updates/deletes (needed only if POIs can have multiple values per name)
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
//	g_print("Enabling keys\n");
//	db_query("ALTER TABLE Road ENABLE KEYS", NULL);
//	db_query("ALTER TABLE RoadName ENABLE KEYS", NULL);
//	db_query("ALTER TABLE Road_RoadName ENABLE KEYS", NULL);
}

void db_disable_keys(void)
{
//	g_print("Disabling keys\n");
//	db_query("ALTER TABLE Road DISABLE KEYS", NULL);
//	db_query("ALTER TABLE RoadName DISABLE KEYS", NULL);
//	db_query("ALTER TABLE Road_RoadName DISABLE KEYS", NULL);
}

#ifdef ROADSTER_DEAD_CODE
/*
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

//
// WordHash functionality
//
#define DB_WORDHASH_TABLENAME 	"WordHash"

gint32 db_wordhash_insert(db_connection_t* pConnection, const gchar* pszWord)
{
	gchar azQuery[MAX_SQLBUFFER_LEN];
	int nResult;

	g_snprintf(azQuery, MAX_SQLBUFFER_LEN,
		"INSERT INTO %s SET ID=NULL, Value='%s';",
		DB_WORDHASH_TABLENAME, pszWord);

	if((nResult = mysql_query(pConnection->m_pMySQLConnection, azQuery)) != MYSQL_RESULT_SUCCESS) {
		g_message("db_word_to_int failed to insert: %s\n", mysql_error(pConnection->m_pMySQLConnection));
		return 0;
	}
	return mysql_insert_id(pConnection->m_pMySQLConnection);
}

gint32 db_wordhash_lookup(db_connection_t* pConnection, const gchar* pszWord)
{
	MYSQL_RES* pResultSet;
	MYSQL_ROW aRow;
	gint nWordNumber = 0;
	int nResult;
	gchar azQuery[MAX_SQLBUFFER_LEN];

	if(!db_is_connected(pConnection)) return FALSE;

	// generate SQL
	g_snprintf(azQuery, MAX_SQLBUFFER_LEN, "SELECT ID FROM %s WHERE Value='%s';", DB_WORDHASH_TABLENAME, pszWord);

	// get row
	if((nResult = mysql_query(pConnection->m_pMySQLConnection, azQuery)) != MYSQL_RESULT_SUCCESS) {
		g_message("db_word_to_int failed to select: %s\n", mysql_error(pConnection->m_pMySQLConnection));
		return 0;
	}
	if((pResultSet = MYSQL_GET_RESULT(pConnection->m_pMySQLConnection)) != NULL) {
		if((aRow = mysql_fetch_row(pResultSet)) != NULL) {
			nWordNumber = atoi(aRow[0]);
		}
		mysql_free_result(pResultSet);
	}

	// Successful?  Return it.
	if(nWordNumber != 0) return nWordNumber;
	else return 0;
}

void db_parse_pointstring(const gchar* pszText, pointstring_t* pPointString, gboolean (*callback_get_point)(mappoint_t**))
{
	// parse string and add points to the string
	const gchar* p = pszText;
	if(p[0] == 'L') { //g_str_has_prefix(p, "LINESTRING")) {
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
*/
#endif
