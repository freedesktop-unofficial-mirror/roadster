/***************************************************************************
 *            searchwindow.h
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
 
#ifndef _SEARCHWINDOW_H
#define _SEARCHWINDOW_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <glade/glade.h>
	
void searchwindow_init(GladeXML* pGladeXML);

void searchwindow_add_result(gint nRoadID, const gchar* pszText, mappoint_t* pPoint);

void searchwindow_clear_results();

#ifdef __cplusplus
}
#endif

#endif /* _SEARCHWINDOW_H */
