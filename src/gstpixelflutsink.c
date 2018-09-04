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

#include <stdio.h>

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
  PROP_OFFSET_TOP,
  PROP_OFFSET_LEFT,
  PROP_FRAMES_SENT,
  PROP_BYTES_WRITTEN,
  PROP_PIXELS_PER_PACKET,
  PROP_CANVAS_WIDTH,
  PROP_CANVAS_HEIGHT,
};

#define DEFAULT_PORT 1337
#define DEFAULT_HOST "localhost"
#define DEFAULT_PPP 10
#define CANVAS_MAX 9999

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
static gboolean gst_pixelflutsink_unlock (GstBaseSink * bsink);
static gboolean gst_pixelflutsink_unlock_stop (GstBaseSink * bsink);

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
  g_object_class_install_property (gobject_class, PROP_OFFSET_TOP,
      g_param_spec_int ("offset-top",
          "Offset Top", "Offset in pixel from the top of canvas", G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_OFFSET_LEFT,
      g_param_spec_int ("offset-left",
          "Offset Left", "Offset in pixel from left side of the canvas", G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_FRAMES_SENT,
      g_param_spec_int ("frames-sent",
          "Frames sent", "Number of frames sent", G_MININT, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BYTES_WRITTEN,
      g_param_spec_int ("bytes-written",
          "Bytes written", "Number of bytes written", G_MININT, G_MAXINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PIXELS_PER_PACKET,
      g_param_spec_uint ("ppp",
          "Pixels per packet", "How many pixels to transmit at once", 0, 10000, DEFAULT_PPP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_CANVAS_WIDTH,
      g_param_spec_uint ("canvas-width",
          "Canvas width", "Width of Pixelflut server's canvas in pixels", 0, CANVAS_MAX, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CANVAS_HEIGHT,
      g_param_spec_uint ("canvas-height",
          "Canvas height", "Height of Pixelflut server's canvas in pixels", 0, CANVAS_MAX, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Pixelflut Sink", "Sink/Video/Network",
      "Sends raw video frames to a Pixelflut server", "Andreas Frisch <fraxinas@schaffenburg.org>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_pixelflut_sink_template));

  /* overwrite virtual GstBaseSink functions */
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_pixelflutsink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_pixelflutsink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_pixelflutsink_setcaps);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_pixelflutsink_unlock);
  gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_pixelflutsink_unlock_stop);

  /* overwrite virtual GstVideoSink functions */
  gstvideosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_pixelflutsink_send_frame);
}

static void
gst_pixelflutsink_init (GstPixelflutSink *self)
{
  self->offset_top = 0;
  self->offset_left = 0;

  self->host = g_strdup (DEFAULT_HOST);
  self->port = DEFAULT_PORT;

  self->socket = NULL;
  self->cancellable = g_cancellable_new ();

  self->pixels_per_packet = DEFAULT_PPP;

  self->is_open = FALSE;

  GST_DEBUG_OBJECT (self, "inited");
}

static void gst_pixelflutsink_finalize (GObject *object)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->socket);

  g_free (self->host);

  GST_INFO_OBJECT (self, "finalized. sent %i frames and %" G_GSIZE_FORMAT
                   " bytes", self->frames_sent, self->bytes_written);

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
    case PROP_OFFSET_LEFT:
      self->offset_left = g_value_get_int (value);
      break;
    case PROP_OFFSET_TOP:
      self->offset_top = g_value_get_int (value);
      break;
    case PROP_PIXELS_PER_PACKET:
      self->pixels_per_packet = g_value_get_uint (value);
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
    case PROP_FRAMES_SENT:
      g_value_set_int (value, self->frames_sent);
      break;
    case PROP_BYTES_WRITTEN:
      g_value_set_int (value, self->bytes_written);
      break;
    case PROP_OFFSET_TOP:
      g_value_set_int (value, self->offset_top);
      break;
    case PROP_OFFSET_LEFT:
      g_value_set_int (value, self->offset_left);
      break;
    case PROP_PIXELS_PER_PACKET:
      g_value_set_uint (value, self->pixels_per_packet);
      break;
    case PROP_CANVAS_WIDTH:
      g_value_set_uint (value, self->canvas_width);
      break;
    case PROP_CANVAS_HEIGHT:
      g_value_set_uint (value, self->canvas_height);
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
  GError *err = NULL;
  GInetAddress *addr = NULL;
  GSocketAddress *saddr = NULL;
  GResolver *resolver = NULL;
  GSocket *socket = NULL;
  gssize wret;
  gboolean is_open;
  gchar *host;
  int port;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "starting");

  GST_OBJECT_LOCK (self);
  is_open = self->is_open;
  host = g_strdup (self->host);
  port = self->port;
  GST_OBJECT_UNLOCK (self);

  if (is_open) {
    ret = TRUE;
    goto cleanup;
  }

  /* resolve hostname if necessary */
  addr = g_inet_address_new_from_string (host);
  if (!addr) {
    GList *results;
    resolver = g_resolver_get_default ();
    results =
        g_resolver_lookup_by_name (resolver, host, self->cancellable,
        &err);
    if (!results)
      goto name_resolve;
    addr = G_INET_ADDRESS (g_object_ref (results->data));

    g_resolver_free_addresses (results);
  }
#ifndef GST_DISABLE_GST_DEBUG
  {
    gchar *ip = g_inet_address_to_string (addr);
    GST_DEBUG_OBJECT (self, "IP address for host %s is %s", host, ip);
    g_free (ip);
  }
#endif
  saddr = g_inet_socket_address_new (addr, port);

  /* create sending client socket */
  GST_DEBUG_OBJECT (self, "opening sending client socket to %s:%d", host,
      port);

  socket =
      g_socket_new (g_socket_address_get_family (saddr), G_SOCKET_TYPE_STREAM,
      G_SOCKET_PROTOCOL_TCP, &err);

  if (!socket)
    goto no_socket;

  GST_DEBUG_OBJECT (self, "opened sending client socket");

  /* connect to server */
  if (!g_socket_connect (socket, saddr, self->cancellable, &err))
    goto connect_failed;

  wret = g_socket_send (socket, "SIZE\n", 5, self->cancellable, &err);
  if (wret != 5)
    goto size_failed;

  gchar readbuf[16];
  gsize rret;
  int scanret, x, y;
  rret = g_socket_receive (socket, readbuf, sizeof(readbuf), self->cancellable, &err);
  if (rret < 8)
    goto size_failed;
  readbuf[rret-1] = '\0';

  scanret = sscanf(readbuf, "SIZE %d%d", &x, &y);
  if (scanret == 2) {
    GST_INFO_OBJECT (self, "canvas size is (%dx%d)", x, y);
  } else {
    err = g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
      "Couldn't parse canvas reply '%s'", readbuf);
    goto size_failed;
  }

  GST_OBJECT_LOCK (self);
  self->is_open = TRUE;
  self->bytes_written = 0;
  self->socket = g_object_ref (socket);
  self->canvas_width = x;
  self->canvas_height = y;
  GST_OBJECT_UNLOCK (self);

  ret = TRUE;
  goto cleanup;

