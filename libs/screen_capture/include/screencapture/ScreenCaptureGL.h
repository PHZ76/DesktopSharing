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

  OpenGL Screen Capture
  ======================
  
  The `ScreenCaptureGL` class can be used when a GL context is available
  and you want to draw the captured screen. Because every OS and game engine
  has it's own way to include the GL headers you need to include this header
  only after you've include the necessary GL headers (and so all GL functions 
  that we need are available). Then, in one file you need to use 
  `#defined SCREEN_CAPTURE_IMPLEMENTATION` to include the implementation of 
  this class (also after including the GL headers.).

  How to include
  ---------------
  In every file where you want to use the `ScreenCaptureGL` class you can use: 
 
         #include <screencapture/ScreenCaptureGL.h>

   And in one .cpp file you need to do:
   
         #define SCREEN_CAPTURE_IMPLEMENTATION
         #include <screencapture/ScreenCaptureGL.h>


  Usage 
  -----
  See src/test_opengl.cpp


  Note:
  -----
  I removed using a projection matrix + view matrix because it's easier to 
  manager just a viewport and rendering a full-viewport triangle-strip to draw
  the contents. This makes it easier to work with e.g. FBOs and RTT features.
  See commit 877dda9005d0bcf793f600100eca7d75387948d4 for a version with 
  projection- and view matrix.

*/
/* --------------------------------------------------------------------------- */
/*                               H E A D E R                                   */ 
/* --------------------------------------------------------------------------- */
#ifndef SCREENCAPTURE_OPENGL_H
#define SCREENCAPTURE_OPENGL_H

#include <math.h>
#include <assert.h>
#include <screencapture/ScreenCapture.h>
#include <screencapture/Utils.h>

#if defined(_WIN32)
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
#elif defined(__linux) or defined(__APPLE__)
   #include <pthread.h>
#endif

namespace sc {
  
  /* --------------------------------------------------------------------------- */
  /* Shaders                                                                     */
  /* --------------------------------------------------------------------------- */
  
  static const char* SCREENCAPTURE_GL_VS = ""
    "#version 330\n"
    "uniform float u_texcoords[8]; "
    ""
    " const vec2[] pos = vec2[4]("
    "   vec2(-1.0, 1.0),"
    "   vec2(-1.0, -1.0),"
    "   vec2(1.0, 1.0),"
    "   vec2(1.0, -1.0)"
    "   );"
    ""
    "out vec2 v_tex; "
    ""
    "void main() { "
    "  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);"
    "  v_tex = vec2(u_texcoords[gl_VertexID * 2], u_texcoords[(gl_VertexID * 2) + 1]);"
    "}"
    "";

  static const char* SCREENCAPTURE_GL_FS_BGRA = ""
    "#version 330\n"
    "uniform sampler2D u_tex;"
    ""
    "in vec2 v_tex;"
    "layout( location = 0 ) out vec4 fragcolor; "
    ""
    "void main() {"
    "  vec4 tc = texture(u_tex, v_tex);"
    "  fragcolor.rgb = tc.bgr;"
    "  fragcolor.a = 1.0f;"
    "}"
    "";
  
  /* --------------------------------------------------------------------------- */
  /* Cross platform mutex.                                                       */
  /* --------------------------------------------------------------------------- */

#if defined(_WIN32)
  struct ScreenMutex {
    CRITICAL_SECTION handle;
  };
#elif defined(__linux) or defined(__APPLE__)
  struct ScreenMutex {
    pthread_mutex_t handle;
  };
#endif
  
  int sc_gl_create_mutex(ScreenMutex& m);
  int sc_gl_destroy_mutex(ScreenMutex& m);
  int sc_gl_lock_mutex(ScreenMutex& m);
  int sc_gl_unlock_mutex(ScreenMutex& m);
  
  /* --------------------------------------------------------------------------- */
  /* OpenGL ScreenCapture drawer                                                 */
  /* --------------------------------------------------------------------------- */
  
