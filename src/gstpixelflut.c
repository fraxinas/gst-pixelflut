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

GST_DEBUG_CATEGORY (pixelflut_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "pixelflutsink", GST_RANK_NONE, GST_TYPE_PIXELFLUTSINK);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    pixelflut,
    "Pixelflut plugins",
    plugin_init, VERSION, "GPL", GST_PACKAGE_NAME, "http://github.com/fraxinas/pixelflut");
