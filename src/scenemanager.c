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
#include "scenemanager.h"

/*
Goals:
 - Keep labels from overlapping
 - Prevent the same text from showing up too often, and choose among the options wisely
*/

// typedef struct roadlabel {
//     geometryset_t* m_pGeometry;
//     gchar* m_pszLabel;
// } roadlabel_t;

void scenemanager_init(void)
{
}

void scenemanager_new(scenemanager_t** ppReturn)
{
	scenemanager_t* pNew = g_new0(scenemanager_t, 1);
	pNew->m_pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);
	*ppReturn = pNew;
}

gboolean scenemanager_can_draw_label(scenemanager_t* pSceneManager, const gchar* pszLabel)
{
	g_assert(pSceneManager != NULL);

	gpointer pKey;
	gpointer pValue;

	// can draw if it doesn't exist in table
	gboolean bOK = (g_hash_table_lookup_extended(pSceneManager->m_pLabelHash,
                                        pszLabel, &pKey, &pValue) == FALSE);

//	g_print("permission for %s: %s\n", pszLabel, bOK ? "YES" : "NO");
	return bOK;
}

void scenemanager_label_drawn(scenemanager_t* pSceneManager, const gchar* pszLabel)
{
	g_assert(pSceneManager != NULL);
//	g_print("drawn! %s\n", pszLabel);
	g_hash_table_insert(pSceneManager->m_pLabelHash, pszLabel, NULL);
}

void scenemanager_clear(scenemanager_t* pSceneManager)
{
	g_assert(pSceneManager != NULL);

	g_hash_table_destroy(pSceneManager->m_pLabelHash);
	pSceneManager->m_pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);
}


#if ROADSTER_DEAD_CODE
static void scenemanager_add_label_line(geometryset_t* pGeometry, gchar* pszLabel) {}
static void scenemanager_add_label_polygon(geometryset_t* pGeometry, gchar* pszLabel) {}
static void scenemanager_draw(void) {}
#endif /* ROADSTER_DEAD_CODE */
