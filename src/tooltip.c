/***************************************************************************
 *            tooltip.c
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

#include <gtk/gtk.h>

#include "main.h"
#include "tooltip.h"

static gboolean tooltip_on_mouse_motion(GtkWidget* w, GdkEventMotion *event);

tooltip_t* tooltip_new()
{
	tooltip_t* pNew = g_new0(tooltip_t, 1);
	
	// create tooltip window
	pNew->pWindow = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));
	//gtk_widget_set_name(GTK_WIDGET(pNew->pWindow), "gtk-tooltips");
	gtk_widget_add_events(GTK_WIDGET(pNew->pWindow), GDK_POINTER_MOTION_MASK | GDK_EXPOSURE_MASK);
	g_signal_connect(G_OBJECT(pNew->pWindow), "motion_notify_event", G_CALLBACK(tooltip_on_mouse_motion), NULL);
	gtk_window_set_resizable(pNew->pWindow, FALSE);	// FALSE means window will resize to hug the label.

	// create frame (draws the nice outline for us) and add to window (we don't need to keep a pointer to this)
	GtkFrame* pFrame = GTK_FRAME(gtk_frame_new(NULL));
	gtk_frame_set_shadow_type(pFrame, GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(pNew->pWindow), GTK_WIDGET(pFrame));


	// create label and add to frame
	pNew->pLabel = GTK_LABEL(gtk_label_new("testing"));
	gtk_container_add(GTK_CONTAINER(pFrame), GTK_WIDGET(pNew->pLabel));

	pNew->bEnabled = TRUE;	// XXX: currently no API to disable it

	return pNew;
}

void tooltip_set_markup(tooltip_t* pTooltip, const gchar* pszMarkup)
{
	gtk_label_set_markup(pTooltip->pLabel, pszMarkup);
}

void tooltip_set_bg_color(tooltip_t* pTooltip, GdkColor* pColor)
{
	gtk_widget_modify_bg(GTK_WIDGET(pTooltip->pWindow), GTK_STATE_NORMAL, pColor);
}

void tooltip_set_upper_left_corner(tooltip_t* pTooltip, gint nX, gint nY)
{
	gtk_window_move(pTooltip->pWindow, nX, nY);
}

void tooltip_show(tooltip_t* pTooltip)
{
	if(pTooltip->bEnabled) {
		gtk_widget_show_all(GTK_WIDGET(pTooltip->pWindow));
	}
}

void tooltip_hide(tooltip_t* pTooltip)
{
	gtk_widget_hide(GTK_WIDGET(pTooltip->pWindow));
}

static gboolean tooltip_on_mouse_motion(GtkWidget* pWidget, GdkEventMotion *__unused)
{
	// in case the mouse makes its way onto the tooltip, hide it.
	gtk_widget_hide(pWidget);
}


