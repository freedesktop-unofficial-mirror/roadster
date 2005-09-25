/***************************************************************************
 *            tooltip.h
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

#ifndef _TOOLTIP_H
#define _TOOLTIP_H

typedef struct {
	GtkWindow* pWindow;
	GtkLabel* pLabel;
	
	gboolean bEnabled;
} tooltip_t;

void tooltip_init();
tooltip_t* tooltip_new();
void tooltip_set_markup(tooltip_t* pTooltip, const gchar* pszMarkup);
void tooltip_set_upper_left_corner(tooltip_t* pTooltip, gint nX, gint nY);
void tooltip_show(tooltip_t* pTooltip);
void tooltip_hide(tooltip_t* pTooltip);
void tooltip_set_bg_color(tooltip_t* pTooltip, GdkColor* pColor);

#endif
