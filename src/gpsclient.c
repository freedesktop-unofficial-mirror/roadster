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
 
#include <gtk/gtk.h>
#include <gps.h>	// gpslib
#include "gpsclient.h"

struct {
	struct gps_data_t * m_pGPSConnection;
	gpsdata_t* m_pPublicGPSData;
} g_GPSClient = {0};

gboolean gpsclient_callback_data_waiting(GIOChannel *source, GIOCondition condition, gpointer data);
//~ static void update_display(char *buf);

static void gpsclient_callback_update(char* p)
{
//	g_print("gpsclient_callback_update(%s)\n", p);
}
void gpsclient_debug_print(void);

static void gpsclient_connect(void)
{
	// don't do anything if already connected
	if(g_GPSClient.m_pPublicGPSData->m_eStatus != GPS_STATUS_NO_GPSD) return;	// already connected

	// make sure we clean up	
	if(g_GPSClient.m_pGPSConnection) {
		gps_close(g_GPSClient.m_pGPSConnection);
	}

	// connect
 	g_GPSClient.m_pGPSConnection = gps_open("localhost", DEFAULT_GPSD_PORT);
	g_GPSClient.m_pPublicGPSData->m_eStatus = GPS_STATUS_NO_GPSD;

	if(g_GPSClient.m_pGPSConnection) {
//		g_print("Connected to GPSD\n");
		
		// 
		gps_set_raw_hook(g_GPSClient.m_pGPSConnection, gpsclient_callback_update);
		
		// turn on streaming of GPS data
		gps_query(g_GPSClient.m_pGPSConnection, "w+x\n");
//		g_print("gps_query = %d\n", b);

//		g_print("g_GPSClient.m_pGPSConnection->gps_fd = %d\n", g_GPSClient.m_pGPSConnection->gps_fd);
		g_io_add_watch(g_io_channel_unix_new(g_GPSClient.m_pGPSConnection->gps_fd), 
			G_IO_IN, gpsclient_callback_data_waiting, NULL);

//		g_print("g_io_add_watch = %d\n", x);
	
		// assume no device
		g_GPSClient.m_pPublicGPSData->m_eStatus = GPS_STATUS_NO_DEVICE;
	}
	else {
//		g_print("FAILED to connect to GPSD\n");
	}	
}
void gpsclient_init()
{
	g_GPSClient.m_pPublicGPSData = g_new0(gpsdata_t, 1);
	
	gpsclient_connect();
}

// fill structure with data
gpsdata_t* gpsclient_getdata()
{
	gpsclient_connect();	// connect if necessary
	
	return g_GPSClient.m_pPublicGPSData;
}

// callback for g_io_add_watch on the GPSD file descriptor
gboolean gpsclient_callback_data_waiting(GIOChannel *source, GIOCondition condition, gpointer data)
{
//	g_print("gpsclient_callback_data_waiting\n");
	gpsdata_t* l = g_GPSClient.m_pPublicGPSData;	// our public data struct

	// is there data waiting on the socket?
	if(condition == G_IO_IN) {
		// read new data
		if(gps_poll(g_GPSClient.m_pGPSConnection) == -1) {
			l->m_eStatus = GPS_STATUS_NO_GPSD;
			g_print("gps_poll failed\n");
			return FALSE;
		}

		// parse new data
		struct gps_data_t* d = g_GPSClient.m_pGPSConnection;	// gpsd data

		// is a GPS device available?
		if(d->online) {
			// Do we have a satellite fix?
			if(d->status != STATUS_NO_FIX) {
				// a GPS device is present and working!
				l->m_eStatus = GPS_STATUS_LIVE;
				l->m_ptPosition.m_fLatitude = d->latitude;
				l->m_ptPosition.m_fLongitude= d->longitude;
				if(d->pdop < PDOP_EXCELLENT) {
					l->m_fSignalQuality = GPS_SIGNALQUALITY_5_EXCELLENT;
				}
				else if(d->pdop < PDOP_GOOD) {
					l->m_fSignalQuality = GPS_SIGNALQUALITY_4_GOOD;
				}
				else if(d->pdop < PDOP_FAIR) {
					l->m_fSignalQuality = GPS_SIGNALQUALITY_3_FAIR;
				}
				else if(d->pdop < PDOP_POOR) {
					l->m_fSignalQuality = GPS_SIGNALQUALITY_2_POOR;
				}
				else {
					l->m_fSignalQuality = GPS_SIGNALQUALITY_1_TERRIBLE;
				}

				//
				// Set speed
				//
				l->m_fSpeedInKilometersPerHour = (d->speed * KNOTS_TO_KPH);
				l->m_fSpeedInMilesPerHour = (d->speed * KNOTS_TO_MPH);

				// dampen noise
				if(l->m_fSignalQuality <= GPS_SIGNALQUALITY_2_POOR &&
					l->m_fSpeedInMilesPerHour <= 2.0)
				{
					g_print("low quality signal, setting speed to 0\n");
					l->m_fSpeedInMilesPerHour = 0.0;
					l->m_fSpeedInKilometersPerHour = 0.0;
				}
			}
			else {
				l->m_eStatus = GPS_STATUS_NO_SIGNAL;
				l->m_ptPosition.m_fLatitude = 0.0;
				l->m_ptPosition.m_fLongitude= 0.0;
			}
		}
		else {
			l->m_eStatus = GPS_STATUS_NO_DEVICE;
		}
	//	gpsclient_debug_print();
	//	g_print("GPS Status: %d\n", l->m_eStatus);
	}
	return TRUE; // keep socket notification coming
}

//~ static void update_display(char *buf)
//~ {
	//~ g_print("********************* what the hell does this do?\n");	
//~ }

void gpsclient_debug_print(void)
{
	struct gps_data_t* d = g_GPSClient.m_pGPSConnection;	// gpsd data

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
