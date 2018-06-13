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


  Screen Capture Display Stream
  =============================

  Screen Capture driver for Mac using the `CGDisplayStream*` API.
  See `Base.h` for more info on the meaning of the functions. 

 */
#ifndef SCREEN_CAPTURE_DISPLAY_STREAM_H
#define SCREEN_CAPTURE_DISPLAY_STREAM_H

#include <stdint.h>
#include <vector>

#include <CoreGraphics/CGDisplayStream.h>
#include <screencapture/Types.h>
#include <screencapture/Base.h>

namespace sc {

  /* ----------------------------------------------------------- */
  
  struct ScreenCaptureDisplayStreamDisplayInfo {
    CGDirectDisplayID id;
  };
  
  /* ----------------------------------------------------------- */
  
  class ScreenCaptureDisplayStream : public Base {

  public:
    /* Allocation */
    ScreenCaptureDisplayStream();
    int init();
    int shutdown();

    /* Control */
    int configure(Settings settings);
    int start();
    void update() { } 
    int stop();

    /* Features */
    int getDisplays(std::vector<Display*>& result);
    int getPixelFormats(std::vector<int>& formats);
    
  public:
    dispatch_queue_t dq;
    CGDisplayStreamRef stream_ref;
    std::vector<Display*> displays;
  };
  
}; /* namespace sc */

#endif
