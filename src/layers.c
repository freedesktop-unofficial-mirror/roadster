/***************************************************************************
 *            layers.c
 *
 *  Copyright 2005
 *  Ian McIntosh <ian_mcintosh@linuxadvocate.org>
 *  Nathan Fredrickson <nathan@silverorange.com>
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

#include <config.h>

#include <cairo.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "main.h"
#include "layers.h"

layer_t * g_aLayers[NUM_LAYERS+1];

gdouble pafDash_Solid[] = {10};
gdouble pafDash_1_14[] = {1, 14};
gint8 panDash_1_14[] = {1, 14};

dashstyle_t g_aDashStyles[NUM_DASH_STYLES] = {
	{NULL, NULL, 0},
	{pafDash_1_14, panDash_1_14, 2},
};

static void layers_load_from_file();
static layer_t* layers_new(gint index, gchar *name);

static void layers_parse_layers(xmlDocPtr doc, xmlNodePtr node);
static void layers_parse_layer(xmlDocPtr doc, xmlNodePtr node);
static void layers_parse_sublayer(xmlDocPtr doc, sublayerstyle_t *sublayer, xmlNodePtr node);
static void layers_parse_sublayer_property(xmlDocPtr doc, sublayerstyle_t *sublayer, xmlNodePtr node);
static void layers_parse_label(xmlDocPtr doc, textlabelstyle_t *label_style, xmlNodePtr node);
static void layers_parse_label_property(xmlDocPtr doc, textlabelstyle_t *label_style, xmlNodePtr node);
static void layers_parse_color(color_t *color, gchar *value);

static void layers_print_layer(layer_t *layer);
static void layers_print_sublayer(sublayerstyle_t *sublayer);
static void layers_print_color(color_t *color);


void
layers_init(void)
{
	g_aLayers[LAYER_NONE] = layers_new(LAYER_NONE, "unused");
	g_aLayers[LAYER_MINORSTREET] = layers_new(LAYER_MINORSTREET, "minor-roads");
	g_aLayers[LAYER_MAJORSTREET] = layers_new(LAYER_MAJORSTREET, "major-roads");
	g_aLayers[LAYER_MINORHIGHWAY] = layers_new(LAYER_MINORHIGHWAY, "minor-highways");
	g_aLayers[LAYER_MINORHIGHWAY_RAMP] = layers_new(LAYER_MINORHIGHWAY_RAMP, "minor-highway-ramps");
	g_aLayers[LAYER_MAJORHIGHWAY] = layers_new(LAYER_MAJORHIGHWAY, "major-highways");
	g_aLayers[LAYER_MAJORHIGHWAY_RAMP] = layers_new(LAYER_MAJORHIGHWAY_RAMP, "major-highway-ramps");
	g_aLayers[LAYER_RAILROAD] = layers_new(LAYER_RAILROAD, "railroads");
	g_aLayers[LAYER_PARK] = layers_new(LAYER_PARK, "parks");
	g_aLayers[LAYER_RIVER] = layers_new(LAYER_RIVER, "rivers");
	g_aLayers[LAYER_LAKE] = layers_new(LAYER_LAKE, "lakes");
	g_aLayers[LAYER_MISC_AREA] = layers_new(LAYER_MISC_AREA, "misc-area");

	/* init libxml */
	LIBXML_TEST_VERSION

	layers_load_from_file();

}

void
layers_deinit(void)
{
	xmlCleanupParser();
}

void
layers_reload(void)
{
	layers_load_from_file();
}

static layer_t*
layers_new(gint index, gchar *name)
{
	layer_t *layer;

	layer = g_new0(layer_t, 1);
	layer->nLayerIndex = index;
	layer->m_pszName = name;

	return layer;
}

