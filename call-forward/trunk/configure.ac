AC_INIT([call-forward], [0.1], [g+770@cobb.uk.net])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_INSTALL
AC_PROG_LIBTOOL

DEP_MODULES="gtk+-2.0 >= 2.4 glib-2.0 dbus-glib-1"

AH_TEMPLATE([GETTEXT_PACKAGE], [Package name for gettext])
GETTEXT_PACKAGE=$PACKAGE
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE")
AC_SUBST(GETTEXT_PACKAGE)
ALL_LINGUAS="id pt de fr zh_TW ro wa sv nl cs es sk sr ru hu zh_CN"

call_forward_localedir='${prefix}/${DATADIRNAME}/locale'
AC_SUBST(call_forward_localedir)

AM_GLIB_GNU_GETTEXT
AC_PROG_INTLTOOL([0.23])

PKG_CHECK_MODULES(HILDON, hildon-1,
	[
		AC_DEFINE(HILDON_VER, 2, [Version of hildon libraries])
		DEP_MODULES="$DEP_MODULES hildon-fm-2"
	],
	[
		AC_DEFINE(HILDON_VER, 0, [Version of hildon libraries])
		PKG_CHECK_MODULES(HILDON, hildon-lgpl hildon-libs)
		DEP_MODULES="$DEP_MODULES hildon-fm"
	])
PKG_CHECK_EXISTS(maemo-version,
	[
		VERSION=`pkg-config --modversion maemo-version`
		AC_MSG_NOTICE([Configuring for Maemo $VERSION])
		CFLAGS="$CFLAGS -DMAEMO_VERSION=\\\"$VERSION\\\""
		MAEMO_MAJOR=`echo $VERSION | cut -b1 -`
		CFLAGS="$CFLAGS -DMAEMO_VERSION_MAJOR=$MAEMO_MAJOR"
	],
	[
		AC_MSG_ERROR([maemo-version not found])
	])
DEP_MODULES="$DEP_MODULES libosso"
AM_CONDITIONAL(MAEMO5, test x$MAEMO_MAJOR = x5)

PKG_CHECK_MODULES(DEPS, $DEP_MODULES)

AC_REPLACE_FUNCS([g_date_set_time_t])

AC_CHECK_LIB(dbus-1, dbus_message_iter_get_basic, AC_DEFINE(HAVE_DBUS_MESSAGE_ITER_GET_BASIC, 1, [Define to 1 if you have the dbus_message_iter_get_basic() function.]))

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
])
AC_OUTPUT([
po/Makefile.in
po/Makefile
call-forward.desktop.in
net.uk.cobb.call_forward.service
])
