/*
 * This example code creates an SDL window and renderer, and then clears the
 * window to a different color every frame, so you'll effectively get a window
 * that's smoothly fading between colors.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <stdio.h>
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  SDL_SetAppMetadata("Example Renderer Clear", "1.0",
                     "com.example.renderer-clear");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("examples/renderer/clear", 640, 480,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(renderer, 640, 480,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
  }
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
  const double now = ((double)SDL_GetTicks()) /
                     1000.0; /* convert from milliseconds to seconds. */
  /* choose the color for the frame we will draw. The sine wave trick makes it
   * fade between colors smoothly. */
  const float red = (float)(0.5 + 0.5 * SDL_sin(now));
  const float green = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
  const float blue = (float)(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));
  SDL_SetRenderDrawColorFloat(
      renderer, red, green, blue,
      SDL_ALPHA_OPAQUE_FLOAT); /* new color, full alpha. */

  /* clear the window to the draw color. */
  SDL_RenderClear(renderer);

  /* put the newly-cleared rendering on the screen. */
  SDL_RenderPresent(renderer);

  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    SDL_Log("received event");
    switch (ev.type) {
    case SDL_EVENT_KEY_DOWN:
      switch (ev.key.key) {
      case SDLK_UP:
      case SDLK_W:
        SDL_Log("W Down");
        break;
      case SDLK_LEFT:
      case SDLK_A:
        SDL_Log("A Down");
        break;

      case SDLK_DOWN:
      case SDLK_S:
        SDL_Log("S Down");
        break;
      case SDLK_RIGHT:
      case SDLK_D:
        SDL_Log("D Down");
        break;
      case SDLK_RETURN:
      case SDLK_SPACE:
        SDL_Log("Space Down");
        break;
      case SDLK_ESCAPE:
        SDL_Log("Escape Down");
        break;
      }
      break;
    case SDL_EVENT_KEY_UP:
      switch (ev.key.key) {
      case SDLK_UP:
        SDL_Log("W Up");
      case SDLK_W:
        SDL_Log("W Up");
        break;
      case SDLK_LEFT:
        SDL_Log("A Up");
      case SDLK_A:
        SDL_Log("A Up");
        break;
      case SDLK_DOWN:
        SDL_Log("S Up");
        break;
      case SDLK_S:
        SDL_Log("S Up");
        break;
      case SDLK_RIGHT:
        SDL_Log("D Up");
      case SDLK_D:
        SDL_Log("D Up");
        break;
      case SDLK_SPACE:
        SDL_Log("Space Up");
        break;
      }

      break;
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;
    }
  }

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  /* SDL will clean up the window/renderer for us. */
  switch (result) {
  case SDL_APP_CONTINUE:
    SDL_Log("App requested to continue (SDL_APP_CONTINUE).");
    break;
  case SDL_APP_SUCCESS:
    SDL_Log("App finished successfully (SDL_APP_SUCCESS).");
    break;
  case SDL_APP_FAILURE:
    SDL_Log("App finished with failure (SDL_APP_FAILURE).");
    break;
  default:
    SDL_Log("App exited with unknown result: %d", result);
    break;
  }
}
