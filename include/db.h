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
	MYSQL *m_pMySQLConnection;
	gchar* m_pzHost;
	gchar* m_pzUserName;
	gchar* m_pzPassword;
	gchar* m_pzDatabase;
} db_connection_t;

typedef MYSQL_RES db_resultset_t;
typedef MYSQL_ROW db_row_t;

#define DB_ROADS_TABLENAME 		("Road")
#define DB_FEATURES_TABLENAME	("Feature")

#include "map.h"
#include "layers.h"

void db_create_tables();

void db_init();
void db_deinit();

// connect
gboolean db_connect(const gchar* pzHost, const gchar* pzUserName, const gchar* pzPassword, const gchar* pzDatabase);
const gchar* db_get_connection_info();

// utility
gboolean db_is_empty();

gboolean db_insert_road(gint nLayerType, gint nAddressLeftStart, gint nAddressLeftEnd, gint nAddressRightStart, gint nAddressRightEnd, GPtrArray* pPointsArray, gint* pReturnID);

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
gint db_get_last_insert_id();

gchar* db_make_escaped_string(const gchar* pszString);
void db_free_escaped_string(gchar* pszString);

void db_parse_point(const gchar* pszText, mappoint_t* pPoint);
void db_parse_pointstring(const gchar* pszText, pointstring_t* pPointString, gboolean (*callback_get_point)(mappoint_t**));

void db_enable_keys(void);
void db_disable_keys(void);

#endif
