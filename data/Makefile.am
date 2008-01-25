@INTLTOOL_DESKTOP_RULE@

NULL =

gladedir = $(pkgdatadir)
glade_DATA = roadster.glade

DESKTOP_IN_FILES= roadster.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

styledir = $(pkgdatadir)
style_DATA = layers.xml search-result-type-city.svg search-result-type-road.svg

tigerdir = $(pkgdatadir)
tiger_DATA = tiger-states.txt tiger-counties.txt

install-data-local:

EXTRA_DIST =				\
	$(glade_DATA)			\
	$(style_DATA)			\
	$(tiger_DATA)			\
	$(DESKTOP_IN_FILES)		\
	$(NULL)

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(NULL)

