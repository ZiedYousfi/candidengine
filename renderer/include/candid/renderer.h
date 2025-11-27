/**
 * @file renderer.h
 * @brief Candid Engine Renderer - Main public API
 *
 * This is the main entry point for the rendering system. It provides a
 * high-level API for rendering that abstracts over platform-specific backends.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <candid/backend.h>
#include <candid/material.h>
#include <candid/mesh.h>
#include <candid/shader.h>
#include <candid/types.h>

/*******************************************************************************
 * Renderer Configuration
 ******************************************************************************/

typedef struct Candid_RendererConfig {
  Candid_Backend backend; /**< Backend to use (AUTO for best) */
  void *native_window;    /**< Platform window handle */
  void *native_surface;   /**< Platform surface (CAMetalLayer, etc.) */
  uint32_t width;
  uint32_t height;
  bool vsync;
  bool debug_mode;               /**< Enable validation/debug layers */
  uint32_t max_frames_in_flight; /**< 2 or 3 recommended */
  const char *app_name;
} Candid_RendererConfig;

/*******************************************************************************
 * Renderer Handle
 ******************************************************************************/

typedef struct Candid_Renderer Candid_Renderer;

/*******************************************************************************
 * Renderer Lifecycle
 ******************************************************************************/

/**
 * Create a new renderer instance
 * @param config Renderer configuration
 * @param out Output renderer handle
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_renderer_create(const Candid_RendererConfig *config,
                                     Candid_Renderer **out);

/**
 * Destroy a renderer instance
 * @param renderer Renderer to destroy
 */
void candid_renderer_destroy(Candid_Renderer *renderer);

/**
 * Resize the renderer surface
 * @param renderer Renderer instance
 * @param width New width
 * @param height New height
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_renderer_resize(Candid_Renderer *renderer, uint32_t width,
                                     uint32_t height);

/**
 * Get the current backend type
 * @param renderer Renderer instance
 * @return The active backend type
 */
Candid_Backend candid_renderer_get_backend(Candid_Renderer *renderer);

/**
 * Get device limits
 * @param renderer Renderer instance
 * @param out Output limits structure
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_renderer_get_limits(Candid_Renderer *renderer,
                                         Candid_DeviceLimits *out);

/*******************************************************************************
 * Resource Creation
 ******************************************************************************/

/**
 * Create a GPU buffer
 */
Candid_Result candid_renderer_create_buffer(Candid_Renderer *renderer,
                                            const Candid_BufferDesc *desc,
                                            Candid_Buffer **out);

/**
 * Destroy a GPU buffer
 */
void candid_renderer_destroy_buffer(Candid_Renderer *renderer,
                                    Candid_Buffer *buffer);

/**
 * Create a texture
 */
Candid_Result candid_renderer_create_texture(Candid_Renderer *renderer,
                                             const Candid_TextureDesc *desc,
                                             Candid_Texture **out);

/**
 * Destroy a texture
 */
void candid_renderer_destroy_texture(Candid_Renderer *renderer,
                                     Candid_Texture *texture);

/**
 * Create a sampler
 */
Candid_Result candid_renderer_create_sampler(Candid_Renderer *renderer,
                                             const Candid_SamplerDesc *desc,
                                             Candid_Sampler **out);

/**
 * Destroy a sampler
 */
void candid_renderer_destroy_sampler(Candid_Renderer *renderer,
                                     Candid_Sampler *sampler);

/**
 * Create a shader module from source or bytecode
 */
Candid_Result
candid_renderer_create_shader_module(Candid_Renderer *renderer,
                                     const Candid_ShaderModuleDesc *desc,
                                     Candid_ShaderModule **out);

/**
 * Destroy a shader module
 */
void candid_renderer_destroy_shader_module(Candid_Renderer *renderer,
                                           Candid_ShaderModule *module);

/**
 * Create a shader program from modules
 */
Candid_Result
candid_renderer_create_shader_program(Candid_Renderer *renderer,
                                      const Candid_ShaderProgramDesc *desc,
                                      Candid_ShaderProgram **out);

/**
 * Destroy a shader program
 */
void candid_renderer_destroy_shader_program(Candid_Renderer *renderer,
                                            Candid_ShaderProgram *program);

/**
 * Get a built-in shader program
 */
Candid_Result candid_renderer_get_builtin_shader(Candid_Renderer *renderer,
                                                 Candid_BuiltinShader shader,
                                                 Candid_ShaderProgram **out);

/**
 * Create a mesh from mesh data
 */
Candid_Result candid_renderer_create_mesh(Candid_Renderer *renderer,
                                          const Candid_MeshDesc *desc,
                                          Candid_Mesh **out);

/**
 * Destroy a mesh
 */
void candid_renderer_destroy_mesh(Candid_Renderer *renderer, Candid_Mesh *mesh);

/**
 * Create a material
 */
Candid_Result candid_renderer_create_material(Candid_Renderer *renderer,
                                              const Candid_MaterialDesc *desc,
                                              Candid_Material **out);

