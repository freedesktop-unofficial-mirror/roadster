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

//
// Generic multi-purpose callbacks
//

void util_close_parent_window(GtkWidget* pWidget, gpointer data)
{
	// Good for close buttons of dialogs.
	gtk_widget_hide(gtk_widget_get_toplevel(pWidget));
}

//
// GNOME / environment related
//
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
	argv[1] = (gchar*)pszDangerousURI;
	argv[2] = NULL;

	if(!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &pError)) {
		// XXX: error message?  or make this function return boolean?
		g_error_free(pError);
	}
	g_free(argv);
}

//
// Misc
//
gchar** util_split_words_onto_two_lines(const gchar* pszText, gint nMinLineLength, gint nMaxLineLength)
{
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
				aLines[1] = util_g_strjoinv_limit(" ", aWords, 1, 2);
			}
			else {
				// 2 words on first line, 1 on second
				aLines[0] = util_g_strjoinv_limit(" ", aWords, 0, 1);
				aLines[1] = g_strdup(aWords[2]);
			}
			break;
		case 4:
			if((aWordLengths[0] + aWordLengths[1] + aWordLengths[2]) < aWordLengths[3]) {
				// 3 and 1
				aLines[0] = util_g_strjoinv_limit(" ", aWords, 0, 2);
				aLines[1] = g_strdup(aWords[3]);
			}
			else if(aWordLengths[0] > (aWordLengths[1] + aWordLengths[2] + aWordLengths[3])) {
				// 1 and 3
				aLines[0] = g_strdup(aWords[0]);
				aLines[1] = util_g_strjoinv_limit(" ", aWords, 1, 3);
			}
			else {
				// 2 and 2
				aLines[0] = util_g_strjoinv_limit(" ", aWords, 0, 1);
				aLines[1] = util_g_strjoinv_limit(" ", aWords, 2, 3);
			}
			break;
		case 5:
			if((aWordLengths[0]+aWordLengths[1]) > (aWordLengths[3]+aWordLengths[4])) {
				// 2 and 3
				aLines[0] = util_g_strjoinv_limit(" ", aWords, 0, 1);
				aLines[1] = util_g_strjoinv_limit(" ", aWords, 2, 4);
			}
			else {
				// 3 and 2
				aLines[0] = util_g_strjoinv_limit(" ", aWords, 0, 2);
				aLines[1] = util_g_strjoinv_limit(" ", aWords, 3, 4);
			}
			break;
		case 6:
			aLines[0] = util_g_strjoinv_limit(" ", aWords, 0, 2);
			aLines[1] = util_g_strjoinv_limit(" ", aWords, 3, 5);
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

gboolean util_parse_hex_color(const gchar* pszString, void* pvReturnColor)
{
	color_t* pReturnColor = (color_t*)pvReturnColor;

	const gchar *p = pszString;
	if (*p == '#') p++;

	if(strlen(p) != 8) {
		g_warning("bad color value found: %s\n", pszString);
		return FALSE;
	}

	// Read RGBA hex doubles (eg. "H8") into buffer and parse
	gchar azBuffer[3] = {0,0,0};

	azBuffer[0] = *p++; azBuffer[1] = *p++;
	pReturnColor->fRed = (gfloat)strtol(azBuffer, NULL, 16)/255.0;
	azBuffer[0] = *p++; azBuffer[1] = *p++;
	pReturnColor->fGreen = (gfloat)strtol(azBuffer, NULL, 16)/255.0;
	azBuffer[0] = *p++; azBuffer[1] = *p++;
	pReturnColor->fBlue = (gfloat)strtol(azBuffer, NULL, 16)/255.0;
	azBuffer[0] = *p++; azBuffer[1] = *p++;
	pReturnColor->fAlpha = (gfloat)strtol(azBuffer, NULL, 16)/255.0;
}

void util_random_color(void* p)
{
	color_t* pColor = (color_t*)p;

	pColor->fRed = (random()%1000)/1000.0;
	pColor->fGreen = (random()%1000)/1000.0;
	pColor->fBlue = (random()%1000)/1000.0;
	pColor->fAlpha = 1.0;
}

//
// GLib/GTK fill-ins
//

// Same as g_strjoinv but doesn't necessarily go to end of strings
gchar* util_g_strjoinv_limit(const gchar* separator, gchar** a, gint iFirst, gint iLast)
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

void util_gtk_widget_set_visible(GtkWidget* pWidget, gboolean bVisible)
{
	if(bVisible) {
		gtk_widget_show(pWidget);
	}
	else {
		gtk_widget_hide(pWidget);
	}
}

#if(!GLIB_CHECK_VERSION(2,6,0))

// This one 
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

