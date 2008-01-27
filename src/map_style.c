/***************************************************************************
 *  		map_style.c
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
#include <string.h>
#include "main.h"
#include "glyph.h"
#include "map_style.h"
#include "util.h"

#define MIN_STYLE_LEVEL (1)
#define MAX_STYLE_LEVEL	(10)

// utility macros for iterating through lists of XML things
#define EACH_ATTRIBUTE_OF_NODE(a,n)		(a) = (n)->properties ; (a) != NULL ; (a) = (a)->next
#define EACH_CHILD_OF_NODE(c,n)			(c) = (n)->children ; (c) != NULL ; (c) = (c)->next

static maplayer_t* map_style_new_layer();

static void map_style_load_from_file(map_t* pMap, const gchar* pszFileName);
static void map_style_parse_file(map_t* pMap, xmlDocPtr pDoc, xmlNodePtr pNode);
static void map_style_parse_layers(map_t* pMap, xmlDocPtr pDoc, xmlNodePtr pParentNode);
static void map_style_parse_layer(map_t* pMap, xmlDocPtr pDoc, xmlNodePtr pNode);
static void map_style_parse_layer_property(map_t* pMap, xmlDocPtr pDoc, maplayer_t *pLayer, xmlNodePtr pNode);

// Debugging
//static void map_style_print_layer(maplayer_t *layer);
//static void map_style_print_color(color_t *color);

//
// Functions
//
void map_style_init(void)
{
	/* init libxml */
	LIBXML_TEST_VERSION ;
}

void map_style_deinit(void)
{
	xmlCleanupParser();
}

void map_style_load(map_t* pMap, const gchar* pszFileName)
{
	g_assert(pMap != NULL);
	g_assert(pszFileName != NULL);
	
	if(pMap->pLayersArray != NULL) {
		g_warning("reloading styles currently leaks memory so... don't do it very often :)\n");
		pMap->pLayersArray = NULL;	// XXX: memory leak :)
	}

	pMap->pLayersArray = g_ptr_array_new();
	map_style_load_from_file(pMap, pszFileName);
}

static maplayer_t* map_style_new_layer()
{
	maplayer_t *pLayer = g_new0(maplayer_t, 1);

	gint i;
	for(i=0 ; i<NUM_ZOOM_LEVELS ; i++) {
		 maplayerstyle_t* pLayerStyle = g_new0(maplayerstyle_t, 1);

		 pLayerStyle->nCapStyle = MAP_CAP_STYLE_DEFAULT;

		 // XXX: need to init the maplayerstyle_t's?
		 pLayer->paStylesAtZoomLevels[i] = pLayerStyle;
	}
	return pLayer;
}

static gboolean map_style_parse_zoomlevel(const gchar* pszZoomLevel, gint* pnReturnMinZoomLevel, gint* pnReturnMaxZoomLevel)
{
	g_assert(pszZoomLevel != NULL);
	g_assert(pnReturnMinZoomLevel != NULL);
	g_assert(pnReturnMaxZoomLevel != NULL);

	gchar* pszStr = g_strdup(pszZoomLevel);
	gboolean bReturn = TRUE;

	// Support the following formats:
	// "4-5", "4"

	gint nMin, nMax;
	gchar* pszSeparator = g_strrstr(pszStr, "-");
	if(pszSeparator != NULL) {
		// Parse a "5-8"
		*pszSeparator = '\0';
		nMin = atoi(pszStr);
		nMax = atoi(pszSeparator+1);
	}
	else {
		// For the single value format, we return the same value for both min and max.
		nMin = nMax = atoi(pszStr);
	}

	if(nMin < MIN_STYLE_LEVEL || nMin > MAX_STYLE_LEVEL) {
		g_warning("zoom-level '%s' out of valid range (%d to %d)\n", pszZoomLevel, MIN_STYLE_LEVEL, MAX_STYLE_LEVEL);
		bReturn = FALSE;
	}
	else if(nMax < MIN_STYLE_LEVEL || nMax > MAX_STYLE_LEVEL) {
		g_warning("zoom-level '%s' out of valid range (%d to %d)\n", pszZoomLevel, MIN_STYLE_LEVEL, MAX_STYLE_LEVEL);
		bReturn = FALSE;
	}
	else {
		*pnReturnMinZoomLevel = nMin;
		*pnReturnMaxZoomLevel = nMax;
	}
	g_free(pszStr);
	return bReturn;
}

#define MAX_VALUES_IN_DASH_STYLE	(20)
#define MAX_DASH_LENGTH		(50)
#define MIN_DASH_LENGTH		(1)

