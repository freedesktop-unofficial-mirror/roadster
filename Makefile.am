## Process this file with automake to produce Makefile.in
## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

SUBDIRS = intl po macros include src pixmaps

roadsterdocdir = ${prefix}/doc/roadster
roadsterdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS\
	TODO\
	ABOUT-NLS

EXTRA_DIST = $(roadsterdoc_DATA)

gnomemenudir = $(prefix)/@NO_PREFIX_PACKAGE_MENU_DIR@/Applications
gnomemenu_DATA = roadster.desktop

roadster_glade_filedir = $(prefix)/@NO_PREFIX_PACKAGE_DATA_DIR@/@PACKAGE@/glade
roadster_glade_file_DATA = roadster.glade

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

