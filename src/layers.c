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

#define EACH_ATTRIBUTE_OF_NODE(a,n)		(a) = (n)->properties ; (a) != NULL ; (a) = (a)->next
#define EACH_CHILD_OF_NODE(c,n)			(c) = (n)->children ; (c) != NULL ; (c) = (c)->next

GPtrArray* g_pLayersArray = NULL;	// XXX: globals suck. :(

gdouble pafDash_1_14[] = {5, 14, 5, 14};
gint8 panDash_1_14[] = {5, 14, 5, 14};

dashstyle_t g_aDashStyles[NUM_DASH_STYLES] = {
	{NULL, NULL, 0},
	{pafDash_1_14, panDash_1_14, 4},
};

static void layers_load_from_file();

static layer_t* layers_new_layer();

static void layers_parse_file(xmlDocPtr pDoc, xmlNodePtr pNode);
static void layers_parse_layer(xmlDocPtr pDoc, xmlNodePtr pNode);
static void layers_parse_layer_property(xmlDocPtr pDoc, layer_t *pLayer, xmlNodePtr pNode);

// Debugging
static void layers_print_layer(layer_t *layer);
static void layers_print_color(color_t *color);

void layers_init(void)
{
	/* init libxml */
	LIBXML_TEST_VERSION

	// Load
	g_pLayersArray = g_ptr_array_new();
	layers_load_from_file();
}

void layers_deinit(void)
{
	xmlCleanupParser();
}

void layers_reload(void)
{
	// XXX: do some cleanup here!
	g_warning("reloading styles leaks memory so... don't do it very often :)\n");
	g_pLayersArray = g_ptr_array_new();
	layers_load_from_file();
}

static layer_t* layers_new_layer()
{
	layer_t *pLayer = g_new0(layer_t, 1);

	gint i;
	for(i=0 ; i<NUM_ZOOM_LEVELS ; i++) {
		 layerstyle_t* pLayerStyle = g_new0(layerstyle_t, 1);

		 pLayerStyle->m_nCapStyle = MAP_CAP_STYLE_DEFAULT;

		 // XXX: need to init the layerstyle_t's?
		 pLayer->m_paStylesAtZoomLevels[i] = pLayerStyle;
	}

	return pLayer;
}

gboolean layers_parse_zoomlevel(const gchar* pszZoomLevel, gint* pnReturnMinZoomLevel, gint* pnReturnMaxZoomLevel)
{
	gchar* pszStr = g_strdup(pszZoomLevel);
	gboolean bReturn = TRUE;

	gint nMin,nMax;

	gchar* pszSeparator = g_strrstr(pszStr, "-");
	if(pszSeparator != NULL) {
		// parse a "5-8"
		*pszSeparator = '\0';
		nMin = atoi(pszStr);
		nMax = atoi(pszSeparator+1);
	}
	else {
		// parse a "5"
		nMin = nMax = atoi(pszStr);
	}

	if(nMin < MIN_ZOOM_LEVEL || nMin > MAX_ZOOM_LEVEL) {
		g_warning("zoom-level '%s' out of valid range (%d to %d)\n", pszZoomLevel, MIN_ZOOM_LEVEL, MAX_ZOOM_LEVEL);
		bReturn = FALSE;
	}
	else if(nMax < MIN_ZOOM_LEVEL || nMax > MAX_ZOOM_LEVEL) {
		g_warning("zoom-level '%s' out of valid range (%d to %d)\n", pszZoomLevel, MIN_ZOOM_LEVEL, MAX_ZOOM_LEVEL);
		bReturn = FALSE;
	}
	else {
		*pnReturnMinZoomLevel = nMin;
		*pnReturnMaxZoomLevel = nMax;
	}
	g_free(pszStr);
	return bReturn;
}

