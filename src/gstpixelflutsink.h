/*
 * GStreamer Pixelflut
 * Copyright 2018 Andreas Frisch <fraxinas@schaffenburg.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_PIXELFLUTSINK_H__
#define __GST_PIXELFLUTSINK_H__

#include <gst/gst.h>
#include <gio/gio.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

G_BEGIN_DECLS

#define GST_TYPE_PIXELFLUTSINK gst_pixelflutsink_get_type ()
G_DECLARE_FINAL_TYPE (GstPixelflutSink, gst_pixelflutsink, GST, PIXELFLUTSINK, GstVideoSink)

/**
 * GstPixelflutSink:
 *
 * Opaque data structure.
 */
struct _GstPixelflutSink
{
  GstVideoSink parent;

  /* video information */
  GstVideoInfo info;

  gint offset_top;
  gint offset_left;

  /* metrics */
  size_t bytes_written;
  gint frames_sent;

  /* server information */
  int port;
  gchar *host;

  /* socket */
  GSocket *socket;
  GCancellable *cancellable;
  gboolean is_open;
  guint pixels_per_packet;
};

G_END_DECLS

#endif /* __GST_PIXELFLUTSINK_H__ */

