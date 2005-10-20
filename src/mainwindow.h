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
#include "map.h"
#include "util.h"

G_BEGIN_DECLS

void mainwindow_init(GladeXML* pGladeXML);
void mainwindow_draw_map(gint nDrawFlags);
// GtkWidget* mainwindow_get_window(void);

// Visibility
void mainwindow_show(void);
void mainwindow_show_on_startup(void);
void mainwindow_hide(void);

// Sensitivity
void mainwindow_set_sensitive(gboolean bSensitive);

// Full screen
void mainwindow_toggle_fullscreen(void);

// Busy
void* mainwindow_set_busy(void);
void mainwindow_set_not_busy(void** pCursor);

// Toolbar / Statusbar / Sidebox
void mainwindow_set_toolbar_visible(gboolean);
gboolean mainwindow_get_toolbar_visible(void);

void mainwindow_set_statusbar_visible(gboolean);
gboolean mainwindow_get_statusbar_visible(void);

void mainwindow_set_sidebox_visible(gboolean);
gboolean mainwindow_get_sidebox_visible(void);

void mainwindow_statusbar_update_zoomscale(void);
void mainwindow_statusbar_update_position(void);

void mainwindow_set_zoomlevel(gint nZoomLevel);

void mainwindow_statusbar_progressbar_set_text(const gchar* pszText);
void mainwindow_statusbar_progressbar_pulse(void);
void mainwindow_statusbar_progressbar_clear(void);
void mainwindow_statusbar_update_zoomscale(void);
void mainwindow_statusbar_update_position(void);

// Map
void mainwindow_map_center_on_mappoint(mappoint_t* pPoint);
void mainwindow_map_get_centerpoint(mappoint_t* pPoint);
void mainwindow_map_slide_to_mappoint(mappoint_t* pPoint);

void mainwindow_add_history();

void mainwindow_scroll_direction(EDirection eScrollDirection, gint nPixels);

#define SIDEBAR_TAB_LOCATIONSETS	0
#define SIDEBAR_TAB_TRACKS		1
//#define SIDEBAR_TAB_GPS			2
#define SIDEBAR_TAB_SEARCH_RESULTS	2

void mainwindow_sidebar_set_tab(gint nTab);

G_END_DECLS

#endif /* _MAINWINDOW_H */
