/**
 * @file renderer.c
 * @brief High-level renderer API implementation
 */

#include <candid/renderer.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*******************************************************************************
 * Renderer Structure
 ******************************************************************************/

struct Candid_Renderer {
  Candid_Backend backend_type;
  const Candid_BackendInterface *backend;
  Candid_Device *device;
  Candid_Color clear_color;
  Candid_Mat4 view_matrix;
  Candid_Mat4 projection_matrix;
  float time;
  float delta_time;
  uint64_t frame_count;
  uint32_t width;
  uint32_t height;
};

/*******************************************************************************
 * Renderer Lifecycle
 ******************************************************************************/

Candid_Result candid_renderer_create(const Candid_RendererConfig *config,
                                     Candid_Renderer **out) {
  if (!config || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Renderer *renderer = calloc(1, sizeof(Candid_Renderer));
  if (!renderer)
    return CANDID_ERROR_OUT_OF_MEMORY;

  /* Select backend */
  Candid_Backend backend = config->backend;
  if (backend == CANDID_BACKEND_AUTO) {
    backend = candid_backend_get_preferred();
  }

  renderer->backend = candid_backend_get(backend);
  if (!renderer->backend) {
    free(renderer);
    return CANDID_ERROR_BACKEND_NOT_SUPPORTED;
  }

  renderer->backend_type = backend;

  /* Create device */
  Candid_DeviceDesc device_desc = {
      .preferred_backend = backend,
      .native_window = config->native_window,
      .native_surface = config->native_surface,
      .width = config->width,
      .height = config->height,
      .vsync = config->vsync,
      .debug_mode = config->debug_mode,
      .app_name = config->app_name,
  };

  Candid_Result result =
      renderer->backend->device_create(&device_desc, &renderer->device);
  if (result != CANDID_SUCCESS) {
    free(renderer);
    return result;
  }

  renderer->width = config->width;
  renderer->height = config->height;
  renderer->clear_color = (Candid_Color){0.2f, 0.2f, 0.2f, 1.0f};

  /* Initialize matrices to identity */
  memset(&renderer->view_matrix, 0, sizeof(renderer->view_matrix));
  renderer->view_matrix.m[0] = 1.0f;
  renderer->view_matrix.m[5] = 1.0f;
  renderer->view_matrix.m[10] = 1.0f;
  renderer->view_matrix.m[15] = 1.0f;

  memset(&renderer->projection_matrix, 0, sizeof(renderer->projection_matrix));
  renderer->projection_matrix.m[0] = 1.0f;
  renderer->projection_matrix.m[5] = 1.0f;
  renderer->projection_matrix.m[10] = 1.0f;
  renderer->projection_matrix.m[15] = 1.0f;

  *out = renderer;
  return CANDID_SUCCESS;
}

void candid_renderer_destroy(Candid_Renderer *renderer) {
  if (!renderer)
    return;

  if (renderer->backend && renderer->device) {
    renderer->backend->device_destroy(renderer->device);
  }

  free(renderer);
}

Candid_Result candid_renderer_resize(Candid_Renderer *renderer, uint32_t width,
                                     uint32_t height) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;

  renderer->width = width;
  renderer->height = height;

  return renderer->backend->swapchain_resize(renderer->device, width, height);
}

Candid_Backend candid_renderer_get_backend(Candid_Renderer *renderer) {
  if (!renderer)
    return CANDID_BACKEND_AUTO;
  return renderer->backend_type;
}

