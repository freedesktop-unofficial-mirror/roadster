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

#ifdef __cplusplus
extern "C"
{
#endif
	
void mainwindow_init(void* pGladeXML);
void mainwindow_draw_map();

void mainwindow_show();
void mainwindow_show_on_startup();
void mainwindow_hide();

void mainwindow_set_sensitive(gboolean bSensitive);

void mainwindow_toggle_fullscreen();

void mainwindow_set_toolbar_visible(gboolean);
gboolean mainwindow_get_toolbar_visible();

void mainwindow_set_statusbar_visible(gboolean);
gboolean mainwindow_get_statusbar_visible();

void mainwindow_set_sidebox_visible(gboolean);
gboolean mainwindow_get_sidebox_visible();

void mainwindow_statusbar_update_zoomscale();
void mainwindow_statusbar_update_position();

void mainwindow_statusbar_progressbar_set_text(const gchar* pszText);
void mainwindow_statusbar_progressbar_pulse();
void mainwindow_statusbar_progressbar_clear();

void mainwindow_begin_import_geography_data();

void* mainwindow_set_busy();
void mainwindow_set_not_busy(void** pCursor);


#ifdef __cplusplus
}
#endif

#endif /* _MAINWINDOW_H */
