/***************************************************************************
 *            locationeditwindow.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

#include "locationeditwindow.h"
#include "main.h"
#include "gui.h"
#include "util.h"

struct {
	GtkWindow* pWindow;

	GtkTreeView* pAttributeTreeView;
	GtkListStore* pAttributeListStore;

	GtkLabel* pCustomAttributesLabel;
	GtkEntry* pLocationNameEntry;
	GtkComboBox* pLocationSetComboBox;
	GtkTextView* pLocationAddressTextView;

	GtkExpander* pAttributeExpander;
	GtkButton* pAttributeAddButton;
	GtkButton* pAttributeRemoveButton;

	GtkListStore* pAttributeNameListStore;

	//
	gint nEditingLocationID;
	gboolean bModified;

} g_LocationEditWindow = {0};

#define ATTRIBUTELIST_COLUMN_NAME 	(0)
#define ATTRIBUTELIST_COLUMN_VALUE 	(1)
#define ATTRIBUTELIST_COLUMN_NAMELISTMODEL_COLUMN (2)

#define ATTRIBUTENAMELIST_COLUMN_NAME 	(0)

static void locationeditwindow_set_title();
//static void locationeditwindow_nameentry_insert_text(GtkEditable *pEditable, gchar *pszNewText, gint nNewTextLen, gint *pPosition, gpointer pUserData);
static void locationeditwindow_configure_attribute_list();
static void locationeditwindow_something_changed_callback(GtkEditable *_unused, gpointer __unused);

void locationeditwindow_show_for_new(gint nDefaultLocationSetID);
void locationeditwindow_show_for_edit(gint nLocationID);

gboolean locationeditwindow_set_expander_label(gpointer _unused);

// When an 'entry' is created to do in-place editing in a tree, we want to attach this "insert-text" hander to it.
#ifdef ROADSTER_DEAD_CODE
/*
static void callback_install_insert_text_callback_on_entry(GtkCellRenderer *pCellRenderer, GtkCellEditable *pEditable, const gchar *pszTreePath, gpointer pUserData)
{
	if(GTK_IS_ENTRY(pEditable)) {
		g_signal_connect(G_OBJECT(GTK_ENTRY(pEditable)), "insert-text", G_CALLBACK(pUserData), NULL);
	}
	else if(GTK_IS_COMBO_BOX_ENTRY(pEditable)) {
		g_signal_connect(G_OBJECT(GTK_COMBO_BOX_ENTRY(pEditable)), "insert-text", G_CALLBACK(pUserData), NULL);
	}
}
*/
#endif

// Save the results of in-place cell editing
static void callback_store_attribute_editing(GtkCellRendererText *pCell, const gchar *pszTreePath, const gchar *pszNewValue, gpointer *pUserData)
{
	gint nColumn = (gint)(pUserData);

	GtkTreeIter iter;
	GtkTreePath* pPath = gtk_tree_path_new_from_string(pszTreePath);
	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(g_LocationEditWindow.pAttributeListStore), &iter, pPath)) {
		gtk_list_store_set(g_LocationEditWindow.pAttributeListStore, &iter, nColumn, pszNewValue, -1);
	}
	gtk_tree_path_free(pPath);

	// data is now modified
	g_LocationEditWindow.bModified = TRUE;
	locationeditwindow_set_title();
}

void locationeditwindow_init(GladeXML* pGladeXML)
{
	// Widgets
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pWindow, GTK_WINDOW, "locationeditwindow");
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pAttributeTreeView, GTK_TREE_VIEW, "locationattributestreeview");
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pCustomAttributesLabel, GTK_LABEL, "customattributeslabel");
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pLocationNameEntry, GTK_ENTRY, "locationnameentry");
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pLocationSetComboBox, GTK_COMBO_BOX, "locationsetcombobox");
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pLocationAddressTextView, GTK_TEXT_VIEW, "locationaddresstextview");

	g_object_set(G_OBJECT(g_LocationEditWindow.pLocationSetComboBox), "has-frame", FALSE, NULL); 

	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pAttributeExpander, GTK_EXPANDER, "attributeexpander");
	gtk_expander_set_use_markup(g_LocationEditWindow.pAttributeExpander, TRUE);

	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pAttributeAddButton, GTK_BUTTON, "attributeaddbutton");
	GLADE_LINK_WIDGET(pGladeXML, g_LocationEditWindow.pAttributeRemoveButton, GTK_BUTTON, "attributeremovebutton");

	locationeditwindow_configure_attribute_list();

	// 
	g_signal_connect(G_OBJECT(g_LocationEditWindow.pLocationNameEntry), "changed", G_CALLBACK(locationeditwindow_something_changed_callback), NULL);
	g_signal_connect(G_OBJECT(g_LocationEditWindow.pLocationSetComboBox), "changed", G_CALLBACK(locationeditwindow_something_changed_callback), NULL);
	g_signal_connect(G_OBJECT(gtk_text_view_get_buffer(g_LocationEditWindow.pLocationAddressTextView)), "changed", G_CALLBACK(locationeditwindow_something_changed_callback), NULL);