gboolean util_match_word_in_sentence(gchar* pszWord, gchar* pszSentence)
{
	// First see if the search string is a prefix of the text...
	gint nWordLength = strlen(pszWord);
	if(strncmp(pszSentence, pszWord, nWordLength) == 0) return TRUE;

	// ...otherwise search inside the text, but only match to beginnings of words.
	// A little hack here: just search for " butter"	XXX: what about eg. "-butter"?
	gchar* pszWordWithSpace = g_strdup_printf(" %s", pszWord);
	gboolean bMatch = (strstr(pszSentence, pszWordWithSpace) != NULL);	// if it returns a pointer, we have a match
	g_free(pszWordWithSpace);

	return bMatch;
}

gboolean util_match_all_words_in_sentence(gchar* pszWords, gchar* pszSentence)
{
	// Split up search string into an array of word strings
	gchar** aWords = g_strsplit(pszWords, " ", 0);	// " " = delimeters, 0 = no max #

	// Make sure all words are in the sentence (order doesn't matter)
	gboolean bAllFound = TRUE;
	gint i;
	for(i = 0 ; aWords[i] != NULL ; i++) {
		if(!util_match_word_in_sentence(aWords[i], pszSentence)) {
			bAllFound = FALSE;
			break;
		}
	}
	g_strfreev(aWords);
	return bAllFound;
}

// This is a fairly slow function... perhaps too slow for huge lists?
gboolean util_treeview_match_all_words_callback(GtkTreeModel *pTreeModel, gint nColumn, const gchar *pszSearchText, GtkTreeIter* pIter, gpointer _unused)
{
	gboolean bMatch = FALSE;

	// Get the row text from the treeview
	gchar* pszRowText = NULL;
	gtk_tree_model_get(pTreeModel, pIter, nColumn, &pszRowText, -1);	// -1 because it's NULL terminated

	// Strip markup from tree view row
	gchar* pszRowTextClean = NULL;
	if(pango_parse_markup(pszRowText, -1, 0, NULL, &pszRowTextClean, NULL, NULL)) {
		// put both strings into lowercase
		gchar* pszRowTextCleanDown = g_utf8_casefold(pszRowTextClean, -1);	// -1 because it's NULL terminated
		gchar* pszSearchTextDown = g_utf8_casefold(pszSearchText, -1);

		bMatch = util_match_all_words_in_sentence(pszSearchTextDown, pszRowTextCleanDown);

		g_free(pszRowTextClean);
		g_free(pszRowTextCleanDown);
		g_free(pszSearchTextDown);
	}
	else {
		g_warning("pango_parse_markup failed on '%s'", pszRowText);
		// bMatch remains FALSE...
	}
	g_free(pszRowText);

	return (bMatch == FALSE);	// NOTE: we must return FALSE for matches... yeah, believe it.
}

