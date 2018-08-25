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
    GST_STATIC_CAPS_ANY
    );

static gboolean gst_pixelflutsink_start (GstBaseSink * bsink);
static gboolean gst_pixelflutsink_stop (GstBaseSink * bsink);

static void gst_pixelflutsink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_pixelflutsink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_pixelflutsink_finalize (GObject *object);

static void
gst_pixelflutsink_class_init (GstPixelflutSinkClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseSinkClass *gstbasesink_class = (GstBaseSinkClass *) klass;

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

