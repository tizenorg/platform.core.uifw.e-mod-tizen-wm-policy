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
                         e_mod_notification.c \
                         e_mod_visivility.c \
                         e_mod_atoms.c \
                         e_mod_atoms.h \
                         e_mod_rotation.c \
                         e_mod_rotation.h \
                         e_mod_utils.h \
                         e_mod_keyboard.c \
                         e_mod_keyboard.h \
                         e_mod_stack.c
if HAVE_WAYLAND_ONLY
module_la_SOURCES += e_mod_wl.c \
					 e_mod_wl.h \
					 e_tizen_policy_server_protocol.c \
					 e_tizen_policy_server_protocol.h
endif

module_la_LIBADD       =
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@
module_la_LDFLAGS      = -module -avoid-version @ENLIGHTENMENT_LIBS@
module_la_DEPENDENCIES = $(top_builddir)/config.h
