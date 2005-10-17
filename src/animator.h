/***************************************************************************
 *            animator.h
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

#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include <gtk/gtk.h>

typedef enum { ANIMATIONTYPE_NONE=0, ANIMATIONTYPE_SLIDE=1, ANIMATIONTYPE_FAST_THEN_SLIDE=2 } EAnimationType;

typedef struct animator {
        GTimer* pTimer;
        EAnimationType eAnimationType;
        gdouble fAnimationTimeInSeconds;
} animator_t;

animator_t* animator_new(EAnimationType eAnimationType, gdouble fAnimationTimeInSeconds);
gdouble animator_get_progress(animator_t* pAnimator);
gboolean animator_is_done(animator_t* pAnimator);
void animator_destroy(animator_t* pAnimator);

#endif
