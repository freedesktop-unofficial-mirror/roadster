dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(roadster, 0.2.9)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS")

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CXX
AC_STDC_HEADERS
AC_PROG_LIBTOOL
IT_PROG_INTLTOOL(0.32)

dnl GNOME_DEBUG_CHECK

ALL_LINGUAS=""

CFLAGS="$CFLAGS -Wall"
LDFLAGS="$LDFLAGS -export-dynamic"

AM_GLIB_GNU_GETTEXT

PACKAGE_LOCALE_DIR="${PACKAGE_PREFIX}/${DATADIRNAME}/locale"
AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR,"$PACKAGE_LOCALE_DIR",
		[The package's locale path for gettext])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl ========= check for gnome libraries ========================================
PKG_CHECK_MODULES(GNOME, libgnomeui-2.0 gtk+-2.0 libglade-2.0 libxml-2.0,,)
AC_SUBST(GNOME_LIBS)
AC_SUBST(GNOME_CFLAGS)

dnl ========= check for cairo ==================================================
PKG_CHECK_MODULES(CAIRO, cairo >= 1.0.0)

AC_SUBST(CAIRO_LIBS)
AC_SUBST(CAIRO_CFLAGS)

dnl ========= check for libsvg =================================================
PKG_CHECK_MODULES(LIBSVG, libsvg libsvg-cairo,
	[AC_DEFINE([HAVE_LIBSVG], [1], [Have libsvg])],
	continue
)
AC_SUBST(LIBSVG_LIBS)
AC_SUBST(LIBSVG_CFLAGS)

dnl ========= deprecated options ===============================================
AC_ARG_ENABLE(deprecated,
AC_HELP_STRING([--disable-deprecated],
	       [Don't allow any deprecated GTK+/etc. features.]),
set_enable_deprecated="$enableval",[
if test -f $srcdir/autogen.sh; then
	is_cvs_version=true
	set_enable_deprecated=no
else
	set_enable_deprecated=yes
	fi
])
AC_MSG_CHECKING([whether to disable deprecated glib/gtk+/etc. features])
if test "$set_enable_deprecated" != "yes"; then
	AC_MSG_RESULT(yes)
	ROADSTER_DISABLE_DEPRECATED="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED"
else
	AC_MSG_RESULT(no)
	ROADSTER_DISABLE_DEPRECATED=""
fi
AC_SUBST(ROADSTER_DISABLE_DEPRECATED)

dnl ========= check for MySQL ==================================================
AC_ARG_WITH(mysql,
    [  --with-mysql=<path>     prefix of mysql installation.],
    [ MYSQL_PREFIX=$with_mysql ],
    [ MYSQL_PREFIX=/usr ])

AC_PATH_PROG(mysqlconfig,mysql_config,,[$MYSQL_PREFIX/bin:$PATH])
if test [ -z "$mysqlconfig" ]
then
    AC_MSG_ERROR([mysql_config executable not found: MySQL 4.1.0 or greater is required.])
else
    AC_MSG_CHECKING(MySQL libraries)
    MYSQL_VERSION=`${mysqlconfig} --version`

    MYSQL_LIBS=`${mysqlconfig} --libs`

    AC_MSG_RESULT($MYSQL_LIBS)
    AC_MSG_CHECKING(mysql includes)
    MYSQL_CFLAGS=`${mysqlconfig} --cflags`

    AX_COMPARE_VERSION("$MYSQL_VERSION", [ge], [4.1.0], 
        AC_MSG_RESULT("$MYSQL_CFLAGS"),
        AC_MSG_ERROR([MySQL version 4.1.0 or greater required.]))

    AC_SUBST(MYSQL_LIBS)
    AC_SUBST(MYSQL_CFLAGS)
fi

dnl ========= check for GPSD ===================================================
AC_ARG_WITH(gpsd,
    [  --with-gpsd=<path>      prefix of gpsd installation.],
    [ GPSD_PREFIX=$with_gpsd ],
    [ GPSD_PREFIX=/usr ])

CPPFLAGS="$CPPFLAGS -I${GPSD_PREFIX}/include"
AC_CHECK_HEADERS(gps.h, , AC_MSG_ERROR([GPSD not found. Use --with-gpsd=prefix to specify the location of GPSD.]))
AC_CHECK_HEADERS(gpsd_config.h)

AC_SUBST(GPSD_PREFIX)
GPSD_LIBS="-L${GPSD_PREFIX}/lib -lgps"
GPSD_CFLAGS="-I${GPSD_PREFIX}/include"
AC_SUBST(GPSD_LIBS)
AC_SUBST(GPSD_CFLAGS)

dnl ========= set data dir =====================================================
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}", [Directory of data files])
  else
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}", [Directory of data files])
  fi
else
  AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${datadir}/${PACKAGE}", [Directory of data files])
fi

dnl ========= set source dir ===================================================
PACKAGE_SOURCE_DIR=`cd $srcdir && pwd`
AC_DEFINE_UNQUOTED(PACKAGE_SOURCE_DIR, "$PACKAGE_SOURCE_DIR",
		[The source path])

dnl ========= output ===========================================================
AC_OUTPUT([
Makefile
data/Makefile
src/Makefile
pixmaps/Makefile
po/Makefile.in
data/roadster.desktop
])