//	g_signal_connect(G_OBJECT(g_LocationEditWindow.pLocationAddressTextView), "changed", G_CALLBACK(locationeditwindow_something_changed_callback), NULL);
//	locationeditwindow_show_for_new(1);		// XXX: debug

	// don't delete window on X, just hide it
	g_signal_connect(G_OBJECT(g_LocationEditWindow.pWindow), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
}

static void locationeditwindow_configure_attribute_list()
{
	GtkCellRenderer* pCellRenderer;
  	GtkTreeViewColumn* pColumn;
	GtkTreeIter iter;

	// set up the location attributes list
	g_LocationEditWindow.pAttributeListStore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(g_LocationEditWindow.pAttributeTreeView, GTK_TREE_MODEL(g_LocationEditWindow.pAttributeListStore));

	g_LocationEditWindow.pAttributeNameListStore = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_list_store_append (g_LocationEditWindow.pAttributeNameListStore, &iter);
		gtk_list_store_set (g_LocationEditWindow.pAttributeNameListStore, &iter, 0, "url", -1);
	gtk_list_store_append (g_LocationEditWindow.pAttributeNameListStore, &iter);
		gtk_list_store_set (g_LocationEditWindow.pAttributeNameListStore, &iter, 0, "phone", -1);

	// NEW COLUMN: Editable "name" column (combobox-entry control)
	pCellRenderer = gtk_cell_renderer_combo_new();
    g_object_set(G_OBJECT(pCellRenderer), 
				 "model", g_LocationEditWindow.pAttributeNameListStore,
				 "editable", TRUE,
				 NULL);
    g_object_set(G_OBJECT(pCellRenderer), 
				 "width-chars", 20,
				 NULL);

	// add signals
//     g_signal_connect(G_OBJECT(pCellRenderer),
//                      "editing-started", G_CALLBACK(callback_install_insert_text_callback_on_entry),
//                      locationeditwindow_nameentry_insert_text);
	g_signal_connect(G_OBJECT(pCellRenderer),
					 "edited", G_CALLBACK(callback_store_attribute_editing),
					 ATTRIBUTELIST_COLUMN_NAME);

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(g_LocationEditWindow.pAttributeTreeView), -1, "Field", pCellRenderer,
												"text", ATTRIBUTELIST_COLUMN_NAME,
												"text-column", ATTRIBUTELIST_COLUMN_NAMELISTMODEL_COLUMN,
												NULL);

	// NEW COLUMN: Editable "value" column (text input)
	pCellRenderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(pCellRenderer), 
				 "editable", TRUE,
				 NULL);

	// add signals
//     g_signal_connect(G_OBJECT(pCellRenderer),
//                      "editing-started", G_CALLBACK(callback_install_insert_text_callback_on_entry),
//                      locationeditwindow_nameentry_insert_text);
	g_signal_connect(G_OBJECT(pCellRenderer),
					 "edited", G_CALLBACK(callback_store_attribute_editing),
					 (gpointer)ATTRIBUTELIST_COLUMN_VALUE);
	
  	pColumn = gtk_tree_view_column_new_with_attributes("Value", pCellRenderer,
													   "text", ATTRIBUTELIST_COLUMN_VALUE,
													   NULL);
	gtk_tree_view_append_column(g_LocationEditWindow.pAttributeTreeView, pColumn);

	// Add test data
	gtk_list_store_append(g_LocationEditWindow.pAttributeListStore, &iter);
	gtk_list_store_set(g_LocationEditWindow.pAttributeListStore, &iter, 
			   ATTRIBUTELIST_COLUMN_NAME, "phone", ATTRIBUTELIST_COLUMN_VALUE, "(617) 555-3314",
			   ATTRIBUTELIST_COLUMN_NAMELISTMODEL_COLUMN, ATTRIBUTENAMELIST_COLUMN_NAME,
			   -1);
	gtk_list_store_append(g_LocationEditWindow.pAttributeListStore, &iter);
	gtk_list_store_set(g_LocationEditWindow.pAttributeListStore, &iter,
			   ATTRIBUTELIST_COLUMN_NAME, "url", ATTRIBUTELIST_COLUMN_VALUE, "http://www.fusionrestaurant.com",
			   ATTRIBUTELIST_COLUMN_NAMELISTMODEL_COLUMN, ATTRIBUTENAMELIST_COLUMN_NAME,
			   -1);
}