static void layers_load_from_file()
{
	xmlDocPtr pDoc = NULL;
	xmlNodePtr pRootElement = NULL;

	// Load style definition file
	// try source directory first (good for development)
	pDoc = xmlReadFile(PACKAGE_SOURCE_DIR"/data/layers.xml", NULL, 0);
	if(pDoc == NULL) {
		pDoc = xmlReadFile(PACKAGE_DATA_DIR"/data/layers.xml", NULL, 0);

		if(pDoc == NULL) {
			g_message("cannot load file layers.xml\n");
			gtk_exit(0);
		}
	}

	pRootElement = xmlDocGetRootElement(pDoc);

	layers_parse_file(pDoc, pRootElement);

	xmlFreeDoc(pDoc);
}


/*****************************************************************
 * layers_parse_* functions for parsing the xml
 *****************************************************************/
static void layers_parse_file(xmlDocPtr pDoc, xmlNodePtr pParentNode)
{
	xmlNodePtr pChildNode = NULL;

	// The one top-level "layers" guy
	if(pParentNode->type == XML_ELEMENT_NODE && strcmp(pParentNode->name, "layers") == 0) {
		// iterate over "layer" objects
		for(pChildNode = pParentNode->children; pChildNode != NULL; pChildNode = pChildNode->next) {
			if(pChildNode->type == XML_ELEMENT_NODE && strcmp(pChildNode->name, "layer") == 0) {
				layers_parse_layer(pDoc, pChildNode);
			}
		}
	}
}

static void
layers_parse_layer(xmlDocPtr pDoc, xmlNodePtr pNode)
{
	xmlAttrPtr pAttribute = NULL;
	gint i;

	// create new layer
	layer_t *pLayer = layers_new_layer();

	// read attributes of the 'layer' node
	for(EACH_ATTRIBUTE_OF_NODE(pAttribute, pNode)) {
		if(strcmp(pAttribute->name, "data-source") == 0) {
			gchar* pszDataSource = xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);

			if(!map_object_type_atoi(pszDataSource, &(pLayer->m_nDataSource))) {
				g_error("bad data source name %s\n", pszDataSource);
			}
		}
		else if(strcmp(pAttribute->name, "draw-type") == 0) {
			gchar* pszDrawType = xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);

			if(!map_layer_render_type_atoi(pszDrawType, &(pLayer->m_nDrawType))) {
				g_error("bad layer draw type name %s\n", pszDrawType);
			}
		}
	}

	// read children of the 'layer' node
	xmlNodePtr pChild = NULL;
	for(EACH_CHILD_OF_NODE(pChild, pNode)) {
		if(strcmp(pChild->name, "property") == 0) {
			layers_parse_layer_property(pDoc, pLayer, pChild);
		}
	}

	// add it to list
	g_ptr_array_add(g_pLayersArray, pLayer);

//	layers_print_layer(pLayer);
}

static void
layers_parse_layer_property(xmlDocPtr pDoc, layer_t *pLayer, xmlNodePtr pNode)
{
	gchar* pszName = NULL;
	gchar* pszValue = NULL;
	gchar* pszZoomLevel = NULL;

	// Read 'name', 'value', and 'level' attributes of this property
	xmlAttrPtr pAttribute = NULL;
	for(EACH_ATTRIBUTE_OF_NODE(pAttribute, pNode)) {
		if(strcmp(pAttribute->name, "name") == 0) {
			pszName = xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);
		}
		else if(strcmp(pAttribute->name, "value") == 0) {
			pszValue = xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);
		}
		else if((strcmp(pAttribute->name, "zoom-level") == 0) || (strcmp(pAttribute->name, "zoom-levels") == 0)) {
			pszZoomLevel = xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);
		}
	}

	gint nMinZoomLevel = MIN_ZOOM_LEVEL;
	gint nMaxZoomLevel = MAX_ZOOM_LEVEL;
	if(pszZoomLevel != NULL) {
		layers_parse_zoomlevel(pszZoomLevel, &nMinZoomLevel, &nMaxZoomLevel);
	}

	if(pszName != NULL && pszValue != NULL) {
		gint i;
		if(strcmp(pszName, "line-width") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->m_paStylesAtZoomLevels[i]->m_fLineWidth = (gdouble)atof(pszValue);
			}
		}
		else if(strcmp(pszName, "line-width") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->m_paStylesAtZoomLevels[i]->m_fLineWidth = (gdouble)atof(pszValue);
			}
		}
		else if(strcmp(pszName, "color") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				util_parse_hex_color(pszValue, &(pLayer->m_paStylesAtZoomLevels[i]->m_clrPrimary));
			}
		}
		else if(strcmp(pszName, "halo-size") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->m_paStylesAtZoomLevels[i]->m_fHaloSize = (gdouble)atof(pszValue);
			}
		}
		else if(strcmp(pszName, "dash-style") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->m_paStylesAtZoomLevels[i]->m_nDashStyle = (gint)atoi(pszValue);	// XXX: must validate #
			}
		}
		else if(strcmp(pszName, "bold") == 0) {
			gboolean bBold = (strcmp(pszValue, "yes") == 0);
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->m_paStylesAtZoomLevels[i]->m_bFontBold = bBold;
			}
		}
		else if(strcmp(pszName, "halo-color") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				util_parse_hex_color(pszValue, &(pLayer->m_paStylesAtZoomLevels[i]->m_clrHalo));
			}
		}
		else if(strcmp(pszName, "font-size") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->m_paStylesAtZoomLevels[i]->m_fFontSize = (gdouble)atof(pszValue);
			}
		}
