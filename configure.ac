AC_INIT([gst-pixelflut],[0.1],[fraxinas@schaffenburg.org])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([src/gstpixelflutsink.c])
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

dnl find a compiler
AC_PROG_CC
AC_PROG_CC_STDC

dnl find libtool
AC_PROG_LIBTOOL

dnl check if the compiler supports '-c' and '-o' options
AM_PROG_CC_C_O

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h stdint.h fcntl.h sys/mman.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Check for Gstreamer 1.0
PKG_CHECK_MODULES(GST, [
	gstreamer-1.0
	gstreamer-video-1.0
	gio-2.0
], [])

dnl set the plugindir where plugins should be installed
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-1.0/plugins"
else
  plugindir="\$(libdir)/gstreamer-1.0"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

GST_PACKAGE_NAME="pixelflut"
AC_DEFINE_UNQUOTED(GST_PACKAGE_NAME, "GST_PACKAGE_NAME", [pixelflut])
AC_SUBST(GST_PACKAGE_NAME)

AC_CONFIG_FILES([
Makefile
m4/Makefile
src/Makefile
])
AC_OUTPUT
