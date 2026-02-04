#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

struct Engine {
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
};

EGLint attribs[] = {EGL_RED_SIZE,
                    8,
                    EGL_GREEN_SIZE,
                    8,
                    EGL_BLUE_SIZE,
                    8,
                    EGL_ALPHA_SIZE,
                    8,
                    EGL_DEPTH_SIZE,
                    16,
                    EGL_STENCIL_SIZE,
                    8,
                    EGL_RENDERABLE_TYPE,
                    EGL_OPENGL_ES2_BIT,
                    EGL_NONE};

void handle_cmd(struct android_app *app, int32_t cmd) {
  struct Engine *engine = (struct Engine *)app->userData;

  switch (cmd) {
  case APP_CMD_INIT_WINDOW:
    if (app->window != NULL && engine->surface == EGL_NO_SURFACE) {

      engine->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      eglInitialize(engine->display, 0, 0);

      EGLConfig config;
      EGLint numConfigs;
      eglChooseConfig(engine->display, attribs, &config, 1, &numConfigs);

      EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
      engine->context = eglCreateContext(engine->display, config,
                                         EGL_NO_CONTEXT, contextAttribs);

      engine->surface =
          eglCreateWindowSurface(engine->display, config, app->window, NULL);
      eglMakeCurrent(engine->display, engine->surface, engine->surface,
                     engine->context);
    }
    break;

  case APP_CMD_TERM_WINDOW:
    if (engine->surface != EGL_NO_SURFACE) {
      eglDestroySurface(engine->display, engine->surface);
      engine->surface = EGL_NO_SURFACE;
    }
    break;
  }
}

void android_main(struct android_app *app) {
  struct Engine engine = {0};
  app->userData = &engine;
  app->onAppCmd = handle_cmd;

  int events;
  struct android_poll_source *source;

  while (1) {
    while (ALooper_pollAll(0, NULL, &events, (void **)&source) >= 0) {
      if (source != NULL)
        source->process(app, source);
      if (app->destroyRequested)
        return;
    }

    if (engine.surface != EGL_NO_SURFACE) {
      glClearColor(0, 0.8, 0.6, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      eglSwapBuffers(engine.display, engine.surface);
    }
  }
}
