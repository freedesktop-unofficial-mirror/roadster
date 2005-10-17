/***************************************************************************
 *            db.h
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

#ifndef _DB_H_
#define _DB_H_

#include <mysql.h>

typedef struct db_connection {
	MYSQL* pMySQLConnection;
	gchar* pzHost;
	gchar* pzUserName;
	gchar* pzPassword;
	gchar* pzDatabase;
} db_connection_t;

typedef MYSQL_RES db_resultset_t;
typedef MYSQL_ROW db_row_t;

#define DB_ROADS_TABLENAME 		("Road")
#define DB_FEATURES_TABLENAME	("Feature")

#include "map.h"

void db_create_tables(void);

void db_init(void);
void db_deinit(void);

// connect
gboolean db_connect(const gchar* pzHost, const gchar* pzUserName, const gchar* pzPassword, const gchar* pzDatabase);
const gchar* db_get_connection_info(void);

// utility
gboolean db_insert_roadname(const gchar* pszName, gint nSuffixID, gint* pnReturnID);

//~ gboolean db_create_points_db(const gchar* name);

//~ // insert
//~ gboolean db_road_insert(db_connection_t* pConnection, gint nLayerType, GPtrArray* pPoints, gint* pReturnID);
//~ gboolean db_point_insert(db_connection_t* pConnection, gint nPointSetID, mappoint_t* pPoint, gint* pReturnID);

//~ // load
//~ gboolean db_load_geometry(db_connection_t* pConnection, maprect_t* pRect, layer_t* pLayer, gint nNumLayers); //, geometryset_t* pGeometrySet);
//~ gboolean db_pointset_get_list(db_connection_t* pConnection, GPtrArray* pPointSet);

gboolean db_query(const gchar* pszSQL, db_resultset_t** ppResultSet);
db_row_t db_fetch_row(db_resultset_t* pResultSet);
void db_free_result(db_resultset_t* pResultSet);
gint db_get_last_insert_id(void);

void db_begin_thread(void);
void db_end_thread(void);

gchar* db_make_escaped_string(const gchar* pszString);
void db_free_escaped_string(gchar* pszString);

//void db_parse_wkb_linestring(const gint8* data, GPtrArray* pPointsArray, gboolean (*callback_alloc_point)(mappoint_t**));
//void db_parse_wkb_linestring(const gint8* data, GArray* pMapPointsArray);
void db_parse_wkb_linestring(const gint8* data, GArray* pMapPointsArray, maprect_t* pBoundingRect);
void db_parse_wkb_point(const gint8* data, mappoint_t* pPoint);

void db_enable_keys(void);
void db_disable_keys(void);

gboolean db_insert_city(const gchar* pszName, gint nStateID, gint* pnReturnCityID);
gboolean db_insert_road(gint nLOD, gint nRoadNameID, gint nLayerType, gint nAddressLeftStart, gint nAddressLeftEnd, gint nAddressRightStart, gint nAddressRightEnd, gint nCityLeftID, gint nCityRightID, const gchar* pszZIPCodeLeft, const gchar* pszZIPCodeRight, GArray* pPointsArray, gint* pReturnID);

gboolean db_city_get_id(const gchar* pszName, gint nStateID, gint* pnReturnID);
gboolean db_state_get_id(const gchar* pszName, gint* pnReturnID);

void db_lock(void);
void db_unlock(void);

#endif
