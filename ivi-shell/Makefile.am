moduledir = $(libdir)/weston

module_LTLIBRARIES = 				\
	$(libweston_layout)			\
	$(ivi_shell)				\
	$(hmi_controller)

AM_CPPFLAGS =					\
	-I$(top_srcdir)/shared			\
	-I$(top_srcdir)/src			\
	-I$(top_builddir)/src			\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DIN_WESTON

westonincludedir = $(includedir)/weston
westoninclude_HEADERS =

if ENABLE_IVI_SHELL
westoninclude_HEADERS +=			\
	ivi-application-client-protocol.h	\
	weston-layout.h				\
	ivi-shell.h

libweston_layout = libweston-layout.la
libweston_layout_la_LDFLAGS = -avoid-version
libweston_layout_la_LIBADD = $(IVI_SHELL_LIBS)
libweston_layout_la_CFLAGS = $(GCC_CFLAGS) $(IVI_SHELL_CFLAGS)
libweston_layout_la_SOURCES =			\
	weston-layout.c				\
	weston-layout.h

ivi_shell = ivi-shell.la
ivi_shell_la_LDFLAGS = -module -avoid-version
ivi_shell_la_LIBADD = $(COMPOSITOR_LIBS) $(IVI_SHELL_LIBS) $(libweston_layout)
ivi_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(IVI_SHELL_CFLAGS)
ivi_shell_la_SOURCES =				\
	ivi-shell.c				\
	ivi-shell.h				\
	weston-layout.h				\
	ivi-application-protocol.c		\
	ivi-application-server-protocol.h

hmi_controller = hmi-controller.la
hmi_controller_la_LDFLAGS = -module -avoid-version
hmi_controller_la_LIBADD = $(IVI_SHELL_LIBS) $(libweston_layout) ../shared/libshared-cairo.la
hmi_controller_la_CFLAGS = $(GCC_CFLAGS) $(IVI_SHELL_CFLAGS)
hmi_controller_la_SOURCES =			\
	hmi-controller.c			\
	hmi-controller.h			\
	hmi-controller-homescreen.c		\
	ivi-application-protocol.c		\
	ivi-application-client-protocol.h	\
	hmi-controller-protocol.c		\
	hmi-controller-client-protocol.h	\
	hmi-controller-server-protocol.h
endif

BUILT_SOURCES =					\
	ivi-application-protocol.c		\
	ivi-application-server-protocol.h	\
	ivi-application-client-protocol.h	\
	hmi-controller-protocol.c		\
	hmi-controller-client-protocol.h	\
	hmi-controller-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk


weston.ini : $(srcdir)/weston.ini.in
	$(AM_V_GEN)$(SED) \
		-e 's|@bindir[@]|$(bindir)|g' \
		-e 's|@abs_top_builddir[@]|$(abs_top_builddir)|g' \
		-e 's|@libexecdir[@]|$(libexecdir)|g' \
		$< > $@

all-local : weston.ini

CLEANFILES += weston.ini