/**
 * Destroy a material
 */
void candid_renderer_destroy_material(Candid_Renderer *renderer,
                                      Candid_Material *material);

/*******************************************************************************
 * Frame Rendering
 ******************************************************************************/

/**
 * Begin a new frame
 * @param renderer Renderer instance
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_renderer_begin_frame(Candid_Renderer *renderer);

/**
 * End the current frame and present
 * @param renderer Renderer instance
 * @return CANDID_SUCCESS on success
 */
Candid_Result candid_renderer_end_frame(Candid_Renderer *renderer);

/**
 * Set clear color for the next render pass
 */
void candid_renderer_set_clear_color(Candid_Renderer *renderer,
                                     Candid_Color color);

/**
 * Set viewport
 */
void candid_renderer_set_viewport(Candid_Renderer *renderer, float x, float y,
                                  float width, float height);

/**
 * Set scissor rectangle
 */
void candid_renderer_set_scissor(Candid_Renderer *renderer, int32_t x,
                                 int32_t y, uint32_t width, uint32_t height);

/*******************************************************************************
 * Draw Commands
 ******************************************************************************/

/**
 * Draw a mesh with a material
 * @param renderer Renderer instance
 * @param mesh Mesh to draw
 * @param material Material to use
 * @param transform Model matrix
 */
void candid_renderer_draw_mesh(Candid_Renderer *renderer, Candid_Mesh *mesh,
                               Candid_Material *material,
                               const Candid_Mat4 *transform);

/**
 * Draw a mesh submesh with a material
 */
void candid_renderer_draw_submesh(Candid_Renderer *renderer, Candid_Mesh *mesh,
                                  uint32_t submesh_index,
                                  Candid_Material *material,
                                  const Candid_Mat4 *transform);

/**
 * Draw instanced meshes
 * @param renderer Renderer instance
 * @param mesh Mesh to draw
 * @param material Material to use
 * @param transforms Array of model matrices
 * @param instance_count Number of instances
 */
void candid_renderer_draw_mesh_instanced(Candid_Renderer *renderer,
                                         Candid_Mesh *mesh,
                                         Candid_Material *material,
                                         const Candid_Mat4 *transforms,
                                         uint32_t instance_count);

/*******************************************************************************
 * Camera / View Setup
 ******************************************************************************/

typedef struct Candid_Camera {
  Candid_Vec3 position;
  Candid_Vec3 target;
  Candid_Vec3 up;
  float fov_y; /**< Vertical FOV in radians */
  float near_plane;
  float far_plane;
  float aspect_ratio; /**< 0 = auto from surface */
} Candid_Camera;

/**
 * Set the camera for rendering
 */
void candid_renderer_set_camera(Candid_Renderer *renderer,
                                const Candid_Camera *camera);

/**
 * Set the view and projection matrices directly
 */
void candid_renderer_set_view_projection(Candid_Renderer *renderer,
                                         const Candid_Mat4 *view,
                                         const Candid_Mat4 *projection);

/*******************************************************************************
 * Utility Functions
 ******************************************************************************/

/**
 * Get the current frame time
 */
float candid_renderer_get_time(Candid_Renderer *renderer);

/**
 * Get the delta time since last frame
 */
float candid_renderer_get_delta_time(Candid_Renderer *renderer);

/**
 * Get the current frame number
 */
uint64_t candid_renderer_get_frame_count(Candid_Renderer *renderer);

/*******************************************************************************
 * Legacy API (for backwards compatibility)
 *
 * These functions maintain compatibility with the original simple API.
 * They create an internal renderer with default settings.
 ******************************************************************************/

/**
 * Legacy mesh type (deprecated, use Candid_MeshData instead)
 */
typedef struct {
  float *vertices[3];
  size_t vertex_count;
  size_t *triangle_vertex[3];
  size_t triangle_count;
} Candid_3D_Mesh;

/**
 * Create a cube mesh with the given size (legacy API)
 * @deprecated Use candid_mesh_create_cube instead
 */
Candid_3D_Mesh Candid_CreateCubeMesh(float size);

typedef struct Renderer Renderer;

/**
 * Create a simple renderer (legacy API)
 * @param native_window_surface Platform surface (CAMetalLayer, etc.)
 * @return Renderer handle or NULL on failure
 */
Renderer *renderer_create(void *native_window_surface);

/**
 * Resize the renderer surface (legacy API)
 */
void renderer_resize(Renderer *r, int width, int height);

/**
 * Set mesh data for the renderer (legacy API)
 * @param r Renderer instance
 * @param mesh Mesh data to use for rendering
 */
void renderer_set_mesh(Renderer *r, const Candid_3D_Mesh *mesh);

/**
 * Draw a frame with time-based animation (legacy API)
 */
void renderer_draw_frame(Renderer *r, float time);

/**
 * Destroy the renderer (legacy API)
 */
void renderer_destroy(Renderer *r);

#ifdef __cplusplus
}
#endif
