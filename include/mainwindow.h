/***************************************************************************
 *            mainwindow.h
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
 
#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H

#include <glade/glade.h>

#ifdef __cplusplus
extern "C"
{
#endif
	
void mainwindow_init(GladeXML* pGladeXML);
void mainwindow_draw_map(void);

void mainwindow_show(void);
void mainwindow_show_on_startup(void);
void mainwindow_hide(void);

void mainwindow_set_sensitive(gboolean bSensitive);

void mainwindow_toggle_fullscreen(void);

void mainwindow_set_toolbar_visible(gboolean);
gboolean mainwindow_get_toolbar_visible(void);

void mainwindow_set_statusbar_visible(gboolean);
gboolean mainwindow_get_statusbar_visible(void);

void mainwindow_set_sidebox_visible(gboolean);
gboolean mainwindow_get_sidebox_visible(void);

GtkWidget* mainwindow_get_window(void);

void mainwindow_statusbar_update_zoomscale(void);
void mainwindow_statusbar_update_position(void);

void mainwindow_statusbar_progressbar_set_text(const gchar* pszText);
void mainwindow_statusbar_progressbar_pulse(void);
void mainwindow_statusbar_progressbar_clear(void);

void mainwindow_begin_import_geography_data(void);

void* mainwindow_set_busy(void);
void mainwindow_set_not_busy(void** pCursor);

/* Funky, auto-lookup glade signal handlers.

   XXX: Better would be to hook these up manually, remove these
   declarations, and make the functions static.
*/
void on_import_maps_activate(GtkWidget *widget, gpointer user_data);
void on_quitmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
gboolean on_application_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
void on_zoomscale_value_changed(GtkRange *range, gpointer user_data);
void on_aboutmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_toolbarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_statusbarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_sidebarmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_zoomin_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_zoomout_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_fullscreenmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
void mainwindow_on_gotomenuitem_activate(GtkMenuItem     *menuitem, gpointer user_data);
void on_importmenuitem_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_gotobutton_clicked(GtkToolButton *toolbutton,  gpointer user_data);
void on_toolbutton_clicked(GtkToolButton *toolbutton, gpointer user_data);
void mainwindow_on_addpointmenuitem_activate(GtkWidget *_unused, gpointer* __unused);
void mainwindow_on_datasetmenuitem_activate(GtkWidget *pWidget, gpointer* p);

#ifdef __cplusplus
}
#endif

#endif /* _MAINWINDOW_H */