static dashstyle_t* dashstyle_new(gdouble* pafValues, gint nValueCount)
{
	g_assert(pafValues != NULL);

	g_assert(nValueCount >= 2);

	dashstyle_t* pNewDashStyle = g_new0(dashstyle_t, 1);

	// The list of gdouble's can just be copied in
	pNewDashStyle->pafDashList = g_malloc(sizeof(gdouble) * nValueCount);
	memcpy(pNewDashStyle->pafDashList, pafValues, sizeof(gdouble) * nValueCount);

	// The list of int8's has to be converted
	pNewDashStyle->panDashList = g_malloc(sizeof(gint8) * nValueCount);
	gint i;
	for(i=0 ; i<nValueCount ; i++) {
		gint8 nVal = (gint)pafValues[i];
		
		// 
		nVal = MIN(nVal, MAX_DASH_LENGTH);
		nVal = MAX(nVal, MIN_DASH_LENGTH);

		pNewDashStyle->panDashList[i] = nVal;
	}

	pNewDashStyle->nDashCount = nValueCount;

	return pNewDashStyle;
}

static gboolean map_style_parse_dashstyle(const gchar* pszDashStyle, dashstyle_t** ppReturnDashStyle)
{
	g_assert(pszDashStyle);
	g_assert(ppReturnDashStyle);
	g_assert(*ppReturnDashStyle == NULL);		// require pointer to NULL pointer

	// Parse a string that looks like this: "3.5 5" etc.

	gdouble afValues[MAX_VALUES_IN_DASH_STYLE];
	gint nValueCount = 0;

	const gchar* pszWorker = pszDashStyle;
	while(*pszWorker != '\0' && nValueCount < MAX_VALUES_IN_DASH_STYLE) {
		// Read one value
		gdouble fNew = atof(pszWorker);		// NOTE: atof() stops after it hits a bad character (eg. a space)
		if(fNew != 0.0) {
			// Add to end of list
			afValues[nValueCount] = fNew;
			nValueCount++;

			// Skip to next whitespace
			while(*pszWorker != ' ' && *pszWorker != '\0') pszWorker++;		// XXX: use utf8 functions?
			// Now skip the whitespace!
			while(*pszWorker == ' ') pszWorker++;
		}
		else {
			// Done.
			break;
		}
	}

	if(nValueCount >= 2) {	// a dash of 1 doesn't make sense
		// Success.
		*ppReturnDashStyle = dashstyle_new(afValues, nValueCount);
		return TRUE;
	}
	return FALSE;
}

static gchar* get_attribute_value(const xmlDocPtr pDoc, const xmlAttrPtr pAttribute)
{
	g_assert(pDoc != NULL);
	g_assert(pAttribute != NULL);

	// allocate a new glib string for this value.  free xmllib string.
	gchar* pszTmp = (char *)xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);
	gchar* pszValue = g_strdup((pszTmp) ? pszTmp : "");
	xmlFree(pszTmp);
	return pszValue;
}

static void map_style_load_from_file(map_t* pMap, const gchar* pszFileName)
{
	g_assert(pMap != NULL);
	g_assert(pszFileName != NULL);

	xmlDocPtr pDoc = NULL;
	xmlNodePtr pRootElement = NULL;

	// Load style definition file
	// try source directory first (good for development)
	gchar* pszPath = g_strdup_printf(PACKAGE_SOURCE_DIR"/data/%s", pszFileName);
	pDoc = xmlReadFile(pszPath, NULL, 0);
	g_free(pszPath);

	// try alternate path
	if(pDoc == NULL) {
		pszPath = g_strdup_printf(PACKAGE_DATA_DIR"/data/%s", pszFileName);
		pDoc = xmlReadFile(pszPath, NULL, 0);
		g_free(pszPath);

	}

	// still NULL?
	if(pDoc == NULL) {
		g_message("cannot load map style file %s\n", pszFileName);
//			gtk_exit(0);
	}
	else {
		pRootElement = xmlDocGetRootElement(pDoc);

		map_style_parse_file(pMap, pDoc, pRootElement);

		xmlFreeDoc(pDoc);
	}
}


/*****************************************************************
 * map_style_parse_* functions for parsing the xml
 *****************************************************************/
static void map_style_parse_file(map_t* pMap, xmlDocPtr pDoc, xmlNodePtr pParentNode)
{
	g_assert(pMap != NULL);
	g_assert(pDoc != NULL);
	g_assert(pParentNode != NULL);

	// Top-level node.
	if(pParentNode->type == XML_ELEMENT_NODE) {
		xmlNodePtr pChildNode = NULL;
		for(EACH_CHILD_OF_NODE(pChildNode, pParentNode)) {
			if(strcmp((char *)pChildNode->name, "layers") == 0) {
				map_style_parse_layers(pMap, pDoc, pChildNode);
			}
		}
	}
}

