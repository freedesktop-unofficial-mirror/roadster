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

#include <gnome.h>

#include <cairo.h>

#ifdef HAVE_LIBSVG
#include <svg-cairo.h>
#endif

typedef enum { GLYPHTYPE_NONE=0, GLYPHTYPE_BITMAP, GLYPHTYPE_VECTOR } EGlyphType;

typedef struct glyph {
	EGlyphType m_eType;
	
#ifdef HAVE_LIBSVG
	svg_cairo_t *m_pCairoSVG;
#endif

	gint m_nWidth;
	gint m_nHeight;
} glyph_t;

struct {
	GPtrArray* m_pGlyphArray;
} g_Glyph;

void glyph_init(void)
{
	g_Glyph.m_pGlyphArray = g_ptr_array_new();
	g_ptr_array_add(g_Glyph.m_pGlyphArray, NULL); // index 0 is taken! (it's the "no glyph" value)
}

gint glyph_load(const gchar* pszPath)
{
#ifdef HAVE_LIBSVG
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
	pNewGlyph->m_pCairoSVG = pCairoSVG;

	svg_cairo_get_size(pNewGlyph->m_pCairoSVG, &(pNewGlyph->m_nWidth), &(pNewGlyph->m_nHeight));

	// add it to array
	gint nGlyphHandle = g_Glyph.m_pGlyphArray->len;  // next available slot
	g_ptr_array_add(g_Glyph.m_pGlyphArray, pNewGlyph);

	return nGlyphHandle;
#else
	return 0;
#endif
}

static gboolean glyph_lookup(gint nGlyphHandle, glyph_t** ppReturnGlyph)
{
	g_assert(ppReturnGlyph != NULL);
	g_assert(*ppReturnGlyph == NULL);	// must be pointer to NULL pointer

	glyph_t* pGlyph = g_ptr_array_index(g_Glyph.m_pGlyphArray, nGlyphHandle);
	if(pGlyph != NULL) {
		*ppReturnGlyph = pGlyph;
		return TRUE;
	}
	return FALSE;
}

void glyph_draw_centered(cairo_t* pCairo, gint nGlyphHandle, gdouble fX, gdouble fY)
{
#ifdef HAVE_LIBSVG
	if(nGlyphHandle == 0) return;

	glyph_t* pGlyph = NULL;
	if(!glyph_lookup(nGlyphHandle, &pGlyph)) {
		g_assert_not_reached();
	}

	cairo_save(pCairo);
		cairo_set_alpha(pCairo, 0.5);
		cairo_translate(pCairo, (fX - (pGlyph->m_nWidth/2)), (fY - (pGlyph->m_nHeight/2)));
		svg_cairo_render(pGlyph->m_pCairoSVG, pCairo);
	cairo_restore(pCairo);
#else
	return;
#endif
}

void glyph_deinit(void)
{
#ifdef HAVE_LIBSVG
	// svg_cairo_destroy(svgc) all glyphs
#endif
}