  class ScreenCaptureGL {
  public:
    ScreenCaptureGL();
    ~ScreenCaptureGL();

    int init();                                                                  /* Initializes the screencapturer driver. Should be used as constructor.*/
    int shutdown();                                                              /* Shutdown the capturer, removes allocated GL textures and memory for the received frames. */
    int configure(Settings settings);                                            /* Configure the capturer, see Types.h for the available settings. */
    int start();                                                                 /* Start capturing the screen. */
    int stop();                                                                  /* Stop capturing the screen. */

    void update();                                                               /* Update the texture; must be called before you want to draw. */ 
    void draw();                                                                 /* Draw the captured screen using at 0, 0 using the provided output_width and output_height of the settings youpassed into configure.*/
    void draw(float x, float y, float w, float h);                               /* Draw the captured screen at the given location. */
    int flip(bool horizontal, bool vertical);                                    /* Flip the screen horizontally or vertically according to Photoshops flip feature. */
    int lock();                                                                  /* Whenever you want to access the `pixels` member you need to lock first. */
    int unlock();                                                                /* Unlock when you're ready witht eh `pixels` member. */
    
  private:
    int setupGraphics();                                                         /* Used internally to create the shaders when they're not yet created, calls `setupShaders()` and `setupTextures()`. */
    int setupShaders();                                                          /* Creates the shader when it's not yet created; all `ScreenCaptureGL instances shared the same shader instance. */
    int setupTextures();                                                         /* Create the texture(s) that are needed to render the captured screen in the set pixel format. */

  public:
    static GLuint prog;                                                          /* The shader program that renders the captured screen (shared among all `ScreenCapture` instances). */
    static GLuint vert;                                                          /* The vertex shader. Shared among all instances of `ScreenCapture` */
    static GLuint frag;                                                          /* The fragment shader. Shared among all instances of `ScreenCapture` */
    static GLuint vao;                                                           /* The vao we use for attributeless rendering. */
    static GLint u_texcoords;                                                    /* Texcoord uniform location; `flip()` uploads the correct texture coordinates. */
    static GLint u_tex;                                                          /* Texture uniform location. */ 
    ScreenMutex mutex;                                                           /* It's possible (and probably will be) that the received pixel buffers are passed to the frame callback from another thread so we need to sync access to it. */
    ScreenCapture cap;                                                           /* The actual capturer */
    Settings settings;                                                           /* The settings you passed into `configure()`. */
    uint8_t* pixels;                                                             /* The pixels we received in the frame callback. */
    bool has_new_frame;                                                          /* Is set to true when in the frame callback. When true, we will update the texture data. */
    GLuint tex0;                                                                 /* First plane, or the only tex when using GL_BGRA */
    GLuint tex1;                                                                 /* Second plane, only used when we receive planar data. */
    int unpack_alignment[3];
    int unpack_row_length[3];
  };

  /* --------------------------------------------------------------------------- */
  
  inline int ScreenCaptureGL::start() {
      return cap.start();
  }

  inline int ScreenCaptureGL::stop() {
    return cap.stop();
  }

  inline int ScreenCaptureGL::lock() {
    
    if (0 != sc_gl_lock_mutex(mutex)) {
      printf("Error: failed to lock the mutex in ScreenCaptureGL.\n");
      return -1;
    }

    return 0;
  }

  inline int ScreenCaptureGL::unlock() {
    
    if (0 != sc_gl_unlock_mutex(mutex)) {
      printf("Error: failed to unlock the mutex in ScreenCaptureGL.\n");
      return -1;
    }

    return 0;
  }

} /* namespace sc */
#endif


/* --------------------------------------------------------------------------- */
/*                       I M P L E M E N T A T I O N                           */ 
/* --------------------------------------------------------------------------- */
#if defined(SCREEN_CAPTURE_IMPLEMENTATION)

namespace sc {

