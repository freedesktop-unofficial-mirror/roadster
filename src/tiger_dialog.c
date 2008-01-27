/*
 * tiger_dialog.c
 * Allows selection of TIGER files for downloading
 *
 * Copyright 2007 Jeff Garrett <jeff@jgarrett.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include "gui.h"
#include "import.h"
#include "tiger.h"
#include "tiger_dialog.h"


#define TIGER_BASE_URL "http://www2.census.gov/geo/tiger/tiger2006se/"


void tiger_dialog_menushow(gpointer user_data, GtkMenuItem *menuitem)
{
	gtk_dialog_run(GTK_DIALOG(user_data));
}

void tiger_dialog_comboinit(GtkComboBox *combobox, gpointer user_data)
{
	/* Called in response to the state combobox "realize" event.
	 * We have to make sure the model for the combobox is
	 * initialized to the list of states.
	 */

	GtkListStore *liststore = gtk_list_store_new(3,
	                                             G_TYPE_STRING,
	                                             G_TYPE_STRING,
	                                             G_TYPE_STRING);
	GSList *states = tiger_get_states();

	for (; states; states = g_slist_next(states))
	{
		struct tiger_state *st = g_slist_nth_data(states, 0);
		GtkTreeIter iter;

		gtk_list_store_append(liststore, &iter);
		gtk_list_store_set(liststore, &iter,
		                   0, st->name,
		                   1, st->fips_code,
		                   2, st->abbrev,
		                   -1);
	}
	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combobox));
	gtk_combo_box_set_model(combobox, GTK_TREE_MODEL(liststore));

	/* Set up the cell renderers */
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox),
	                           GTK_CELL_RENDERER(cell), TRUE);
	/* Column 0 of the list is the state name */
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combobox),
	                              GTK_CELL_RENDERER(cell), "text", 0);

	/* Select the first state.  This will also trigger a call
	 * to tiger_dialog_setstate() which will set up the county
	 * treeview's model.
	 */
	gtk_combo_box_set_active(combobox, 0);
}

void tiger_dialog_selectstate(gpointer user_data, GtkComboBox *combobox)
{
	GtkTreeModel *model = gtk_combo_box_get_model(combobox);
	gint state_idx = gtk_combo_box_get_active(combobox);

	GtkTreeView *county_view = GTK_TREE_VIEW(user_data);

	gchar *path = g_strdup_printf("%d", state_idx);
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		g_warning("ITER NOT SET!\n");


	GValue abbrev = {0};
	gtk_tree_model_get_value(model, &iter, 2, &abbrev);
	const gchar *state_abbrev = g_value_get_string(&abbrev);

	GSList *counties = tiger_get_counties(state_abbrev);

	GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(county_view));
	if (!store)
	{
		store = gtk_list_store_new(3, G_TYPE_STRING,
		                           G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(county_view, GTK_TREE_MODEL(store));

		GtkCellRenderer *cell = gtk_cell_renderer_text_new();
		gtk_tree_view_insert_column_with_attributes(county_view, 0, "County", cell, "text", 0, NULL);

		GtkTreeSelection *sel = gtk_tree_view_get_selection(county_view);
		gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
	}
	gtk_list_store_clear(store);
	for (; counties ; counties = g_slist_next(counties))
	{
		struct tiger_county *ct = g_slist_nth_data(counties, 0);
		GtkTreeIter iter;

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   0, ct->name,
		                   1, ct->fips_code,
		                   2, ct->state_abbrev,
		                   -1);
	}
	g_value_unset(&abbrev);
}

void tiger_dialog_import(gpointer user_data, gint response, GtkDialog *dialog)
{
	GtkTreeView *county_view = GTK_TREE_VIEW(user_data);

	gtk_widget_hide(GTK_WIDGET(dialog));
	if (response != GTK_RESPONSE_OK)
		return;

	/* Start the import */
	GtkTreeSelection *sel = gtk_tree_view_get_selection(county_view);

	GtkTreeModel *model;
	GList *paths = gtk_tree_selection_get_selected_rows(sel, &model);

	for (; paths; paths = g_list_next(paths))
	{
		GtkTreeIter iter;
		gtk_tree_model_get_iter(model, &iter, g_list_nth_data(paths, 0));

		GValue tmp = {0};

		gtk_tree_model_get_value(model, &iter, 1, &tmp);
		gchar *fips  = g_value_dup_string(&tmp);
		g_value_unset(&tmp);

		gtk_tree_model_get_value(model, &iter, 2, &tmp);
		gchar *abbrev = g_value_dup_string(&tmp);
		g_value_unset(&tmp);

		gchar *uri = g_strdup_printf(TIGER_BASE_URL "%s/TGR%s.ZIP", abbrev, fips);
		g_warning("Import from %s", uri);
		import_from_uri(uri);
	}

	g_list_foreach(paths, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(paths);
}