//         else if(strcmp(pszName, "join-style") == 0) {
//             for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
//                 if(strcmp(pszValue, "mitre") == 0) {
//                     pLayer->m_paStylesAtZoomLevels[i]->m_nJoinStyle = CAIRO_LINE_JOIN_MITER;
//                 }
//                 else if(strcmp(pszValue, "round") == 0) {
//                     pLayer->m_paStylesAtZoomLevels[i]->m_nJoinStyle = CAIRO_LINE_JOIN_ROUND;
//                 }
//             }
//         }
		else if(strcmp(pszName, "line-cap") == 0) {
			if(strcmp(pszValue, "square") == 0) {
				for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
					pLayer->m_paStylesAtZoomLevels[i]->m_nCapStyle = MAP_CAP_STYLE_SQUARE;
				}
			}
			else {
				if(strcmp(pszValue, "round") != 0) { g_warning("bad value for line-cap found: '%s' (valid options are 'square' and 'round')\n", pszValue); }

				for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
					pLayer->m_paStylesAtZoomLevels[i]->m_nCapStyle = MAP_CAP_STYLE_ROUND;
				}
			}
		}
	}
}

/******************************************************************
 * layers_print_* functions for debugging
 *****************************************************************/
static void
layers_print_color(color_t* pColor)
{
	g_print("color: %3.2f, %3.2f, %3.2f, %3.2f\n", pColor->m_fRed, pColor->m_fGreen, pColor->m_fBlue, pColor->m_fAlpha);
}

/*
  	color_t m_clrPrimary;	// Color used for polygon fill or line stroke
	gdouble m_fLineWidth;

	gint m_nJoinStyle;
	gint m_nCapStyle;

	gint m_nDashStyle;

	// XXX: switch to this:
	//dashstyle_t m_pDashStyle;	// can be NULL

	// Used just for text
	gdouble m_fFontSize;
	gboolean m_bBold;
	gdouble m_fHaloSize;	// actually a stroke width
	color_t m_clrHalo;
*/

static void
layers_print_layer(layer_t *pLayer)
{
	int i;

	for(i = 0 ; i < NUM_ZOOM_LEVELS ; i++) {
		g_print("\nzoom level %d\n", i+1);
		layerstyle_t* pStyle = pLayer->m_paStylesAtZoomLevels[i];

		g_print("  line width: %f\n", pStyle->m_fLineWidth);
		g_print("  primary "); layers_print_color(&(pStyle->m_clrPrimary));
		g_print("  join style: %d\n", pStyle->m_nJoinStyle);
		g_print("  cap style: %d\n", pStyle->m_nCapStyle);
		g_print("  dash style: %d\n", pStyle->m_nDashStyle);
	}
}


