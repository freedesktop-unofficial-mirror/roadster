/***************************************************************************
 *            main.h
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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <gtk/gtk.h>

#define USE_GNOME_VFS		// comment this out to get a faster single-threaded compile (can't import, though)
#define USE_GFREELIST

#define MOUSE_BUTTON_LEFT				(1)		// These are X/GDK/GTK numbers, now with names.
#define MOUSE_BUTTON_RIGHT				(3)

//#define ENABLE_TIMING

typedef struct color {
	gfloat fRed;
	gfloat fGreen;
	gfloat fBlue;
	gfloat fAlpha;
} color_t;

#endif

