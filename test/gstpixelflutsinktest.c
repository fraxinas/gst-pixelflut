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

#include <gst/gst.h>

typedef struct _GstPixelflutSinkTest GstPixelflutSinkTest;
struct _GstPixelflutSinkTest
{
  gint offset_top, offset_left;
  guint canvas_width, canvas_height;
  GstElement *pipeline;
  GstElement *source;
  GstElement *videoconvert;
  GstElement *videoscale;
  GstElement *capsfilter;
  GstElement *pixelflutsink;
  GstBus *bus;
  GMainLoop *main_loop;
};

GstPixelflutSinkTest *gst_pixelflutsink_test_new (void);
static gboolean handle_message (GstBus *bus, GstMessage *message, gpointer data);
static void pad_added_handler (GstElement *src, GstPad *new_pad, gpointer data);

static const gchar *source_uri = NULL;
static const gchar *host = "127.0.0.1";
static guint port = 1337;
static guint interval = 10;
static guint width = 320;
static guint height = 240;

static GOptionEntry entries[] = {
  {"uri", 'u', 0, G_OPTION_ARG_STRING, &source_uri, "Source URI", NULL},
  {"host", 'h', 0, G_OPTION_ARG_STRING, &host, "Pixelflut server host", NULL},
  {"port", 'p', 0, G_OPTION_ARG_INT, &port, "Pixelflut server port", NULL},
  {"interval", 'i', 0, G_OPTION_ARG_INT, &interval, "Interval between offset moves in ms", NULL},
  {"width", 'x', 0, G_OPTION_ARG_INT, &width, "Painting width of input in px", NULL},
  {"height", 'y', 0, G_OPTION_ARG_INT, &height, "Paiting height of input in px", NULL},
  {NULL, 0, 0, 0, NULL, NULL, NULL}
};

GstPixelflutSinkTest *gst_pixelflutsink_test_new (void)
{
  GstPixelflutSinkTest *test;

  test = g_new0 (GstPixelflutSinkTest, 1);

  return test;
}

static void pad_added_handler (GstElement *src, GstPad *new_pad, gpointer data) {
  GstPixelflutSinkTest *t = data;
  GstPad *sink_pad;
  GstPadLinkReturn ret;
  GstCaps *pad_caps = NULL;
  GstStructure *pad_struct = NULL;
  const gchar *pad_type = NULL;
  gint fps_num = 99, fps_denom = 66;

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

  /* Check the new pad's type */
  pad_caps = gst_pad_get_current_caps (new_pad);
  pad_struct = gst_caps_get_structure (pad_caps, 0);
  pad_type = gst_structure_get_name (pad_struct);
  if (!g_str_has_prefix (pad_type, "video/x-raw")) {
    return;
  }

  /* Check if it's a still image */
  gst_structure_get_fraction (pad_struct, "framerate", &fps_num, &fps_denom);
  g_print ("framerate = %d/%d\n", fps_num, fps_denom);
  if (fps_num != 0) {
    sink_pad = gst_element_get_static_pad (t->videoconvert, "sink");
  } else {
    /* plug an element that turns a still image into a video stream */
    gboolean ret;
    GstElement *imagefreeze = gst_element_factory_make ("imagefreeze", NULL);
    g_assert (imagefreeze);
    ret = gst_bin_add (GST_BIN (t->pipeline), imagefreeze);
    ret = gst_element_link (imagefreeze, t->videoconvert);
    ret = gst_element_sync_state_with_parent (imagefreeze);
    sink_pad = gst_element_get_static_pad (imagefreeze, "sink");
  }

  /* Attempt the link */
  g_assert (sink_pad);
  if (gst_pad_is_linked (sink_pad)) {
    g_print ("We are already linked. Ignoring.\n");
    return;
  }
  ret = gst_pad_link (new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED (ret)) {
    g_print ("Type is '%s' but link failed.\n", pad_type);
  } else {
    g_print ("Link succeeded (type '%s').\n", pad_type);
  }
}

gboolean move_offset (GstPixelflutSinkTest *t)
{
  static gint x_direction;
  static gint y_direction;

  if (t->offset_left + width >= t->canvas_width) {
    x_direction = -1;
  } else if (t->offset_left <= 0) {
    x_direction = +1;
  }

  if (t->offset_top + height >= t->canvas_height) {
    y_direction = -1;
  } else if (t->offset_top <= 0) {
    y_direction = +1;
  }

  g_print ("Move offset to (%d, %d)\r", t->offset_left, t->offset_top);

  g_object_set (G_OBJECT (t->pixelflutsink),
    "offset-top", t->offset_top,
    "offset-left", t->offset_left, NULL);

  t->offset_top += y_direction;
  t->offset_left += x_direction;

  return TRUE;
}

