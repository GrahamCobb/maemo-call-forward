bin_PROGRAMS = call-forward
call_forward_SOURCES = call-forward.c
call_forward_LDADD = $(DEPS_LIBS) $(HILDON_LIBS)

AM_CPPFLAGS = $(DEPS_CFLAGS) $(HILDON_CFLAGS) -DPREFIX=\"@prefix@\" \
	-D_GNU_SOURCE -Wall -DDBUS_API_SUBJECT_TO_CHANGE \
	-DPACKAGE_LOCALE_DIR=\"@call_forward_localedir@\"

desktopdir = $(datadir)/applications/hildon
desktop_in_files = call-forward.desktop.in

servicedir = $(datadir)/dbus-1/services
service_in_files = net.uk.cobb.call-forward.service

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
service_DATA = $(service_in_files:.service.in=.service)

iconsdir = $(datadir)/icons/hicolor

nobase_icons_DATA = scalable/hildon/call-forward.png

scalable/hildon/gpe-calendar.png: gpe-calendar-64x64.png
	-mkdir -p scalable/hildon
	cp $< $@

EXTRA_DIST = $(service_DATA) \
		$(nobase_icons_DATA) \
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
