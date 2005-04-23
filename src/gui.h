/***************************************************************************
 *            gui.h
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
 
#ifndef _GUI_H_
#define _GUI_H_

#include <gtk/gtk.h>
#include <glade/glade.h>

#define GLADE_FILE_NAME	("roadster.glade")

extern GtkWidget *g_pApplicationWidget;
extern GtkWidget *g_pDisplaySettingsWidget;

struct gui_state_t {
	float m_fCenterX;
	float m_fCenterY;

	guint8 m_nZoomLevel;
};

//
// prototypes
//
void gui_init(void);
void gui_run(void);
extern void gui_exit(void);
GladeXML* gui_load_xml(gchar* pszFileName, gchar* pszXMLTreeRoot);
#endif
