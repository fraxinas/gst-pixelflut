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

GST_DEBUG_CATEGORY_STATIC (pixelflutsink_debug);
#define GST_CAT_DEFAULT pixelflutsink_debug

/* Use the GstVideoSink Base class */
G_DEFINE_TYPE (GstPixelflutSink, gst_pixelflutsink, GST_TYPE_VIDEO_SINK);

/* GstPixelflutsink properties */
enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
};

#define DEFAULT_PORT 1337
#define DEFAULT_HOST "localhost"

/* Define a generic SINKPAD template */
static GstStaticPadTemplate gst_pixelflut_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ "
            "ARGB, BGRA, ABGR, RGBA, xRGB,"
            "RGBx, xBGR, BGRx, RGB, BGR }"))
    );

static gboolean gst_pixelflutsink_start (GstBaseSink * bsink);
static gboolean gst_pixelflutsink_stop (GstBaseSink * bsink);
static gboolean gst_pixelflutsink_setcaps (GstBaseSink * bsink, GstCaps * caps);

static void gst_pixelflutsink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_pixelflutsink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_pixelflutsink_finalize (GObject *object);

static GstFlowReturn gst_pixelflutsink_send_frame (GstVideoSink * videosink, GstBuffer * buffer);

static void
gst_pixelflutsink_class_init (GstPixelflutSinkClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;
  GstVideoSinkClass *gstvideosink_class = (GstVideoSinkClass *) klass;

  /* allows filtering debug output with GST_DEBUG=pixelflut*:DEBUG */
  GST_DEBUG_CATEGORY_INIT (pixelflutsink_debug, "pixelflutsink", 0, "pixelflutsink");

  /* overwrite virtual GObject functions */
  gobject_class->set_property = gst_pixelflutsink_set_property;
  gobject_class->get_property = gst_pixelflutsink_get_property;
  gobject_class->finalize     = gst_pixelflutsink_finalize;

  /* install plugin properties */
  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host", "The host/IP to send the packets to",
          DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "The port to send the packets to",
          0, 65535, DEFAULT_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Pixelflut Sink", "Sink/Video/Network",
      "Sends raw video frames to a Pixelflut server", "Andreas Frisch <fraxinas@schaffenburg.org>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_pixelflut_sink_template));

  /* overwrite virtual GstBaseSink functions */
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_pixelflutsink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_pixelflutsink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_pixelflutsink_setcaps);

  /* overwrite virtual GstVideoSink functions */
  gstvideosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_pixelflutsink_send_frame);
}

static void
gst_pixelflutsink_init (GstPixelflutSink *self)
{
  self->host = g_strdup (DEFAULT_HOST);
  self->port = DEFAULT_PORT;

  self->is_open = FALSE;

  GST_DEBUG_OBJECT (self, "inited");
}

static void gst_pixelflutsink_finalize (GObject *object)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (object);

  g_free (self->host);

  GST_INFO_OBJECT (self, "finalized");

  /* "chain up" to base class method */
  G_OBJECT_CLASS (gst_pixelflutsink_parent_class)->finalize (object);
}

static gboolean
gst_pixelflutsink_setcaps (GstBaseSink * basesink, GstCaps * caps)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (basesink);
  GstVideoInfo info;

  GST_DEBUG_OBJECT (self, "setcaps %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  GST_OBJECT_LOCK (self);
  self->info = info;
  GST_OBJECT_UNLOCK (self);

  return TRUE;

  /* ERRORS */
invalid_caps:
  {
    GST_ERROR_OBJECT (self, "invalid caps");
    return FALSE;
  }
}

static void
gst_pixelflutsink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (object);

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_HOST:
      if (!g_value_get_string (value)) {
        g_warning ("host property cannot be NULL");
        break;
      }
      g_free (self->host);
      self->host = g_value_dup_string (value);
      GST_DEBUG ("set host property to '%s'", self->host);
      break;
    case PROP_PORT:
      self->port = g_value_get_int (value);
      GST_DEBUG ("set port property to %d", self->port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_pixelflutsink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (object);

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, self->host);
      break;
    case PROP_PORT:
      g_value_set_int (value, self->port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_pixelflutsink_start (GstBaseSink * bsink)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (bsink);
  gboolean is_open;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "starting");

  GST_OBJECT_LOCK (self);
  is_open = self->is_open;
  GST_OBJECT_UNLOCK (self);

  if (is_open) {
    ret = TRUE;
    goto cleanup;
  }

  /* Start processing - open resources here */

  ret = TRUE;
  goto cleanup;

cleanup:
  {
    GST_OBJECT_LOCK (self);
    self->is_open = ret;
    GST_OBJECT_UNLOCK (self);

    return ret;
  }
}

static gboolean
gst_pixelflutsink_stop (GstBaseSink * bsink)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (bsink);
  gboolean is_open;

  GST_DEBUG_OBJECT (self, "stop");

  GST_OBJECT_LOCK (self);
  is_open = self->is_open;
  GST_OBJECT_UNLOCK (self);

  if (is_open)
    return TRUE;

  /* Stop processing - close resources here */

  GST_OBJECT_LOCK (self);
  self->is_open = FALSE;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static GstFlowReturn
gst_pixelflutsink_send_frame (GstVideoSink * vsink, GstBuffer * buffer)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (vsink);
  GstVideoInfo info;
  GstVideoFrame frame;
  guint8 *data;
  gint offsets[3];   // offsets of color bytes in the pixel
  gint height, width;// frame dimensions
  gint plane_stride; // number of bytes per row
  gint pixel_stride; // number of bytes per pixel
  gint x, y;         // coordinate iterators
  gint row_wrap;     // to skip padding bytes in rows
  gchar outbuf[22];  // to assemble the pixeflut command
  gsize frame_written = 0;

  GST_OBJECT_LOCK (self);
  info = self->info;
  GST_OBJECT_UNLOCK (self);

  /* Fill GstVideoFrame structure so that pixel data can accessed */
  if (!gst_video_frame_map (&frame, &info, buffer, GST_MAP_READ))
    goto invalid_frame;

  data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);

  plane_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);
  width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, 0);
  height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, 0);
  pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (&frame, 0);

  offsets[0] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 0);
  offsets[1] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 1);
  offsets[2] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 2);

  row_wrap = plane_stride - pixel_stride * width;

  if (1) { // strategy line_by_line
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        g_snprintf (outbuf, 22,
                    "PX %d %d %02x%02x%02x\n",
                    x, y,
                    data[offsets[0]],
                    data[offsets[1]],
                    data[offsets[2]]);

        GST_TRACE_OBJECT (self, "wrote: %s", outbuf);
        frame_written += strlen(outbuf);

        data += pixel_stride;
      }
      data += row_wrap;
    }
  }
  gst_video_frame_unmap (&frame);

  GST_LOG_OBJECT (self, "frame finished. %" G_GSIZE_FORMAT " bytes would be written", frame_written);

  return GST_FLOW_OK;

  /* ERRORS */
invalid_frame:
  {
    GST_WARNING_OBJECT (self, "invalid frame");
    return GST_FLOW_ERROR;
  }
}
