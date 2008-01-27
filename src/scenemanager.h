/***************************************************************************
 *            scenemanager.h
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

#ifndef _SCENEMANAGER_H_
#define _SCENEMANAGER_H_

// Flags
#define SCENEMANAGER_FLAG_NONE				(0)
#define SCENEMANAGER_FLAG_FULLY_ON_SCREEN	(1)
#define SCENEMANAGER_FLAG_PARTLY_ON_SCREEN	(2)

typedef struct scenemanager {
	GdkRegion* pTakenRegion;

	gint nWindowWidth;
	gint nWindowHeight;

	GHashTable* pLabelHash;
} scenemanager_t;

void scenemanager_new(scenemanager_t** ppReturn);
void scenemanager_set_screen_dimensions(scenemanager_t* pSceneManager, gint nWindowWidth, gint nWindowHeight);

gboolean scenemanager_can_draw_label_at(scenemanager_t* pSceneManager, const gchar* pszLabel, GdkPoint* pScreenLocation, gint nFlags);
gboolean scenemanager_can_draw_polygon(scenemanager_t* pSceneManager, GdkPoint *pPoints, gint nNumPoints, gint nFlags);
gboolean scenemanager_can_draw_rectangle(scenemanager_t* pSceneManager, GdkRectangle* pRect, gint nFlags);

void scenemanager_claim_label(scenemanager_t* pSceneManager, gchar* pszLabel);
void scenemanager_claim_polygon(scenemanager_t* pSceneManager, GdkPoint *pPoints, gint nNumPoints);
void scenemanager_claim_rectangle(scenemanager_t* pSceneManager, GdkRectangle* pRect);

void scenemanager_clear(scenemanager_t* pSceneManager);

#endif