static void
layers_load_from_file()
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root_element = NULL;
	//int i;

	// Load style definition file
	// try source directory first (good for development)
	doc = xmlReadFile(PACKAGE_SOURCE_DIR"/data/layers.xml", NULL, 0);
	if (doc == NULL) {
		doc = xmlReadFile(PACKAGE_DATA_DIR"/data/layers.xml", NULL, 0);

		if (doc == NULL) {
			g_message("cannot load file layers.xml\n");
			gtk_exit(0);
		}
	}

	root_element = xmlDocGetRootElement(doc);

	layers_parse_layers(doc, root_element);

	/*
	for (i = 1; i <= MAX_ZOOMLEVEL; i++)
		layers_print_layer(g_aLayers[i]);
	*/

	xmlFreeDoc(doc);
}


/*****************************************************************
 * layers_parse_* functions for parsing the xml
 *****************************************************************/
static void
layers_parse_layers(xmlDocPtr doc, xmlNodePtr node)
{
	xmlNodePtr cur_node = NULL;

	if (node->type == XML_ELEMENT_NODE && strcmp(node->name, "roadster-layers") == 0) {
		for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name, "layer") == 0) {
				layers_parse_layer(doc, cur_node);
			}
		}
	}
}

static void
layers_parse_layer(xmlDocPtr doc, xmlNodePtr node)
{
	xmlNodePtr cur_node = NULL;
	xmlAttrPtr cur_attr = NULL;
	gchar *layer_name;
	layer_t *layer = NULL;
	gint i;

	for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
		if (!strcmp(cur_attr->name, "name")) {
			layer_name = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
			printf("parsing layer: %s\n", layer_name);
			for (i = 0; i <= NUM_LAYERS; i++) {
				if (!strcmp(g_aLayers[i]->m_pszName, layer_name))
					layer = g_aLayers[i];
			}
		}
	}

	if (layer) {
		i = 0;
		for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
			if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name, "sublayer") == 0) {
				if (i < 2)
					layers_parse_sublayer(doc, &(layer->m_Style.m_aSubLayers[i]), cur_node);
				i++;
			} else if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name, "label") == 0) {
				layers_parse_label(doc, &(layer->m_TextLabelStyle), cur_node);
			}
		}
	}
}

static void
layers_parse_sublayer(xmlDocPtr doc, sublayerstyle_t *sublayer, xmlNodePtr node)
{
	xmlNodePtr cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name, "property") == 0) {
			layers_parse_sublayer_property(doc, sublayer, cur_node);
		}
	}
}

static void
layers_parse_sublayer_property(xmlDocPtr doc, sublayerstyle_t *sublayer, xmlNodePtr node)
{
	xmlAttrPtr cur_attr = NULL;
	gchar *name = NULL;
	gchar *levelstr = NULL;
	gchar *value = NULL;

	for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
		if (!strcmp(cur_attr->name, "name"))
			name = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
		else if (!strcmp(cur_attr->name, "level"))
			levelstr = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
		else if (!strcmp(cur_attr->name, "value"))
			value = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
	}

	if (name && value) {
		gdouble width;
		gint i, level;

		level = (levelstr? atoi(levelstr): 0);

		if (!strcmp(name, "width")) {
			for (i = level-1; i < MAX_ZOOMLEVEL; i++)
				sublayer->m_afLineWidths[i] = (gdouble)atof(value);

		} else if (!strcmp(name, "color")) {
			layers_parse_color(&(sublayer->m_clrColor), value);

		} else if (!strcmp(name, "join-style")) {
			if (!strcmp(value, "mitre"))
				sublayer->m_nJoinStyle = CAIRO_LINE_JOIN_MITER;
			else if (!strcmp(value, "round"))
				sublayer->m_nJoinStyle = CAIRO_LINE_JOIN_ROUND;

		} else if (!strcmp(name, "cap-style")) {
			if (!strcmp(value, "butt"))
				sublayer->m_nCapStyle = CAIRO_LINE_CAP_BUTT;
			else if (!strcmp(value, "round"))
				sublayer->m_nCapStyle = CAIRO_LINE_CAP_ROUND;

		} else if (!strcmp(name, "dash-style")) {
			sublayer->m_nDashStyle = (gint)atoi(value);

		}
	}
}

