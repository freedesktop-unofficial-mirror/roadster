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

#include "main.h"
#include "scenemanager.h"

/*
Goals:
 - Keep text labels and other screen objects from overlapping
 - Prevent the same text from showing up too often (currently not more than once)
*/

void scenemanager_init(void)
{
}

void scenemanager_new(scenemanager_t** ppReturn)
{
	// create new scenemanager and return it
	scenemanager_t* pNew = g_new0(scenemanager_t, 1);
	pNew->m_pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);
	pNew->m_pTakenRegion = gdk_region_new();
	*ppReturn = pNew;
}

gboolean scenemanager_can_draw_label_at(scenemanager_t* pSceneManager, const gchar* pszLabel, GdkPoint* __unused_pScreenLocation)
{
	g_assert(pSceneManager != NULL);
	g_assert(pszLabel != NULL);
	
	// g_assert(pScreenLocation != NULL);
	// NOTE: ignore pScreenLocation for now

	gpointer pKey, pValue;

	// Can draw if it doesn't exist in table
	return (FALSE == g_hash_table_lookup_extended(pSceneManager->m_pLabelHash, pszLabel, &pKey, &pValue));
}

gboolean scenemanager_can_draw_polygon(scenemanager_t* pSceneManager, GdkPoint *pPoints, gint nNumPoints)
{
	GdkRegion* pNewRegion = gdk_region_polygon(pPoints, nNumPoints, GDK_WINDING_RULE);
	
	// Set pNewRegion to the intersection of it and the 'taken region'
	gdk_region_intersect(pNewRegion, pSceneManager->m_pTakenRegion);
	gboolean bOK = gdk_region_empty(pNewRegion);	// it's ok to draw here if the intersection is empty
        gdk_region_destroy(pNewRegion);

	return bOK;
}

gboolean scenemanager_can_draw_rectangle(scenemanager_t* pSceneManager, GdkRectangle* pRect)
{
	GdkRegion* pNewRegion = gdk_region_rectangle(pRect);

	// Set pNewRegion to the intersection of it and the 'taken region'
	gdk_region_intersect(pNewRegion, pSceneManager->m_pTakenRegion);
	gboolean bOK = gdk_region_empty(pNewRegion);	// it's ok to draw here if the intersection is empty
        gdk_region_destroy(pNewRegion);

	return bOK;
}

void scenemanager_claim_label(scenemanager_t* pSceneManager, const gchar* pszLabel)
{
	g_assert(pSceneManager != NULL);

	// Just putting the label into the hash is enough
	g_hash_table_insert(pSceneManager->m_pLabelHash, pszLabel, NULL);
}

void scenemanager_claim_polygon(scenemanager_t* pSceneManager, GdkPoint *pPoints, gint nNumPoints)
{
	// Create a GdkRegion from the given points and union it with the 'taken region'
	GdkRegion* pNewRegion = gdk_region_polygon(pPoints, nNumPoints, GDK_WINDING_RULE);
	gdk_region_union(pSceneManager->m_pTakenRegion, pNewRegion);
        gdk_region_destroy(pNewRegion);
}

void scenemanager_claim_rectangle(scenemanager_t* pSceneManager, GdkRectangle* pRect)
{
	// Add the area of the rectangle to the region
	gdk_region_union_with_rect(pSceneManager->m_pTakenRegion, pRect);
}

void scenemanager_clear(scenemanager_t* pSceneManager)
{
	g_assert(pSceneManager != NULL);

	// destroy and recreate hash table (better way to clear it?)
	g_hash_table_destroy(pSceneManager->m_pLabelHash);
	pSceneManager->m_pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);

	// Empty the region (better way?)
	gdk_region_destroy(pSceneManager->m_pTakenRegion);
	pSceneManager->m_pTakenRegion = gdk_region_new();
}
