## Process this file with automake to produce Makefile.in

## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

INCLUDES = \
	$(GNOME_CFLAGS)\
	 -I/usr/include/mysql

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = roadster

roadster_SOURCES = \
	main.c\
	db.c\
	gui.c\
	mainwindow.c\
	gotowindow.c\
	map.c\
	geometryset.c\
	layers.c\
	import.c\
	import_tiger.c\
	importwindow.c\
	util.c\
	datasetwindow.c\
	welcomewindow.c\
	gpsclient.c\
	locationset.c\
	searchwindow.c\
	search_road.c\
	search_location.c\
	search.c\
	scenemanager.c\
	databasewindow.c

roadster_LDFLAGS = \
	 -L/usr/lib/mysql

roadster_LDADD =  \
	$(GNOME_LIBS)\
	 -lcairo -lmysqlclient -lgps