  /* See: https://gist.github.com/roxlu/6152fccfdd0446533e1b for latest version. */
  /* --------------------------------------------------------------------------- */
  /* Embeddable OpenGL wrappers.                                                 */
  /* --------------------------------------------------------------------------- */
  static int create_shader(GLuint* out, GLenum type, const char* src);           /* Create a shader, returns 0 on success, < 0 on error. e.g. create_shader(&vert, GL_VERTEX_SHADER, DRAWER_VS); */
  static int create_program(GLuint* out, GLuint vert, GLuint frag, int link);    /* Create a program from the given vertex and fragment shader. returns 0 on success, < 0 on error. e.g. create_program(&prog, vert, frag, 1); */
  static int print_shader_compile_info(GLuint shader);                           /* Prints the compile info of a shader. returns 0 when shader compiled, < 0 on error. */
  static int print_program_link_info(GLuint prog);                               /* Prints the program link info. returns 0 on success, < 0 on error. */
  /* --------------------------------------------------------------------------- */

  static void sc_gl_frame_callback(PixelBuffer& buffer);
  
  /* --------------------------------------------------------------------------- */
  
  GLuint ScreenCaptureGL::prog = 0;
  GLuint ScreenCaptureGL::vert = 0;
  GLuint ScreenCaptureGL::frag = 0;
  GLuint ScreenCaptureGL::vao = 0;
  GLint ScreenCaptureGL::u_texcoords = -1;
  GLint ScreenCaptureGL::u_tex = -1;

  /* --------------------------------------------------------------------------- */

  ScreenCaptureGL::ScreenCaptureGL()
    :cap(sc_gl_frame_callback, this)
    ,pixels(NULL)
    ,has_new_frame(false)
    ,tex0(0)
    ,tex1(0)
  {
    memset(unpack_alignment, 0x00, sizeof(unpack_alignment));
    memset(unpack_row_length, 0x00, sizeof(unpack_row_length));
  }

  ScreenCaptureGL::~ScreenCaptureGL() {
    
    shutdown();
  }

  int ScreenCaptureGL::init() {

    if (0 != sc_gl_create_mutex(mutex)) {
      return -1;
    }

    if (0 != cap.init()) {
      return -1;
    }

    return 0;
  }

  int ScreenCaptureGL::shutdown() {

    int r = 0;
    
    if (0 != sc_gl_destroy_mutex(mutex)) {
      r -= 1;
    }

    if (0 != cap.shutdown()) {
      r -= 2;
    }

    if (NULL != pixels) {
      delete[] pixels;
      pixels = NULL;
    }

    if (0 != tex0) {
      glDeleteTextures(1, &tex0);
      tex0 = 0;
    }

    if (0 != tex1) {
      glDeleteTextures(1, &tex1);
      tex1 = 0;
    }

    has_new_frame = false;

    return r;
  }

  int ScreenCaptureGL::configure(Settings cfg) {

    settings = cfg;

    if (SC_BGRA != cfg.pixel_format) {
      printf("Error: currently we only support SC_BGRA in ScreenCaptureGL.\n");
      return -1;
    }
    
    if (0 != cap.configure(cfg)) {
      return -2;
    }

    if (NULL != pixels) {
      delete[] pixels;
      pixels = NULL;
    }

    if (0 != setupGraphics()) {
      return -4;
    }
    
    return 0;
  }
  
  int ScreenCaptureGL::setupGraphics() {

    if (0 == prog) {
      if (0 != setupShaders()) {
        return -1;
      }
    }

    if (0 == tex0) {
      if (0 != setupTextures()) {
        return -2;
      }
    }

    if (0 == vao) {
      glGenVertexArrays(1, &vao);
    }
    
    /* We should store the flip as state, not in the global shader. */
    if (0 != flip(false, false)) {
      /* @todo - we should remove all GL objects here. */
      return -3;
    }

    return 0;
  }

