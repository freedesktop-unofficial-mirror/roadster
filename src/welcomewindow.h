/***************************************************************************
 *            welcomewindow.h
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
 
#ifndef _WELCOMEWINDOW_H
#define _WELCOMEWINDOW_H

#ifdef __cplusplus
extern "C"
{
#endif

void welcomewindow_init(GladeXML* pGladeXML);
void welcomewindow_show(void);

/* Funky, auto-lookup glade signal handlers.

   XXX: Better would be to hook these up manually, remove these
   declarations, and make the functions static.
*/
void welcomewindow_on_url_clicked(GtkWidget* pButton, gpointer data);
void welcomewindow_on_okbutton_clicked(GtkWidget* pButton, gpointer data);

#ifdef __cplusplus
}
#endif

#endif /* _WELCOMEWINDOW_H */
