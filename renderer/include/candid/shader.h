/**
 * @file shader.h
 * @brief Shader abstraction with HLSL-like semantics and cross-compilation
 *
 * The shader system uses HLSL as the primary shading language and
 * cross-compiles to native formats (Metal Shading Language, SPIR-V) at build
 * time or runtime.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <candid/mesh.h>
#include <candid/types.h>

/*******************************************************************************
 * Shader Stage
 ******************************************************************************/

typedef enum Candid_ShaderStage {
  CANDID_SHADER_STAGE_VERTEX = 1 << 0,
  CANDID_SHADER_STAGE_FRAGMENT = 1 << 1,
  CANDID_SHADER_STAGE_COMPUTE = 1 << 2,
  CANDID_SHADER_STAGE_GEOMETRY = 1 << 3, /**< Not supported on all backends */
  CANDID_SHADER_STAGE_TESSELLATION = 1
                                     << 4, /**< Not supported on all backends */
} Candid_ShaderStage;

/*******************************************************************************
 * Shader Source Types
 ******************************************************************************/

typedef enum Candid_ShaderSourceType {
  CANDID_SHADER_SOURCE_HLSL,     /**< HLSL source code */
  CANDID_SHADER_SOURCE_MSL,      /**< Metal Shading Language */
  CANDID_SHADER_SOURCE_GLSL,     /**< GLSL source code */
  CANDID_SHADER_SOURCE_SPIRV,    /**< Pre-compiled SPIR-V bytecode */
  CANDID_SHADER_SOURCE_METALLIB, /**< Pre-compiled Metal library */
  CANDID_SHADER_SOURCE_DXIL,     /**< Pre-compiled DXIL bytecode */
} Candid_ShaderSourceType;

/*******************************************************************************
 * Shader Compilation
 ******************************************************************************/

typedef struct Candid_ShaderCompileOptions {
  Candid_ShaderStage stage;
  const char *entry_point; /**< Entry function name (e.g., "main", "VSMain") */
  const char *target_profile;  /**< Target profile (e.g., "vs_6_0", "ps_6_0") */
  bool enable_debug;           /**< Include debug info */
  bool optimize;               /**< Enable optimizations */
  uint32_t optimization_level; /**< 0-3, higher = more aggressive */
  const char **defines;        /**< Preprocessor defines (name=value pairs) */
  uint32_t define_count;
  const char **include_paths; /**< Include search paths */
  uint32_t include_path_count;
} Candid_ShaderCompileOptions;

typedef struct Candid_ShaderBytecode {
  const void *data;
  size_t size;
  Candid_ShaderSourceType type;
} Candid_ShaderBytecode;

/*******************************************************************************
 * Shader Module (single stage)
 ******************************************************************************/

typedef struct Candid_ShaderModuleDesc {
  Candid_ShaderStage stage;
  const char *source; /**< Source code (if source_type is a text format) */
  size_t source_size; /**< Length of source (0 = null-terminated) */
  Candid_ShaderSourceType source_type;
  const void *bytecode; /**< Pre-compiled bytecode (if source_type is binary) */
  size_t bytecode_size;
  const char *entry_point; /**< Entry function name */
  const char *label;       /**< Debug label */
} Candid_ShaderModuleDesc;

typedef struct Candid_ShaderModule Candid_ShaderModule;

/*******************************************************************************
 * Shader Program (linked stages)
 ******************************************************************************/

typedef struct Candid_ShaderProgramDesc {
  Candid_ShaderModule *vertex;
  Candid_ShaderModule *fragment;
  Candid_ShaderModule *compute;
  const char *label;
} Candid_ShaderProgramDesc;

typedef struct Candid_ShaderProgram Candid_ShaderProgram;

/*******************************************************************************
 * Uniform / Constant Buffer Binding
 ******************************************************************************/

typedef enum Candid_UniformType {
  CANDID_UNIFORM_FLOAT,
  CANDID_UNIFORM_FLOAT2,
  CANDID_UNIFORM_FLOAT3,
  CANDID_UNIFORM_FLOAT4,
  CANDID_UNIFORM_INT,
  CANDID_UNIFORM_INT2,
  CANDID_UNIFORM_INT3,
  CANDID_UNIFORM_INT4,
  CANDID_UNIFORM_MAT3,
  CANDID_UNIFORM_MAT4,
  CANDID_UNIFORM_SAMPLER,
  CANDID_UNIFORM_TEXTURE,
  CANDID_UNIFORM_BUFFER,
} Candid_UniformType;

typedef struct Candid_UniformDesc {
  const char *name;
  Candid_UniformType type;
  uint32_t binding;          /**< Binding point / register */
  uint32_t set;              /**< Descriptor set (Vulkan) / space (DX12) */
  Candid_ShaderStage stages; /**< Which stages use this uniform */
  size_t size;               /**< Size in bytes (for buffers) */
  uint32_t array_count;      /**< 1 for non-arrays */
} Candid_UniformDesc;

/*******************************************************************************
 * Shader Reflection
 ******************************************************************************/

#define CANDID_MAX_UNIFORMS 64
#define CANDID_MAX_VERTEX_INPUTS 16
#define CANDID_MAX_RENDER_TARGETS 8