  int ScreenCaptureGL::setupShaders() {

     if (0 != prog) {
      printf("Error: trying to setup shaders but the shader is already created.\n");
      return -1;
    }

    if (SC_NONE == settings.pixel_format) {
      printf("Error: cannot setup screencapture shaders; pixel format not set in settings.\n");
      return -2;
    }

    if (0 != create_shader(&vert, GL_VERTEX_SHADER, SCREENCAPTURE_GL_VS)) {
      printf("Error: failed to create the screencapture vertex shader.\n");
      return -2;
    }

    if (SC_BGRA == settings.pixel_format) {
      if (0 != create_shader(&frag, GL_FRAGMENT_SHADER, SCREENCAPTURE_GL_FS_BGRA)) {
        printf("Error: failed to create the screencapture fragment shader.\n");
        return -3;
      }
    }
    else {
      printf("Error: failed to create the shader; no supported shader found.\n");
      glDeleteShader(vert);
      vert = 0;
      return -4;
    }

    if (0 != create_program(&prog, vert, frag, 1)) {
      printf("Error: failed to create the screencapture program.\n");
      glDeleteShader(vert);
      glDeleteShader(frag);
      vert = 0;
      frag = 0;
      return -5;
    }

    glUseProgram(prog);

    u_texcoords = glGetUniformLocation(prog, "u_texcoords");
    u_tex = glGetUniformLocation(prog, "u_tex");

    assert(-1 != u_texcoords);
    assert(-1 != u_tex);
    
    return 0;
  }

  int ScreenCaptureGL::setupTextures() {

    if (0 > settings.output_width) {
      printf("Error: cannot setup the texture for screencapture, output width is invalid: %d\n", settings.output_width);
      return -1;
    }

    if (0 > settings.output_height) {
      printf("Error: cannot setup the texture for screencapture, output height is invalid: %d\n", settings.output_height);
      return -2;
    }

    if (0 != tex0) {
      /* @todo we could delete the current texture. */
      printf("Error: trying to setup the textures, but it seems that the texture is already created.\n");
      return -3;
    }

    if (SC_BGRA == settings.pixel_format) {


      glGenTextures(1, &tex0);
      glBindTexture(GL_TEXTURE_2D, tex0);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, settings.output_width, settings.output_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
      printf("Error: we didn't create textures for the ScreenCaptureGL because we need to add support for the pixel format: %s\n", screencapture_pixelformat_to_string(settings.pixel_format).c_str());
      return -4;
    }
      
    return 0;
  }

  void ScreenCaptureGL::update() {

    bool changed_unpack = false;

    cap.update();

#if !defined(NDEBUG)    
    if (SC_BGRA != settings.pixel_format) {
      printf("Error: trying to update the ScreenCaptureGL, but we only support uploading of SC_BGRA data atm.\n");
      return;
    }
#endif

    /* Update the pixel data. */
    /* @todo maybe we can profile if we should lock the `pixels` a bit shorter. */
    lock();
    {
      if (true == has_new_frame) {
        
        /* When we have a new frame, make sure we set the correct upload state. */
        if (0 != unpack_row_length[0]) {
          glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length[0]);
          changed_unpack = true;
        }
        
        if (0 != unpack_alignment[0]) {
          glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
          changed_unpack = true;
        }

        glBindTexture(GL_TEXTURE_2D, tex0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, settings.output_width,  settings.output_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        has_new_frame = false;

        /* Reset unpack state. */
        if (true == changed_unpack) {
          glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
          glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }
      }
    }
    unlock();
  }

  void ScreenCaptureGL::draw() {
    draw(0.0f, 0.0f, settings.output_width, settings.output_height);
  }

