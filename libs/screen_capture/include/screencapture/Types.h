/*
  -------------------------------------------------------------------------

  Copyright 2015 roxlu <info#AT#roxlu.com>
  
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  
      http://www.apache.org/licenses/LICENSE-2.0
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  -------------------------------------------------------------------------

 */

#ifndef SCREEN_CAPTURE_TYPES_H
#define SCREEN_CAPTURE_TYPES_H

#include <stdint.h>
#include <string>

/* Screen Capture Drivers */
#define SC_DISPLAY_STREAM 1
#define SC_DUPLICATE_OUTPUT_DIRECT3D11 2

#if defined (__APPLE__)
#  define SC_DEFAULT_DRIVER SC_DISPLAY_STREAM
#elif defined(_WIN32)
#  define SC_DEFAULT_DRIVER SC_DUPLICATE_OUTPUT_DIRECT3D11
#endif

/* General "Unset" value */
#define SC_NONE 0

/* Pixel Formats */
#define SC_420V 1                                                /* 2-plane "video" range YCbCr 4:2:0 */
#define SC_420F 2                                                /* 2-plane "full" range YCbCr 4:2:0 */
#define SC_BGRA 3                                                /* Packed Little Endian ARGB8888 */
#define SC_L10R 4                                                /* Packet Little Endian ARGB2101010 */
                                                                         
/* Capture state. */                                                     
#define SC_STATE_INIT          (1 << 0)                          /* Initialised, init() called, memory allocated.  */
#define SC_STATE_CONFIGURED    (1 << 1)                          /* Configured, configure() called. */
#define SC_STATE_STARTED       (1 << 2)                          /* Started, start() called. */
#define SC_STATE_STOPPED       (1 << 3)                          /* Stopped, stop() called. */
#define SC_STATE_SHUTDOWN      (1 << 4)                          /* Shutdown, shutdown() called, memory deallocated. */

namespace sc {

  /* ----------------------------------------------------------- */

  std::string screencapture_pixelformat_to_string(int format);

  /* ----------------------------------------------------------- */
  
  class PixelBuffer;
  
  typedef void(*screencapture_callback)(PixelBuffer& buffer);

  /* ----------------------------------------------------------- */

  class PixelBuffer {
  public:
    PixelBuffer();                                               /* Initializes; resets all members. */
    ~PixelBuffer();                                              /* Cleans up, resets all members. */
    int init(int w, int h, int fmt);                             /* Sets the given width, height and pixel format members. */
    
  public:
    int pixel_format;                                            /* The pixel format; should be the same as the requested pixel format you pass to the `configure()` function of the screen capture instance. */
    uint8_t* plane[3];                                           /* Pointers to the base addresses where you can find the pixel data. When we have non-planar data, plane[0] will point to the pixels. */
    size_t stride[3];                                            /* Strides per plane. */
    size_t nbytes[3];                                            /* Bytes per plane. */
    size_t width;                                                /* Width of the captured frame. */
    size_t height;                                               /* Height of the captured frame. */
    void* user;                                                  /* User data; set to the user pointer you pass into the capturer. */ 
  };

  /* ----------------------------------------------------------- */
  class Settings {
  public:
    Settings();
    
  public:
    int display;                                                 /* The display number which is shown when you call `listDisplays()`. This is an index into the displays vector you get from `getDisplays()`. */
    int pixel_format;                                            /* The pixel format that you want to use when capturing. */
    int output_width;                                            /* The width for the buffer you'll receive. */
    int output_height;                                           /* The height fr the buffer you'll receive. */
  };

  /* ----------------------------------------------------------- */
  
  struct Display {
    std::string name;                                            /* Human readable name of the display. Set by the driver. */
    void* info;                                                  /* Opaque platform, iplementation specifc info. */
  };

} /* namespace sc */

#endif