gboolean util_gtk_tree_view_select_next(GtkTreeView* pTreeView)
{
	gboolean bReturn = FALSE;

	GtkTreeSelection* pSelection = gtk_tree_view_get_selection(pTreeView);
	GtkTreeModel* pModel = gtk_tree_view_get_model(pTreeView);
	GtkTreeIter iter;
	if(gtk_tree_selection_get_selected(pSelection, NULL, &iter)) {
		GtkTreePath* pPath = gtk_tree_model_get_path(pModel, &iter);

		gtk_tree_path_next(pPath);	// NOTE: this returns void for some reason...
		gtk_tree_selection_select_path(pSelection, pPath);
		bReturn = TRUE;

		// Make the new cell visible
		gtk_tree_view_scroll_to_cell(pTreeView, pPath, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free(pPath);
	}
	// if none selected, select the first one
	else if(gtk_tree_model_get_iter_first(pModel, &iter)) {
		gtk_tree_selection_select_iter(pSelection, &iter);
		bReturn = TRUE;
	}
	return bReturn;
}

gboolean util_gtk_tree_view_select_previous(GtkTreeView* pTreeView)
{
	gboolean bReturn = FALSE;

	GtkTreeSelection* pSelection = gtk_tree_view_get_selection(pTreeView);
	GtkTreeModel* pModel = gtk_tree_view_get_model(pTreeView);
	GtkTreeIter iter;
	if(gtk_tree_selection_get_selected(pSelection, &pModel, &iter)) {
		
		GtkTreePath* pPath = gtk_tree_model_get_path(pModel, &iter);

		if(gtk_tree_path_prev(pPath)) {
			gtk_tree_selection_select_path(pSelection, pPath);
			bReturn = TRUE;
		}
		// XXX: should we wrap to first?

		// Make the new cell visible
		gtk_tree_view_scroll_to_cell(pTreeView, pPath, NULL, FALSE, 0.0, 0.0);
		gtk_tree_path_free(pPath);
	}

	// XXX: we currently don't support starting at the end
	return bReturn;
}

gchar* util_str_replace_many(const gchar* pszSource, util_str_replace_t* aReplacements, gint nNumReplacements)
{
	GString* pStringBuffer = g_string_sized_new(250);	// initial length... just a guess.

	const gchar* pszSourceWalker = pszSource;
	while(*pszSourceWalker != '\0') {
		gboolean bFound = FALSE;
		gint i;
		for(i=0 ; i<nNumReplacements ; i++) {
			//g_print("comparing %s and %s\n", pszSourceWalker, aReplacements[i].pszOld);
			if(strncmp(pszSourceWalker, aReplacements[i].pszFind, strlen(aReplacements[i].pszFind)) == 0) {
				pStringBuffer = g_string_append(pStringBuffer, aReplacements[i].pszReplace);
				pszSourceWalker += strlen(aReplacements[i].pszFind);
				bFound = TRUE;
				break;
			}
		}

		if(bFound == FALSE) {
			pStringBuffer = g_string_append_c(pStringBuffer, *pszSourceWalker);
			pszSourceWalker++;
		}
		//g_print("pStringBuffer = %s (%d)\n", pStringBuffer->str, pStringBuffer->len);
	}

	gchar* pszReturn = pStringBuffer->str;
	g_string_free(pStringBuffer, FALSE);	// do NOT free the 'str' memory
	return pszReturn;
}

gboolean util_gtk_window_is_fullscreen(GtkWindow* pWindow)
{
    GdkWindow* pTopLevelGDKWindow = gdk_window_get_toplevel(GTK_WIDGET(pWindow)->window);
	g_return_if_fail(pTopLevelGDKWindow != NULL);

	GdkWindowState windowstate = gdk_window_get_state(pTopLevelGDKWindow);

	return((windowstate & GDK_WINDOW_STATE_FULLSCREEN) > 0);
}

gboolean util_gtk_window_set_fullscreen(GtkWindow* pWindow, gboolean bFullscreen)
{
	GdkWindow* pTopLevelGDKWindow = gdk_window_get_toplevel(GTK_WIDGET(pWindow)->window);
	g_return_if_fail(pTopLevelGDKWindow != NULL);
	if(bFullscreen) {
		gdk_window_fullscreen(pTopLevelGDKWindow);
	}
	else {
		gdk_window_unfullscreen(pTopLevelGDKWindow);
	}
}

//
// 
//
static void _util_gtk_entry_set_hint_text(GtkEntry* pEntry, const gchar* pszHint)
{
	gtk_widget_modify_text(GTK_WIDGET(pEntry), GTK_STATE_NORMAL, &(GTK_WIDGET(pEntry)->style->text_aa[GTK_WIDGET_STATE(pEntry)]));
	gtk_entry_set_text(pEntry, pszHint);
}

static void _util_gtk_entry_clear_hint_text(GtkEntry* pEntry)
{
	gtk_widget_modify_text(GTK_WIDGET(pEntry), GTK_STATE_NORMAL, NULL); 
	gtk_entry_set_text(pEntry, "");
}

static gboolean _util_gtk_entry_on_focus_in_event(GtkEntry* pEntry, GdkEventFocus* _unused, const gchar* pszHint)
{
	// If the box is showing pszHint, clear it
	const gchar* pszExistingText = gtk_entry_get_text(pEntry);
	if(strcmp(pszExistingText, pszHint) == 0) {
		_util_gtk_entry_clear_hint_text(pEntry);
	}
	return FALSE;	// always say we didn't handle it so the normal processing happens
}

static gboolean _util_gtk_entry_on_focus_out_event(GtkEntry* pEntry, GdkEventFocus* _unused, const gchar* pszHint)
{
	// If the box is empty, set the hint text
	const gchar* pszExistingText = gtk_entry_get_text(pEntry);
	if(strcmp(pszExistingText, "") == 0) {
		_util_gtk_entry_set_hint_text(pEntry, pszHint);
	}
	return FALSE;	// always say we didn't handle it so the normal processing happens
}

// The API
void util_gtk_entry_add_hint_text(GtkEntry* pEntry, const gchar* pszHint)
{
	g_signal_connect(G_OBJECT(pEntry), "focus-in-event", G_CALLBACK(_util_gtk_entry_on_focus_in_event), (gpointer)pszHint);
	g_signal_connect(G_OBJECT(pEntry), "focus-out-event", G_CALLBACK(_util_gtk_entry_on_focus_out_event), (gpointer)pszHint);

	// Init it
	_util_gtk_entry_on_focus_out_event(pEntry, NULL, pszHint);
}


