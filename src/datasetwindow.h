/***************************************************************************
 *            datasetwindow.h
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

#ifndef _DATASETWINDOW_H
#define _DATASETWINDOW_H

#include <glade/glade.h>

G_BEGIN_DECLS

void datasetwindow_init(GladeXML* pGladeXML);
void datasetwindow_show(void);

/* Funky, auto-lookup glade signal handlers.

   XXX: Better would be to hook these up manually, remove these
   declarations, and make the functions static.
*/
void datasetwindow_on_datasetdeletebutton_clicked(GtkWidget *widget, gpointer user_data);
void datasetwindow_on_datasetimportbutton_clicked(GtkWidget *widget, gpointer user_data);

G_END_DECLS

#endif /* _DATASETWINDOW_H */
