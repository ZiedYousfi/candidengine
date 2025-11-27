/**
 * @file mesh.c
 * @brief Mesh data generation and manipulation
 */

#include <candid/mesh.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*******************************************************************************
 * Vertex Layout Helpers
 ******************************************************************************/

static Candid_VertexLayout create_standard_layout(void) {
  Candid_VertexLayout layout = {0};

  layout.attribute_count = 6;
  layout.buffer_count = 1;
  layout.strides[0] = sizeof(Candid_Vertex);

  // Position
  layout.attributes[0].semantic = CANDID_SEMANTIC_POSITION;
  layout.attributes[0].format = CANDID_VERTEX_FORMAT_FLOAT3;
  layout.attributes[0].offset = offsetof(Candid_Vertex, position);
  layout.attributes[0].buffer_index = 0;

  // Normal
  layout.attributes[1].semantic = CANDID_SEMANTIC_NORMAL;
  layout.attributes[1].format = CANDID_VERTEX_FORMAT_FLOAT3;
  layout.attributes[1].offset = offsetof(Candid_Vertex, normal);
  layout.attributes[1].buffer_index = 0;

  // Tangent
  layout.attributes[2].semantic = CANDID_SEMANTIC_TANGENT;
  layout.attributes[2].format = CANDID_VERTEX_FORMAT_FLOAT4;
  layout.attributes[2].offset = offsetof(Candid_Vertex, tangent);
  layout.attributes[2].buffer_index = 0;

  // TexCoord0
  layout.attributes[3].semantic = CANDID_SEMANTIC_TEXCOORD0;
  layout.attributes[3].format = CANDID_VERTEX_FORMAT_FLOAT2;
  layout.attributes[3].offset = offsetof(Candid_Vertex, texcoord0);
  layout.attributes[3].buffer_index = 0;

  // TexCoord1
  layout.attributes[4].semantic = CANDID_SEMANTIC_TEXCOORD1;
  layout.attributes[4].format = CANDID_VERTEX_FORMAT_FLOAT2;
  layout.attributes[4].offset = offsetof(Candid_Vertex, texcoord1);
  layout.attributes[4].buffer_index = 0;

  // Color
  layout.attributes[5].semantic = CANDID_SEMANTIC_COLOR0;
  layout.attributes[5].format = CANDID_VERTEX_FORMAT_FLOAT4;
  layout.attributes[5].offset = offsetof(Candid_Vertex, color);
  layout.attributes[5].buffer_index = 0;

  return layout;
}

/*******************************************************************************
 * Cube Mesh
 ******************************************************************************/

