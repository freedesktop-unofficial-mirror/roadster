/*
 * tiger.c
 * TIGER helper functions
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

#include <glib.h>
#include "config.h"
#include "tiger.h"


#define TIGER_STATES_FILE	DATADIR "/tiger-states.txt"
#define TIGER_COUNTIES_FILE	DATADIR "/tiger-counties.txt"

static GSList *state_list;
static GHashTable *county_list;


GSList *tiger_get_states()
{
	gchar *contents, **lines, **iter, **tokens;
	struct tiger_state *st;
	gint *fips_code;

	if (state_list)
		return state_list;

	if (!g_file_get_contents(TIGER_STATES_FILE, &contents, NULL, NULL))
	{
		g_warning("Cannot read TIGER states file %s\n", TIGER_STATES_FILE);
		return NULL;
	}

	/* The data file of TIGER states is tab-delimited with fields
	 * for the state FIPS code, the abbreviation, and the state name.
	 */
	lines = g_strsplit(contents, "\n", 0);
	for (iter = lines; iter && **iter; iter++)
	{
		st = g_new0(struct tiger_state, 1);
		fips_code = g_new0(gint, 1);

		tokens = g_strsplit(*iter, "\t", 0);
		st->fips_code = g_strdup(tokens[0]);
		st->abbrev    = g_strdup(tokens[1]);
		st->name      = g_strdup(tokens[2]);

		state_list = g_slist_append(state_list, st);

		g_strfreev(tokens);
	}
	g_strfreev(lines);
	g_free(contents);

	return state_list;
}

GSList *tiger_get_counties(gchar *state_abbrev)
{
	gchar *contents, **lines, **iter, **tokens;

	if (county_list)
		return g_hash_table_lookup(county_list, state_abbrev);

	county_list = g_hash_table_new(g_str_hash, g_str_equal);
	if (!g_file_get_contents(TIGER_COUNTIES_FILE, &contents, NULL, NULL))
	{
		g_warning("Cannot read TIGER counties file %s\n", TIGER_COUNTIES_FILE);
		return NULL;
	}

	lines = g_strsplit(contents, "\n", 0);
	for (iter = lines; iter && **iter; iter++)
	{
		struct tiger_county *ct = g_new0(struct tiger_county, 1);

		tokens = g_strsplit(*iter, "\t", 0);
		ct->fips_code    = g_strdup(tokens[0]);
		ct->state_abbrev = g_strdup(tokens[1]);
		ct->name         = g_strdup(tokens[2]);

		GSList *list = g_hash_table_lookup(county_list, ct->state_abbrev);
		list = g_slist_append(list, ct);
		g_hash_table_insert(county_list, ct->state_abbrev, list);

		g_strfreev(tokens);
	}
	g_strfreev(lines);
	g_free(contents);

	return g_hash_table_lookup(county_list, state_abbrev);
}
