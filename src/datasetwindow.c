/***************************************************************************
 *            datasetwindow.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "../include/util.h"
#include "../include/mainwindow.h"

#define DATASETLIST_COLUMN_ID			(0)
#define DATASETLIST_COLUMN_NAME 		(1)
#define DATASETLIST_COLUMN_IMPORT_DATE 	(2)
#define DATASETLIST_NUM_COLUMNS 		(3)

struct {
	GtkWindow* m_pWindow;
	GtkTreeView* m_pDataSetListTreeView;
	GtkListStore* m_pDataSetListModel;
	GtkButton* m_pDeleteButton;
	GtkButton* m_pImportButton;
} g_DataSetWindow;

void datasetwindow_on_dataset_list_selection_changed(GtkTreeSelection *treeselection, gpointer user_data);

void datasetwindow_init(GladeXML* pGladeXML)
{
	/* Find all widgets */
	g_DataSetWindow.m_pWindow						= GTK_WINDOW(glade_xml_get_widget(pGladeXML, "datasetwindow")); g_return_if_fail(g_DataSetWindow.m_pWindow != NULL);
	g_DataSetWindow.m_pDeleteButton					= GTK_BUTTON(glade_xml_get_widget(pGladeXML, "datasetdeletebutton")); g_return_if_fail(g_DataSetWindow.m_pDeleteButton != NULL);
	g_DataSetWindow.m_pImportButton					= GTK_BUTTON(glade_xml_get_widget(pGladeXML, "datasetimportbutton")); g_return_if_fail(g_DataSetWindow.m_pImportButton != NULL);
	g_DataSetWindow.m_pDataSetListTreeView			= GTK_TREE_VIEW(glade_xml_get_widget(pGladeXML, "datasettreeview")); g_return_if_fail(g_DataSetWindow.m_pDataSetListTreeView != NULL);;

	/* when user hits X, just hide the window */
    g_signal_connect(G_OBJECT(g_DataSetWindow.m_pWindow), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
	
	/* create a model for the data set list (treeview) */
	g_DataSetWindow.m_pDataSetListModel = gtk_list_store_new(DATASETLIST_NUM_COLUMNS, 
		G_TYPE_INT,
		G_TYPE_STRING,
		G_TYPE_STRING);
	gtk_tree_view_set_model(g_DataSetWindow.m_pDataSetListTreeView, GTK_TREE_MODEL(g_DataSetWindow.m_pDataSetListModel));

	/* Configure tree view */
	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;

	/* add Set Name column (with a text renderer) */
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Name", pCellRenderer, "text", DATASETLIST_COLUMN_NAME, NULL);	
	gtk_tree_view_append_column(g_DataSetWindow.m_pDataSetListTreeView, pColumn);

	/* add Import Date column (with a text renderer) */
	pCellRenderer = gtk_cell_renderer_text_new();
	pColumn = gtk_tree_view_column_new_with_attributes("Import Date", pCellRenderer, "text", DATASETLIST_COLUMN_IMPORT_DATE, NULL);	
	gtk_tree_view_append_column(g_DataSetWindow.m_pDataSetListTreeView, pColumn);	

	/* add handler for when the selection changes */
	GtkTreeSelection* pTreeSelection = gtk_tree_view_get_selection(g_DataSetWindow.m_pDataSetListTreeView);
	g_signal_connect(G_OBJECT(pTreeSelection), "changed", (GtkSignalFunc)datasetwindow_on_dataset_list_selection_changed, NULL);

	/*
	** add some fake data (FIXME: remove this)
	*/
	struct { gint ID; gchar* name; gchar* date; } list[] = {
		{10, "TGR25025.ZIP", "Thu Jan 27 16:11:12 EST 2005"},
		{11, "TGR25027.ZIP", "Thu Jan 27 16:12:55 EST 2005"},
		{12, "TGR25029.ZIP", "Thu Jan 27 16:14:51 EST 2005"},
	};
	GtkTreeIter iter;
	int i;
	for(i=0 ; i<NUM_ELEMS(list) ;i++) {
		gtk_list_store_append(g_DataSetWindow.m_pDataSetListModel, &iter);

		gtk_list_store_set(g_DataSetWindow.m_pDataSetListModel, &iter, 
		DATASETLIST_COLUMN_ID, list[i].ID,
		DATASETLIST_COLUMN_NAME, list[i].name,
		DATASETLIST_COLUMN_IMPORT_DATE, list[i].date,
		-1);
	}
}

void datasetwindow_show()
{
	gtk_widget_show(GTK_WIDGET(g_DataSetWindow.m_pWindow));
	gtk_window_present(g_DataSetWindow.m_pWindow);
}

void datasetwindow_on_dataset_list_selection_changed(GtkTreeSelection *treeselection, gpointer user_data)
{
	/* Make "delete" button sensitive if >0 rows selected */
	gtk_widget_set_sensitive(GTK_WIDGET(g_DataSetWindow.m_pDeleteButton), gtk_tree_selection_count_selected_rows(treeselection) > 0);	
}

gboolean datasetwindow_confirm_delete(const gchar* pszName)
{
	/* Create confirmation dialog */
	GtkWidget* pDialog = gtk_message_dialog_new(g_DataSetWindow.m_pWindow,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_WARNING,	/* according to HIG 2.0 */
                                             GTK_BUTTONS_NONE, 		/* we'll add buttons later */
                                             "Delete all record from selected data set '%s'?\n\nThis action cannot be undone!", pszName);
	gtk_dialog_add_button(GTK_DIALOG(pDialog), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
	gtk_dialog_add_button(GTK_DIALOG(pDialog), "Delete", GTK_RESPONSE_ACCEPT);

	/* Show dialog and return user's yes/no choice */
	gint nResponse = gtk_dialog_run(GTK_DIALOG(pDialog));
	gtk_widget_destroy(pDialog);
	return(nResponse == GTK_RESPONSE_ACCEPT);
}

void datasetwindow_on_datasetdeletebutton_clicked(GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *pDataSetSelection;
	GtkTreeModel* pModel = GTK_TREE_MODEL(g_DataSetWindow.m_pDataSetListModel);
	pDataSetSelection = gtk_tree_view_get_selection(g_DataSetWindow.m_pDataSetListTreeView);

	gint nID;
	gchar* pszName;
	if(gtk_tree_selection_get_selected(pDataSetSelection, &pModel, &iter)) {
		gtk_tree_model_get(pModel, &iter,
						 DATASETLIST_COLUMN_ID, &nID,
						 DATASETLIST_COLUMN_NAME, &pszName,
						-1);
		
		if(datasetwindow_confirm_delete(pszName)) {
			
		}
	}
	else {
		g_print("none selected\n");
	}
}

void datasetwindow_on_datasetimportbutton_clicked(GtkWidget *widget, gpointer user_data)
{
	mainwindow_begin_import_geography_data();
}