void locationeditwindow_hide(void)
{
	gtk_widget_hide(GTK_WIDGET(g_LocationEditWindow.pWindow));
}

void locationeditwindow_show_for_new(gint nDefaultLocationSetID)
{
	// Set controls to default values
	gtk_entry_set_text(g_LocationEditWindow.pLocationNameEntry, "");
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(g_LocationEditWindow.pLocationAddressTextView), "", -1);
	gtk_combo_box_set_active(g_LocationEditWindow.pLocationSetComboBox, nDefaultLocationSetID);
	gtk_list_store_clear(g_LocationEditWindow.pAttributeListStore);

	// Don't change user's previous setting here
	//gtk_expander_set_expanded(g_LocationEditWindow.pAttributeExpander, FALSE);

	g_LocationEditWindow.bModified = FALSE;
	locationeditwindow_set_title();
	locationeditwindow_set_expander_label(NULL);

	gtk_widget_grab_focus(GTK_WIDGET(g_LocationEditWindow.pLocationNameEntry));
	gtk_widget_show(GTK_WIDGET(g_LocationEditWindow.pWindow));
	gtk_window_present(g_LocationEditWindow.pWindow);
}

void locationeditwindow_show_for_edit(gint nLocationID)
{
	// void location_load_attributes(gint nLocationID, GPtrArray* pAttributeArray);
	g_LocationEditWindow.bModified = FALSE;
	locationeditwindow_set_title();

	locationeditwindow_set_expander_label(NULL);
}

gboolean locationeditwindow_set_expander_label(gpointer _unused)
{
	gint nNumLocationAttributes = 0; //g_LocationEditWindow.pAttributeNameListStore;	// XXX: use a real count

	// NOTE: Do not expand/close the expander-- keep the user's old setting
	gchar* pszExpanderLabel;
//	if(gtk_expander_get_expanded(g_LocationEditWindow.pAttributeExpander)) {
		pszExpanderLabel = g_strdup_printf("Custom Values <i>(%d)</i>", nNumLocationAttributes);
//	}
//	else {
//		pszExpanderLabel = g_strdup_printf("Show Custom Values <i>(%d)</i>", nNumLocationAttributes);
//	}
	gtk_expander_set_label(g_LocationEditWindow.pAttributeExpander, pszExpanderLabel);
	g_free(pszExpanderLabel);

	return FALSE;	// "don't call us again" -- we're unfortunately an idle-time callback
}

static void locationeditwindow_set_title()
{
	const gchar* pszNameValue = gtk_entry_get_text(g_LocationEditWindow.pLocationNameEntry);	// note: pointer to internal memory

	gchar* pszNewName;
	if(pszNameValue[0] == '\0') {
		pszNewName = g_strdup_printf("%sunnamed POI", (g_LocationEditWindow.bModified) ? "*" : "");
	}
	else {
		pszNewName = g_strdup_printf("%s%s", (g_LocationEditWindow.bModified) ? "*" : "", pszNameValue);
	}

	gtk_window_set_title(g_LocationEditWindow.pWindow, pszNewName);
	g_free(pszNewName);
}

// Widget Callbacks

static void locationeditwindow_something_changed_callback(GtkEditable *_unused, gpointer __unused)
{
	// NOTE: This callback is shared by several widgets
	g_LocationEditWindow.bModified = TRUE;
	locationeditwindow_set_title();
}

void locationeditwindow_on_attributeexpander_activate(GtkExpander *_unused, gpointer __unused)
{
	// HACK: doesn't work when called directly (gets wrong value from expander?) so call it later
	g_idle_add(locationeditwindow_set_expander_label, NULL);
}

// static void locationeditwindow_nameentry_insert_text(GtkEditable *pEditable, gchar *pszNewText, gint nNewTextLen, gint *pPosition, gpointer pUserData)
// {
//     g_print("nNewTextLen=%d\n", nNewTextLen);
//
//     int i;
//     gchar *pszReplacement = g_utf8_strdown(pszNewText, nNewTextLen);
//
//     g_signal_handlers_block_by_func(GTK_OBJECT(pEditable),
//                                     GTK_SIGNAL_FUNC(locationeditwindow_nameentry_insert_text),
//                                     pUserData);
//     gtk_editable_insert_text(pEditable, pszReplacement, nNewTextLen, pPosition);
//     g_signal_handlers_unblock_by_func(GTK_OBJECT(pEditable),
//                                       GTK_SIGNAL_FUNC(locationeditwindow_nameentry_insert_text),
//                                       pUserData);
//
//     gtk_signal_emit_stop_by_name(GTK_OBJECT(pEditable), "insert_text");
//
//     g_free(pszReplacement);
// }
