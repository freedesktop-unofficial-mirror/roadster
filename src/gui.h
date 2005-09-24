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

#define GLADE_LINK_WIDGET(glade, ptr, type, name)	ptr = type(glade_xml_get_widget(glade, name)); g_return_if_fail(ptr != NULL)

#define GLADE_FILE_NAME	("roadster.glade")

//
// prototypes
//
void gui_init(void);
void gui_run(void);
void gui_exit(void);
GladeXML* gui_load_xml(gchar* pszFileName, gchar* pszXMLTreeRoot);
#endif
