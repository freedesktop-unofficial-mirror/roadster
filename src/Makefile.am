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
	gui.c\
	mainwindow.c\
	gotowindow.c\
	map.c\
	layers.c\
	import.c\
	import_tiger.c\
	importwindow.c\
	util.c\
	datasetwindow.c\
	welcomewindow.c\
	gpsclient.c\
	location.c\
	locationset.c\
	searchwindow.c\
	search_road.c\
	search_location.c\
	search.c\
	scenemanager.c\
	databasewindow.c\
	point.c\
	pointstring.c\
	track.c\
	glyph.c\
	map_draw_cairo.c\
	map_draw_gdk.c\
	road.c\
	prefs.c\
	animator.c\
	gfreelist.c

roadster_LDADD = \
	$(GNOME_LIBS) \
	$(CAIRO_LIBS) \
	$(LIBSVG_LIBS) \
	$(MYSQL_LIBS) \
	$(GPSD_LIBS) \
	$(NULL)

