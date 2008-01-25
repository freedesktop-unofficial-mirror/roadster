/***************************************************************************
 *            gpsclient.c
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

/*
Purpose of gpsclient.c:
 - Provide a sanitized interface to GPSD.
*/

#define HAVE_GPSD		// XXX: this should come from configure.ac
#include "config.h"

#include <gtk/gtk.h>

#ifdef HAVE_GPSD_CONFIG_H
#include <gpsd_config.h>
#endif
#include <gps.h>
#include "main.h"
#include "gpsclient.h"

struct {
#ifdef HAVE_GPSD 
	struct gps_data_t * pGPSConnection;	// gps_data_t is from gpsd.h
#endif
	gpsdata_t* pPublicGPSData;			// our public struct (annoyingly similar name...)
} g_GPSClient = {0};

gboolean gpsclient_callback_data_waiting(GIOChannel *source, GIOCondition condition, gpointer data);
static void gpsclient_connect(void);

void gpsclient_init()
{
	g_GPSClient.pPublicGPSData = g_new0(gpsdata_t, 1);

#ifndef HAVE_GPSD
	// No libgpsd at compile time
	g_GPSClient.pPublicGPSData->eStatus = GPS_STATUS_NO_GPS_COMPILED_IN;
#endif
	gpsclient_connect();
}

static void gpsclient_connect(void)
{
#ifdef HAVE_GPSD 
	// don't do anything if already connected
	if(g_GPSClient.pPublicGPSData->eStatus != GPS_STATUS_NO_GPSD) return;	// already connected

	// make sure we clean up	
	if(g_GPSClient.pGPSConnection) {
		gps_close(g_GPSClient.pGPSConnection);
	}

//	g_print("Attempting connection to GPSD...\n");

	// connect
 	g_GPSClient.pGPSConnection = gps_open("localhost", DEFAULT_GPSD_PORT);

	if(g_GPSClient.pGPSConnection) {
		// turn on streaming of GPS data
		gps_query(g_GPSClient.pGPSConnection, "w+x\n");

		g_io_add_watch(g_io_channel_unix_new(g_GPSClient.pGPSConnection->gps_fd),
			G_IO_IN, gpsclient_callback_data_waiting, NULL);

		// assume no GPS device is present
		g_GPSClient.pPublicGPSData->eStatus = GPS_STATUS_NO_DEVICE;
	}
	else {
		g_GPSClient.pPublicGPSData->eStatus = GPS_STATUS_NO_GPSD;
	}
#endif
}

const gpsdata_t* gpsclient_getdata()
{
	gpsclient_connect();	// connect if necessary

	return g_GPSClient.pPublicGPSData;
}

// callback for g_io_add_watch on the GPSD file descriptor
gboolean gpsclient_callback_data_waiting(GIOChannel *_source_unused, GIOCondition eCondition, gpointer _data_unused)
{
#ifdef HAVE_GPSD
//	g_print("Data from GPSD...\n");
	g_assert(g_GPSClient.pGPSConnection != NULL);
	g_assert(g_GPSClient.pPublicGPSData != NULL);

	gpsdata_t* l = g_GPSClient.pPublicGPSData;	// our public data struct, for easy access

	// is there data waiting on the socket?
	if(eCondition == G_IO_IN) {
		// read new data
		if(gps_poll(g_GPSClient.pGPSConnection) == -1) {
			l->eStatus = GPS_STATUS_NO_GPSD;
			g_print("gps_poll failed\n");
			return FALSE;
		}

		// parse new data
		struct gps_data_t* d = g_GPSClient.pGPSConnection;	// gpsd data

		// is a GPS device available?
		if(d->online) {
			// Do we have a satellite fix?
			if(d->status != STATUS_NO_FIX) {
				// a GPS device is present and working!
				l->eStatus = GPS_STATUS_LIVE;
				l->ptPosition.fLatitude = d->fix.latitude;
				l->ptPosition.fLongitude= d->fix.longitude;
				if(d->pdop < PDOP_EXCELLENT) {
					l->fSignalQuality = GPS_SIGNALQUALITY_5_EXCELLENT;
				}
				else if(d->pdop < PDOP_GOOD) {
					l->fSignalQuality = GPS_SIGNALQUALITY_4_GOOD;
				}
				else if(d->pdop < PDOP_FAIR) {
					l->fSignalQuality = GPS_SIGNALQUALITY_3_FAIR;
				}
				else if(d->pdop < PDOP_POOR) {
					l->fSignalQuality = GPS_SIGNALQUALITY_2_POOR;
				}
				else {
					l->fSignalQuality = GPS_SIGNALQUALITY_1_TERRIBLE;
				}

				// Set speed
				l->fSpeedInKilometersPerHour = (d->fix.speed * KNOTS_TO_KPH);
				l->fSpeedInMilesPerHour = (d->fix.speed * KNOTS_TO_MPH);

				// Dampen Noise when not moving fast enough for trustworthy data
				if(l->fSignalQuality <= GPS_SIGNALQUALITY_2_POOR &&
					l->fSpeedInMilesPerHour <= 2.0)
				{
					l->fSpeedInMilesPerHour = 0.0;
					l->fSpeedInKilometersPerHour = 0.0;
				}
			}
			else {
				l->eStatus = GPS_STATUS_NO_SIGNAL;
				l->ptPosition.fLatitude = 0.0;
				l->ptPosition.fLongitude= 0.0;
			}
		}
		else {
			l->eStatus = GPS_STATUS_NO_DEVICE;
		}
	}
	else {
		//g_print("eCondition: %d\n", eCondition);
	}
	return TRUE; // TRUE = keep socket notification coming
#endif
}

#ifdef ROADSTER_DEAD_CODE
/*
static void gpsclient_debug_print(void)
{
	struct gps_data_t* d = g_GPSClient.pGPSConnection;	// gpsd data

	g_print("online = %d, ", d->online);
	g_print("latitude = %f, ", d->latitude);
	g_print("longitude = %f, ", d->longitude);
	g_print("speed = %f, ", d->speed);
	g_print("track = %f, ", d->track);
	g_print("status = %d, ", d->status);
	g_print("mode = %d, ", d->mode);
	g_print("satellites_used = %d, ", d->satellites_used);

	g_print("pdop = %f, ", d->pdop);
	g_print("hdop = %f, ", d->hdop);
	g_print("vdop = %f, ", d->vdop);
	
	g_print("satellites = %d, ", d->satellites);

	g_print("gps_fd = %d\n\n", d->gps_fd);
}
*/
#endif
