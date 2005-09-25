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

#define ENABLE_NO_DUPLICATE_LABELS

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
	pNew->pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);
	pNew->pTakenRegion = gdk_region_new();
	*ppReturn = pNew;
}

// NOTE: must be called before any scenemanager_can_draw_* calls
void scenemanager_set_screen_dimensions(scenemanager_t* pSceneManager, gint nWindowWidth, gint nWindowHeight)
{
	pSceneManager->nWindowWidth = nWindowWidth;
	pSceneManager->nWindowHeight = nWindowHeight;
}

gboolean scenemanager_can_draw_label_at(scenemanager_t* pSceneManager, const gchar* pszLabel, GdkPoint* __unused_pScreenLocation, gint nFlags)
{
#ifdef ENABLE_NO_DUPLICATE_LABELS
	g_assert(pSceneManager != NULL);
	g_assert(pszLabel != NULL);

	// g_assert(pScreenLocation != NULL);
	// NOTE: ignore pScreenLocation for now
	gpointer pKey, pValue;

	// Can draw if it doesn't exist in table
	return (FALSE == g_hash_table_lookup_extended(pSceneManager->pLabelHash, pszLabel, &pKey, &pValue));
#else
	return TRUE;
#endif
}

gboolean scenemanager_can_draw_polygon(scenemanager_t* pSceneManager, GdkPoint *pPoints, gint nNumPoints, gint nFlags)
{
	//
	// 1) Enforce on-screen rules
	//
	if(nFlags & SCENEMANAGER_FLAG_FULLY_ON_SCREEN) {
		// all points must be within screen box
		gint i;
		for(i=0 ; i<nNumPoints ; i++) {
			GdkPoint* pPoint = &pPoints[i];
			if(pPoint->x < 0 || pPoint->x > pSceneManager->nWindowWidth) return FALSE;
			if(pPoint->y < 0 || pPoint->y > pSceneManager->nWindowHeight) return FALSE;
		}
		// else go on to test below
	}
	else if(nFlags & SCENEMANAGER_FLAG_PARTLY_ON_SCREEN) {
		// one point must be withing screen box   XXX: handle polygon bigger than window case?
		gint i;
		gboolean bFound = FALSE;
		for(i=0 ; i<nNumPoints ; i++) {
			GdkPoint* pPoint = &pPoints[i];
			if(pPoint->x > 0 && pPoint->x < pSceneManager->nWindowWidth) {
				bFound = TRUE;
				break;
			}
			if(pPoint->y > 0 && pPoint->y < pSceneManager->nWindowHeight) {
				bFound = TRUE;
				break;
			}
		}
		if(!bFound) return FALSE;
		// else go on to test below
	}

	//
	// 2) Enforce overlap rules
	//
	GdkRegion* pNewRegion = gdk_region_polygon(pPoints, nNumPoints, GDK_WINDING_RULE);

	gdk_region_intersect(pNewRegion, pSceneManager->pTakenRegion); // sets pNewRegion to the intersection of itself and the 'taken region'
	gboolean bOK = gdk_region_empty(pNewRegion);	// it's ok to draw here if the intersection is empty
	gdk_region_destroy(pNewRegion);

	return bOK;
}

gboolean scenemanager_can_draw_rectangle(scenemanager_t* pSceneManager, GdkRectangle* pRect, gint nFlags)
{
	//
	// 1) Enforce on-screen rules
	//
	if(nFlags & SCENEMANAGER_FLAG_FULLY_ON_SCREEN) {
		// basic rect1 contains rect2 test
		if((pRect->x) <= 0) return FALSE;
		if((pRect->y) <= 0) return FALSE;
		if((pRect->x + pRect->width) > pSceneManager->nWindowWidth) return FALSE;
		if((pRect->y + pRect->height) > pSceneManager->nWindowHeight) return FALSE;
	}
	else if(nFlags & SCENEMANAGER_FLAG_PARTLY_ON_SCREEN) {
		// basic rect intersect test
		if((pRect->x + pRect->width) <= 0) return FALSE;
		if((pRect->y + pRect->height) <= 0) return FALSE;
		if((pRect->x) > pSceneManager->nWindowWidth) return FALSE;
		if((pRect->y) > pSceneManager->nWindowHeight) return FALSE;
	}

	//
	// 2) Enforce overlap rules
	//
	GdkRegion* pNewRegion = gdk_region_rectangle(pRect);

	gdk_region_intersect(pNewRegion, pSceneManager->pTakenRegion); // sets pNewRegion to the intersection of itself and the 'taken region'
	gboolean bOK = gdk_region_empty(pNewRegion);	// it's ok to draw here if the intersection is empty
	gdk_region_destroy(pNewRegion);

	return bOK;
}

void scenemanager_claim_label(scenemanager_t* pSceneManager, const gchar* pszLabel)
{
#ifdef ENABLE_NO_DUPLICATE_LABELS
	g_assert(pSceneManager != NULL);

	// Just putting the label into the hash is enough
	g_hash_table_insert(pSceneManager->pLabelHash, pszLabel, NULL);
#endif
}

void scenemanager_claim_polygon(scenemanager_t* pSceneManager, GdkPoint *pPoints, gint nNumPoints)
{
	// Create a GdkRegion from the given points and union it with the 'taken region'
	GdkRegion* pNewRegion = gdk_region_polygon(pPoints, nNumPoints, GDK_WINDING_RULE);
	gdk_region_union(pSceneManager->pTakenRegion, pNewRegion);
	gdk_region_destroy(pNewRegion);
}

void scenemanager_claim_rectangle(scenemanager_t* pSceneManager, GdkRectangle* pRect)
{
	// Add the area of the rectangle to the region
	gdk_region_union_with_rect(pSceneManager->pTakenRegion, pRect);
}

void scenemanager_clear(scenemanager_t* pSceneManager)
{
	g_assert(pSceneManager != NULL);

	// destroy and recreate hash table (XXX: better way to clear it?)
	g_hash_table_destroy(pSceneManager->pLabelHash);
	pSceneManager->pLabelHash = g_hash_table_new(g_str_hash, g_str_equal);

	// Empty the region (XXX: better way?)
	gdk_region_destroy(pSceneManager->pTakenRegion);
	pSceneManager->pTakenRegion = gdk_region_new();
}
