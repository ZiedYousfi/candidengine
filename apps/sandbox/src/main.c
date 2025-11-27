#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <candid/renderer.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

  // Metal SDL view
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

  int w, h;
  SDL_GetWindowSize(window, &w, &h);

  Candid_RendererConfig config = {
      .backend = CANDID_BACKEND_AUTO,
      .native_surface = layer_ptr,
      .width = (uint32_t)w,
      .height = (uint32_t)h,
      .vsync = true,
      .debug_mode = false,
      .max_frames_in_flight = 2,
      .app_name = "Candid Sandbox",
  };

  Candid_Renderer *renderer = NULL;
  Candid_Result result = candid_renderer_create(&config, &renderer);
  if (result != CANDID_SUCCESS) {
    SDL_Log("candid_renderer_create failed: %d", result);
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Candid_MeshData cube_data = {0};
  result = candid_mesh_create_cube(1.0f, &cube_data);
  if (result != CANDID_SUCCESS) {
    SDL_Log("Failed to create cube mesh data: %d", result);
    candid_renderer_destroy(renderer);
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Candid_MeshDesc mesh_desc = {
      .data = cube_data,
      .submesh_count = 0,
      .label = "Cube",
  };
  candid_mesh_calculate_aabb(&cube_data, &mesh_desc.bounds);

  Candid_Mesh *cube_mesh = NULL;
  result = candid_renderer_create_mesh(renderer, &mesh_desc, &cube_mesh);
  if (result != CANDID_SUCCESS) {
    SDL_Log("Failed to create GPU mesh: %d", result);
    candid_mesh_data_free(&cube_data);
    candid_renderer_destroy(renderer);
    SDL_Metal_DestroyView(view);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Free CPU-side mesh data (GPU has its own copy)
  candid_mesh_data_free(&cube_data);

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
        candid_renderer_resize(renderer, (uint32_t)new_w, (uint32_t)new_h);
      }
    }

    t += 0.01f;

    // Build model transform (rotation)
    float rot_y = t * 0.8f;
    float rot_x = t * 0.4f;
    float cos_y = cosf(rot_y);
    float sin_y = sinf(rot_y);
    float cos_x = cosf(rot_x);
    float sin_x = sinf(rot_x);

    Candid_Mat4 transform = {0};
    transform.m[0] = cos_y;
    transform.m[2] = sin_y;
    transform.m[5] = cos_x;
    transform.m[6] = -sin_x;
    transform.m[8] = -sin_y;
    transform.m[9] = sin_x * cos_y;
    transform.m[10] = cos_x * cos_y;
    transform.m[14] = -3.0f; // translate back
    transform.m[15] = 1.0f;

    // Render frame
    candid_renderer_begin_frame(renderer);
    candid_renderer_draw_mesh(renderer, cube_mesh, NULL, &transform);
    candid_renderer_end_frame(renderer);

  }

  // Cleanup
  candid_renderer_destroy_mesh(renderer, cube_mesh);
  candid_renderer_destroy(renderer);
  SDL_Metal_DestroyView(view);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
