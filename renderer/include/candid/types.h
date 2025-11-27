/**
 * @file types.h
 * @brief Core types for the Candid renderer abstraction layer
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*******************************************************************************
 * Backend Selection
 ******************************************************************************/

typedef enum Candid_Backend {
  CANDID_BACKEND_AUTO = 0, /**< Auto-select best backend for platform */
  CANDID_BACKEND_METAL,    /**< Metal (macOS, iOS) */
  CANDID_BACKEND_VULKAN,   /**< Vulkan (Windows, Linux, Android) */
  CANDID_BACKEND_D3D12,    /**< Direct3D 12 (Windows) */
  CANDID_BACKEND_WEBGPU,   /**< WebGPU (Web, native) */
  CANDID_BACKEND_COUNT
} Candid_Backend;

/*******************************************************************************
 * Result / Error Handling
 ******************************************************************************/

typedef enum Candid_Result {
  CANDID_SUCCESS = 0,
  CANDID_ERROR_INVALID_ARGUMENT,
  CANDID_ERROR_OUT_OF_MEMORY,
  CANDID_ERROR_BACKEND_NOT_SUPPORTED,
  CANDID_ERROR_DEVICE_LOST,
  CANDID_ERROR_SHADER_COMPILATION,
  CANDID_ERROR_RESOURCE_CREATION,
  CANDID_ERROR_UNKNOWN
} Candid_Result;

/*******************************************************************************
 * Primitive Types
 ******************************************************************************/

typedef struct Candid_Vec2 {
  float x, y;
} Candid_Vec2;

typedef struct Candid_Vec3 {
  float x, y, z;
} Candid_Vec3;

typedef struct Candid_Vec4 {
  float x, y, z, w;
} Candid_Vec4;

typedef struct Candid_Mat4 {
  float m[16]; /**< Column-major order */
} Candid_Mat4;

typedef struct Candid_Color {
  float r, g, b, a;
} Candid_Color;

/*******************************************************************************
 * Buffer Types
 ******************************************************************************/

typedef enum Candid_BufferUsage {
  CANDID_BUFFER_USAGE_VERTEX = 1 << 0,
  CANDID_BUFFER_USAGE_INDEX = 1 << 1,
  CANDID_BUFFER_USAGE_UNIFORM = 1 << 2,
  CANDID_BUFFER_USAGE_STORAGE = 1 << 3,
  CANDID_BUFFER_USAGE_TRANSFER_SRC = 1 << 4,
  CANDID_BUFFER_USAGE_TRANSFER_DST = 1 << 5,
} Candid_BufferUsage;

typedef enum Candid_BufferMemory {
  CANDID_BUFFER_MEMORY_GPU_ONLY,   /**< Fast GPU memory, requires staging */
  CANDID_BUFFER_MEMORY_CPU_TO_GPU, /**< CPU writable, GPU readable */
  CANDID_BUFFER_MEMORY_GPU_TO_CPU, /**< GPU writable, CPU readable (readback) */
} Candid_BufferMemory;

typedef struct Candid_BufferDesc {
  size_t size;
  uint32_t usage; /**< Candid_BufferUsage flags */
  Candid_BufferMemory memory;
  const void *initial_data; /**< Optional initial data */
  const char *label;        /**< Debug label */
} Candid_BufferDesc;

/*******************************************************************************
 * Texture Types
 ******************************************************************************/

typedef enum Candid_TextureFormat {
  CANDID_TEXTURE_FORMAT_RGBA8_UNORM,
  CANDID_TEXTURE_FORMAT_RGBA8_SRGB,
  CANDID_TEXTURE_FORMAT_BGRA8_UNORM,
  CANDID_TEXTURE_FORMAT_BGRA8_SRGB,
  CANDID_TEXTURE_FORMAT_R8_UNORM,
  CANDID_TEXTURE_FORMAT_RG8_UNORM,
  CANDID_TEXTURE_FORMAT_RGBA16_FLOAT,
  CANDID_TEXTURE_FORMAT_RGBA32_FLOAT,
  CANDID_TEXTURE_FORMAT_DEPTH32_FLOAT,
  CANDID_TEXTURE_FORMAT_DEPTH24_STENCIL8,
} Candid_TextureFormat;

typedef enum Candid_TextureUsage {
  CANDID_TEXTURE_USAGE_SAMPLED = 1 << 0,
  CANDID_TEXTURE_USAGE_STORAGE = 1 << 1,
  CANDID_TEXTURE_USAGE_RENDER_TARGET = 1 << 2,
  CANDID_TEXTURE_USAGE_DEPTH_STENCIL = 1 << 3,
  CANDID_TEXTURE_USAGE_TRANSFER_SRC = 1 << 4,
  CANDID_TEXTURE_USAGE_TRANSFER_DST = 1 << 5,
} Candid_TextureUsage;