no_socket:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
        ("Failed to create socket: %s", err->message));
    goto cleanup;
  }
name_resolve:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (self, "Cancelled name resolval");
    } else {
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
          ("Failed to resolve host '%s': %s", host, err->message));
    }
    goto cleanup;
  }
connect_failed:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (self, "Cancelled connecting");
    } else {
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
          ("Failed to connect to host '%s:%d': %s", host, port,
              err->message));
    }
    goto cleanup;
  }
size_failed:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (self, "Cancelled connecting");
    } else {
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_WRITE, (NULL),
          ("Failed to query SIZE from Pixelflut server '%s:%d': %s", host, port,
              err ? err->message : ""));
    }
    goto cleanup;
  }

cleanup:
  {
    g_clear_error (&err);
    g_clear_object (&resolver);
    g_clear_object (&addr);
    g_clear_object (&saddr);
    g_clear_object (&socket);
    g_free (host);

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
  GError *err = NULL;
  gboolean is_open;
  GSocket *socket;

  GST_DEBUG_OBJECT (self, "stop");

  GST_OBJECT_LOCK (self);
  is_open = self->is_open;
  socket = self->socket;
  GST_OBJECT_UNLOCK (self);

  if (is_open)
    return TRUE;

  if (socket) {
    GST_DEBUG_OBJECT (self, "closing socket");

    if (!g_socket_close (socket, &err)) {
      GST_ERROR_OBJECT (self, "Failed to close socket: %s", err->message);
      g_clear_error (&err);
    }
    g_object_unref (socket);
    socket = NULL;
  }

  GST_OBJECT_LOCK (self);
  self->socket = socket;
  self->is_open = FALSE;
  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

static gboolean
gst_pixelflutsink_unlock (GstBaseSink * bsink)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (bsink);

  GST_DEBUG_OBJECT (self, "set to flushing");
  /* Unlock pending access to resources */
  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
gst_pixelflutsink_unlock_stop (GstBaseSink * bsink)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (bsink);

  GST_DEBUG_OBJECT (self, "unset flushing");
  /* Clear previous unlock request */
  g_cancellable_reset (self->cancellable);

  return TRUE;
}

