/***************************************************************************
 *            importwindow.c
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

#include <glade/glade.h>
#include <gtk/gtk.h>
#include "main.h"
#include "db.h"
#include "import.h"
#include "mainwindow.h"
#include "importwindow.h"
#include "util.h"
#include "gui.h"

struct {
	GtkWindow* pWindow;
	GtkProgressBar* pProgressBar;
	GtkButton* pOKButton;
//	GtkButton* pCancelButton;
	GtkTextView* pLogTextView;
	
	gint nCurrentFile;
	gint nTotalFiles;
} g_ImportWindow;

void importwindow_init(GladeXML* pGladeXML)
{
	GLADE_LINK_WIDGET(pGladeXML, g_ImportWindow.pWindow, GTK_WINDOW, "importwindow");
	GLADE_LINK_WIDGET(pGladeXML, g_ImportWindow.pProgressBar, GTK_PROGRESS_BAR, "importprogressbar");
	GLADE_LINK_WIDGET(pGladeXML, g_ImportWindow.pOKButton, GTK_BUTTON, "importokbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_ImportWindow.pLogTextView, GTK_TEXT_VIEW, "importlogtextview");
}

void importwindow_show(void)
{
	gtk_widget_show(GTK_WIDGET(g_ImportWindow.pWindow));
	gtk_window_present(g_ImportWindow.pWindow);
}

void importwindow_hide(void)
{
	gtk_widget_hide(GTK_WIDGET(g_ImportWindow.pWindow));
}

void importwindow_log_append(const gchar* pszFormat, ...)
{
	gchar azNewText[5000];

	va_list args;
	va_start(args, pszFormat);	
	g_vsnprintf(azNewText, 5000, pszFormat, args);
	va_end(args);

	GtkTextBuffer* pBuffer = gtk_text_view_get_buffer(g_ImportWindow.pLogTextView);
	gtk_text_buffer_insert_at_cursor(pBuffer, azNewText, -1);

	// Scroll to end of text
	GtkTextIter iter;
	gtk_text_buffer_get_end_iter(pBuffer, &iter);
	gtk_text_view_scroll_to_iter(g_ImportWindow.pLogTextView, &iter, 0.0, FALSE,0,0);
	
	GTK_PROCESS_MAINLOOP;
}

void importwindow_progress_pulse(void)
{
//	gtk_progress_bar_set_text(g_ImportWindow.pProgressBar, azBuffer);
	
	gtk_progress_bar_pulse(g_ImportWindow.pProgressBar);

	// ensure the UI gets updated
	GTK_PROCESS_MAINLOOP;
}

void importwindow_begin(GSList* pSelectedFileList)
{
	// empty progress buffer
	GtkTextBuffer* pBuffer = gtk_text_view_get_buffer(g_ImportWindow.pLogTextView);
	gtk_text_buffer_set_text(pBuffer, "", -1);
	
	gtk_widget_show(GTK_WIDGET(g_ImportWindow.pWindow));
	gtk_window_present(g_ImportWindow.pWindow);
	gtk_widget_set_sensitive(GTK_WIDGET(g_ImportWindow.pOKButton), FALSE);

	GTK_PROCESS_MAINLOOP;

	g_ImportWindow.nTotalFiles = g_slist_length(pSelectedFileList);
	g_ImportWindow.nCurrentFile = 1;

	g_print("Importing %d file(s)\n", g_ImportWindow.nTotalFiles);

	gint nTotalSuccess = 0;
	GSList* pFile = pSelectedFileList;
	while(pFile != NULL) {
		// do some work
		if(import_from_uri((gchar*)pFile->data)) {
			nTotalSuccess++;
		}

		// Move to next file
		pFile = pFile->next;
		g_ImportWindow.nCurrentFile++;
	}
	// nTotalSuccess / g_ImportWindow.nTotalFiles

	gtk_progress_bar_set_fraction(g_ImportWindow.pProgressBar, 1.0);
	gtk_widget_set_sensitive(GTK_WIDGET(g_ImportWindow.pOKButton), TRUE);

	if(nTotalSuccess == g_ImportWindow.nTotalFiles) {
		gtk_progress_bar_set_text(g_ImportWindow.pProgressBar, "Completed Successfully");
	}
	else {
		gtk_progress_bar_set_text(g_ImportWindow.pProgressBar, "Completed with Errors");
	}
	GTK_PROCESS_MAINLOOP;

	// redraw map to show any potential new data (?)

//	map_set_zoomlevel(7);
	mainwindow_draw_map(DRAWFLAG_ALL);
}

//~ void importwindow_on_okbutton_clicked(GtkWidget* pWidget, gpointer pdata)
//~ {
	//~ // set button insensitive
	//~ gtk_widget_set_sensitive(GTK_WIDGET(g_ImportWindow.pOKButton), FALSE);

	//~ // hide dialog
	//~ gtk_widget_hide(GTK_WIDGET(g_ImportWindow.pWindow));
//~ }
