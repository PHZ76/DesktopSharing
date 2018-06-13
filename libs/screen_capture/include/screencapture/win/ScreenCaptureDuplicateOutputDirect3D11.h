/*-*-c++-*-*/
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


  Screen Capture using the Duplication Output API with DXGI/D3D11
  ---------------------------------------------------------------

  This class implements the D3D11/DXGI Duplication Output feature 
  whic allows us to do screen capture on Windows 8+. 

 */
#ifndef SCREEN_CAPTURE_DUPLICATE_OUTPUT_DIRECT3D11_H
#define SCREEN_CAPTURE_DUPLICATE_OUTPUT_DIRECT3D11_H

#include <vector>
#include <stdint.h>
#include <DXGI.h>
#include <DXGI1_2.h> /* For IDXGIOutput1 */
#include <D3D11.h>   /* For the D3D11* interfaces. */
#include <screencapture/Types.h>
#include <screencapture/Base.h>
#include <screencapture/win/ScreenCaptureRendererDirect3D11.h>

namespace sc {

  class ScreenCaptureDuplicateOutputDirect3D11 : public Base {
    
  public:
    /* Allocation */
    ScreenCaptureDuplicateOutputDirect3D11();
    int init();
    int shutdown();

    /* Control */
    int configure(Settings settings);
    int start();
    void update();
    int stop();

    /* Features */
    int getDisplays(std::vector<Display*>& result);
    int getPixelFormats(std::vector<int>& formats);

  private:
    int updateMouse(DXGI_OUTDUPL_FRAME_INFO* info);            /* Checks the current mouse info. */ 

  public:
    IDXGIFactory1* factory;                                    /* Used to create DXGI objects and e.g. enumerating adapters.*/
    ID3D11Device* device;                                      /* The device that we use to create graphics objects. */
    ID3D11DeviceContext* context;                              /* The context that we use to render. */
    IDXGIOutput1* output;                                      /* The selected display, set in configure(). */
    DXGI_OUTPUT_DESC output_desc;                              /* Contains the desktop coordinates, set in configure(). */
    IDXGIOutputDuplication* duplication;                       /* The duplication object, set in configure(). */
    DXGI_OUTDUPL_FRAME_INFO frame_info;                        /* Information about the frame, in update(). */
    IDXGIResource* frame;                                      /* The captured frame, in update(). */
    ScreenCaptureRendererDirect3D11 renderer;                  /* The object which transforms the received frame into the desired output. */
    PixelBuffer pixel_buffer;                                  /* The pixel buffer that we pass into the callback. */
    bool has_frame;                                            /* Is set to true when we acquired a next frame in Release(). According to the docs it's best to keep access to the required frame and release it just before calling AcquireNextFrame(). See the remarks here https://msdn.microsoft.com/en-us/library/windows/desktop/hh404623(v=vs.85).aspx */
    std::vector<IDXGIAdapter1*> adapters;                      /* An adapter represents a video card. We use it to retrieve the attached outputs (displays). */
    std::vector<IDXGIOutput*> outputs;                         /* The outputs we retrieved in init(). An IDXGIOutput represents an display/monitor. */
    std::vector<sc::Display*> displays;                        /* We collect the displays in init(). */
    std::vector<uint8_t> pointer_in_pixels;
    std::vector<uint8_t> pointer_out_pixels;
  }; 

  
} /* namespace sc */

#endif



