bin_PROGRAMS = call-forward
call_forward_SOURCES = call-forward.c
call_forward_LDADD = $(DEPS_LIBS) $(HILDON_LIBS)

AM_CPPFLAGS = $(DEPS_CFLAGS) $(HILDON_CFLAGS) -DPREFIX=\"@prefix@\" -D_GNU_SOURCE -Wall -DDBUS_API_SUBJECT_TO_CHANGE

desktopdir = $(datadir)/applications/hildon
desktop_in_files = call-forward.desktop.in

servicedir = $(datadir)/dbus-1/services
service_in_files = net.uk.cobb.call-forward.service

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
service_DATA = $(service_in_files:.service.in=.service)

EXTRA_DIST = $(service_DATA) \
             net.uk.cobb.call-forward.service.in \
             call-forward.desktop.in.in \
             intltool-extract.in \
             intltool-merge.in \
             intltool-update.in \
             po/call-forward.pot

DISTCLEANFILES = intltool-extract \
                 intltool-merge \
                 intltool-update \
                 call-forward.desktop

clean-local:
	rm -rf autom4te.cache
	rm -f *.d *~