typedef struct Candid_ShaderReflection {
  Candid_UniformDesc uniforms[CANDID_MAX_UNIFORMS];
  uint32_t uniform_count;

  struct {
    const char *name;
    Candid_VertexSemantic semantic;
    Candid_VertexFormat format;
    uint32_t location;
  } vertex_inputs[CANDID_MAX_VERTEX_INPUTS];
  uint32_t vertex_input_count;

  struct {
    Candid_TextureFormat format;
    uint32_t location;
  } render_targets[CANDID_MAX_RENDER_TARGETS];
  uint32_t render_target_count;

  bool has_depth_output;
} Candid_ShaderReflection;

/*******************************************************************************
 * Built-in Shader Library
 ******************************************************************************/

/**
 * Built-in shaders provided by the engine
 */
typedef enum Candid_BuiltinShader {
  CANDID_SHADER_UNLIT,         /**< Simple unlit shader */
  CANDID_SHADER_BLINN_PHONG,   /**< Classic Blinn-Phong lighting */
  CANDID_SHADER_PBR_METALLIC,  /**< PBR metallic-roughness workflow */
  CANDID_SHADER_PBR_SPECULAR,  /**< PBR specular-glossiness workflow */
  CANDID_SHADER_SKYBOX,        /**< Skybox/environment map */
  CANDID_SHADER_SHADOW_MAP,    /**< Shadow map generation */
  CANDID_SHADER_POST_TONEMAP,  /**< HDR tonemapping */
  CANDID_SHADER_POST_FXAA,     /**< FXAA anti-aliasing */
  CANDID_SHADER_DEBUG_NORMALS, /**< Visualize normals */
  CANDID_SHADER_DEBUG_UV,      /**< Visualize UVs */
  CANDID_SHADER_COUNT
} Candid_BuiltinShader;

/*******************************************************************************
 * Shader Compilation API
 ******************************************************************************/

/**
 * Compile HLSL source to the target backend format
 * @param source HLSL source code
 * @param source_size Length of source (0 = null-terminated)
 * @param options Compilation options
 * @param out_bytecode Output bytecode (caller must free with
 * candid_shader_free_bytecode)
 * @return CANDID_SUCCESS on success
 */
Candid_Result
candid_shader_compile_hlsl(const char *source, size_t source_size,
                           const Candid_ShaderCompileOptions *options,
                           Candid_ShaderBytecode *out_bytecode);

/**
 * Free bytecode allocated by shader compilation
 */
void candid_shader_free_bytecode(Candid_ShaderBytecode *bytecode);

/**
 * Get reflection data from compiled shader
 * @param bytecode Compiled shader bytecode
 * @param out_reflection Output reflection data
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_shader_reflect(const Candid_ShaderBytecode *bytecode,
                                    Candid_ShaderReflection *out_reflection);

/**
 * Load pre-compiled shader from file
 * @param path Path to shader file
 * @param out_bytecode Output bytecode
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_shader_load_file(const char *path,
                                      Candid_ShaderBytecode *out_bytecode);

#ifdef __cplusplus
}
#endif

/*******************************************************************************
 * HLSL Shader Macros and Conventions
 *
 * The engine expects shaders to follow these conventions:
 *
 * Vertex Input Semantics:
 *   POSITION   - float3 position
 *   NORMAL     - float3 normal
 *   TANGENT    - float4 tangent (w = handedness)
 *   TEXCOORD0  - float2 primary UV
 *   TEXCOORD1  - float2 secondary UV
 *   COLOR0     - float4 vertex color
 *   BLENDWEIGHT- float4 blend weights (skinning)
 *   BLENDINDICES-uint4 blend indices (skinning)
 *
 * Constant Buffer Slots:
 *   b0 - Per-frame data (camera, time, etc.)
 *   b1 - Per-object data (model matrix, etc.)
 *   b2 - Per-material data
 *   b3+ - Custom
 *
 * Texture Slots:
 *   t0 - Albedo/Diffuse
 *   t1 - Normal map
 *   t2 - Metallic/Roughness or Specular/Glossiness
 *   t3 - Ambient Occlusion
 *   t4 - Emissive
 *   t5+ - Custom
 *
 * Sampler Slots:
 *   s0 - Linear wrap
 *   s1 - Linear clamp
 *   s2 - Point wrap
 *   s3 - Point clamp
 *   s4+ - Custom
 *
 * Example Vertex Shader:
 *
 *   cbuffer PerFrame : register(b0) {
 *       float4x4 ViewProjection;
 *       float3 CameraPosition;
 *       float Time;
 *   };
 *
 *   cbuffer PerObject : register(b1) {
 *       float4x4 Model;
 *       float4x4 NormalMatrix;
 *   };
 *
 *   struct VSInput {
 *       float3 Position : POSITION;
 *       float3 Normal   : NORMAL;
 *       float2 TexCoord : TEXCOORD0;
 *   };
 *
 *   struct VSOutput {
 *       float4 Position : SV_Position;
 *       float3 WorldPos : TEXCOORD0;
 *       float3 Normal   : TEXCOORD1;
 *       float2 TexCoord : TEXCOORD2;
 *   };
 *
 *   VSOutput VSMain(VSInput input) {
 *       VSOutput output;
 *       float4 worldPos = mul(Model, float4(input.Position, 1.0));
 *       output.Position = mul(ViewProjection, worldPos);
 *       output.WorldPos = worldPos.xyz;
 *       output.Normal = mul((float3x3)NormalMatrix, input.Normal);
 *       output.TexCoord = input.TexCoord;
 *       return output;
 *   }
 *
 ******************************************************************************/