Candid_Result candid_renderer_get_limits(Candid_Renderer *renderer,
                                         Candid_DeviceLimits *out) {
  if (!renderer || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->device_get_limits(renderer->device, out);
}

/*******************************************************************************
 * Resource Creation (delegated to backend)
 ******************************************************************************/

Candid_Result candid_renderer_create_buffer(Candid_Renderer *renderer,
                                            const Candid_BufferDesc *desc,
                                            Candid_Buffer **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->buffer_create(renderer->device, desc, out);
}

void candid_renderer_destroy_buffer(Candid_Renderer *renderer,
                                    Candid_Buffer *buffer) {
  if (!renderer)
    return;
  renderer->backend->buffer_destroy(renderer->device, buffer);
}

Candid_Result candid_renderer_create_texture(Candid_Renderer *renderer,
                                             const Candid_TextureDesc *desc,
                                             Candid_Texture **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->texture_create(renderer->device, desc, out);
}

void candid_renderer_destroy_texture(Candid_Renderer *renderer,
                                     Candid_Texture *texture) {
  if (!renderer)
    return;
  renderer->backend->texture_destroy(renderer->device, texture);
}

Candid_Result candid_renderer_create_sampler(Candid_Renderer *renderer,
                                             const Candid_SamplerDesc *desc,
                                             Candid_Sampler **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->sampler_create(renderer->device, desc, out);
}

void candid_renderer_destroy_sampler(Candid_Renderer *renderer,
                                     Candid_Sampler *sampler) {
  if (!renderer)
    return;
  renderer->backend->sampler_destroy(renderer->device, sampler);
}

Candid_Result
candid_renderer_create_shader_module(Candid_Renderer *renderer,
                                     const Candid_ShaderModuleDesc *desc,
                                     Candid_ShaderModule **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->shader_module_create(renderer->device, desc, out);
}

void candid_renderer_destroy_shader_module(Candid_Renderer *renderer,
                                           Candid_ShaderModule *module) {
  if (!renderer)
    return;
  renderer->backend->shader_module_destroy(renderer->device, module);
}

Candid_Result
candid_renderer_create_shader_program(Candid_Renderer *renderer,
                                      const Candid_ShaderProgramDesc *desc,
                                      Candid_ShaderProgram **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->shader_program_create(renderer->device, desc, out);
}

void candid_renderer_destroy_shader_program(Candid_Renderer *renderer,
                                            Candid_ShaderProgram *program) {
  if (!renderer)
    return;
  renderer->backend->shader_program_destroy(renderer->device, program);
}

Candid_Result candid_renderer_get_builtin_shader(Candid_Renderer *renderer,
                                                 Candid_BuiltinShader shader,
                                                 Candid_ShaderProgram **out) {
  (void)renderer;
  (void)shader;
  (void)out;
  /* TODO: Implement built-in shader library */
  return CANDID_ERROR_RESOURCE_CREATION;
}

Candid_Result candid_renderer_create_mesh(Candid_Renderer *renderer,
                                          const Candid_MeshDesc *desc,
                                          Candid_Mesh **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->mesh_create(renderer->device, desc, out);
}

void candid_renderer_destroy_mesh(Candid_Renderer *renderer,
                                  Candid_Mesh *mesh) {
  if (!renderer)
    return;
  renderer->backend->mesh_destroy(renderer->device, mesh);
}

Candid_Result candid_renderer_create_material(Candid_Renderer *renderer,
                                              const Candid_MaterialDesc *desc,
                                              Candid_Material **out) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  return renderer->backend->material_create(renderer->device, desc, out);
}

void candid_renderer_destroy_material(Candid_Renderer *renderer,
                                      Candid_Material *material) {
  if (!renderer)
    return;
  renderer->backend->material_destroy(renderer->device, material);
}

/*******************************************************************************
 * Frame Rendering
 ******************************************************************************/

Candid_Result candid_renderer_begin_frame(Candid_Renderer *renderer) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;
  /* Frame tracking would happen here */
  return CANDID_SUCCESS;
}

Candid_Result candid_renderer_end_frame(Candid_Renderer *renderer) {
  if (!renderer)
    return CANDID_ERROR_INVALID_ARGUMENT;

  renderer->frame_count++;
  return renderer->backend->swapchain_present(renderer->device);
}

void candid_renderer_set_clear_color(Candid_Renderer *renderer,
                                     Candid_Color color) {
  if (!renderer)
    return;
  renderer->clear_color = color;
}

void candid_renderer_set_viewport(Candid_Renderer *renderer, float x, float y,
                                  float width, float height) {
  (void)renderer;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  /* Stored for use in render pass */
}

void candid_renderer_set_scissor(Candid_Renderer *renderer, int32_t x,
                                 int32_t y, uint32_t width, uint32_t height) {
  (void)renderer;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  /* Stored for use in render pass */
}

/*******************************************************************************
 * Draw Commands
 ******************************************************************************/

void candid_renderer_draw_mesh(Candid_Renderer *renderer, Candid_Mesh *mesh,
                               Candid_Material *material,
                               const Candid_Mat4 *transform) {
  (void)renderer;
  (void)mesh;
  (void)material;
  (void)transform;
  /* TODO: Queue draw command */
}

void candid_renderer_draw_submesh(Candid_Renderer *renderer, Candid_Mesh *mesh,
                                  uint32_t submesh_index,
                                  Candid_Material *material,
                                  const Candid_Mat4 *transform) {
  (void)renderer;
  (void)mesh;
  (void)submesh_index;
  (void)material;
  (void)transform;
  /* TODO: Queue draw command */
}