static void map_style_parse_layers(map_t* pMap, xmlDocPtr pDoc, xmlNodePtr pParentNode)
{
	g_assert(pMap != NULL);
	g_assert(pDoc != NULL);
	g_assert(pParentNode != NULL);

//	g_print("map_style_parse_layers()\n");
	xmlNodePtr pChildNode = NULL;

	// iterate over "layer" objects
	for(EACH_CHILD_OF_NODE(pChildNode, pParentNode)) {
	//for(pChildNode = pParentNode->children; pChildNode != NULL; pChildNode = pChildNode->next) {
		if(pChildNode->type == XML_ELEMENT_NODE && strcmp((char *)pChildNode->name, "layer") == 0) {
			map_style_parse_layer(pMap, pDoc, pChildNode);
		}
	}
}

static void map_style_parse_layer(map_t* pMap, xmlDocPtr pDoc, xmlNodePtr pNode)
{
	g_assert(pMap != NULL);
	g_assert(pDoc != NULL);
	g_assert(pNode != NULL);

	xmlAttrPtr pAttribute = NULL;
	//gint i;

	// create new layer
	maplayer_t *pLayer = map_style_new_layer();

	// read attributes of the 'layer' node
	for(EACH_ATTRIBUTE_OF_NODE(pAttribute, pNode)) {
		if(strcmp((char *)pAttribute->name, "data-source") == 0) {
			gchar* pszDataSource = (char *)xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);

			if(!map_object_type_atoi(pszDataSource, &(pLayer->nDataSource))) {
				g_error("bad data source name %s\n", pszDataSource);
			}
		}
		else if(strcmp((char *)pAttribute->name, "draw-type") == 0) {
			gchar* pszDrawType = (char *)xmlNodeListGetString(pDoc, pAttribute->xmlChildrenNode, 1);

			if(!map_layer_render_type_atoi(pszDrawType, &(pLayer->nDrawType))) {
				g_error("bad layer draw type name %s\n", pszDrawType);
			}
		}
	}

	// read children of the 'layer' node
	xmlNodePtr pChild = NULL;
	for(EACH_CHILD_OF_NODE(pChild, pNode)) {
		if(strcmp((char *)pChild->name, "property") == 0) {
			map_style_parse_layer_property(pMap, pDoc, pLayer, pChild);
		}
	}

	// add it to list
	g_ptr_array_add(pMap->pLayersArray, pLayer);

//	map_style_print_layer(pLayer);
}

static void map_style_parse_layer_property(map_t* pMap, xmlDocPtr pDoc, maplayer_t *pLayer, xmlNodePtr pNode)
{
	g_assert(pMap != NULL);
	g_assert(pLayer != NULL);
	g_assert(pNode != NULL);

	gchar* pszName = NULL;
	gchar* pszValue = NULL;
	gchar* pszZoomLevel = NULL;

	// Read 'name', 'value', and 'level' attributes of this property
	xmlAttrPtr pAttribute = NULL;
	for(EACH_ATTRIBUTE_OF_NODE(pAttribute, pNode)) {
		if(strcmp((char *)pAttribute->name, "name") == 0) {
			g_free(pszName);
			pszName = get_attribute_value(pDoc, pAttribute);
		}
		else if(strcmp((char *)pAttribute->name, "value") == 0) {
			g_free(pszValue);
			pszValue = get_attribute_value(pDoc, pAttribute);
		}
		else if((strcmp((char *)pAttribute->name, "zoom-level") == 0) || (strcmp((char *)pAttribute->name, "zoom-levels") == 0)) {
			g_free(pszZoomLevel);
			pszZoomLevel = get_attribute_value(pDoc, pAttribute);
		}
	}

	gint nMinZoomLevel = MIN_STYLE_LEVEL;
	gint nMaxZoomLevel = MAX_STYLE_LEVEL;
	if(pszZoomLevel != NULL) {
		map_style_parse_zoomlevel(pszZoomLevel, &nMinZoomLevel, &nMaxZoomLevel);
	}

	if(pszName != NULL && pszValue != NULL) {
		gint i;
		if(strcmp(pszName, "line-width") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->fLineWidth = (gdouble)atof(pszValue);
			}
		}
		else if(strcmp(pszName, "line-width") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->fLineWidth = (gdouble)atof(pszValue);
			}
		}
		else if(strcmp(pszName, "pixel-offset-x") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->nPixelOffsetX = (gint)atoi(pszValue);
			}
		}
		else if(strcmp(pszName, "pixel-offset-y") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->nPixelOffsetY = (gint)atoi(pszValue);
			}
		}
		else if(strcmp(pszName, "color") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				util_parse_hex_color(pszValue, &(pLayer->paStylesAtZoomLevels[i]->clrPrimary));
			}
		}
		else if(strcmp(pszName, "halo-size") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->fHaloSize = (gdouble)atof(pszValue);
			}
		}
		else if(strcmp(pszName, "dash-pattern") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				// NOTE: unlike all the atoi() calls in the loops, we actually DO need to parse this multiple times because each
				// layer should have its own copy
				dashstyle_t* pNewDashStyle = NULL;
				if(map_style_parse_dashstyle(pszValue, &pNewDashStyle)) {
					pLayer->paStylesAtZoomLevels[i]->pDashStyle = pNewDashStyle;
				}
				else {
					g_warning("bad dash style '%s' (should look like \"5.0 3.5\"\n", pszValue);
					break;
				}
			}
		}
		else if(strcmp(pszName, "fill-image") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->pGlyphFill = glyph_load(pszValue);
			}
		}
		else if(strcmp(pszName, "bold") == 0) {
			gboolean bBold = (strcmp(pszValue, "yes") == 0);
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->bFontBold = bBold;
			}
		}
		else if(strcmp(pszName, "halo-color") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				util_parse_hex_color(pszValue, &(pLayer->paStylesAtZoomLevels[i]->clrHalo));
			}
		}
		else if(strcmp(pszName, "font-size") == 0) {
			for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
				pLayer->paStylesAtZoomLevels[i]->fFontSize = (gdouble)atof(pszValue);
			}
		}