  void ScreenCaptureGL::draw(float x, float y, float w, float h) {
    
#if !defined(NDEBUG)
    if (0 == prog) {
      printf("Error: cannot draw because the shader program hasn't been created.\n");
      return;
    }
#endif
    
    /* Based on the pixelf format bind the correct textures. */
    if (SC_BGRA == settings.pixel_format) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, tex0);
    }
    else {
      printf("Error: cannot bind texture for the current pixel format. %s\n", screencapture_pixelformat_to_string(settings.pixel_format).c_str());
      return;
    }

    glViewport(x, y, w, h);

    /* And draw. */
    glUseProgram(prog);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  int ScreenCaptureGL::flip(bool horizontal, bool vertical) {

    float* texcoords = NULL;
    float tex_normal[] =     { 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0 } ;
    float tex_vert[] =       { 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0 } ;
    float tex_hori[] =       { 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0 } ;
    float tex_verthori[] =   { 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 } ;

    if (0 == prog) {
      printf("Error: You're trying to flip the capture device but we're not yet setup.\n");
      return -1;
    }

    if (-1 == u_texcoords) {
      printf("Error: the u_texcoords uniform location hasn't been set.\n");
      return -2;
    }

    if (false == horizontal && false == vertical) {
      texcoords = tex_normal;
    }
    else if(false == horizontal && true == vertical) {
      texcoords = tex_vert;
    }
    else if(true == horizontal && false == vertical) {
      texcoords = tex_hori;
    }
    else if (true == horizontal && true == vertical) {
      texcoords = tex_verthori;
    }

    if (NULL == texcoords) {
      printf("Error: texcoords == NULL in %s\n", __FUNCTION__);
      return -2;
    }

    glUseProgram(prog);
    glUniform1fv(u_texcoords, 8, texcoords);

    return 0;
  }

  /* ------------------------------------------------------------------------*/
  /* Embeddable OpenGL wrappers.                                             */
  /* ------------------------------------------------------------------------*/

  static int create_shader(GLuint* out, GLenum type, const char* src) {

    *out = glCreateShader(type);
    glShaderSource(*out, 1, &src, NULL);
    glCompileShader(*out);

    if (0 != print_shader_compile_info(*out)) {
      *out = 0;
      return -1;
    }

    return 0;
  }

  /* create a program, store the result in *out. when link == 1 we link too. returns -1 on error, otherwise 0 */
  static int create_program(GLuint* out, GLuint vert, GLuint frag, int link) {
    *out = glCreateProgram();
    glAttachShader(*out, vert);
    glAttachShader(*out, frag);

    if(1 == link) {
      glLinkProgram(*out);
      if (0 != print_program_link_info(*out)) {
        *out = 0;
        return -1;
      }
    }

    return 0;
  }

  /* checks + prints program link info. returns 0 when linking didn't result in an error, on link erorr < 0 */
  static int print_program_link_info(GLuint prog) {
    
    GLint status = 0;
    GLint count = 0;
    GLchar* error = NULL;
    GLsizei nchars = 0;

    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if(status) {
      return 0;
    }

    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &count);
    if(count <= 0) {
      return 0;
    }

    error = (GLchar*)malloc(count);
    glGetProgramInfoLog(prog, count, &nchars, error);
    if (nchars <= 0) {
      free(error);
      error = NULL;
      return -1;
    }

    printf("\nPROGRAM LINK ERROR");
    printf("\n--------------------------------------------------------\n");
    printf("%s", error);
    printf("--------------------------------------------------------\n\n");

    free(error);
    error = NULL;
    return -1;
  }

  /* checks the compile info, if it didn't compile we return < 0, otherwise 0 */
  static int print_shader_compile_info(GLuint shader) {

    GLint status = 0;
    GLint count = 0;
    GLchar* error = NULL;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status) {
      return 0;
    }

    error = (GLchar*) malloc(count);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &count);
    if(count <= 0) {
      free(error);
      error = NULL;
      return 0;
    }

    glGetShaderInfoLog(shader, count, NULL, error);
    printf("\nSHADER COMPILE ERROR");
    printf("\n--------------------------------------------------------\n");
    printf("%s", error);
    printf("--------------------------------------------------------\n\n");

    free(error);
    error = NULL;
    return -1;
  }

  /* --------------------------------------------------------------------------- */
  /* Cross platform mutex.                                                       */
  /* --------------------------------------------------------------------------- */

