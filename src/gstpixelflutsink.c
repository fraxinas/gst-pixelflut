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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstpixelflutsink.h"

/* Use the GstVideoSink Base class */
G_DEFINE_TYPE (GstPixelflutSink, gst_pixelflutsink, GST_TYPE_VIDEO_SINK);

static void
gst_pixelflutsink_class_init (GstPixelflutSinkClass *klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;

  gst_element_class_set_static_metadata (element_class,
      "Pixelflut Sink", "Sink/Video/Network",
      "Sends raw video frames to a Pixelflut server", "Andreas Frisch <fraxinas@schaffenburg.org>");
}

static void
gst_pixelflutsink_init (GstPixelflutSink *self)
{
  GST_DEBUG_OBJECT (self, "inited");
}
