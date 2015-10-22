MAINTAINERCLEANFILES = Makefile.in
MODULE = e-mod-tizen-wm-policy

# data files for the module
filesdir = $(libdir)/enlightenment/modules/$(MODULE)
files_DATA = e-module-policy.edj \
             e-module-policy-sysinfo.edj

pkgdir                 = $(libdir)/enlightenment/modules/$(MODULE)/$(MODULE_ARCH)
pkg_LTLIBRARIES        = module.la
module_la_SOURCES      = e_mod_config.c \
                         e_mod_main.c \
                         e_mod_main.h \
                         e_mod_softkey.c \
                         e_mod_notification.c \
                         e_mod_visivility.c \
                         e_mod_rotation.c \
                         e_mod_rotation.h \
                         e_mod_utils.h \
                         e_mod_keyboard.c \
                         e_mod_keyboard.h \
                         e_mod_stack.c
if HAVE_WAYLAND_ONLY
module_la_SOURCES += e_mod_wl.c \
					 e_mod_wl.h \
					 e_mod_sysinfo.c \
					 e_mod_sysinfo.h \
					 e_mod_rotation_wl.c \
					 e_mod_rotation_wl.h \
					 tizen_policy_ext-protocol.c \
					 tizen_policy_ext-server-protocol.h
else
module_la_SOURCES += e_mod_atoms.c \
                     e_mod_atoms.h
endif

module_la_LIBADD       =
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@ @TZSH_SERVER_CFLAGS@
module_la_LDFLAGS      = -module -avoid-version @ENLIGHTENMENT_LIBS@ @TZSH_SERVER_LIBS@
module_la_DEPENDENCIES = $(top_builddir)/config.h
