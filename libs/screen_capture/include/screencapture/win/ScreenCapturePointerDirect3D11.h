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



  Screen Capture Pointer Drawer
  ------------------------------

  The Mac 'CGDisplayStream' API has a flag to embed the pointer into the
  captured framebuffers; Windows doesn't seem to have this feature so we
  need to draw it manually which is done with this class.

  Some remarks on D3D11 and Matrices. 
  -----------------------------------

  We're using the 'DirectXMath' library that is part of the Win8 SDK.
  The XMMatrix uses row-major order, see the note on ordering in this 
  article https://msdn.microsoft.com/en-us/library/windows/desktop/ff729728(v=vs.85).aspx 
  (see for "Matrix Ordering"). See this post that describes a bit of the 
  confusing about matrix ordering and Direct3D: http://www.catalinzima.com/2012/12/a-word-on-matrices/

  Because DirectXMath is row-order we can either transpose it before
  sending it to the GPU, or use the "row_major" identifier in the shader.
  We're using "row_major" in the shader so we don't need to transpose. 

 */
#ifndef SCREEN_CAPTURE_POINTER_DIRECT3D11_H
#define SCREEN_CAPTURE_POINTER_DIRECT3D11_H

#include <vector>
#include <DXGI.h>
#include <D3D11.h>       /* For the D3D11* interfaces. */
#include <D3DCompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

using namespace DirectX;

namespace sc {

  /* ----------------------------------------------------------- */

  class ScreenCapturePointerTextureDirect3D11 {
  public:
    ScreenCapturePointerTextureDirect3D11(ID3D11Device* device, ID3D11DeviceContext* context);
    ~ScreenCapturePointerTextureDirect3D11();
    int updatePixels(int w, int h, uint8_t* pixels);

  public:
    int width;
    int height;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView* view;
  };
  
  /* ----------------------------------------------------------- */
  
  class ScreenCapturePointerDirect3D11 {
  public:
    ScreenCapturePointerDirect3D11();
    ~ScreenCapturePointerDirect3D11(); 
    int init(ID3D11Device* device, ID3D11DeviceContext* context);    /* Creates the necessary D3D11 objects that we need to render a pointer. */
    int shutdown();
    void draw();
    void setViewport(D3D11_VIEWPORT vp);                             /* Must be called whenever the viewport changes so we can update the orthographic matrix. */
    void setScale(float sx, float sy); /* The viewport scale; needs to be used to scale the pointer.*/
    int updatePointerPixels(int w, int h, uint8_t* pixels);          /* Change the texture data of the pointer we need to draw. */
    void updatePointerPosition(float x, float y);
    
  private:
    int createShadersAndBuffers();                                   /* Creates the VS and PS shader + vertex buffer, layout. */
    int createVertexBuffer();                                        /* Creates the vertex buffer. */
    int createConstantBuffer();

  public:
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* pointer_tex;
    ID3D11ShaderResourceView* pointer_view;
    ID3D11InputLayout* input_layout;
    ID3D11Buffer* vertex_buffer;
    ID3D11Buffer* conb_pm;
    ID3D11Buffer* conb_mm;
    ID3D11PixelShader* ps;
    ID3D11VertexShader* vs;
    float sx; /* x scale of the resized viewport, used to scale the pointer. */
    float sy;
    float vp_height;
    float vp_width;
    ScreenCapturePointerTextureDirect3D11* curr_pointer;
    std::vector<ScreenCapturePointerTextureDirect3D11*> pointers;   /* The pointers; we create a different texture whenever the width/height of a pointer changes.*/
  };

  inline void ScreenCapturePointerDirect3D11::setScale(float xscale, float yscale) {
    sx = xscale;
    sy = yscale;
  }
  
} /* namespace sc */

#endif
