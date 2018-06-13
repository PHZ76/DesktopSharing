/* -*-c++-*- */
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

  Screen Capture
  ===============

  This is the main API that you use to capture from the screen. 
  See the test/-.cpp files for some examples on how to use the 
  API. In general you call `init()` when you app starts and `shutdown()`
  when you exit. Init will allocate the necessary buffers for the 
  used driver and `shutdown()` will deallocate it again. What is allocated
  is different per implementation.  Once initialized you configure the 
  capturer with a Settings object (see Types.h). Then when configured
  you can use `start()` to start capturing from the stream, and your 
  given callback will receive the pixel buffer (this will probably 
  be called from another thread). To stop the capture process, you 
  call `stop()`.  You can call `start()` and `stop()` repeadetly.

  Usage:
  -----
  See the test files in `test/-.cpp`, but in general:
  
  ````c++
  
      ScreenCapture cap(callback);
      cap.init();
      cap.configure(settings);
      cap.start();
      
      while (graphics_loop) {
      
      }
      
      cap.stop();
      cap.shutdown();

  ````

  **
  At the time of writing we only support full screen capture, no
  support for rectangles or windows yet.
  **

 */
#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <screencapture/Base.h>
#include <screencapture/Types.h>

#if defined(__APPLE__)
#  include <screencapture/mac/ScreenCaptureDisplayStream.h>
#elif defined(_WIN32)
#  include <screencapture/win/ScreenCaptureDuplicateOutputDirect3D11.h>
#endif

namespace sc {

  /* ----------------------------------------------------------- */

  class ScreenCapture {
  public:
    ScreenCapture(screencapture_callback callback, void* user = NULL, int driver = SC_DEFAULT_DRIVER);  /* Create a screen capture object. We will call the `callback` when we receive a new frame and pass it a PixelBuffer object (see Types.h). The received pixel buffer object will have it's member user set to the given user pointer. Optionally you can pass driver that you want to use. */
    ~ScreenCapture();                                                                                   /* Cleanes up the screen capturer. */

    /* Allocation */
    int init();                                                                                         /* Initializes the driver, allocates memory.  Use as constructor. */
    int shutdown();                                                                                     /* Shutsdown the driver to it's initial state and deallocates any allocated memory. Use as destructor. */

    /* Control */
    int configure(Settings settings);                                                                   /* Before you can use the screencapturer you must configure it. See the examples in `test/.cpp` and Types.h for the available settings. */
    int start();                                                                                        /* Start capturing. You can use start/stop() multiple times. */
    void update();
    int stop();                                                                                         /* Stop capturing. */

    /* Features. */
    int getDisplays(std::vector<Display*>& displays);                                                   /* Get a list of available displays. */
    int getPixelFormats(std::vector<int>& formats);                                                     /* Get the supported pixel formats. */
    int listDisplays();                                                                                 /* Prints out the displays in the console. */
    int listPixelFormats();                                                                             /* Prints out the supposed pixel formats. */
    int isPixelFormatSupported(int fmt);                                                                /* Checks if the given pixel format is supported. See Types.h for available pixel formats. */

    /* Query state. */
    int isInit();                        
    int isShutdown();
    int isConfigured();
    int isStarted();
    int isStopped();
    
  public:
    Base* impl;
  };

  /* ----------------------------------------------------------- */

  inline int ScreenCapture::getPixelFormats(std::vector<int>& formats) {
    
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to retrieve the pixel formats  because we `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif

    return impl->getPixelFormats(formats);
  }

  inline int ScreenCapture::getDisplays(std::vector<Display*>& displays) {
    
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to retrieve the displays because we `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif

    if (0 != isInit()) {
      printf("Error: cannot list displays because we're not initialized. Call init() first.\n");
    }

    return impl->getDisplays(displays);
  }
  
  inline int ScreenCapture::isConfigured() {
    
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to check isConfigured() because `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif
    
    return impl->isConfigured();
  }

  inline int ScreenCapture::isInit() {
    
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to check isInit() because `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif
    
    return impl->isInit();
  }
  
  inline int ScreenCapture::isShutdown() {
    
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to check isShutdown() because `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif
    
    return impl->isShutdown();
  }
  
  inline int ScreenCapture::isStarted() {
        
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to check isStarted() because `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif

    return impl->isStarted();
  }
  
  inline int ScreenCapture::isStopped() {
            
#if !defined(NDEBUG)
    if (NULL == impl) {
      printf("Error: failed to check isStopped() because `impl` is NULL in ScreenCapture. Exiting.\n");
      exit(EXIT_FAILURE);
    }
#endif

    return impl->isStopped();
  }
  
} /* namespace sc */

#endif
