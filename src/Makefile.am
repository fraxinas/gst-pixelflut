include $(top_srcdir)/common/glib-gen.mak

plugin_LTLIBRARIES = libgstpixelflut.la

libgstpixelflut_la_SOURCES = gstpixelflut.c gstpixelflutsink.c
libgstpixelflut_la_CFLAGS = $(GST_CFLAGS) $(GIO_CFLAGS)
libgstpixelflut_la_LIBADD =  $(GST_LIBS) -lgstbase-1.0 -lgstvideo-1.0 $(GIO_LIBS)
libgstpixelflut_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstpixelflut_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstpixelflutsink.h
