/**
 * @file backend.h
 * @brief Backend abstraction interface - platform-agnostic rendering API
 *
 * This header defines the interface that all rendering backends must implement.
 * The actual implementation is selected at compile time or runtime based on
 * the target platform.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <candid/material.h>
#include <candid/mesh.h>
#include <candid/shader.h>
#include <candid/types.h>

/*******************************************************************************
 * Device / Context
 ******************************************************************************/

typedef struct Candid_Device Candid_Device;
typedef struct Candid_Swapchain Candid_Swapchain;
typedef struct Candid_CommandBuffer Candid_CommandBuffer;

typedef struct Candid_DeviceDesc {
  Candid_Backend preferred_backend;
  void *native_window;  /**< Platform window handle */
  void *native_surface; /**< Platform surface (e.g., CAMetalLayer) */
  uint32_t width;
  uint32_t height;
  bool vsync;
  bool debug_mode; /**< Enable validation layers */
  const char *app_name;
} Candid_DeviceDesc;

typedef struct Candid_DeviceLimits {
  uint32_t max_texture_size;
  uint32_t max_cube_map_size;
  uint32_t max_texture_array_layers;
  uint32_t max_vertex_attributes;
  uint32_t max_vertex_buffers;
  uint32_t max_uniform_buffer_size;
  uint32_t max_storage_buffer_size;
  uint32_t max_compute_workgroup_size[3];
  uint32_t max_compute_workgroups[3];
  float max_anisotropy;
  bool supports_geometry_shader;
  bool supports_tessellation;
  bool supports_compute;
  bool supports_ray_tracing;
} Candid_DeviceLimits;

/*******************************************************************************
 * Backend Interface (Virtual Table)
 *
 * Each backend implements these functions. The renderer dispatches calls
 * through this interface.
 ******************************************************************************/

