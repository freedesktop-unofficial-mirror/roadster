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

#include <gtk/gtk.h>

#include "animator.h"
#include "util.h"

void animator_init()
{

}

animator_t* animator_new(EAnimationType eAnimationType, gdouble fAnimationTimeInSeconds)
{
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

	// for now use a constant slide
	gdouble fReturn;
	 
	// constant movement
	//fReturn = fTimePercent;

	// The formula for a parabola is:  y = Ax^2 + Bx + C
	// Here, x is an input percentage (of time) from 0.0 to 1.0
	// ...and y is an output percetange (distance) from 0.0 to 1.0

	if(pAnimator->m_eAnimationType == ANIMATIONTYPE_SLIDE) {
		// Slide - accelerate for the first half (standard upward facing parabola going from x=0 to x=0.5) 
		if(fTimePercent < 0.5) {
			// use parabola function y=2x^2 + 0x + 0
			fReturn = (2*fTimePercent*fTimePercent);
		}
		// Slide - decelerate for the last half (try graphing this with x instead of fTimePercent and y instead of fReturn) 
		else {
			// this meets up with the above equation at (0.5,0.5)
			fReturn = 1.0 - (2.0 * (1.0 - fTimePercent) * (1.0 - fTimePercent));
		}
	}
	else if(pAnimator->m_eAnimationType == ANIMATIONTYPE_FAST_THEN_SLIDE) {
		if(fTimePercent < 0.5) {
			// go 80% of the distance in half the time
			fReturn = (fTimePercent / 0.5) * 0.8;
		}
		else {
			fReturn = 1.0 - (2.0 * (1.0 - fTimePercent) * (1.0 - fTimePercent));

			// compress it into last 10%
			fReturn = (fReturn * 0.2) + 0.8;
		}
	}
	else {
		g_assert_not_reached();
	}

	// Make 100% sure it's capped to 0.0 -> 1.0
	fReturn = min(fReturn, 1.0);
	fReturn = max(fReturn, 0.0);
	return fReturn;
}

/*
UINT CSequencer::SlideTo(UINT iFrame, ENTITY* pEntity, LONG nDestX, LONG nDestY, double fTripTime)
{
        RECT rcEntity;
        
        rcEntity = pEntity->rcDest;
        
        LONG nStartX = rcEntity.left;
        LONG nStartY = rcEntity.top;
        
        // We want trips to take about 'tripTime' seconds.
        //
	gdouble fDeltaX = nDestX - nStartX;
	gdouble fDeltaY = nDestY - nStartY;
	gdouble fDistance = (LONG)sqrt((fDeltaX * fDeltaX) + (fDeltaY * fDeltaY));
	fDistance = max( fDistance, 1.0 );

        LONG cFrames = (LONG)(m_uTargetFPS * fTripTime);

        // (avoid divide-by-zero problems)
        //
        dxy = max( dxy, 1 );
        cFrames = max( cFrames, 1 );
        
        // Graceful movement is solved by the parabola:
        //       y = (2*dxy*x*x)/(f*f) - (2*dxy*x)/(f) + (dxy)/(2)
        // where y = distance from starting point
        //   and x = frame number
        //
        // Find the coefficients (Ax^2 + Bx +C) of that formula.
        //
        double A =    ((double)(2 * dxy) / (cFrames * cFrames));
        double B = 0 -((double)(2 * dxy) / (cFrames));
	double C =    ((double)(    dxy) / (2));

	// Perform the first half of the movement (where we're accelerating)
	//
 
	LONG x, y;


	for (LONG iMove = cFrames/2; iMove < cFrames; iMove++)
    {
		double percentMove = ((A * iMove * iMove) + (B * iMove) + C) / dxy;

		x = nStartX + (LONG)( percentMove * dx );
		y = nStartY + (LONG)( percentMove * dy );

		MoveRectangle(&rcEntity, x,y);
		
		this->AddAction_MoveEntity(iFrame++, pEntity, rcEntity);
	}

    // Perform the second half of the movement (where we're decelerating)
    //
	for (iMove = 0; iMove < cFrames/2; iMove++)
    {
		double percentMove = 1.0 - (A * iMove * iMove + B * iMove + C) / dxy;

		x = nStartX + (LONG)( percentMove * dx );
		y = nStartY + (LONG)( percentMove * dy );

		MoveRectangle(&rcEntity, x,y);

		this->AddAction_MoveEntity(iFrame++, pEntity, rcEntity);
	}

	// Make sure they end up exactly where we told them to go
	if(x != nDestX || y != nDestY) {
		MoveRectangle(&rcEntity, x, y);

		this->AddAction_MoveEntity(iFrame, pEntity, rcEntity);
	}

	return iFrame;
}
*/