static void
layers_parse_label(xmlDocPtr doc, textlabelstyle_t *label_style, xmlNodePtr node)
{
	xmlNodePtr cur_node = NULL;

	for (cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE && strcmp(cur_node->name, "property") == 0) {
			layers_parse_label_property(doc, label_style, cur_node);
		}
	}
}

static void
layers_parse_label_property(xmlDocPtr doc, textlabelstyle_t *label_style, xmlNodePtr node)
{
	xmlAttrPtr cur_attr = NULL;
	gchar *name = NULL;
	gchar *levelstr = NULL;
	gchar *value = NULL;

	for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
		if (!strcmp(cur_attr->name, "name"))
			name = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
		else if (!strcmp(cur_attr->name, "level"))
			levelstr = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
		else if (!strcmp(cur_attr->name, "value"))
			value = xmlNodeListGetString(doc, cur_attr->xmlChildrenNode, 1);
	}

	if (name && value) {
		gint i, level;

		level = (levelstr? atoi(levelstr): 0);

		if (!strcmp(name, "font-size")) {
			for (i = level-1; i < MAX_ZOOMLEVEL; i++)
				label_style->m_afFontSizeAtZoomLevel[i] = (gdouble)atof(value);
			
		} else if (!strcmp(name, "bold")) {
			for (i = level-1; i < MAX_ZOOMLEVEL; i++)
				label_style->m_abBoldAtZoomLevel[i] = (strcmp(value, "yes")? 0: 1);
			
		} else if (!strcmp(name, "halo")) {
			for (i = level-1; i < MAX_ZOOMLEVEL; i++)
				label_style->m_afHaloAtZoomLevel[i] = (gdouble)atof(value);
			
		} else if (!strcmp(name, "color")) {
			layers_parse_color(&(label_style->m_clrColor), value);

		}
	}
}

static void
layers_parse_color(color_t *color, gchar *value)
{
	gchar *ptr;

	ptr = value;
	if (*ptr == '#') ptr++;

	if (strlen(ptr) < 8) {
		g_warning("bad color value found: %s\n", value);
		return;
	}

	// Read RGBA hex doubles (eg. "H8") in reverse order
	ptr += 6;
	color->m_fAlpha = (gfloat)strtol(ptr, NULL, 16)/255.0;
	*ptr = '\0';
	ptr -= 2;
	color->m_fBlue = (gfloat)strtol(ptr, NULL, 16)/255.0;
	*ptr = '\0';
	ptr -= 2;
	color->m_fGreen = (gfloat)strtol(ptr, NULL, 16)/255.0;
	*ptr = '\0';
	ptr -= 2;
	color->m_fRed = (gfloat)strtol(ptr, NULL, 16)/255.0;
}


/******************************************************************
 * layers_print_* functions for debugging
 *****************************************************************/
static void
layers_print_layer(layer_t *layer)
{
	printf("---------------\nlayer: %s (%d)\n", layer->m_pszName, layer->nLayerIndex);
	layers_print_sublayer(&(layer->m_Style.m_aSubLayers[0]));
	layers_print_sublayer(&(layer->m_Style.m_aSubLayers[1]));
	//layers_print_labelstyle(&(layer->m_TextLabelStyle));
}

static void
layers_print_sublayer(sublayerstyle_t *sublayer)
{
	int i;

	printf("line widths: ");
	for(i = MIN_ZOOMLEVEL; i <= MAX_ZOOMLEVEL; i++) printf(" %2.2f", sublayer->m_afLineWidths[i]);
	printf("\n");

	layers_print_color(&(sublayer->m_clrColor));
	
	printf("join style: %d\n", sublayer->m_nJoinStyle);
	printf("cap style: %d\n", sublayer->m_nCapStyle);
	printf("dash style: %d\n", sublayer->m_nDashStyle);
}

static void
layers_print_color(color_t *color)
{
	printf("color: %3.2f, %3.2f, %3.2f, %3.2f\n", color->m_fRed, color->m_fGreen, color->m_fBlue, color->m_fAlpha);
}

