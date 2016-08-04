dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT(e-mod-tizen-wm-policy, 0.1, kk.moon@samsung.com)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE(1.8 subdir-objects)
AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST
AC_C___ATTRIBUTE__

define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL

#AM_GNU_GETTEXT([external])
#AM_GNU_GETTEXT_VERSION(0.14)

have_wayland_only=no
AC_ARG_ENABLE([wayland-only],
  AS_HELP_STRING([--enable-wayland-only],[enable wayland-only version of enlightenment @<:@default=disabled@:>@]),
  [have_wayland_only=$enableval],
  [have_wayland_only=no])
AC_MSG_CHECKING([whether wayland-only version is enabled])
if test "x${have_wayland_only}" != "xno"; then
  AC_DEFINE_UNQUOTED([HAVE_WAYLAND_ONLY],[1],[enable wayland-only version of enlightenment])
fi
AM_CONDITIONAL(HAVE_WAYLAND_ONLY, [test "x${have_wayland_only}" = xyes])

PKG_CHECK_MODULES(ENLIGHTENMENT, [enlightenment])
AC_SUBST(ENLIGHTENMENT_CFLAGS)
AC_SUBST(ENLIGHTENMENT_LIBS)

# Checks for ttrace header files
PKG_CHECK_MODULES(TTRACE,
		[ttrace],
		[have_ttrace="yes"], [have_ttrace="no"])

if test "x$have_ttrace" = "xyes"; then
	AC_DEFINE(ENABLE_TTRACE, 1, [ttrace available])
fi

AC_CHECK_LIB(dl, dlopen, dlopen_libs=-ldl)
AC_SUBST(dlopen_libs)

release=$(pkg-config --variable=release enlightenment)
MODULE_ARCH="$host_os-$host_cpu-$release"
AC_SUBST(MODULE_ARCH)
AC_DEFINE_UNQUOTED(MODULE_ARCH, "$MODULE_ARCH", "Module architecture")

datadir=$(pkg-config --variable=modules enlightenment)/${PACKAGE}

# for include e_comp_wl.h
if test "x${have_wayland_only}" != "xno"; then
  AC_DEFINE_UNQUOTED([HAVE_WAYLAND],[1],[enable wayland support])
fi

#### WM Rotation
want_auto_rotation=
have_auto_rotation=

AC_ARG_ENABLE([auto-rotation],
  [AS_HELP_STRING([--enable-auto-rotation], [enable auto rotation @<:@default=disabled@:>@])],
  [want_auto_rotation=$enableval],
  [want_auto_rotation=no])

AC_MSG_CHECKING([whether auto rotation is enabled])
AC_MSG_RESULT([${want_auto_rotation}])

if test "x${want_auto_rotation}" == "xyes"; then
   # Checks for sensord
   PKG_CHECK_MODULES([SENSORD], [sensor],
                     [
                       have_auto_rotation=yes
                       AC_DEFINE_UNQUOTED([HAVE_AUTO_ROTATION], [1], [enable auto rotation])
                     ],
                     [have_auto_rotation=no])
fi

AM_CONDITIONAL([HAVE_AUTO_ROTATION], [test "x${have_auto_rotation}" = "xyes"])
#### End of WM Rotation

AC_CONFIG_FILES([Makefile
		 src/Makefile])
AC_OUTPUT