Candid_Result candid_mesh_create_cube(float size, Candid_MeshData *out) {
  if (!out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  const float h = size * 0.5f;

  // 24 vertices (4 per face for proper normals)
  const uint32_t vertex_count = 24;
  const uint32_t index_count = 36;

  Candid_Vertex *vertices = calloc(vertex_count, sizeof(Candid_Vertex));
  uint16_t *indices = malloc(index_count * sizeof(uint16_t));

  if (!vertices || !indices) {
    free(vertices);
    free(indices);
    return CANDID_ERROR_OUT_OF_MEMORY;
  }

  // Face data: position offsets, normal, and UV coords
  const struct {
    float nx, ny, nz;
    float positions[4][3];
  } faces[6] = {
      // Front (+Z)
      {0, 0, 1, {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}}},
      // Back (-Z)
      {0, 0, -1, {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}}},
      // Right (+X)
      {1, 0, 0, {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}}},
      // Left (-X)
      {-1, 0, 0, {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}}},
      // Top (+Y)
      {0, 1, 0, {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}}},
      // Bottom (-Y)
      {0, -1, 0, {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}}},
  };

  const float uvs[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

  for (uint32_t face = 0; face < 6; ++face) {
    uint32_t base = face * 4;
    for (uint32_t v = 0; v < 4; ++v) {
      vertices[base + v].position.x = faces[face].positions[v][0];
      vertices[base + v].position.y = faces[face].positions[v][1];
      vertices[base + v].position.z = faces[face].positions[v][2];
      vertices[base + v].normal.x = faces[face].nx;
      vertices[base + v].normal.y = faces[face].ny;
      vertices[base + v].normal.z = faces[face].nz;
      vertices[base + v].texcoord0.x = uvs[v][0];
      vertices[base + v].texcoord0.y = uvs[v][1];
      vertices[base + v].color = (Candid_Color){1, 1, 1, 1};
    }

    // Two triangles per face
    uint32_t idx_base = face * 6;
    indices[idx_base + 0] = (uint16_t)(base + 0);
    indices[idx_base + 1] = (uint16_t)(base + 1);
    indices[idx_base + 2] = (uint16_t)(base + 2);
    indices[idx_base + 3] = (uint16_t)(base + 0);
    indices[idx_base + 4] = (uint16_t)(base + 2);
    indices[idx_base + 5] = (uint16_t)(base + 3);
  }

  out->vertices = vertices;
  out->vertex_count = vertex_count;
  out->vertex_stride = sizeof(Candid_Vertex);
  out->indices = indices;
  out->index_count = index_count;
  out->index_format = CANDID_INDEX_FORMAT_UINT16;
  out->layout = create_standard_layout();
  out->topology = CANDID_PRIMITIVE_TRIANGLE_LIST;

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Sphere Mesh
 ******************************************************************************/

Candid_Result candid_mesh_create_sphere(float radius, uint32_t segments,
                                        uint32_t rings, Candid_MeshData *out) {
  if (!out || segments < 3 || rings < 2)
    return CANDID_ERROR_INVALID_ARGUMENT;

  const uint32_t vertex_count = (segments + 1) * (rings + 1);
  const uint32_t index_count = segments * rings * 6;

  Candid_Vertex *vertices = calloc(vertex_count, sizeof(Candid_Vertex));
  uint16_t *indices = malloc(index_count * sizeof(uint16_t));

  if (!vertices || !indices) {
    free(vertices);
    free(indices);
    return CANDID_ERROR_OUT_OF_MEMORY;
  }

  // Generate vertices
  uint32_t v = 0;
  for (uint32_t ring = 0; ring <= rings; ++ring) {
    float phi = (float)M_PI * (float)ring / (float)rings;
    float sin_phi = sinf(phi);
    float cos_phi = cosf(phi);

    for (uint32_t seg = 0; seg <= segments; ++seg) {
      float theta = 2.0f * (float)M_PI * (float)seg / (float)segments;
      float sin_theta = sinf(theta);
      float cos_theta = cosf(theta);

      float x = cos_theta * sin_phi;
      float y = cos_phi;
      float z = sin_theta * sin_phi;

      vertices[v].position.x = radius * x;
      vertices[v].position.y = radius * y;
      vertices[v].position.z = radius * z;
      vertices[v].normal.x = x;
      vertices[v].normal.y = y;
      vertices[v].normal.z = z;
      vertices[v].texcoord0.x = (float)seg / (float)segments;
      vertices[v].texcoord0.y = (float)ring / (float)rings;
      vertices[v].color = (Candid_Color){1, 1, 1, 1};
      ++v;
    }
  }

  // Generate indices
  uint32_t i = 0;
  for (uint32_t ring = 0; ring < rings; ++ring) {
    for (uint32_t seg = 0; seg < segments; ++seg) {
      uint16_t a = (uint16_t)(ring * (segments + 1) + seg);
      uint16_t b = (uint16_t)(a + segments + 1);
      uint16_t c = (uint16_t)(a + 1);
      uint16_t d = (uint16_t)(b + 1);

      indices[i++] = a;
      indices[i++] = b;
      indices[i++] = c;
      indices[i++] = c;
      indices[i++] = b;
      indices[i++] = d;
    }
  }

  out->vertices = vertices;
  out->vertex_count = vertex_count;
  out->vertex_stride = sizeof(Candid_Vertex);
  out->indices = indices;
  out->index_count = index_count;
  out->index_format = CANDID_INDEX_FORMAT_UINT16;
  out->layout = create_standard_layout();
  out->topology = CANDID_PRIMITIVE_TRIANGLE_LIST;

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Plane Mesh
 ******************************************************************************/

Candid_Result candid_mesh_create_plane(float width, float height,
                                       uint32_t subdivisions_x,
                                       uint32_t subdivisions_y,
                                       Candid_MeshData *out) {
  if (!out || subdivisions_x < 1 || subdivisions_y < 1)
    return CANDID_ERROR_INVALID_ARGUMENT;

  const uint32_t verts_x = subdivisions_x + 1;
  const uint32_t verts_y = subdivisions_y + 1;
  const uint32_t vertex_count = verts_x * verts_y;
  const uint32_t index_count = subdivisions_x * subdivisions_y * 6;

  Candid_Vertex *vertices = calloc(vertex_count, sizeof(Candid_Vertex));
  uint16_t *indices = malloc(index_count * sizeof(uint16_t));

  if (!vertices || !indices) {
    free(vertices);
    free(indices);
    return CANDID_ERROR_OUT_OF_MEMORY;
  }

  float half_w = width * 0.5f;
  float half_h = height * 0.5f;

  // Generate vertices
  uint32_t v = 0;
  for (uint32_t y = 0; y < verts_y; ++y) {
    float ty = (float)y / (float)subdivisions_y;
    for (uint32_t x = 0; x < verts_x; ++x) {
      float tx = (float)x / (float)subdivisions_x;

      vertices[v].position.x = -half_w + tx * width;
      vertices[v].position.y = 0.0f;
      vertices[v].position.z = -half_h + ty * height;
      vertices[v].normal.x = 0.0f;
      vertices[v].normal.y = 1.0f;
      vertices[v].normal.z = 0.0f;
      vertices[v].texcoord0.x = tx;
      vertices[v].texcoord0.y = ty;
      vertices[v].color = (Candid_Color){1, 1, 1, 1};
      ++v;
    }
  }

  // Generate indices
  uint32_t i = 0;
  for (uint32_t y = 0; y < subdivisions_y; ++y) {
    for (uint32_t x = 0; x < subdivisions_x; ++x) {
      uint16_t a = (uint16_t)(y * verts_x + x);
      uint16_t b = (uint16_t)(a + verts_x);
      uint16_t c = (uint16_t)(a + 1);
      uint16_t d = (uint16_t)(b + 1);

      indices[i++] = a;
      indices[i++] = b;
      indices[i++] = c;
      indices[i++] = c;
      indices[i++] = b;
      indices[i++] = d;
    }
  }

  out->vertices = vertices;
  out->vertex_count = vertex_count;
  out->vertex_stride = sizeof(Candid_Vertex);
  out->indices = indices;
  out->index_count = index_count;
  out->index_format = CANDID_INDEX_FORMAT_UINT16;
  out->layout = create_standard_layout();
  out->topology = CANDID_PRIMITIVE_TRIANGLE_LIST;

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Cylinder Mesh
 ******************************************************************************/

Candid_Result candid_mesh_create_cylinder(float radius, float height,
                                          uint32_t segments,
                                          Candid_MeshData *out) {
  if (!out || segments < 3)
    return CANDID_ERROR_INVALID_ARGUMENT;

  // Vertices: top cap center + top ring + bottom ring + bottom cap center
  // Plus duplicate rings for proper normals on caps
  const uint32_t ring_verts = segments + 1;
  const uint32_t vertex_count =
      ring_verts * 4 + 2; // 2 rings for side, 2 for caps, 2 centers
  const uint32_t side_indices = segments * 6;
  const uint32_t cap_indices = segments * 3 * 2;
  const uint32_t index_count = side_indices + cap_indices;

  Candid_Vertex *vertices = calloc(vertex_count, sizeof(Candid_Vertex));
  uint16_t *indices = malloc(index_count * sizeof(uint16_t));

  if (!vertices || !indices) {
    free(vertices);
    free(indices);
    return CANDID_ERROR_OUT_OF_MEMORY;
  }

  float half_h = height * 0.5f;
  uint32_t v = 0;

  // Top ring (side normals)
  for (uint32_t s = 0; s <= segments; ++s) {
    float theta = 2.0f * (float)M_PI * (float)s / (float)segments;
    float cos_t = cosf(theta);
    float sin_t = sinf(theta);

    vertices[v].position.x = radius * cos_t;
    vertices[v].position.y = half_h;
    vertices[v].position.z = radius * sin_t;
    vertices[v].normal.x = cos_t;
    vertices[v].normal.y = 0.0f;
    vertices[v].normal.z = sin_t;
    vertices[v].texcoord0.x = (float)s / (float)segments;
    vertices[v].texcoord0.y = 0.0f;
    vertices[v].color = (Candid_Color){1, 1, 1, 1};
    ++v;
  }

  // Bottom ring (side normals)
  for (uint32_t s = 0; s <= segments; ++s) {
    float theta = 2.0f * (float)M_PI * (float)s / (float)segments;
    float cos_t = cosf(theta);
    float sin_t = sinf(theta);

    vertices[v].position.x = radius * cos_t;
    vertices[v].position.y = -half_h;
    vertices[v].position.z = radius * sin_t;
    vertices[v].normal.x = cos_t;
    vertices[v].normal.y = 0.0f;
    vertices[v].normal.z = sin_t;
    vertices[v].texcoord0.x = (float)s / (float)segments;
    vertices[v].texcoord0.y = 1.0f;
    vertices[v].color = (Candid_Color){1, 1, 1, 1};
    ++v;
  }

  // Top cap center
  uint32_t top_center = v;
  vertices[v].position = (Candid_Vec3){0, half_h, 0};
  vertices[v].normal = (Candid_Vec3){0, 1, 0};
  vertices[v].texcoord0 = (Candid_Vec2){0.5f, 0.5f};
  vertices[v].color = (Candid_Color){1, 1, 1, 1};
  ++v;

  // Top cap ring
  uint32_t top_ring_start = v;
  for (uint32_t s = 0; s <= segments; ++s) {
    float theta = 2.0f * (float)M_PI * (float)s / (float)segments;
    float cos_t = cosf(theta);
    float sin_t = sinf(theta);

    vertices[v].position.x = radius * cos_t;
    vertices[v].position.y = half_h;
    vertices[v].position.z = radius * sin_t;
    vertices[v].normal = (Candid_Vec3){0, 1, 0};
    vertices[v].texcoord0.x = 0.5f + 0.5f * cos_t;
    vertices[v].texcoord0.y = 0.5f + 0.5f * sin_t;
    vertices[v].color = (Candid_Color){1, 1, 1, 1};
    ++v;
  }

  // Bottom cap center
  uint32_t bottom_center = v;
  vertices[v].position = (Candid_Vec3){0, -half_h, 0};
  vertices[v].normal = (Candid_Vec3){0, -1, 0};
  vertices[v].texcoord0 = (Candid_Vec2){0.5f, 0.5f};
  vertices[v].color = (Candid_Color){1, 1, 1, 1};
  ++v;

  // Bottom cap ring
  uint32_t bottom_ring_start = v;
  for (uint32_t s = 0; s <= segments; ++s) {
    float theta = 2.0f * (float)M_PI * (float)s / (float)segments;
    float cos_t = cosf(theta);
    float sin_t = sinf(theta);

    vertices[v].position.x = radius * cos_t;
    vertices[v].position.y = -half_h;
    vertices[v].position.z = radius * sin_t;
    vertices[v].normal = (Candid_Vec3){0, -1, 0};
    vertices[v].texcoord0.x = 0.5f + 0.5f * cos_t;
    vertices[v].texcoord0.y = 0.5f - 0.5f * sin_t;
    vertices[v].color = (Candid_Color){1, 1, 1, 1};
    ++v;
  }

  // Generate indices
  uint32_t i = 0;

  // Side
  for (uint32_t s = 0; s < segments; ++s) {
    uint16_t a = (uint16_t)s;
    uint16_t b = (uint16_t)(s + ring_verts);
    uint16_t c = (uint16_t)(s + 1);
    uint16_t d = (uint16_t)(s + ring_verts + 1);

    indices[i++] = a;
    indices[i++] = b;
    indices[i++] = c;
    indices[i++] = c;
    indices[i++] = b;
    indices[i++] = d;
  }

  // Top cap
  for (uint32_t s = 0; s < segments; ++s) {
    indices[i++] = (uint16_t)top_center;
    indices[i++] = (uint16_t)(top_ring_start + s + 1);
    indices[i++] = (uint16_t)(top_ring_start + s);
  }

  // Bottom cap
  for (uint32_t s = 0; s < segments; ++s) {
    indices[i++] = (uint16_t)bottom_center;
    indices[i++] = (uint16_t)(bottom_ring_start + s);
    indices[i++] = (uint16_t)(bottom_ring_start + s + 1);
  }

  out->vertices = vertices;
  out->vertex_count = vertex_count;
  out->vertex_stride = sizeof(Candid_Vertex);
  out->indices = indices;
  out->index_count = index_count;
  out->index_format = CANDID_INDEX_FORMAT_UINT16;
  out->layout = create_standard_layout();
  out->topology = CANDID_PRIMITIVE_TRIANGLE_LIST;

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Mesh Utilities
 ******************************************************************************/

void candid_mesh_data_free(Candid_MeshData *data) {
  if (!data)
    return;
  free((void *)data->vertices);
  free((void *)data->indices);
  memset(data, 0, sizeof(*data));
}

Candid_Result candid_mesh_calculate_normals(Candid_MeshData *data) {
  if (!data || !data->vertices || !data->indices)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Vertex *verts = (Candid_Vertex *)data->vertices;

  // Reset normals
  for (size_t i = 0; i < data->vertex_count; ++i) {
    verts[i].normal = (Candid_Vec3){0, 0, 0};
  }

  // Accumulate face normals
  const uint16_t *idx16 = NULL;
  const uint32_t *idx32 = NULL;

  if (data->index_format == CANDID_INDEX_FORMAT_UINT16) {
    idx16 = (const uint16_t *)data->indices;
  } else {
    idx32 = (const uint32_t *)data->indices;
  }

  for (size_t i = 0; i < data->index_count; i += 3) {
    uint32_t i0, i1, i2;
    if (idx16) {
      i0 = idx16[i];
      i1 = idx16[i + 1];
      i2 = idx16[i + 2];
    } else {
      i0 = idx32[i];
      i1 = idx32[i + 1];
      i2 = idx32[i + 2];
    }

    Candid_Vec3 p0 = verts[i0].position;
    Candid_Vec3 p1 = verts[i1].position;
    Candid_Vec3 p2 = verts[i2].position;

    // Edge vectors
    Candid_Vec3 e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    Candid_Vec3 e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};

    // Cross product
    Candid_Vec3 n = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                     e1.x * e2.y - e1.y * e2.x};

    // Accumulate
    verts[i0].normal.x += n.x;
    verts[i0].normal.y += n.y;
    verts[i0].normal.z += n.z;
    verts[i1].normal.x += n.x;
    verts[i1].normal.y += n.y;
    verts[i1].normal.z += n.z;
    verts[i2].normal.x += n.x;
    verts[i2].normal.y += n.y;
    verts[i2].normal.z += n.z;
  }

  // Normalize
  for (size_t i = 0; i < data->vertex_count; ++i) {
    Candid_Vec3 *n = &verts[i].normal;
    float len = sqrtf(n->x * n->x + n->y * n->y + n->z * n->z);
    if (len > 1e-6f) {
      n->x /= len;
      n->y /= len;
      n->z /= len;
    }
  }

  return CANDID_SUCCESS;
}

Candid_Result candid_mesh_calculate_tangents(Candid_MeshData *data) {
  if (!data || !data->vertices || !data->indices)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Vertex *verts = (Candid_Vertex *)data->vertices;

  // Allocate temporary tangent/bitangent accumulators
  Candid_Vec3 *tan1 = calloc(data->vertex_count, sizeof(Candid_Vec3));
  Candid_Vec3 *tan2 = calloc(data->vertex_count, sizeof(Candid_Vec3));

  if (!tan1 || !tan2) {
    free(tan1);
    free(tan2);
    return CANDID_ERROR_OUT_OF_MEMORY;
  }

  const uint16_t *idx16 = NULL;
  const uint32_t *idx32 = NULL;

  if (data->index_format == CANDID_INDEX_FORMAT_UINT16) {
    idx16 = (const uint16_t *)data->indices;
  } else {
    idx32 = (const uint32_t *)data->indices;
  }

  for (size_t i = 0; i < data->index_count; i += 3) {
    uint32_t i0, i1, i2;
    if (idx16) {
      i0 = idx16[i];
      i1 = idx16[i + 1];
      i2 = idx16[i + 2];
    } else {
      i0 = idx32[i];
      i1 = idx32[i + 1];
      i2 = idx32[i + 2];
    }

    Candid_Vec3 p0 = verts[i0].position;
    Candid_Vec3 p1 = verts[i1].position;
    Candid_Vec3 p2 = verts[i2].position;

    Candid_Vec2 uv0 = verts[i0].texcoord0;
    Candid_Vec2 uv1 = verts[i1].texcoord0;
    Candid_Vec2 uv2 = verts[i2].texcoord0;

    float x1 = p1.x - p0.x, y1 = p1.y - p0.y, z1 = p1.z - p0.z;
    float x2 = p2.x - p0.x, y2 = p2.y - p0.y, z2 = p2.z - p0.z;

    float s1 = uv1.x - uv0.x, t1 = uv1.y - uv0.y;
    float s2 = uv2.x - uv0.x, t2 = uv2.y - uv0.y;

    float r = s1 * t2 - s2 * t1;
    if (fabsf(r) < 1e-6f)
      r = 1.0f;
    r = 1.0f / r;

    Candid_Vec3 sdir = {(t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                        (t2 * z1 - t1 * z2) * r};

    Candid_Vec3 tdir = {(s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                        (s1 * z2 - s2 * z1) * r};

    tan1[i0].x += sdir.x;
    tan1[i0].y += sdir.y;
    tan1[i0].z += sdir.z;
    tan1[i1].x += sdir.x;
    tan1[i1].y += sdir.y;
    tan1[i1].z += sdir.z;
    tan1[i2].x += sdir.x;
    tan1[i2].y += sdir.y;
    tan1[i2].z += sdir.z;

    tan2[i0].x += tdir.x;
    tan2[i0].y += tdir.y;
    tan2[i0].z += tdir.z;
    tan2[i1].x += tdir.x;
    tan2[i1].y += tdir.y;
    tan2[i1].z += tdir.z;
    tan2[i2].x += tdir.x;
    tan2[i2].y += tdir.y;
    tan2[i2].z += tdir.z;
  }

  // Calculate tangents with handedness
  for (size_t i = 0; i < data->vertex_count; ++i) {
    Candid_Vec3 n = verts[i].normal;
    Candid_Vec3 t = tan1[i];

    // Gram-Schmidt orthogonalize
    float dot = n.x * t.x + n.y * t.y + n.z * t.z;
    Candid_Vec3 tangent = {t.x - n.x * dot, t.y - n.y * dot, t.z - n.z * dot};

    float len = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y +
                      tangent.z * tangent.z);
    if (len > 1e-6f) {
      tangent.x /= len;
      tangent.y /= len;
      tangent.z /= len;
    }

    // Handedness
    Candid_Vec3 cross = {n.y * t.z - n.z * t.y, n.z * t.x - n.x * t.z,
                         n.x * t.y - n.y * t.x};
    float w =
        (cross.x * tan2[i].x + cross.y * tan2[i].y + cross.z * tan2[i].z < 0.0f)
            ? -1.0f
            : 1.0f;

    verts[i].tangent = (Candid_Vec4){tangent.x, tangent.y, tangent.z, w};
  }

  free(tan1);
  free(tan2);
  return CANDID_SUCCESS;
}

Candid_Result candid_mesh_calculate_aabb(const Candid_MeshData *data,
                                         Candid_AABB *out) {
  if (!data || !data->vertices || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  const Candid_Vertex *verts = (const Candid_Vertex *)data->vertices;

  out->min = verts[0].position;
  out->max = verts[0].position;

  for (size_t i = 1; i < data->vertex_count; ++i) {
    Candid_Vec3 p = verts[i].position;
    if (p.x < out->min.x)
      out->min.x = p.x;
    if (p.y < out->min.y)
      out->min.y = p.y;
    if (p.z < out->min.z)
      out->min.z = p.z;
    if (p.x > out->max.x)
      out->max.x = p.x;
    if (p.y > out->max.y)
      out->max.y = p.y;
    if (p.z > out->max.z)
      out->max.z = p.z;
  }

  return CANDID_SUCCESS;
}