typedef struct Candid_TextureDesc {
  uint32_t width;
  uint32_t height;
  uint32_t depth;        /**< 1 for 2D textures */
  uint32_t mip_levels;   /**< 0 = auto-calculate */
  uint32_t array_layers; /**< 1 for non-array textures */
  Candid_TextureFormat format;
  uint32_t usage;    /**< Candid_TextureUsage flags */
  const char *label; /**< Debug label */
} Candid_TextureDesc;

/*******************************************************************************
 * Sampler Types
 ******************************************************************************/

typedef enum Candid_SamplerFilter {
  CANDID_SAMPLER_FILTER_NEAREST,
  CANDID_SAMPLER_FILTER_LINEAR,
} Candid_SamplerFilter;

typedef enum Candid_SamplerAddressMode {
  CANDID_SAMPLER_ADDRESS_REPEAT,
  CANDID_SAMPLER_ADDRESS_MIRROR_REPEAT,
  CANDID_SAMPLER_ADDRESS_CLAMP_TO_EDGE,
  CANDID_SAMPLER_ADDRESS_CLAMP_TO_BORDER,
} Candid_SamplerAddressMode;

typedef struct Candid_SamplerDesc {
  Candid_SamplerFilter min_filter;
  Candid_SamplerFilter mag_filter;
  Candid_SamplerFilter mip_filter;
  Candid_SamplerAddressMode address_u;
  Candid_SamplerAddressMode address_v;
  Candid_SamplerAddressMode address_w;
  float max_anisotropy;
  Candid_Color border_color;
  const char *label;
} Candid_SamplerDesc;

/*******************************************************************************
 * Comparison Functions (for depth/stencil)
 ******************************************************************************/

typedef enum Candid_CompareFunc {
  CANDID_COMPARE_NEVER,
  CANDID_COMPARE_LESS,
  CANDID_COMPARE_EQUAL,
  CANDID_COMPARE_LESS_EQUAL,
  CANDID_COMPARE_GREATER,
  CANDID_COMPARE_NOT_EQUAL,
  CANDID_COMPARE_GREATER_EQUAL,
  CANDID_COMPARE_ALWAYS,
} Candid_CompareFunc;

/*******************************************************************************
 * Blend Types
 ******************************************************************************/

typedef enum Candid_BlendFactor {
  CANDID_BLEND_ZERO,
  CANDID_BLEND_ONE,
  CANDID_BLEND_SRC_COLOR,
  CANDID_BLEND_ONE_MINUS_SRC_COLOR,
  CANDID_BLEND_DST_COLOR,
  CANDID_BLEND_ONE_MINUS_DST_COLOR,
  CANDID_BLEND_SRC_ALPHA,
  CANDID_BLEND_ONE_MINUS_SRC_ALPHA,
  CANDID_BLEND_DST_ALPHA,
  CANDID_BLEND_ONE_MINUS_DST_ALPHA,
} Candid_BlendFactor;

typedef enum Candid_BlendOp {
  CANDID_BLEND_OP_ADD,
  CANDID_BLEND_OP_SUBTRACT,
  CANDID_BLEND_OP_REVERSE_SUBTRACT,
  CANDID_BLEND_OP_MIN,
  CANDID_BLEND_OP_MAX,
} Candid_BlendOp;

/*******************************************************************************
 * Primitive Topology
 ******************************************************************************/

typedef enum Candid_PrimitiveTopology {
  CANDID_PRIMITIVE_POINT_LIST,
  CANDID_PRIMITIVE_LINE_LIST,
  CANDID_PRIMITIVE_LINE_STRIP,
  CANDID_PRIMITIVE_TRIANGLE_LIST,
  CANDID_PRIMITIVE_TRIANGLE_STRIP,
} Candid_PrimitiveTopology;

/*******************************************************************************
 * Index Format
 ******************************************************************************/

typedef enum Candid_IndexFormat {
  CANDID_INDEX_FORMAT_UINT16,
  CANDID_INDEX_FORMAT_UINT32,
} Candid_IndexFormat;

/*******************************************************************************
 * Cull Mode / Front Face
 ******************************************************************************/

typedef enum Candid_CullMode {
  CANDID_CULL_NONE,
  CANDID_CULL_FRONT,
  CANDID_CULL_BACK,
} Candid_CullMode;

typedef enum Candid_FrontFace {
  CANDID_FRONT_FACE_CCW, /**< Counter-clockwise */
  CANDID_FRONT_FACE_CW,  /**< Clockwise */
} Candid_FrontFace;

#ifdef __cplusplus
}
#endif
