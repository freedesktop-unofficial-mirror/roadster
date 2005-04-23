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

#include "main.h"
#include "util.h"

void util_close_parent_window(GtkWidget* pWidget, gpointer data)
{
	gtk_widget_hide(gtk_widget_get_toplevel(pWidget));
}

gboolean util_running_gnome(void)
{
	return((g_getenv("GNOME_DESKTOP_SESSION_ID") != NULL) && (g_find_program_in_path("gnome-open") != NULL));
}

void util_open_uri(const char* pszDangerousURI)
{
	// NOTE: URI is potentially from a 3rd party, so consider it DANGEROUS.
	// This is why we use g_spawn_async, which lets us mark the URI as a parameter instead
	// of just part of the command line (it could be "; rm / -rf" or something...)

	char *pszCommand = NULL;
	GError *pError = NULL;

	// if they are running gnome, use the gnome web browser
	if (util_running_gnome() == FALSE) return;


	gchar **argv = g_malloc0(sizeof(gchar*) * 3);
	argv[0] = "gnome-open";
	argv[1] = pszDangerousURI;
	argv[2] = NULL;

	if(!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &pError)) {
		// XXX: error message?
		g_error_free(pError);
	}
	g_free(argv);
	g_free(pszCommand);
}

// if glib < 2.6
#if(!GLIB_CHECK_VERSION(2,6,0))
gint g_strv_length(const gchar** a)
{
	gint nCount=0;
	const gchar** pp = a;
	while(*pp != NULL) {
		nCount++;
		pp++;
	}
	return nCount;
}
#endif

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
