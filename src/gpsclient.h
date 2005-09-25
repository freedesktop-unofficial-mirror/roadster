/***************************************************************************
 *            gpsclient.h
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

#ifndef _GPSCLIENT_H
#define _GPSCLIENT_H

G_BEGIN_DECLS

#include "map.h"

typedef enum {
	GPS_STATUS_NO_GPS_COMPILED_IN = -1,		// no gpsd present at compile time

	GPS_STATUS_NO_GPSD = 0,		// daemon is absent :(		(0 should be default state)
	GPS_STATUS_NO_DEVICE = 1,	// no physical GPS device :(
	GPS_STATUS_NO_SIGNAL = 2,	// can't get a signal :(
	GPS_STATUS_LIVE = 3,		// :) everything groovy
} EGPSClientStatus;

typedef enum {
	GPS_SIGNALQUALITY_1_TERRIBLE = 0,		// (0 should be default state)
	GPS_SIGNALQUALITY_2_POOR = 1,
	GPS_SIGNALQUALITY_3_FAIR = 2,
	GPS_SIGNALQUALITY_4_GOOD = 3,
	GPS_SIGNALQUALITY_5_EXCELLENT = 4,
} EGPSClientSignalQuality;

// PDOP (positional dilution of precision) values. PDOP combines HDOP and VDOP.
// lower is better.  these numbers are semi-arbitrary, and depend on the use
// (in our case, navigating a car, where a few feet here and there isn't important).
#define PDOP_EXCELLENT 	(2.0)
#define PDOP_GOOD		(3.0)
#define PDOP_FAIR		(4.0)
#define PDOP_POOR		(5.0)

// public gpsdata
typedef struct gpsdata {
	EGPSClientStatus eStatus;
	mappoint_t ptPosition;
	gdouble fSignalQuality;
	gdouble fSpeedInKilometersPerHour;
	gdouble fSpeedInMilesPerHour;
} gpsdata_t;

void gpsclient_init(void);
const gpsdata_t* gpsclient_getdata(void);

G_END_DECLS

#endif /* _GPSCLIENT_H */
