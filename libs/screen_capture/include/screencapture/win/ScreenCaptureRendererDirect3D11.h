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



  Render the Captured Screen into a offscreen buffer.
  ----------------------------------------------------------

  The 'ScreenCaptureRenderDirect3D11' class receives frames with a 
  reference to the 2D texture. This class is designed about the idea that we
  take this texture and scale it to a certain destination size and perform 
  color transforms (if necessary). 

  @todo Check if XMMATRIX intrinsics are supported; there is a check for this in the D3D11 SDK. 

 */

#ifndef SCREEN_CAPTURE_SCALE_TRANSFORM_DIRECT3D11_H
#define SCREEN_CAPTURE_SCALE_TRANSFORM_DIRECT3D11_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <stdint.h>
#include <DXGI.h>
#include <DXGI1_2.h>                                                   /* For IDXGIOutput1 */
#include <D3D11.h>                                                     /* For the D3D11* interfaces. */
#include <D3DCompiler.h>
#include <screencapture/win/ScreenCaptureUtilsDirect3D11.h>
#include <screencapture/win/ScreenCapturePointerDirect3D11.h>

namespace sc {

  typedef void(*scale_color_callback)(uint8_t* pixels, int stride, int width, int height, void* user);

  /* ----------------------------------------------------------- */
  
  class ScreenCaptureRendererSettingsDirect3D11 {
  public:
    ScreenCaptureRendererSettingsDirect3D11();
    
  public:
    ID3D11Device* device;                                                /* Pointer to the D3D11 Device. */
    ID3D11DeviceContext* context;                                        /* Pointer to the D3D11 Device Context. */
    int output_width;                                                    /* The width of the resulting output texture when calling scale(). */
    int output_height;                                                   /* The height of the resulting output texture when calling scale(). */
    scale_color_callback cb_scaled;                                      /* Gets called when we've scaled a frame. */
    void* cb_user;                                                       /* Gets passed into cb_scaled. */
  };

  /* ----------------------------------------------------------- */

  class ScreenCaptureRendererDirect3D11 {
  public:
    ScreenCaptureRendererDirect3D11();
    ~ScreenCaptureRendererDirect3D11();
    int init(ScreenCaptureRendererSettingsDirect3D11 cfg);               /* Initializes the scale/transform/pointer features; creates textures etc.. */
    int shutdown();                                                      /* Cleans up; sets the device and context members to NULL. */
    int scale(ID3D11Texture2D* tex);                                     /* Scale the given texture to the output_width and output_height of the settings passed into init(). */
    int isInit();                                                        /* Returns 0 when we're initialized. */
    int updatePointerPixels(int w, int h, uint8_t* pixels);              /* Update the pixels of the pointer; pixels is in the BGRA format.*/
    void updatePointerPosition(float x, float y);                        /* Update the pointer position; the X and Y should be in the original coordinates as they appear on your screen; so no scaling! */
    
  private:
    HRESULT createVertexBuffer();                                        /* Creates the full screen vertex buffer to render the texture. */
    HRESULT createDestinationObjects();                                  /* Creates the necessary D3D11 objects so we can render a source texture into a offscreen destination target.  */
    
  public:
    int is_init;                                                         /* Used to check if we're initialised. When initialized it's set to 0, otherwise < 0. */ 
    ID3D11Device* device;                                                /* Not owned by this class. The device that we use to create graphics objects. */
    ID3D11DeviceContext* context;                                        /* Not owned by this class. The context that we use to render. */
    ID3D11Texture2D* dest_tex;                                           /* The texture into which the transformed result will be written. */
    ID3D11Texture2D* staging_tex;                                        /* It seems that we can't do GPU > CPU transfers for texture which are a RENDER_TARGET. We need a STAGING one. @todo check what's the fastest solution to download texture data from gpu > cpu */
    ID3D11ShaderResourceView* src_tex_view;                              /* The shader resource view for the destination texture. This represents the texture of the desktop.*/
    ID3D11RenderTargetView* dest_target_view;                            /* The output render target. */
    ID3D11SamplerState* sampler;                                         /* The sampler for sampling the source texture and scaling it into the dest texture. */
    ID3D11PixelShader* ps_scale;                                         /* Mostly a pass though shader used to scale. */ 
    ID3D11VertexShader* vs_scale;                                        /* Vertex shader, used to scale. */
    ID3D11InputLayout* input_layout;                                     /* The input layout that defines our vertex buffer. */
    ID3D11Buffer* vertex_buffer;                                         /* The Position + Texcoord vertex buffer. */
    ID3D11BlendState* blend_state;                                       /* We use alpha blending to merge the pointer texture. */
    D3D11_VIEWPORT scale_viewport;                                       /* The viewport which represents the size of the output render. This most like a scaled down version of the desktop. e.g. when your desktop is 1680x1050 the viewport will be rescaled so that it fits the output size of the capture. */
    ScreenCaptureRendererSettingsDirect3D11 settings;                    /* We copy the settings that are passed into init(). */
    ScreenCapturePointerDirect3D11 pointer;                              /* Takes care of drawing the pointer. */
  }; 

  /* ----------------------------------------------------------- */

  inline int ScreenCaptureRendererDirect3D11::isInit() {
    return is_init;
  }

  inline int ScreenCaptureRendererDirect3D11::updatePointerPixels(int w, int h, uint8_t* pixels) {
    return pointer.updatePointerPixels(w, h, pixels);
  }

  inline void ScreenCaptureRendererDirect3D11::updatePointerPosition(float x, float y) {
    pointer.updatePointerPosition(x, y);
  }
  
} /* namespace sc */

#endif