#if defined(_WIN32)

  int sc_gl_create_mutex(ScreenMutex& m) {
     InitializeCriticalSection(&m.handle);
    return 0;
  }

  int sc_gl_lock_mutex(ScreenMutex& m) {
    EnterCriticalSection(&m.handle);
    return 0;
  }

  int sc_gl_unlock_mutex(ScreenMutex& m) {
    LeaveCriticalSection(&m.handle);
    return 0;
  }

  int sc_gl_destroy_mutex(ScreenMutex& m) {
     DeleteCriticalSection(&m.handle);
    return 0;
  }

#elif defined(__linux) or defined(__APPLE__)

  int sc_gl_create_mutex(ScreenMutex& m) {

    if (0 != pthread_mutex_init(&m.handle, NULL)) {
      printf("Error: failed to create the mutex to sync the pixel data in ScreenCaptureGL.\n");
      return -1;
    }

    return 0;
  }

  int sc_gl_lock_mutex(ScreenMutex& m) {
    if (0 != pthread_mutex_lock(&m.handle)) {
      printf("Error: failed to lock the mutex to sync the pixel data in ScreenCaptureGL.\n");
      return -1;
    }

    return 0;
  }

  int sc_gl_unlock_mutex(ScreenMutex& m) {
    if (0 != pthread_mutex_unlock(&m.handle)) {
      printf("Error: failed to unlock the mutex to sync the pixel data in ScreenCaptureGL.\n");
      return -1;
    }

    return 0;
  }

  int sc_gl_destroy_mutex(ScreenMutex& m) {
    
    if (0 != pthread_mutex_destroy(&m.handle)) {
      printf("Error: failed to destroy the mutex to sync the pixel data in ScreenCaptureGL.\n");
      return -2;
    }

    return 0;
  }

  #endif

  
  /* --------------------------------------------------------------------------- */
  
  static void sc_gl_frame_callback(PixelBuffer& buffer) {

    ScreenCaptureGL* gl = static_cast<ScreenCaptureGL*>(buffer.user);
    if (NULL == gl) {
      printf("Error: failed to cast the user pointer to a ScreenCapture*\n");
      return;
    }

    if (SC_BGRA == buffer.pixel_format) {
     
      if (0 == buffer.nbytes[0]) {
        printf("Error: the number of bytes hasn't been set in the ScreenCapture.\n");
        return;
      }
      
      if (NULL == buffer.plane[0]) {
        printf("Error: the plane data pointer is NULL.\n");
        return;
      }

      if (NULL == gl->pixels) {
        
        gl->pixels = new uint8_t[buffer.nbytes[0]];

        /* Check if we need to change the alignment. */
        int vals[] = { 1, 2, 4, 8 } ;
        int unpack = 0;
        for (int i = 0; i < 4; ++i) {
          int p = pow(2, vals[i]);
          int d = buffer.stride[0] % p;
          if (0 != d) {
            break;
          }
          unpack = vals[i];
        }
        
        /* 4 is default. */
        if (4 != unpack) {
          gl->unpack_alignment[0] = unpack;
        }

        /* Do we need a different unpack row length? (the 4 is from GL_BGRA) */
        if (buffer.stride[0] > buffer.width * 4) {
          gl->unpack_row_length[0] = buffer.stride[0] / 4;
        }
      }

      gl->lock();
      {
        memcpy(gl->pixels, buffer.plane[0], buffer.nbytes[0]);
        gl->has_new_frame = true;
      }
      gl->unlock();
    }
  }
  
  /* --------------------------------------------------------------------------- */
} /* namespace sc */

#endif

#undef SCREEN_CAPTURE_IMPLEMENTATION
