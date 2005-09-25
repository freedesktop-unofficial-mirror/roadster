/***************************************************************************
 *            glyph.c
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

#include <gtk/gtk.h>
#include <cairo.h>

#include "glyph.h"

struct {
	GPtrArray* pGlyphArray;	// to store all glyphs we hand out
} g_Glyph;

void glyph_init(void)
{
	g_Glyph.pGlyphArray = g_ptr_array_new();
}

#define MAX_GLYPH_FILE_NAME_LEN		(30)

gboolean glyph_is_safe_file_name(const gchar* pszName)
{
	// XXX: this is better done with a regex...
/*
	// NOTE: we want to be VERY strict here, as this can come from attackers
	g_assert(sizeof(gchar) == 1);

	// may contain only: a-z A-z 0-9 - _ and optionally a . followed by 3 or 4 characters
	gchar* p = pszName;
	gint nLen = 0;
	while(*p != '\0' && *p != '.') {
		if((g_ascii_is_alpha(*p) || g_ascii_is_digit(*p) || *p == '-' || *p == '_') == FALSE) {

		}
		p++;
		nLen++;
	}

	if(*p == '.') {
		nLen = 0;
		while(*p != '\0') {
			if((g_ascii_is_alpha(*p) || g_ascii_is_digit(*p) || *p == '-' || *p == '_') == FALSE) {
				return FALSE;
			}
			p++;
			nLen++;
		}
		if(nLen > 4) {
			// extension can't be longer than 4
			return FALSE;
		}
	}

     return (nLen < MAX_GLYPH_FILE_NAME_LEN);
*/
	return TRUE;
}

void _glyph_load_at_size_into_struct(glyph_t* pNewGlyph, const gchar* pszName, gint nMaxWidth, gint nMaxHeight)
{
	// pszName is an icon name without extension or path
	static const gchar* apszExtensions[] = {"svg", "png", "jpg", "gif"};	// in the order to try

	gchar* pszPath = "data/";	// XXX: just for development

	GdkPixbuf* pNewPixbuf = NULL;

	// Try base name with each supported extension
	gint iExtension;
	for(iExtension = 0 ; iExtension < G_N_ELEMENTS(apszExtensions) ; iExtension++) {
		gchar* pszFilePath = g_strdup_printf("%s%s.%s", pszPath, pszName, apszExtensions[iExtension]);
		pNewPixbuf = gdk_pixbuf_new_from_file_at_scale(pszFilePath, nMaxWidth, nMaxHeight, TRUE, NULL);	// NOTE: scales image to fit in this size
		g_free(pszFilePath);

		if(pNewPixbuf != NULL) break;	// got it!
	}

	// Create a fake pixbuf if not found
	if(pNewPixbuf == NULL) {
		pNewPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, nMaxWidth, nMaxHeight);
		gdk_pixbuf_fill(pNewPixbuf, 0xFF000080);
	}
	pNewGlyph->pPixbuf = pNewPixbuf;
	pNewGlyph->nWidth = gdk_pixbuf_get_width(pNewPixbuf);
	pNewGlyph->nHeight = gdk_pixbuf_get_height(pNewPixbuf);
}

glyph_t* glyph_load_at_size(const gchar* pszName, gint nMaxWidth, gint nMaxHeight)
{
	// NOTE: We always return something!
	glyph_t* pNewGlyph = g_new0(glyph_t, 1);

	pNewGlyph->pszName = g_strdup(pszName);
	pNewGlyph->nMaxWidth = nMaxWidth;
	pNewGlyph->nMaxHeight = nMaxHeight;

	// call internal function to fill the struct
	_glyph_load_at_size_into_struct(pNewGlyph, pszName, nMaxWidth, nMaxHeight);

	g_ptr_array_add(g_Glyph.pGlyphArray, pNewGlyph);

	return pNewGlyph;
}

GdkPixbuf* glyph_get_pixbuf(const glyph_t* pGlyph)
{
	g_assert(pGlyph != NULL);
	g_assert(pGlyph->pPixbuf != NULL);

	return pGlyph->pPixbuf;
}

void glyph_draw_centered(cairo_t* pCairo, gint nGlyphHandle, gdouble fX, gdouble fY)
{
//     GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size("/home/yella/Desktop/interstate-sign.svg", 32, 32, NULL);
//     gdk_draw_pixbuf(GTK_WIDGET(g_MainWindow.pDrawingArea)->window,
//                     GTK_WIDGET(g_MainWindow.pDrawingArea)->style->fg_gc[GTK_WIDGET_STATE(g_MainWindow.pDrawingArea)],
//                     pixbuf,
//                     0,0,                        // src
//                     100,100,                    // x/y to draw to
//                     32,32,                      // width/height
//                     GDK_RGB_DITHER_NONE,0,0);   // no dithering
}

void glyph_free(glyph_t* pGlyph)
{
	g_assert(pGlyph);
	gdk_pixbuf_unref(pGlyph->pPixbuf);
	g_free(pGlyph->pszName);
	g_free(pGlyph);
}

void glyph_deinit(void)
{
	gint i;
	for(i=0 ; i<g_Glyph.pGlyphArray->len ; i++) {
		glyph_free(g_ptr_array_index(g_Glyph.pGlyphArray, i));
	}
	g_ptr_array_free(g_Glyph.pGlyphArray, TRUE);
}

void glyph_reload_all(void)
{
	gint i;
	for(i=0 ; i<g_Glyph.pGlyphArray->len ; i++) {
		glyph_t* pGlyph = g_ptr_array_index(g_Glyph.pGlyphArray, i);

		gdk_pixbuf_unref(pGlyph->pPixbuf);	pGlyph->pPixbuf = NULL;
		// the rest of the fields remain.

		_glyph_load_at_size_into_struct(pGlyph, pGlyph->pszName, pGlyph->nMaxWidth, pGlyph->nMaxHeight);
	}
}

#ifdef ROADSTER_DEAD_CODE
/* libsvg is in gdk_pixbuf so we probably don't need this stuff

=== libsvg loading ===
	svg_cairo_t* pCairoSVG = NULL;

	if(SVG_CAIRO_STATUS_SUCCESS != svg_cairo_create(&pCairoSVG)) {
		g_warning("svg_cairo_create failed\n");
		return 0;
	}
	if(SVG_CAIRO_STATUS_SUCCESS != svg_cairo_parse(pCairoSVG, pszPath)) {
		g_warning("svg_cairo_parse failed on '%s'\n", pszPath);
		svg_cairo_destroy(pCairoSVG);
		return 0;
	}
	glyph_t* pNewGlyph = g_new0(glyph_t, 1);
	pNewGlyph->pCairoSVG = pCairoSVG;

	svg_cairo_get_size(pNewGlyph->pCairoSVG, &(pNewGlyph->nWidth), &(pNewGlyph->nHeight));

	// add it to array
	gint nGlyphHandle = g_Glyph.pGlyphArray->len;  // next available slot
	g_ptr_array_add(g_Glyph.pGlyphArray, pNewGlyph);

	return nGlyphHandle;

=== libsvg drawing ===

	if(nGlyphHandle == 0) return;

	glyph_t* pGlyph = NULL;
	if(!glyph_lookup(nGlyphHandle, &pGlyph)) {
		g_assert_not_reached();
	}

	cairo_save(pCairo);
		cairo_set_alpha(pCairo, 0.5);
		cairo_translate(pCairo, (fX - (pGlyph->nWidth/2)), (fY - (pGlyph->nHeight/2)));
		svg_cairo_render(pGlyph->pCairoSVG, pCairo);
	cairo_restore(pCairo);
*/
#endif
