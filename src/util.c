/***************************************************************************
 *            util.c
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

#include "util.h"

void util_close_parent_window(GtkWidget* pWidget, gpointer data)
{
	gtk_widget_hide(gtk_widget_get_toplevel(pWidget));
}

#if ROADSTER_DEAD_CODE
#include <stdlib.h>
#include "layers.h"		// for color_t -- move it elsewhere!
static void util_random_color(color_t* pColor)
{
	pColor->m_fRed = (random()%1000)/1000.0;
	pColor->m_fGreen = (random()%1000)/1000.0;
	pColor->m_fBlue = (random()%1000)/1000.0;
	pColor->m_fAlpha = 1.0;
}
#endif /* ROADSTER_DEAD_CODE */