//         else if(strcmp(pszName, "join-style") == 0) {
//             for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
//                 if(strcmp(pszValue, "mitre") == 0) {
//                     pLayer->paStylesAtZoomLevels[i]->nJoinStyle = CAIRO_LINE_JOIN_MITER;
//                 }
//                 else if(strcmp(pszValue, "round") == 0) {
//                     pLayer->paStylesAtZoomLevels[i]->nJoinStyle = CAIRO_LINE_JOIN_ROUND;
//                 }
//             }
//         }
		else if(strcmp(pszName, "line-cap") == 0) {
			if(strcmp(pszValue, "square") == 0) {
				for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
					pLayer->paStylesAtZoomLevels[i]->nCapStyle = MAP_CAP_STYLE_SQUARE;
				}
			}
			else {
				if(strcmp(pszValue, "round") != 0) { g_warning("bad value for line-cap found: '%s' (valid options are 'square' and 'round')\n", pszValue); }

				for(i = nMinZoomLevel - 1; i < nMaxZoomLevel ; i++) {
					pLayer->paStylesAtZoomLevels[i]->nCapStyle = MAP_CAP_STYLE_ROUND;
				}
			}
		}
	}
	g_free(pszName);
	g_free(pszValue);
	g_free(pszZoomLevel);
}

/******************************************************************
 * map_style_print_* functions for debugging
 *****************************************************************/
#if 0
static void map_style_print_color(color_t* pColor)
{
	g_assert(pColor != NULL);
	g_print("color: %3.2f, %3.2f, %3.2f, %3.2f\n", pColor->fRed, pColor->fGreen, pColor->fBlue, pColor->fAlpha);
}
#endif

/*
  	color_t clrPrimary;	// Color used for polygon fill or line stroke
	gdouble fLineWidth;

	gint nJoinStyle;
	gint nCapStyle;

	gint nDashStyle;

	// XXX: switch to this:
	//dashstyle_t pDashStyle;	// can be NULL

	// Used just for text
	gdouble fFontSize;
	gboolean bBold;
	gdouble fHaloSize;	// actually a stroke width
	color_t clrHalo;
*/

#if 0
static void map_style_print_layer(maplayer_t *pLayer)
{
	g_assert(pLayer != NULL);

	int i;
	for(i = 0 ; i < NUM_ZOOM_LEVELS ; i++) {
		g_print("\nzoom level %d\n", i+1);
		maplayerstyle_t* pStyle = pLayer->paStylesAtZoomLevels[i];

		g_print("  line width: %f\n", pStyle->fLineWidth);
		g_print("  primary "); map_style_print_color(&(pStyle->clrPrimary));
		g_print("  join style: %d\n", pStyle->nJoinStyle);
		g_print("  cap style: %d\n", pStyle->nCapStyle);
		//g_print("  dash style: %d\n", pStyle->nDashStyle);
	}
}
#endif
