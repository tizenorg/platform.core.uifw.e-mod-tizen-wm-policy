MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I./rotation

MODULE = e-mod-tizen-wm-policy

# data files for the module
filesdir = $(libdir)/enlightenment/modules/$(MODULE)
files_DATA = e-module-policy.edj

pkgdir                 = $(libdir)/enlightenment/modules/$(MODULE)/$(MODULE_ARCH)
pkg_LTLIBRARIES        = module.la

WL_SRC   =
ROT_SRC  = rotation/e_mod_rotation.c \
           rotation/e_mod_rotation.h \
           rotation/e_mod_rotation_settings.c \
           rotation/e_mod_rotation_settings.h

if HAVE_AUTO_ROTATION
ROT_SRC  += rotation/e_mod_sensord.c \
            rotation/e_mod_sensord.h
endif

if HAVE_WAYLAND_ONLY
WL_SRC   += e_mod_wl.c \
            e_mod_wl.h \
            tizen_policy_ext-protocol.c \
            tizen_policy_ext-server-protocol.h \
            e_mod_quickpanel.c \
            e_mod_quickpanel.h \
            e_mod_indicator.c \
            e_mod_indicator.h \
            e_mod_volume.c \
            e_mod_volume.h \
            e_mod_lockscreen.c \
            e_mod_lockscreen.h \
            e_mod_gesture.c \
            e_mod_gesture.h

ROT_SRC  += rotation/e_mod_rotation_wl.c \
            rotation/e_mod_rotation_wl.h 
endif

module_la_SOURCES      = e_mod_config.c \
                         e_mod_main.c \
                         e_mod_main.h \
                         e_mod_softkey.c \
                         e_mod_visibility.c \
                         e_mod_utils.h \
                         e_mod_keyboard.c \
                         e_mod_keyboard.h \
                         e_mod_stack.c \
                         e_mod_private_data.h \
                         e_mod_transit.c \
                         e_mod_transit.h \
                         e_mod_transform_mode.c \
                         e_mod_transform_mode.h \
                         $(WL_SRC) \
                         $(ROT_SRC)

module_la_LIBADD       =
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@ @TZSH_SERVER_CFLAGS@ @TTRACE_CFLAGS@ @CYNARA_CFLAGS@
module_la_LDFLAGS      = -module -avoid-version @ENLIGHTENMENT_LIBS@ @TZSH_SERVER_LIBS@ @TTRACE_LIBS@ @CYNARA_LIBS@
module_la_DEPENDENCIES = $(top_builddir)/config.h

if HAVE_AUTO_ROTATION
module_la_CFLAGS       += @SENSORD_CFLAGS@
module_la_LDFLAGS      += @SENSORD_LIBS@
endif
