/**
 * @file material.h
 * @brief Material system with shader and texture bindings
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <candid/shader.h>
#include <candid/types.h>

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/

typedef struct Candid_Texture Candid_Texture;
typedef struct Candid_Sampler Candid_Sampler;
typedef struct Candid_Buffer Candid_Buffer;

/*******************************************************************************
 * Material Types
 ******************************************************************************/

typedef enum Candid_AlphaMode {
  CANDID_ALPHA_MODE_OPAQUE,
  CANDID_ALPHA_MODE_MASK,
  CANDID_ALPHA_MODE_BLEND,
} Candid_AlphaMode;

/**
 * PBR Metallic-Roughness material properties
 */
typedef struct Candid_PBRMetallicRoughness {
  Candid_Color base_color_factor;
  float metallic_factor;
  float roughness_factor;
  Candid_Texture *base_color_texture;
  Candid_Texture *metallic_roughness_texture;
} Candid_PBRMetallicRoughness;

/**
 * PBR Specular-Glossiness material properties
 */
typedef struct Candid_PBRSpecularGlossiness {
  Candid_Color diffuse_factor;
  Candid_Vec3 specular_factor;
  float glossiness_factor;
  Candid_Texture *diffuse_texture;
  Candid_Texture *specular_glossiness_texture;
} Candid_PBRSpecularGlossiness;

/**
 * Common material properties
 */
typedef struct Candid_MaterialDesc {
  const char *name;
  Candid_ShaderProgram *shader; /**< Custom shader (NULL = use default) */

  /* PBR Properties (metallic-roughness is default) */
  bool use_specular_glossiness;
  union {
    Candid_PBRMetallicRoughness metallic_roughness;
    Candid_PBRSpecularGlossiness specular_glossiness;
  } pbr;

  /* Common textures */
  Candid_Texture *normal_texture;
  float normal_scale;
  Candid_Texture *occlusion_texture;
  float occlusion_strength;
  Candid_Texture *emissive_texture;
  Candid_Vec3 emissive_factor;

  /* Alpha */
  Candid_AlphaMode alpha_mode;
  float alpha_cutoff; /**< For CANDID_ALPHA_MODE_MASK */

  /* Rendering hints */
  bool double_sided;
  bool unlit; /**< Ignore lighting, use base color only */

  /* Custom uniform buffer (for custom shaders) */
  Candid_Buffer *custom_uniforms;
} Candid_MaterialDesc;

typedef struct Candid_Material Candid_Material;

/*******************************************************************************
 * Render State
 ******************************************************************************/

typedef struct Candid_BlendState {
  bool enabled;
  Candid_BlendFactor src_color;
  Candid_BlendFactor dst_color;
  Candid_BlendOp color_op;
  Candid_BlendFactor src_alpha;
  Candid_BlendFactor dst_alpha;
  Candid_BlendOp alpha_op;
  uint8_t write_mask; /**< RGBA bits */
} Candid_BlendState;

typedef struct Candid_DepthStencilState {
  bool depth_test_enabled;
  bool depth_write_enabled;
  Candid_CompareFunc depth_compare;
  bool stencil_enabled;
  /* Stencil ops omitted for brevity */
} Candid_DepthStencilState;

typedef struct Candid_RasterizerState {
  Candid_CullMode cull_mode;
  Candid_FrontFace front_face;
  bool wireframe;
  float depth_bias;
  float depth_bias_slope_scale;
  bool depth_clip_enabled;
  bool scissor_enabled;
} Candid_RasterizerState;

#ifdef __cplusplus
}
#endif
