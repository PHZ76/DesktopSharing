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
#ifndef SCREEN_CAPTURE_UTILS_H
#define SCREEN_CAPTURE_UTILS_H

namespace sc {

  void create_identity_matrix(float* m);
  void create_ortho_matrix(float l, float r, float b, float t, float n, float f, float* m);    /* e.g.   create_ortho_matrix(0.0f, width, height, 0.0f, 0.0f, 100.0f, ortho); */
  void create_translation_matrix(float x, float y, float z, float* m);
  void print_matrix(float* m);
  
} /* namespace sc */

#endif