static gboolean
handle_message (GstBus * bus, GstMessage * message, gpointer data)
{
  GstPixelflutSinkTest *t = data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_WARNING:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_warning (message, &error, &debug);
      g_print ("WARNING from %s: %s\n", GST_ELEMENT_NAME (message->src), error->message);
      return FALSE;
    }
    case GST_MESSAGE_ERROR:
    {
      GError *error = NULL;
      gchar *debug;

      gst_message_parse_error (message, &error, &debug);
      g_print ("ERROR from %s: %s\n", GST_ELEMENT_NAME (message->src), error->message);
      g_main_loop_quit (t->main_loop);
      return FALSE;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState oldstate, newstate, pending;
      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);
      if (GST_ELEMENT (message->src) == t->pipeline) {
        switch (GST_STATE_TRANSITION (oldstate, newstate)) {
          case GST_STATE_CHANGE_NULL_TO_READY:
            g_object_get (t->pixelflutsink, "canvas-width", &t->canvas_width,
                                            "canvas-height", &t->canvas_height,
                                            NULL);
            g_print ("canvas size = (%dx%d)\n", t->canvas_width, t->canvas_height);
            break;
          default:
            break;
        }
      }
      break;
    }
    default:
      break;
  }
  return TRUE;
}

int main(int argc, char *argv[]) {
  GstPixelflutSinkTest *t;
  GOptionContext *context;
  GError *error = NULL;
  gboolean ret;

  context = g_option_context_new ("- Gstreamer Pixelflutsink test application");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("Error initializing option parser: %s\n", error->message);
    return 1;
  }

  gst_init (&argc, &argv);

  t = gst_pixelflutsink_test_new();

  t->main_loop = g_main_loop_new (NULL, TRUE);

  t->pipeline = gst_pipeline_new ("pixelflutsinktest");

  if (source_uri) {
    t->source = gst_element_factory_make ("uridecodebin", NULL);
    g_assert (t->source);
    g_object_set (G_OBJECT (t->source), "uri", source_uri, NULL);

    /* decodebin can't be directly linked because its pads don't appear
    until PLAYING state, so we need to do the linking in a pad-added callback */
    g_signal_connect (t->source, "pad-added", G_CALLBACK (pad_added_handler), t);
  } else {
    t->source = gst_element_factory_make ("videotestsrc", NULL);
    g_assert (t->source);
  }

  /* videoconvert element to adapt the color format */
  t->videoconvert = gst_element_factory_make ("videoconvert", NULL);
  g_assert (t->videoconvert);

  t->videoscale = gst_element_factory_make ("videoscale", NULL);
  g_assert (t->videoscale);

  /* set specified size */
  t->capsfilter = gst_element_factory_make ("capsfilter", NULL);
  g_assert (t->capsfilter);
  const gchar *s_caps = g_strdup_printf ("video/x-raw, width=%d, height=%d", width, height);
  gst_util_set_object_arg (G_OBJECT (t->capsfilter), "caps", s_caps);

  t->pixelflutsink = gst_element_factory_make ("pixelflutsink", NULL);
  g_assert (t->pixelflutsink);
  g_object_set (G_OBJECT (t->pixelflutsink), "host", host, NULL);
  g_object_set (G_OBJECT (t->pixelflutsink), "port", port, NULL);

  gst_bin_add_many (GST_BIN (t->pipeline), t->source, t->videoconvert, t->videoscale, t->capsfilter, t->pixelflutsink, NULL);

  ret = gst_element_link_many (t->videoconvert, t->videoscale, t->capsfilter, t->pixelflutsink, NULL);
  g_assert (ret);

  if (source_uri) {
    g_print ("start playing URI %s\n", source_uri);
  } else {
    g_print ("start playing with a videotestsource\n");
    gst_element_link (t->source, t->videoconvert);
  }

  /* attach a bus message handler */
  t->bus = gst_pipeline_get_bus (GST_PIPELINE (t->pipeline));
  gst_bus_add_watch (t->bus, handle_message, t);

  gst_element_set_state (t->pipeline, GST_STATE_PLAYING);

  g_print ("adding move callback with timeout=%d ms\n", interval);
  g_timeout_add (interval, (GSourceFunc) move_offset, t);

  g_main_loop_run (t->main_loop);

  gst_element_set_state (t->pipeline, GST_STATE_NULL);
  gst_object_unref (t->pipeline);
  return 0;
}
