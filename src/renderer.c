#include "renderer.h"
#include <stdlib.h>
#include <string.h>

static Candid_3D_Mesh *Candid_CreateCubeMesh(const float size) {
  const size_t vertex_count = 8;
  const size_t triangle_count = 12;

  Candid_3D_Mesh *mesh = (Candid_3D_Mesh *)malloc(sizeof(Candid_3D_Mesh));
  if (!mesh)
    return NULL;

  mesh->vertex_count = vertex_count;
  mesh->triangle_count = triangle_count;

  /* allocate the SoA vertex arrays (x, y, z) */
  for (int i = 0; i < 3; ++i) {
    mesh->vertices[i] = (float *)malloc(vertex_count * sizeof(float));
    if (!mesh->vertices[i]) {
      /* on failure free previously allocated arrays and the mesh */
      for (int j = 0; j < i; ++j)
        free(mesh->vertices[j]);
      free(mesh);
      return NULL;
    }
  }

  /* allocate triangle index arrays (a, b, c) */
  for (int i = 0; i < 3; ++i) {
    mesh->triangle_vertex[i] =
        (size_t *)malloc(triangle_count * sizeof(size_t));
    if (!mesh->triangle_vertex[i]) {
      for (int j = 0; j < 3; ++j) {
        if (mesh->vertices[j])
          free(mesh->vertices[j]);
      }
      for (int j = 0; j < i; ++j)
        free(mesh->triangle_vertex[j]);
      free(mesh);
      return NULL;
    }
  }

  const float hs = size * 0.5f; /* half-size */

  /* vertex positions (index 0..7) */
  const float vx[8] = {-hs, hs, hs, -hs, -hs, hs, hs, -hs};
  const float vy[8] = {-hs, -hs, hs, hs, -hs, -hs, hs, hs};
  const float vz[8] = {-hs, -hs, -hs, -hs, hs, hs, hs, hs};

  /* copy the vertex positions into the SoA arrays */
  for (size_t i = 0; i < vertex_count; ++i) {
    mesh->vertices[0][i] = vx[i];
    mesh->vertices[1][i] = vy[i];
    mesh->vertices[2][i] = vz[i];
  }

  /* Triangles (12), two triangles per cube face
     using the vertex indices defined above */
  const size_t tri_a[12] = {0, 2, 4, 6, 1, 6, 0, 7, 3, 6, 0, 5};
  const size_t tri_b[12] = {1, 3, 5, 7, 5, 2, 4, 3, 2, 7, 1, 4};
  const size_t tri_c[12] = {2, 0, 6, 4, 6, 1, 7, 0, 6, 3, 5, 0};

  /* copy triangle indices into the SoA arrays */
  for (size_t i = 0; i < triangle_count; ++i) {
    mesh->triangle_vertex[0][i] = tri_a[i];
    mesh->triangle_vertex[1][i] = tri_b[i];
    mesh->triangle_vertex[2][i] = tri_c[i];
  }

  return mesh;
}
