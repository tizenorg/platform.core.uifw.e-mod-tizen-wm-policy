MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I./rotation \
              -I./services

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

SRVS_SRC = services/e_mod_quickpanel.c \
           services/e_mod_quickpanel.h \
           services/e_mod_lockscreen.h \
           services/e_mod_lockscreen.c \
           services/e_mod_transit.c    \
           services/e_mod_transit.h    \
           services/e_mod_gesture.c    \
           services/e_mod_gesture.h    \
           services/e_mod_region.h     \
           services/e_mod_region.c

if HAVE_AUTO_ROTATION
ROT_SRC  += rotation/e_mod_sensord.c \
            rotation/e_mod_sensord.h
endif

WL_SRC   += e_mod_wl.c \
            e_mod_wl.h \
            e_mod_wl_display.c \
            e_mod_wl_display.h \
            tizen_policy_ext-protocol.c \
            tizen_policy_ext-server-protocol.h

ROT_SRC  += rotation/e_mod_rotation_wl.c \
            rotation/e_mod_rotation_wl.h 

SRVS_SRC += services/e_mod_volume.c \
            services/e_mod_volume.h

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
                         e_mod_transform_mode.c \
                         e_mod_transform_mode.h \
                         e_mod_conformant.c \
                         e_mod_conformant.h \
                         $(WL_SRC) \
                         $(ROT_SRC) \
                         $(SRVS_SRC)

module_la_LIBADD       =
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@ @TZSH_SERVER_CFLAGS@ @TTRACE_CFLAGS@ @CYNARA_CFLAGS@
module_la_LDFLAGS      = -module -avoid-version @ENLIGHTENMENT_LIBS@ @TZSH_SERVER_LIBS@ @TTRACE_LIBS@ @CYNARA_LIBS@
module_la_DEPENDENCIES = $(top_builddir)/config.h

if HAVE_AUTO_ROTATION
module_la_CFLAGS       += @SENSORD_CFLAGS@
module_la_LDFLAGS      += @SENSORD_LIBS@
endif
