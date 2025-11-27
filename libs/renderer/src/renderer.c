#include <candid/renderer.h>
#include <stdlib.h>
#include <string.h>

Candid_3D_Mesh Candid_CreateCubeMesh(const float size) {
  const size_t vertex_count = 8;
  const size_t triangle_count = 12;

  Candid_3D_Mesh mesh = {.vertices = {NULL, NULL, NULL},
                         .vertex_count = vertex_count,
                         .triangle_vertex = {NULL, NULL, NULL},
                         .triangle_count = triangle_count};

  const float half = size / 2.0f;

  float *vx = NULL, *vy = NULL, *vz = NULL;
  size_t *t0 = NULL, *t1 = NULL, *t2 = NULL;

  vx = malloc(vertex_count * sizeof(float));
  vy = malloc(vertex_count * sizeof(float));
  vz = malloc(vertex_count * sizeof(float));
  if (!vx || !vy || !vz)
    goto fail;

  t0 = malloc(triangle_count * sizeof(size_t));
  t1 = malloc(triangle_count * sizeof(size_t));
  t2 = malloc(triangle_count * sizeof(size_t));
  if (!t0 || !t1 || !t2)
    goto fail;

  /* Vertices (x, y, z) */
  vx[0] = -half;
  vy[0] = -half;
  vz[0] = -half; /* 0 */
  vx[1] = half;
  vy[1] = -half;
  vz[1] = -half; /* 1 */
  vx[2] = half;
  vy[2] = half;
  vz[2] = -half; /* 2 */
  vx[3] = -half;
  vy[3] = half;
  vz[3] = -half; /* 3 */
  vx[4] = -half;
  vy[4] = -half;
  vz[4] = half; /* 4 */
  vx[5] = half;
  vy[5] = -half;
  vz[5] = half; /* 5 */
  vx[6] = half;
  vy[6] = half;
  vz[6] = half; /* 6 */
  vx[7] = -half;
  vy[7] = half;
  vz[7] = half; /* 7 */

  /* Triangles (each triangle is three vertex indices) */
  /* Front (z+) */
  t0[0] = 4;
  t1[0] = 5;
  t2[0] = 6;
  t0[1] = 4;
  t1[1] = 6;
  t2[1] = 7;
  /* Back (z-) */
  t0[2] = 0;
  t1[2] = 3;
  t2[2] = 2;
  t0[3] = 0;
  t1[3] = 2;
  t2[3] = 1;
  /* Right (x+) */
  t0[4] = 1;
  t1[4] = 2;
  t2[4] = 6;
  t0[5] = 1;
  t1[5] = 6;
  t2[5] = 5;
  /* Left (x-) */
  t0[6] = 0;
  t1[6] = 4;
  t2[6] = 7;
  t0[7] = 0;
  t1[7] = 7;
  t2[7] = 3;
  /* Top (y+) */
  t0[8] = 3;
  t1[8] = 7;
  t2[8] = 6;
  t0[9] = 3;
  t1[9] = 6;
  t2[9] = 2;
  /* Bottom (y-) */
  t0[10] = 0;
  t1[10] = 1;
  t2[10] = 5;
  t0[11] = 0;
  t1[11] = 5;
  t2[11] = 4;

  mesh.vertices[0] = vx;
  mesh.vertices[1] = vy;
  mesh.vertices[2] = vz;

  mesh.triangle_vertex[0] = t0;
  mesh.triangle_vertex[1] = t1;
  mesh.triangle_vertex[2] = t2;

  return mesh;

fail:
  free(vx);
  free(vy);
  free(vz);
  free(t0);
  free(t1);
  free(t2);
  /* mesh still has NULL pointers for vertices and triangles */

  return mesh;
}
