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

#define GTK_PROCESS_MAINLOOP  while (gtk_events_pending ()) { gtk_main_iteration (); }

#define SWAP(x, y)                   { (x) ^= (y) ^= (x) ^= (y); }

void util_random_color(void* pColor);
gboolean util_parse_hex_color(const gchar* pszString, void* pReturnColor);

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

void util_close_parent_window(GtkWidget* pWidget, gpointer data);
void util_open_uri(const char* pszURI);

void util_gtk_widget_set_visible(GtkWidget* pWidget, gboolean bVisible);
gboolean util_treeview_match_all_words_callback(GtkTreeModel *pTreeModel, gint nColumn, const gchar *pszSearchText, GtkTreeIter* pIter, gpointer _unused);

gboolean util_gtk_tree_view_select_next(GtkTreeView* pTreeView);
gboolean util_gtk_tree_view_select_previous(GtkTreeView* pTreeView);

// if glib < 2.6
#if(!GLIB_CHECK_VERSION(2,6,0))
gint g_strv_length(const gchar** a);
#endif

// Pretend it's glib
gchar* util_g_strjoinv_limit(const gchar* separator, gchar** a, gint iFirst, gint iLast);

typedef struct {
	gchar* pszFind;
	gchar* pszReplace;
} util_str_replace_t;

gchar* util_str_replace_many(const gchar* pszSource, util_str_replace_t* aReplacements, gint nNumReplacements);

// GtkEntry "hint"
void util_gtk_entry_add_hint_text(GtkEntry* pEntry, const gchar* pszMessage);

#endif