void candid_renderer_draw_mesh_instanced(Candid_Renderer *renderer,
                                         Candid_Mesh *mesh,
                                         Candid_Material *material,
                                         const Candid_Mat4 *transforms,
                                         uint32_t instance_count) {
  (void)renderer;
  (void)mesh;
  (void)material;
  (void)transforms;
  (void)instance_count;
  /* TODO: Queue instanced draw command */
}

/*******************************************************************************
 * Camera
 ******************************************************************************/

void candid_renderer_set_camera(Candid_Renderer *renderer,
                                const Candid_Camera *camera) {
  if (!renderer || !camera)
    return;

  float aspect = camera->aspect_ratio;
  if (aspect <= 0.0f && renderer->width > 0 && renderer->height > 0) {
    aspect = (float)renderer->width / (float)renderer->height;
  }

  /* Calculate view matrix (look-at) */
  Candid_Vec3 f = {camera->target.x - camera->position.x,
                   camera->target.y - camera->position.y,
                   camera->target.z - camera->position.z};
  float f_len = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z);
  if (f_len > 0.0f) {
    f.x /= f_len;
    f.y /= f_len;
    f.z /= f_len;
  }

  Candid_Vec3 s = {f.y * camera->up.z - f.z * camera->up.y,
                   f.z * camera->up.x - f.x * camera->up.z,
                   f.x * camera->up.y - f.y * camera->up.x};
  float s_len = sqrtf(s.x * s.x + s.y * s.y + s.z * s.z);
  if (s_len > 0.0f) {
    s.x /= s_len;
    s.y /= s_len;
    s.z /= s_len;
  }

  Candid_Vec3 u = {s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z,
                   s.x * f.y - s.y * f.x};

  renderer->view_matrix.m[0] = s.x;
  renderer->view_matrix.m[1] = u.x;
  renderer->view_matrix.m[2] = -f.x;
  renderer->view_matrix.m[3] = 0.0f;
  renderer->view_matrix.m[4] = s.y;
  renderer->view_matrix.m[5] = u.y;
  renderer->view_matrix.m[6] = -f.y;
  renderer->view_matrix.m[7] = 0.0f;
  renderer->view_matrix.m[8] = s.z;
  renderer->view_matrix.m[9] = u.z;
  renderer->view_matrix.m[10] = -f.z;
  renderer->view_matrix.m[11] = 0.0f;
  renderer->view_matrix.m[12] =
      -(s.x * camera->position.x + s.y * camera->position.y +
        s.z * camera->position.z);
  renderer->view_matrix.m[13] =
      -(u.x * camera->position.x + u.y * camera->position.y +
        u.z * camera->position.z);
  renderer->view_matrix.m[14] = f.x * camera->position.x +
                                f.y * camera->position.y +
                                f.z * camera->position.z;
  renderer->view_matrix.m[15] = 1.0f;

  /* Calculate projection matrix (perspective) */
  float tan_half_fov = tanf(camera->fov_y * 0.5f);
  float range = camera->far_plane - camera->near_plane;

  memset(&renderer->projection_matrix, 0, sizeof(renderer->projection_matrix));
  renderer->projection_matrix.m[0] = 1.0f / (aspect * tan_half_fov);
  renderer->projection_matrix.m[5] = 1.0f / tan_half_fov;
  renderer->projection_matrix.m[10] =
      -(camera->far_plane + camera->near_plane) / range;
  renderer->projection_matrix.m[11] = -1.0f;
  renderer->projection_matrix.m[14] =
      -(2.0f * camera->far_plane * camera->near_plane) / range;
}

void candid_renderer_set_view_projection(Candid_Renderer *renderer,
                                         const Candid_Mat4 *view,
                                         const Candid_Mat4 *projection) {
  if (!renderer)
    return;
  if (view)
    renderer->view_matrix = *view;
  if (projection)
    renderer->projection_matrix = *projection;
}

/*******************************************************************************
 * Utility Functions
 ******************************************************************************/

float candid_renderer_get_time(Candid_Renderer *renderer) {
  if (!renderer)
    return 0.0f;
  return renderer->time;
}

float candid_renderer_get_delta_time(Candid_Renderer *renderer) {
  if (!renderer)
    return 0.0f;
  return renderer->delta_time;
}

uint64_t candid_renderer_get_frame_count(Candid_Renderer *renderer) {
  if (!renderer)
    return 0;
  return renderer->frame_count;
}

/*******************************************************************************
 * Legacy API (backward compatibility with old Candid_3D_Mesh)
 ******************************************************************************/

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
