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
void gui_init();
void gui_run();
extern gboolean gui_redraw_map_if_needed();
extern void gui_set_tool_label(gchar* pMessage);
extern void gui_exit();
extern GtkWidget* gui_get_top_window();

//~ void gui_show_goto_window();
//~ void gui_hide_goto_window();

//~ void gui_show_colors_window();
//~ void gui_hide_colors_window();

//~ void gui_show_about_dialog();
//~ void gui_hide_about_window();

//~ void gui_show_preferences_window();
//~ void gui_hide_preferences_window();

void cursor_init();
//~ void gui_statusbar_update_zoomscale();
//~ void gui_statusbar_update_position();
//~ void gui_statusbar_set_position(gchar* pMessage);
//~ void gui_statusbar_set_zoomscale(gchar* pMessage);

//~ gboolean gui_get_toolbar_visible();
//~ void gui_set_toolbar_visible(gboolean bVisible);

//~ void gui_toggle_fullscreen();

//~ void gui_zoomin();
//~ void gui_zoomout();
#endif
