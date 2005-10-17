/***************************************************************************
 *            glyph.h
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

#ifndef _GLYPH_H_
#define _GLYPH_H_

#include <gdk/gdk.h>
#include <cairo.h>

typedef struct {
	GdkPixbuf* pPixbuf;		// pixbuf is just a decoded image
	GdkPixmap* pPixmap;		// pixmap is converted to screen color depth, etc.
	gint nWidth;
	gint nHeight;
	gint nMaxWidth;
	gint nMaxHeight;
	gchar* pszName;
	gint nReferenceCount;
} glyph_t;

void glyph_init();
glyph_t* glyph_load_at_size(const gchar* pszName, gint nMaxWidth, gint nMaxHeight);
glyph_t* glyph_load(const gchar* pszName);

void glyph_reload_all(void);

GdkPixbuf* glyph_get_pixbuf(const glyph_t* pGlyph);
GdkPixmap* glyph_get_pixmap(glyph_t* pGlyph, GtkWidget* pTargetWidget);
//void glyph_draw_centered(cairo_t* pCairo, gint nGlyphHandle, gdouble fX, gdouble fY);
void glyph_draw_centered(glyph_t* pGlyph, GdkDrawable* pTargetDrawable, GdkGC* pGC, gdouble fX, gdouble fY);
void glyph_deinit(void);

#endif
