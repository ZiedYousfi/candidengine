#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <candid/renderer.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("Cross-backend renderer (SDL3 + Metal backend)", 800,
                       600, SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  // Vue Metal SDL
  SDL_MetalView view = SDL_Metal_CreateView(window);
  if (!view) {
    SDL_Log("SDL_Metal_CreateView failed: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  void *layer_ptr = SDL_Metal_GetLayer(view);
  if (!layer_ptr) {
    SDL_Log("SDL_Metal_GetLayer failed: %s", SDL_GetError());
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Renderer *renderer = renderer_create(layer_ptr);
  if (!renderer) {
    SDL_Log("renderer_create failed");
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  int w, h;
  SDL_GetWindowSize(window, &w, &h);
  renderer_resize(renderer, w, h);

  int running = 1;
  float t = 0.0f;

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        running = 0;
      } else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        int new_w, new_h;
        SDL_GetWindowSize(window, &new_w, &new_h);
        renderer_resize(renderer, new_w, new_h);
      }
    }

    t += 0.01f;
    renderer_draw_frame(renderer, t);

    SDL_Delay(16);
  }

  renderer_destroy(renderer);
  SDL_Metal_DestroyView(view);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
