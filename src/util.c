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

	GError *pError = NULL;

	// if they are running gnome, use the gnome web browser
	if (util_running_gnome() == FALSE) return;

	gchar **argv = g_malloc0(sizeof(gchar*) * 3);
	argv[0] = "gnome-open";
	argv[1] = pszDangerousURI;
	argv[2] = NULL;

	if(!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &pError)) {
		// XXX: error message?  or make this function return boolean?
		g_error_free(pError);
	}
	g_free(argv);
}

gchar* g_strjoinv_limit(const gchar* separator, gchar** a, gint iFirst, gint iLast)
{
	g_assert(iFirst <= iLast);
	g_assert(iLast < g_strv_length(a));

	gchar* pszSave;

	// replace first unwanted string with NULL (save old value)
	pszSave = a[iLast+1];
	a[iLast+1] = NULL;

	// use built-in function for joining (returns a newly allocated string)
	gchar* pszReturn = g_strjoinv(separator, &a[iFirst]);

	// restore old value
	a[iLast+1] = pszSave;
	return pszReturn;
}

gchar** util_split_words_onto_two_lines(const gchar* pszText, gint nMinLineLength, gint nMaxLineLength)
{
	g_assert_not_reached();	// untested

#define MAX_WORDS_WE_CAN_HANDLE (6)

	// NOTE: 'nMinLineLength' and 'nMaxLineLength' are loosely enforced

	//
	// Split sentence into words, count words, measure words
	//
	gchar** aWords = g_strsplit(pszText, " ", 0);	// " " = delimeters, 0 = no max #
	gint nWordCount = g_strv_length(aWords);

	gint* aWordLengths = g_new0(gint, nWordCount);
	gint i;
	for(i=0 ; i<nWordCount ; i++) {
		aWordLengths[i] = strlen(aWords[i]);
	}

	gint nTotalLength = strlen(pszText);

	//
	// Assemble into sentences
	//
	gchar** aLines = g_new0(gchar*, 2);

	if(nTotalLength <= nMaxLineLength || (nWordCount > MAX_WORDS_WE_CAN_HANDLE)) {
		// done!
		aLines[0] = g_strdup(pszText);
	}
	else {
		// We could probably write a nice algorithm to handle this generically...
		// ...but hardcoding it is easier to write and debug.

		switch(nWordCount) {
		case 1:
			aLines[0] = g_strdup(pszText);	// no other choice
			break;
		case 2:
			// Don't put little words by themselves on a line (yes, the result may be > nMaxCharsOnOneLine)
			if((aWordLengths[0] < nMinLineLength) || (aWordLengths[1] < nMinLineLength)) {
				aLines[0] = g_strdup(pszText);
			}
			else {
				// two lines
				aLines[0] = g_strdup(aWords[0]);
				aLines[1] = g_strdup(aWords[1]);
			}
			break;
		case 3:
			// Definitely two lines.  Put the middle word with the shorter of the first|second.
			if(aWordLengths[0] > aWordLengths[2]) {
				// 1 word on first line, 2 on second
				aLines[0] = g_strdup(aWords[0]);
				aLines[1] = g_strjoinv_limit(" ", aWords, 1, 2);
			}
			else {
				// 2 words on first line, 1 on second
				aLines[0] = g_strjoinv_limit(" ", aWords, 0, 1);
				aLines[1] = g_strdup(aWords[2]);
			}
			break;
		case 4:
			if((aWordLengths[0] + aWordLengths[1] + aWordLengths[2]) < aWordLengths[3]) {
				// 3 and 1
				aLines[0] = g_strjoinv_limit(" ", aWords, 0, 2);
				aLines[1] = g_strdup(aWords[3]);
			}
			else if(aWordLengths[0] > (aWordLengths[1] + aWordLengths[2] + aWordLengths[3])) {
				// 1 and 3
				aLines[0] = g_strdup(aWords[0]);
				aLines[1] = g_strjoinv_limit(" ", aWords, 1, 3);
			}
			else {
				// 2 and 2
				aLines[0] = g_strjoinv_limit(" ", aWords, 0, 1);
				aLines[1] = g_strjoinv_limit(" ", aWords, 2, 3);
			}
			break;
		case 5:
			if((aWordLengths[0]+aWordLengths[1]) > (aWordLengths[3]+aWordLengths[4])) {
				// 2 and 3
				aLines[0] = g_strjoinv_limit(" ", aWords, 0, 1);
				aLines[1] = g_strjoinv_limit(" ", aWords, 2, 4);
			}
			else {
				// 3 and 2
				aLines[0] = g_strjoinv_limit(" ", aWords, 0, 2);
				aLines[1] = g_strjoinv_limit(" ", aWords, 3, 4);
			}
			break;
		case 6:
			aLines[0] = g_strjoinv_limit(" ", aWords, 0, 2);
			aLines[1] = g_strjoinv_limit(" ", aWords, 3, 5);
			break;
		default:
			g_assert_not_reached();
			break;
		}
	}

	g_strfreev(aWords);
	g_free(aWordLengths);

	return aLines;
}

#if(!GLIB_CHECK_VERSION(2,6,0))
// if glib < 2.6 we need to provide this function ourselves
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
void util_random_color(color_t* pColor)
{
	pColor->m_fRed = (random()%1000)/1000.0;
	pColor->m_fGreen = (random()%1000)/1000.0;
	pColor->m_fBlue = (random()%1000)/1000.0;
	pColor->m_fAlpha = 1.0;
}
#endif /* ROADSTER_DEAD_CODE */
