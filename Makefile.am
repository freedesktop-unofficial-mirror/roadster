SUBDIRS = src data pixmaps po

intltool_extra = intltool-extract.in intltool-merge.in intltool-update.in

EXTRA_DIST = $(intltool_extra)

DISTCLEANFILES = 		\
	intltool-extract 	\
	intltool-merge 		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install