typedef struct Candid_BackendInterface {
  const char *name;
  Candid_Backend type;

  /* Device lifecycle */
  Candid_Result (*device_create)(const Candid_DeviceDesc *desc,
                                 Candid_Device **out);
  void (*device_destroy)(Candid_Device *device);
  Candid_Result (*device_get_limits)(Candid_Device *device,
                                     Candid_DeviceLimits *out);

  /* Swapchain */
  Candid_Result (*swapchain_resize)(Candid_Device *device, uint32_t width,
                                    uint32_t height);
  Candid_Result (*swapchain_present)(Candid_Device *device);

  /* Buffer operations */
  Candid_Result (*buffer_create)(Candid_Device *device,
                                 const Candid_BufferDesc *desc,
                                 Candid_Buffer **out);
  void (*buffer_destroy)(Candid_Device *device, Candid_Buffer *buffer);
  Candid_Result (*buffer_update)(Candid_Device *device, Candid_Buffer *buffer,
                                 size_t offset, const void *data, size_t size);
  void *(*buffer_map)(Candid_Device *device, Candid_Buffer *buffer);
  void (*buffer_unmap)(Candid_Device *device, Candid_Buffer *buffer);

  /* Texture operations */
  Candid_Result (*texture_create)(Candid_Device *device,
                                  const Candid_TextureDesc *desc,
                                  Candid_Texture **out);
  void (*texture_destroy)(Candid_Device *device, Candid_Texture *texture);
  Candid_Result (*texture_upload)(Candid_Device *device,
                                  Candid_Texture *texture, uint32_t mip_level,
                                  uint32_t array_layer, const void *data,
                                  size_t size);

  /* Sampler operations */
  Candid_Result (*sampler_create)(Candid_Device *device,
                                  const Candid_SamplerDesc *desc,
                                  Candid_Sampler **out);
  void (*sampler_destroy)(Candid_Device *device, Candid_Sampler *sampler);

  /* Shader operations */
  Candid_Result (*shader_module_create)(Candid_Device *device,
                                        const Candid_ShaderModuleDesc *desc,
                                        Candid_ShaderModule **out);
  void (*shader_module_destroy)(Candid_Device *device,
                                Candid_ShaderModule *module);
  Candid_Result (*shader_program_create)(Candid_Device *device,
                                         const Candid_ShaderProgramDesc *desc,
                                         Candid_ShaderProgram **out);
  void (*shader_program_destroy)(Candid_Device *device,
                                 Candid_ShaderProgram *program);

  /* Mesh operations */
  Candid_Result (*mesh_create)(Candid_Device *device,
                               const Candid_MeshDesc *desc, Candid_Mesh **out);
  void (*mesh_destroy)(Candid_Device *device, Candid_Mesh *mesh);

  /* Material operations */
  Candid_Result (*material_create)(Candid_Device *device,
                                   const Candid_MaterialDesc *desc,
                                   Candid_Material **out);
  void (*material_destroy)(Candid_Device *device, Candid_Material *material);

  /* Command buffer */
  Candid_Result (*cmd_begin)(Candid_Device *device, Candid_CommandBuffer **out);
  Candid_Result (*cmd_end)(Candid_Device *device, Candid_CommandBuffer *cmd);
  Candid_Result (*cmd_submit)(Candid_Device *device, Candid_CommandBuffer *cmd);

  /* Render pass */
  Candid_Result (*cmd_begin_render_pass)(Candid_CommandBuffer *cmd,
                                         const Candid_Color *clear_color,
                                         float clear_depth,
                                         uint8_t clear_stencil);
  void (*cmd_end_render_pass)(Candid_CommandBuffer *cmd);
  void (*cmd_set_viewport)(Candid_CommandBuffer *cmd, float x, float y,
                           float width, float height, float min_depth,
                           float max_depth);
  void (*cmd_set_scissor)(Candid_CommandBuffer *cmd, int32_t x, int32_t y,
                          uint32_t width, uint32_t height);

  /* Draw commands */
  void (*cmd_bind_pipeline)(Candid_CommandBuffer *cmd,
                            Candid_ShaderProgram *program,
                            const Candid_RasterizerState *raster,
                            const Candid_DepthStencilState *depth_stencil,
                            const Candid_BlendState *blend);
  void (*cmd_bind_vertex_buffer)(Candid_CommandBuffer *cmd, uint32_t slot,
                                 Candid_Buffer *buffer, size_t offset);
  void (*cmd_bind_index_buffer)(Candid_CommandBuffer *cmd,
                                Candid_Buffer *buffer, size_t offset,
                                Candid_IndexFormat format);
  void (*cmd_bind_uniform_buffer)(Candid_CommandBuffer *cmd, uint32_t slot,
                                  Candid_Buffer *buffer, size_t offset,
                                  size_t size);
  void (*cmd_bind_texture)(Candid_CommandBuffer *cmd, uint32_t slot,
                           Candid_Texture *texture, Candid_Sampler *sampler);
  void (*cmd_push_constants)(Candid_CommandBuffer *cmd,
                             Candid_ShaderStage stages, uint32_t offset,
                             const void *data, size_t size);
  void (*cmd_draw)(Candid_CommandBuffer *cmd, uint32_t vertex_count,
                   uint32_t instance_count, uint32_t first_vertex,
                   uint32_t first_instance);
  void (*cmd_draw_indexed)(Candid_CommandBuffer *cmd, uint32_t index_count,
                           uint32_t instance_count, uint32_t first_index,
                           int32_t vertex_offset, uint32_t first_instance);
  void (*cmd_draw_mesh)(Candid_CommandBuffer *cmd, Candid_Mesh *mesh,
                        Candid_Material *material,
                        const Candid_Mat4 *transform);

  /* Compute commands */
  void (*cmd_dispatch)(Candid_CommandBuffer *cmd, uint32_t x, uint32_t y,
                       uint32_t z);
} Candid_BackendInterface;

/*******************************************************************************
 * Backend Registration
 ******************************************************************************/

/**
 * Get the interface for a specific backend
 * @param backend The backend type
 * @return Pointer to the backend interface, or NULL if not available
 */
const Candid_BackendInterface *candid_backend_get(Candid_Backend backend);

/**
 * Get the best available backend for the current platform
 * @return The recommended backend type
 */
Candid_Backend candid_backend_get_preferred(void);

/**
 * Check if a backend is available on the current platform
 * @param backend The backend to check
 * @return true if available
 */
bool candid_backend_is_available(Candid_Backend backend);

/**
 * Get a list of all available backends
 * @param out Array to fill with available backends
 * @param max_count Maximum number of backends to return
 * @return Number of available backends
 */
uint32_t candid_backend_get_available(Candid_Backend *out, uint32_t max_count);

#ifdef __cplusplus
}
#endif
