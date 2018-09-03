# GStreamer Pixelflut

## OwO What's this?
This is a plugin for GStreamer 1.0 [1] which can be attached as a sink element to a pipeline in order to send the stream to a Pixelflut [2] server.

(CC) by Andreas Frisch <fraxinas@schaffenburg.org>

## Disclaimer
This plugin isn't in any way optimized for high performance pixelfluting. It's meant for educational purpose in a GStreamer plugin programming workshop [3].

## Building

* You need to have `gstreamer-1.0` and plugins (and on some distros the corresponding `-dev` packages) plus `gio-2.0` installed.
* after `git clone $repo`

```bash
cd gst-pixelflut
autoreconf --force --install
./configure
make
```
in order to run gstreamer with the newly built plugin, you need to install it or run `export GST_PLUGIN_PATH=$PWD` while in the $workdir

test with `gst-inspect-1.0 pixelflutsink`

## Example 1
```
gst-launch-1.0 videotestsrc ! videoconvert ! pixelflutsink host=10.42.23.69
```
This will flut the given pixelflut server with test a test video (320x240).

## Example 2
```
gst-launch-1.0 uridecodebin uri=file:///home/user/testimage.jpg ! imagefreeze ! videoscale ! videoconvert ! video/x-raw, width=120, height=80 ! pixelflutsink host=127.0.0.1 -v
```
1. `uridecodebin` will read and decode a given uri or file (in this case an image file).
2. `imagefreeze` turns a single `video/x-raw` buffer (image) into a continuous video stream
3. `videoscale` can change the dimensions of the frames
4. `videoconvert` is needed to change the video format (colorspace), in this case from I420 to RGB as the pixelflutsink requires it
5. `video/x-raw...` is a `capsfilter` element with specified `caps` for setting a specific size

Remarks: the `host` property defaults to localhost, however this may not resolve correctly for IPv6, hence the explicit IPv4 localhost IP, usually you would have an external pixelflut server anyways.

## Plugin Info
```
Factory Details:
  Rank                     none (0)
  Long-name                Pixelflut Sink
  Klass                    Sink/Video/Network
  Description              Sends raw video frames to a Pixelflut server
  Author                   Andreas Frisch <fraxinas@schaffenburg.org>

Plugin Details:
  Name                     pixelflut
  Description              Pixelflut plugins
  Filename                 /home/fraxinas/projects/gst-pixelflut/src/.libs/libgstpixelflut.so
  Version                  0.1
  License                  GPL
  Source module            gst-pixelflut
  Binary package           GST_PACKAGE_NAME
  Origin URL               http://github.com/fraxinas/pixelflut

GObject
 +----GInitiallyUnowned
       +----GstObject
             +----GstElement
                   +----GstBaseSink
                         +----GstVideoSink
                               +----GstPixelflutSink

Pad Templates:
  SINK template: 'sink'
    Availability: Always
    Capabilities:
      video/x-raw
                 format: { (string)ARGB, (string)BGRA, (string)ABGR, (string)RGBA, (string)xRGB, (string)RGBx, (string)xBGR, (string)BGRx, (string)RGB, (string)BGR }
                  width: [ 1, 2147483647 ]
                 height: [ 1, 2147483647 ]
              framerate: [ 0/1, 2147483647/1 ]

Pads:
  SINK: 'sink'
    Pad Template: 'sink'

Element Properties:
  ...GstVideoSink base properties omitted...
  host                : The host/IP to send the packets to
                        flags: readable, writable
                        String. Default: "localhost"
  port                : The port to send the packets to
                        flags: readable, writable
                        Integer. Range: 0 - 65535 Default: 1337
  offset-top          : Offset in pixel from the top of canvas
                        flags: readable, writable, changeable in NULL, READY, PAUSED or PLAYING state
                        Integer. Range: -2147483648 - 2147483647 Default: 0
  offset-left         : Offset in pixel from left side of the canvas
                        flags: readable, writable, changeable in NULL, READY, PAUSED or PLAYING state
                        Integer. Range: -2147483648 - 2147483647 Default: 0
  frames-sent         : Number of frames sent
                        flags: readable
                        Integer. Range: -2147483648 - 2147483647 Default: 0
  bytes-written       : Number of bytes written
                        flags: readable
                        Integer. Range: -2147483648 - 2147483647 Default: 0
  ppp                 : How many pixels to transmit at once
                        flags: readable, writable, changeable in NULL, READY, PAUSED or PLAYING state
                        Unsigned Integer. Range: 0 - 1000 Default: 10
  canvas-width        : Width of Pixelflut server's canvas in pixels
                        flags: readable
                        Unsigned Integer. Range: 0 - 9999 Default: 0
  canvas-height       : Height of Pixelflut server's canvas in pixels
                        flags: readable
                        Unsigned Integer. Range: 0 - 9999 Default: 0
```

## Test Application
There's a little test application that will output a moving 320x240 `videotestsrc` to a pixelflut server on 127.0.0.1:1337 by default. It can also stream from images or videos.

### Testapp Example ###
```
./gstpixelflutsinktest --uri https://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_480p_h264.mov -i 1000 -x 480
```

### Testapp Usage ###
```
Usage:
  gstpixelflutsinktest [OPTION?] - Gstreamer Pixelflutsink test application

Help Options:
  -?, --help                        Show help options
  --help-all                        Show all help options
  --help-gst                        Show GStreamer Options

Application Options:
  -u, --uri                         Source URI
  -h, --host                        Pixelflut server host
  -p, --port                        Pixelflut server port
  -i, --interval                    Interval between offset moves in ms
  -x, --width                       Painting width of input in px
  -y, --height                      Paiting height of input in px
```

## References
* [1] Gstreamer: https://gstreamer.freedesktop.org/
* [2] Pixelflut Server: https://github.com/defnull/pixelflut
* [3] Workshop: https://pads.schaffenburg.org/GstreamerWorkshop