static GstFlowReturn
gst_pixelflutsink_send_frame (GstVideoSink * vsink, GstBuffer * buffer)
{
  GstPixelflutSink *self = GST_PIXELFLUTSINK (vsink);
  GstVideoInfo info;
  GstVideoFrame frame;
  gint offset_left, offset_top;
  guint canvas_w, canvas_h;
  guint8 *data;
  gint offsets[3];   // offsets of color bytes in the pixel
  gint height, width;// frame dimensions
  gint plane_stride; // number of bytes per row
  gint pixel_stride; // number of bytes per pixel
  gint x, y;         // coordinate iterators
  gint row_wrap;     // to skip padding bytes in rows
  gboolean has_alpha;// whether frame has alpha plane
  gchar alpha_str[3];// optional suffix for transparency
  guint ppp, ppp_count = 0; // pixels per packet
  gsize packet_offset = 0;  // for fragmented sending
  GError *err = NULL;
  gboolean is_open;
  GSocket *socket;
  gint fragments_count = 0;
  gssize fragment_written = 0;
  gsize frame_written = 0;

  GST_OBJECT_LOCK (self);
  offset_left = self->offset_left;
  offset_top = self->offset_top;
  info = self->info;
  is_open = self->is_open;
  socket = self->socket;
  ppp = self->pixels_per_packet;
  canvas_w = self->canvas_width;
  canvas_h = self->canvas_height;
  GST_OBJECT_UNLOCK (self);

  gchar outbuf[22*ppp+1];  // to assemble the pixeflut command

  g_return_val_if_fail (is_open, GST_FLOW_FLUSHING);

  /* Fill GstVideoFrame structure so that pixel data can accessed */
  if (!gst_video_frame_map (&frame, &info, buffer, GST_MAP_READ))
    goto invalid_frame;

  has_alpha = GST_VIDEO_FORMAT_INFO_HAS_ALPHA (self->info.finfo);

  data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);

  plane_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);
  width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, 0);
  height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, 0);
  pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (&frame, 0);

  offsets[0] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 0);
  offsets[1] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 1);
  offsets[2] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 2);
  if (has_alpha) {
    offsets[3] = GST_VIDEO_FRAME_COMP_OFFSET (&frame, 3);
  }

  row_wrap = plane_stride - pixel_stride * width;

  if (1) { // strategy line_by_line
    for (y = 0; (y < height && y+offset_top <= canvas_h); y++) {
      for (x = 0; (x < width && x+offset_left <= canvas_w); x++) {
        if (has_alpha) {
          guint8 val = data[offsets[3]];
          if (val == 0x00) {
            /* skip fully transparent pixel */
            data += pixel_stride;
            continue;
          }
          g_snprintf (alpha_str, 3, "%02x", val);
        }
        g_snprintf (outbuf+packet_offset, 22,
                    "PX %d %d %02x%02x%02x%s\n",
                    x+offset_left, y+offset_top,
                    data[offsets[0]],
                    data[offsets[1]],
                    data[offsets[2]],
                    has_alpha ? alpha_str : "");

        packet_offset = strlen(outbuf);

        if (ppp_count && ppp_count % ppp == 0) {
          fragment_written = 0;
          while (fragment_written < packet_offset) {
            gssize wret;
            fragments_count++;
            wret =
              g_socket_send (socket, outbuf + fragment_written,
              packet_offset - fragment_written, self->cancellable, &err);
            if (wret < 0) {
              goto write_error;
            }
            fragment_written += wret;
            if (gst_debug_category_get_threshold (pixelflutsink_debug) >= GST_LEVEL_MEMDUMP) {
              GST_TRACE_OBJECT (self, "sent: %s", outbuf);
            }
            GST_TRACE_OBJECT (self, "sent: wret=%" G_GSSIZE_FORMAT " fragment_written=%"
                              G_GSSIZE_FORMAT " frame_written=%" G_GSIZE_FORMAT " fragments_count=%d",
                              wret, fragment_written, frame_written, fragments_count);
            packet_offset = 0;
            ppp_count = 0;
          }
          frame_written += fragment_written;
        }
        ppp_count++;
        data += pixel_stride;
      }
      data += row_wrap;
    }
  }
  gst_video_frame_unmap (&frame);

  GST_OBJECT_LOCK (self);
  self->bytes_written += frame_written;
  self->frames_sent++;
  GST_OBJECT_UNLOCK (self);

  GST_LOG_OBJECT (self, "frame finished. %" G_GSIZE_FORMAT "bytes sent in %d fragments",
                  frame_written, fragments_count);

  return GST_FLOW_OK;

  /* ERRORS */
invalid_frame:
  {
    GST_WARNING_OBJECT (self, "invalid frame");
    return GST_FLOW_ERROR;
  }
write_error:
  {
    GstFlowReturn ret;

    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      ret = GST_FLOW_FLUSHING;
      GST_DEBUG_OBJECT (self, "Cancelled writing to socket");
    } else {
      GST_ELEMENT_ERROR (self, RESOURCE, WRITE,
          ("Error while sending data."),
          ("Only %" G_GSIZE_FORMAT " of %" G_GSIZE_FORMAT " bytes written: %s",
              fragment_written, strlen(outbuf), err->message));
      ret = GST_FLOW_ERROR;
    }
    gst_video_frame_unmap (&frame);
    g_clear_error (&err);
    return ret;
  }
}
