@INTLTOOL_DESKTOP_RULE@

NULL =

gladedir = $(pkgdatadir)
glade_DATA = 				\
	roadster.glade		\
	$(NULL)

DESKTOP_IN_FILES= roadster.desktop.in
DESKTOP_FILES= $(DESKTOP_IN_FILES:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILES)

install-data-local:

EXTRA_DIST =				\
	$(glade_DATA)			\
	$(DESKTOP_IN_FILES)		\
	$(NULL)

DISTCLEANFILES = \
	$(DESKTOP_FILES)	\
	$(NULL)

