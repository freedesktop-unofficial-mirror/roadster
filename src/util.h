/***************************************************************************
 *            util.h
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <gtk/gtk.h>

#define NUM_ELEMS(a) (sizeof(a) / sizeof(a[0]))

void util_random_color(void* pColor);

//#define ENABLE_TIMING

#ifdef ENABLE_TIMING
#define TIMER_BEGIN(name, str)	GTimer* name = g_timer_new(); g_print("\n%s (%f)\n", str, g_timer_elapsed(name, NULL))
#define TIMER_SHOW(name, str)	g_print(" %s (%f)\n", str, g_timer_elapsed(name, NULL))
#define TIMER_END(name, str)	g_print("%s (%f)\n", str, g_timer_elapsed(name, NULL)); g_timer_destroy(name); name = NULL
#else
#define TIMER_BEGIN(name, str)
#define TIMER_SHOW(name, str)
#define TIMER_END(name, str)
#endif

#define	min(x,y)		((x)<(y)?(x):(y))
#define	max(x,y)		((x)>(y)?(x):(y))

#define is_even(x)		(((x) & 1) == 0)
#define is_odd(x)		(((x) & 1) == 1)

/* Funky, auto-lookup glade signal handlers.

   XXX: Better would be to hook these up manually, remove these
   declarations, and make the functions static.
*/
void util_close_parent_window(GtkWidget* pWidget, gpointer data);

#endif
