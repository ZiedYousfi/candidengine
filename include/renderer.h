#pragma once

#include <stddef.h>
#include <stdint.h>


typedef struct {
  float *vertices[3];
  size_t vertex_count;
  size_t *triangle_vertex[3];
  size_t triangle_count;
} Candid_3D_Mesh;

static Candid_3D_Mesh *Candid_CreateCubeMesh(const float size);
