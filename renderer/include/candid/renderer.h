#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct {
  float *vertices[3];
  size_t vertex_count;
  size_t *triangle_vertex[3];
  size_t triangle_count;
} Candid_3D_Mesh;

/**
 * Create a cube mesh with the given size.
 * @param size The size of the cube.
 * @return A new Candid_3D_Mesh representing a cube.
 */
Candid_3D_Mesh Candid_CreateCubeMesh(float size);

typedef struct Renderer Renderer;

Renderer *renderer_create(void *native_window_surface);
void renderer_resize(Renderer *r, int width, int height);
void renderer_draw_frame(Renderer *r, float time);
void renderer_destroy(Renderer *r);

#ifdef __cplusplus
}
#endif
