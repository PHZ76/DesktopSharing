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
#ifndef SCREEN_CAPTURE_UTILS_DIRECT3D_H
#define SCREEN_CAPTURE_UTILS_DIRECT3D_H

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <D3D11.h>

#define COM_RELEASE(ptr)          \
  if (NULL != ptr) {              \
    ptr->Release();               \
    ptr = NULL;                   \
  }

namespace sc {

  std::string hresult_to_string(HRESULT hr);
  std::vector<std::string> get_d3d11_bind_flags(UINT flag);
  void print_d3d11_texture2d_info(ID3D11Texture2D* tex);
  
} /* namespace sc */
#endif
