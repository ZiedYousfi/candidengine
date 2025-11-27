/**
 * @file mesh.h
 * @brief Mesh abstraction with proper vertex attributes and GPU resources
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <candid/types.h>

/*******************************************************************************
 * Vertex Attribute Types
 ******************************************************************************/

typedef enum Candid_VertexFormat {
  CANDID_VERTEX_FORMAT_FLOAT,
  CANDID_VERTEX_FORMAT_FLOAT2,
  CANDID_VERTEX_FORMAT_FLOAT3,
  CANDID_VERTEX_FORMAT_FLOAT4,
  CANDID_VERTEX_FORMAT_INT,
  CANDID_VERTEX_FORMAT_INT2,
  CANDID_VERTEX_FORMAT_INT3,
  CANDID_VERTEX_FORMAT_INT4,
  CANDID_VERTEX_FORMAT_UINT,
  CANDID_VERTEX_FORMAT_UINT2,
  CANDID_VERTEX_FORMAT_UINT3,
  CANDID_VERTEX_FORMAT_UINT4,
  CANDID_VERTEX_FORMAT_BYTE4_NORM,  /**< 4 bytes normalized to [0, 1] */
  CANDID_VERTEX_FORMAT_BYTE4_SNORM, /**< 4 bytes normalized to [-1, 1] */
  CANDID_VERTEX_FORMAT_SHORT2,
  CANDID_VERTEX_FORMAT_SHORT4,
  CANDID_VERTEX_FORMAT_SHORT2_NORM,
  CANDID_VERTEX_FORMAT_SHORT4_NORM,
} Candid_VertexFormat;

/**
 * Semantic hints for vertex attributes (for shader binding)
 */
typedef enum Candid_VertexSemantic {
  CANDID_SEMANTIC_POSITION,
  CANDID_SEMANTIC_NORMAL,
  CANDID_SEMANTIC_TANGENT,
  CANDID_SEMANTIC_BITANGENT,
  CANDID_SEMANTIC_TEXCOORD0,
  CANDID_SEMANTIC_TEXCOORD1,
  CANDID_SEMANTIC_COLOR0,
  CANDID_SEMANTIC_COLOR1,
  CANDID_SEMANTIC_JOINTS,
  CANDID_SEMANTIC_WEIGHTS,
  CANDID_SEMANTIC_CUSTOM,
} Candid_VertexSemantic;

/**
 * Describes a single vertex attribute
 */
typedef struct Candid_VertexAttribute {
  Candid_VertexSemantic semantic;
  Candid_VertexFormat format;
  uint32_t offset;       /**< Offset in bytes within the vertex */
  uint32_t buffer_index; /**< Which vertex buffer this comes from */
} Candid_VertexAttribute;

#define CANDID_MAX_VERTEX_ATTRIBUTES 16
#define CANDID_MAX_VERTEX_BUFFERS 8

/**
 * Complete vertex layout description
 */
typedef struct Candid_VertexLayout {
  Candid_VertexAttribute attributes[CANDID_MAX_VERTEX_ATTRIBUTES];
  uint32_t attribute_count;
  uint32_t strides[CANDID_MAX_VERTEX_BUFFERS]; /**< Stride per buffer */
  uint32_t buffer_count;
} Candid_VertexLayout;

/*******************************************************************************
 * Mesh Data (CPU side)
 ******************************************************************************/

/**
 * Standard vertex with common attributes
 */
typedef struct Candid_Vertex {
  Candid_Vec3 position;
  Candid_Vec3 normal;
  Candid_Vec4 tangent; /**< w = handedness (-1 or 1) */
  Candid_Vec2 texcoord0;
  Candid_Vec2 texcoord1;
  Candid_Color color;
} Candid_Vertex;

/**
 * Skinned vertex for skeletal animation
 */
typedef struct Candid_SkinnedVertex {
  Candid_Vertex base;
  uint8_t joints[4]; /**< Joint indices */
  float weights[4];  /**< Blend weights (should sum to 1) */
} Candid_SkinnedVertex;

/**
 * Mesh data descriptor (CPU-side data before upload)
 */
typedef struct Candid_MeshData {
  const void *vertices;
  size_t vertex_count;
  size_t vertex_stride; /**< Bytes per vertex */
  const void *indices;
  size_t index_count;
  Candid_IndexFormat index_format;
  Candid_VertexLayout layout;
  Candid_PrimitiveTopology topology;
} Candid_MeshData;

/*******************************************************************************
 * GPU Mesh Resource (opaque handle)
 ******************************************************************************/

typedef struct Candid_Mesh Candid_Mesh;

/*******************************************************************************
 * Bounding Volume
 ******************************************************************************/

typedef struct Candid_AABB {
  Candid_Vec3 min;
  Candid_Vec3 max;
} Candid_AABB;

typedef struct Candid_BoundingSphere {
  Candid_Vec3 center;
  float radius;
} Candid_BoundingSphere;

/*******************************************************************************
 * Submesh for multi-material support
 ******************************************************************************/

typedef struct Candid_Submesh {
  uint32_t index_offset;
  uint32_t index_count;
  uint32_t material_index;
  Candid_AABB bounds;
} Candid_Submesh;

#define CANDID_MAX_SUBMESHES 64

typedef struct Candid_MeshDesc {
  Candid_MeshData data;
  Candid_Submesh submeshes[CANDID_MAX_SUBMESHES];
  uint32_t submesh_count;
  Candid_AABB bounds;
  const char *label;
} Candid_MeshDesc;

/*******************************************************************************
 * Primitive Mesh Generators
 ******************************************************************************/

/**
 * Generate a cube mesh with the given size
 * @param size The size of the cube
 * @param out Output mesh data (caller owns memory)
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_create_cube(float size, Candid_MeshData *out);

/**
 * Generate a sphere mesh
 * @param radius Sphere radius
 * @param segments Number of horizontal segments
 * @param rings Number of vertical rings
 * @param out Output mesh data (caller owns memory)
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_create_sphere(float radius, uint32_t segments,
                                        uint32_t rings, Candid_MeshData *out);

/**
 * Generate a plane mesh
 * @param width Plane width
 * @param height Plane height
 * @param subdivisions_x X subdivisions
 * @param subdivisions_y Y subdivisions
 * @param out Output mesh data (caller owns memory)
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_create_plane(float width, float height,
                                       uint32_t subdivisions_x,
                                       uint32_t subdivisions_y,
                                       Candid_MeshData *out);

/**
 * Generate a cylinder mesh
 * @param radius Cylinder radius
 * @param height Cylinder height
 * @param segments Number of segments around the circumference
 * @param out Output mesh data (caller owns memory)
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_create_cylinder(float radius, float height,
                                          uint32_t segments,
                                          Candid_MeshData *out);

/**
 * Free mesh data allocated by mesh generators
 * @param data Mesh data to free
 */
void candid_mesh_data_free(Candid_MeshData *data);

/**
 * Calculate normals for a mesh (modifies vertices in place)
 * @param data Mesh data with positions, will have normals computed
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_calculate_normals(Candid_MeshData *data);

/**
 * Calculate tangents for a mesh (requires normals and texcoords)
 * @param data Mesh data
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_calculate_tangents(Candid_MeshData *data);

/**
 * Calculate AABB for mesh data
 * @param data Mesh data
 * @param out Output AABB
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_mesh_calculate_aabb(const Candid_MeshData *data,
                                         Candid_AABB *out);

#ifdef __cplusplus
}
#endif
