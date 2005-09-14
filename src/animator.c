/***************************************************************************
 *            animator.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "animator.h"

void animator_init()
{

}

animator_t* animator_new(EAnimationType eAnimationType, gdouble fAnimationTimeInSeconds)
{
	g_assert(fAnimationTimeInSeconds > 0.0);

	animator_t* pNew = g_new0(animator_t, 1);
	
	pNew->m_pTimer = g_timer_new();
	pNew->m_eAnimationType = eAnimationType;
	pNew->m_fAnimationTimeInSeconds = fAnimationTimeInSeconds;
	
	return pNew;
}

void animator_destroy(animator_t* pAnimator)
{
	if(pAnimator == NULL) return;	// allow NULL

	g_timer_destroy(pAnimator->m_pTimer);

	g_free(pAnimator);
}

gboolean animator_is_done(animator_t* pAnimator)
{
	g_assert(pAnimator != NULL);

	gdouble fElapsedSeconds = g_timer_elapsed(pAnimator->m_pTimer, NULL);
	return (fElapsedSeconds >= pAnimator->m_fAnimationTimeInSeconds);
}

gdouble animator_get_time_percent(animator_t* pAnimator)
{
	g_assert(pAnimator != NULL);

	gdouble fElapsedSeconds = g_timer_elapsed(pAnimator->m_pTimer, NULL);

	// Cap at 1.0
	if(fElapsedSeconds >= pAnimator->m_fAnimationTimeInSeconds) {
		return 1.0;
	}
	return (fElapsedSeconds / pAnimator->m_fAnimationTimeInSeconds);
}

// returns a floating point 0.0 to 1.0
gdouble animator_get_progress(animator_t* pAnimator)
{
	g_assert(pAnimator != NULL);

	gdouble fTimePercent = animator_get_time_percent(pAnimator);
 
	// The formula for a parabola is:  y = Ax^2 + Bx + C
	// x is an input percentage (of time) from 0.0 to 1.0
	// y is an output percetange (distance) from 0.0 to 1.0

	gdouble fReturn;
	switch(pAnimator->m_eAnimationType) {
	case ANIMATIONTYPE_SLIDE:
		// Slide - accelerate for the first half (standard upward facing parabola going from x=0 to x=0.5) 
		if(fTimePercent < 0.5) {
			// use parabola function y=2x^2 + 0x + 0
			fReturn = (2*fTimePercent*fTimePercent);
		}
		// Slide - decelerate for the last half (try graphing this with x instead of fTimePercent and y instead of fReturn) 
		else {
			// this meets up with the above equation at (0.5,0.5)
			fReturn = 1.0 - (2.0 * (1.0-fTimePercent) * (1.0-fTimePercent));
		}
		break;
	case ANIMATIONTYPE_FAST_THEN_SLIDE:
		// graph this to understand fully. :)
		fReturn = (1 - ((1-fTimePercent) * (1-fTimePercent) * (1-fTimePercent) * (1-fTimePercent)));
		// NOTE: the more (1-x)s we add, the more time it will spend "slowing down"
		break;
	default:
		g_assert_not_reached();
		break;
	}

	// Make 100% sure it's capped to 0.0 -> 1.0
	fReturn = MIN(fReturn, 1.0);
	fReturn = MAX(fReturn, 0.0);
	return fReturn;
}
