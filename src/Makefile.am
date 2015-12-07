MAINTAINERCLEANFILES = Makefile.in
MODULE = e-mod-tizen-wm-policy

# data files for the module
filesdir = $(libdir)/enlightenment/modules/$(MODULE)
files_DATA = e-module-policy.edj

pkgdir                 = $(libdir)/enlightenment/modules/$(MODULE)/$(MODULE_ARCH)
pkg_LTLIBRARIES        = module.la
module_la_SOURCES      = e_mod_config.c \
                         e_mod_main.c \
                         e_mod_main.h \
                         e_mod_softkey.c \
                         e_mod_visibility.c \
                         e_mod_rotation.c \
                         e_mod_rotation.h \
                         e_mod_utils.h \
                         e_mod_keyboard.c \
                         e_mod_keyboard.h \
                         e_mod_stack.c
if HAVE_WAYLAND_ONLY
module_la_SOURCES += e_mod_wl.c \
					 e_mod_wl.h \
					 e_mod_rotation_wl.c \
					 e_mod_rotation_wl.h \
					 tizen_policy_ext-protocol.c \
					 tizen_policy_ext-server-protocol.h \
					 e_mod_quickpanel.c \
					 e_mod_quickpanel.h \
					 e_mod_indicator.c \
					 e_mod_indicator.h \
					 e_mod_volume.c \
					 e_mod_volume.h \
					 e_mod_gesture.c \
					 e_mod_gesture.h
endif

module_la_LIBADD       =
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@ @TZSH_SERVER_CFLAGS@
module_la_LDFLAGS      = -module -avoid-version @ENLIGHTENMENT_LIBS@ @TZSH_SERVER_LIBS@
module_la_DEPENDENCIES = $(top_builddir)/config.h
