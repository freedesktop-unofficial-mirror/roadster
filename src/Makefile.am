## Process this file with automake to produce Makefile.in

NULL =

INCLUDES= \
	-DDATADIR=\"$(pkgdatadir)\" \
	-DGNOMELOCALEDIR=\"$(datadir)/locale\" \
	-DGNOMEICONDIR=\""$(datadir)/pixmaps"\" \
	-I. \
	$(GNOME_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(LIBSVG_CFLAGS) \
	$(MYSQL_CFLAGS) \
	$(GPSD_CFLAGS) \
	$(ROADSTER_DISABLE_DEPRECATED) \
	$(NULL)

bin_PROGRAMS = roadster

roadster_SOURCES = \
	main.c\
	db.c\
	downloadmanager.c\
	directionswindow.c\
	gui.c\
	mainwindow.c\
	gotowindow.c\
	map.c\
	mapinfowindow.c\
	map_draw_cairo.c\
	map_draw_gdk.c\
	map_history.c\
	map_hittest.c\
	map_math.c\
	map_style.c\
	map_tilemanager.c\
	import.c\
	import_tiger.c\
	importwindow.c\
	util.c\
	gpsclient.c\
	location.c\
	locationset.c\
	locationeditwindow.c\
	searchwindow.c\
	search_road.c\
	search_location.c\
	search_city.c\
	search_coordinate.c\
	search.c\
	scenemanager.c\
	glyph.c\
	road.c\
	animator.c\
	tooltipwindow.c\
	test_poly.c

roadster_LDADD = \
	$(GNOME_LIBS) \
	$(CAIRO_LIBS) \
	$(LIBSVG_LIBS) \
	$(MYSQL_LIBS) \
	$(GPSD_LIBS) \
	$(NULL)

