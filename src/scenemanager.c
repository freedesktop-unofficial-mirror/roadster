/***************************************************************************
 *            scenemanager.c
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
#include "../include/geometryset.h"
#include "../include/scenemanager.h"

/*
Goals:
 - Keep labels from overlapping
 - Prevent the same text from showing up too often, and choose among the options wisely
*/

// typedef struct roadlabel {
//     geometryset_t* m_pGeometry;
//     gchar* m_pszLabel;
// } roadlabel_t;

struct {
	GPtrArray* m_p;
	GHashTable* m_pLabelHash;
} g_SceneManager;

void scenemanager_init(void)
{
	g_SceneManager.m_pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);
}

#if ROADSTER_DEAD_CODE
static void scenemanager_add_label_line(geometryset_t* pGeometry, gchar* pszLabel)
{
	
}

static void scenemanager_add_label_polygon(geometryset_t* pGeometry, gchar* pszLabel)
{
	
}

static void scenemanager_draw(void)
{
	
}

static void scenemanager_clear(void)
{
	g_hash_table_destroy(g_SceneManager.m_pLabelHash);

	scenemanager_init();
}
#endif /* ROADSTER_DEAD_CODE */
