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

//#include <gnome.h>

typedef struct scenemanager {
	GPtrArray* m_p;
	GHashTable* m_pLabelHash;
} scenemanager_t;

void scenemanager_init(void);
void scenemanager_new(scenemanager_t** ppReturn);

gboolean scenemanager_can_draw_label(scenemanager_t* pSceneManager, const gchar* pszLabel);
void scenemanager_label_drawn(scenemanager_t* pSceneManager, const gchar* pszLabel);
void scenemanager_clear(scenemanager_t* pSceneManager);


#endif
