#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <candid/renderer.h>
#include <stdlib.h>

/**
 * Helper function to free a Candid_3D_Mesh
 */
static void free_mesh(Candid_3D_Mesh *mesh) {
  if (!mesh)
    return;
  free(mesh->vertices[0]);
  free(mesh->vertices[1]);
  free(mesh->vertices[2]);
  free(mesh->triangle_vertex[0]);
  free(mesh->triangle_vertex[1]);
  free(mesh->triangle_vertex[2]);
  mesh->vertices[0] = mesh->vertices[1] = mesh->vertices[2] = NULL;
  mesh->triangle_vertex[0] = mesh->triangle_vertex[1] =
      mesh->triangle_vertex[2] = NULL;
}

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

  // Create the test cube mesh in the sandbox app
  Candid_3D_Mesh cube_mesh = Candid_CreateCubeMesh(1.0f);
  if (!cube_mesh.vertices[0] || !cube_mesh.triangle_vertex[0]) {
    SDL_Log("Failed to create cube mesh");
    renderer_destroy(renderer);
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Pass the mesh to the renderer
  renderer_set_mesh(renderer, &cube_mesh);

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

  // Cleanup
  free_mesh(&cube_mesh);
  renderer_destroy(renderer);
  SDL_Metal_DestroyView(view);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
