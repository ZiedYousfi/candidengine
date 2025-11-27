/**
 * @file backend_vulkan.c
 * @brief Vulkan backend implementation for cross-platform support
 *
 * This implements the Candid_BackendInterface for Vulkan API.
 * Vulkan support requires the volk library for dynamic loading.
 */

#include <candid/backend.h>
#include <stdlib.h>
#include <string.h>

#ifdef CANDID_VULKAN_SUPPORT

#define VOLK_IMPLEMENTATION
#include <volk.h>

/*******************************************************************************
 * Internal Structures
 ******************************************************************************/

struct Candid_Device {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  uint32_t swapchain_image_count;
  VkRenderPass render_pass;
  VkFramebuffer *framebuffers;
  VkCommandPool command_pool;
  VkCommandBuffer *command_buffers;
  VkSemaphore *image_available_semaphores;
  VkSemaphore *render_finished_semaphores;
  VkFence *in_flight_fences;
  uint32_t current_frame;
  uint32_t max_frames_in_flight;
  VkImage depth_image;
  VkDeviceMemory depth_image_memory;
  VkImageView depth_image_view;
  VkDebugUtilsMessengerEXT debug_messenger;
  bool validation_enabled;
  uint32_t width;
  uint32_t height;
  uint32_t graphics_family;
  uint32_t present_family;
};

struct Candid_Buffer {
  VkBuffer buffer;
  VkDeviceMemory memory;
  size_t size;
  Candid_BufferMemory memory_type;
  void *mapped;
};

struct Candid_Texture {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  Candid_TextureDesc desc;
};

struct Candid_Sampler {
  VkSampler sampler;
};

struct Candid_ShaderModule {
  VkShaderModule module;
  Candid_ShaderStage stage;
};

struct Candid_ShaderProgram {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  VkDescriptorSetLayout descriptor_set_layout;
  Candid_ShaderModule *vertex;
  Candid_ShaderModule *fragment;
};

struct Candid_Mesh {
  Candid_Buffer *vertex_buffer;
  Candid_Buffer *index_buffer;
  uint32_t vertex_count;
  uint32_t index_count;
  Candid_IndexFormat index_format;
  Candid_VertexLayout layout;
  Candid_AABB bounds;
};

struct Candid_Material {
  Candid_ShaderProgram *shader;
  Candid_MaterialDesc desc;
  VkDescriptorSet descriptor_set;
};

struct Candid_CommandBuffer {
  VkCommandBuffer vk_command_buffer;
  Candid_Device *device;
  uint32_t image_index;
  bool in_render_pass;
};

/*******************************************************************************
 * Validation Layer Callback
 ******************************************************************************/

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
               VkDebugUtilsMessageTypeFlagsEXT type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
  (void)type;
  (void)user_data;

  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    fprintf(stderr, "[Vulkan Validation] %s\n", callback_data->pMessage);
  }

  return VK_FALSE;
}

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  return UINT32_MAX;
}

static VkFormat texture_format_to_vk(Candid_TextureFormat format) {
  switch (format) {
  case CANDID_TEXTURE_FORMAT_RGBA8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case CANDID_TEXTURE_FORMAT_RGBA8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case CANDID_TEXTURE_FORMAT_BGRA8_UNORM:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case CANDID_TEXTURE_FORMAT_BGRA8_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case CANDID_TEXTURE_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;
  case CANDID_TEXTURE_FORMAT_RG8_UNORM:
    return VK_FORMAT_R8G8_UNORM;
  case CANDID_TEXTURE_FORMAT_RGBA16_FLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case CANDID_TEXTURE_FORMAT_RGBA32_FLOAT:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case CANDID_TEXTURE_FORMAT_DEPTH32_FLOAT:
    return VK_FORMAT_D32_SFLOAT;
  case CANDID_TEXTURE_FORMAT_DEPTH24_STENCIL8:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

static VkCompareOp compare_func_to_vk(Candid_CompareFunc func) {
  switch (func) {
  case CANDID_COMPARE_NEVER:
    return VK_COMPARE_OP_NEVER;
  case CANDID_COMPARE_LESS:
    return VK_COMPARE_OP_LESS;
  case CANDID_COMPARE_EQUAL:
    return VK_COMPARE_OP_EQUAL;
  case CANDID_COMPARE_LESS_EQUAL:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case CANDID_COMPARE_GREATER:
    return VK_COMPARE_OP_GREATER;
  case CANDID_COMPARE_NOT_EQUAL:
    return VK_COMPARE_OP_NOT_EQUAL;
  case CANDID_COMPARE_GREATER_EQUAL:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case CANDID_COMPARE_ALWAYS:
    return VK_COMPARE_OP_ALWAYS;
  default:
    return VK_COMPARE_OP_LESS;
  }
}

static VkFilter sampler_filter_to_vk(Candid_SamplerFilter filter) {
  switch (filter) {
  case CANDID_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;
  case CANDID_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  default:
    return VK_FILTER_LINEAR;
  }
}

static VkSamplerAddressMode
sampler_address_to_vk(Candid_SamplerAddressMode mode) {
  switch (mode) {
  case CANDID_SAMPLER_ADDRESS_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case CANDID_SAMPLER_ADDRESS_MIRROR_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case CANDID_SAMPLER_ADDRESS_CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case CANDID_SAMPLER_ADDRESS_CLAMP_TO_BORDER:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  default:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

/*******************************************************************************
 * Device Functions (Stubs - to be implemented)
 ******************************************************************************/

static Candid_Result vulkan_device_create(const Candid_DeviceDesc *desc,
                                          Candid_Device **out) {
  if (!desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  /* Initialize Volk */
  VkResult result = volkInitialize();
  if (result != VK_SUCCESS) {
    return CANDID_ERROR_BACKEND_NOT_SUPPORTED;
  }

  Candid_Device *device = calloc(1, sizeof(Candid_Device));
  if (!device)
    return CANDID_ERROR_OUT_OF_MEMORY;

  device->width = desc->width;
  device->height = desc->height;
  device->validation_enabled = desc->debug_mode;
  device->max_frames_in_flight = 2;

  /* Create Vulkan instance */
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = desc->app_name ? desc->app_name : "Candid Engine",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Candid Engine",
      .engineVersion = VK_MAKE_VERSION(0, 1, 0),
      .apiVersion = VK_API_VERSION_1_2,
  };

  const char *extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
      VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
      VK_EXT_METAL_SURFACE_EXTENSION_NAME,
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  };

  const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]),
      .ppEnabledExtensionNames = extensions,
#if defined(__APPLE__)
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
  };

  if (desc->debug_mode) {
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = validation_layers;
  }

  result = vkCreateInstance(&create_info, NULL, &device->instance);
  if (result != VK_SUCCESS) {
    free(device);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  volkLoadInstance(device->instance);

  /* Setup debug messenger if validation enabled */
  if (desc->debug_mode) {
    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };
    vkCreateDebugUtilsMessengerEXT(device->instance, &debug_info, NULL,
                                   &device->debug_messenger);
  }

  /* TODO: Complete Vulkan initialization:
   * 1. Create surface from native_surface
   * 2. Pick physical device
   * 3. Create logical device with queues
   * 4. Create swapchain
   * 5. Create render pass
   * 6. Create framebuffers
   * 7. Create command pool and buffers
   * 8. Create synchronization objects
   */

  *out = device;
  return CANDID_SUCCESS;
}

static void vulkan_device_destroy(Candid_Device *device) {
  if (!device)
    return;

  if (device->device) {
    vkDeviceWaitIdle(device->device);
  }

  /* Cleanup resources in reverse order */
  /* TODO: Destroy all Vulkan resources */

  if (device->debug_messenger) {
    vkDestroyDebugUtilsMessengerEXT(device->instance, device->debug_messenger,
                                    NULL);
  }

  if (device->instance) {
    vkDestroyInstance(device->instance, NULL);
  }

  free(device);
}

static Candid_Result vulkan_device_get_limits(Candid_Device *device,
                                              Candid_DeviceLimits *out) {
  if (!device || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  memset(out, 0, sizeof(*out));

  if (device->physical_device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device->physical_device, &props);

    out->max_texture_size = props.limits.maxImageDimension2D;
    out->max_cube_map_size = props.limits.maxImageDimensionCube;
    out->max_texture_array_layers = props.limits.maxImageArrayLayers;
    out->max_vertex_attributes = props.limits.maxVertexInputAttributes;
    out->max_vertex_buffers = props.limits.maxVertexInputBindings;
    out->max_uniform_buffer_size = props.limits.maxUniformBufferRange;
    out->max_storage_buffer_size = props.limits.maxStorageBufferRange;
    out->max_compute_workgroup_size[0] =
        props.limits.maxComputeWorkGroupSize[0];
    out->max_compute_workgroup_size[1] =
        props.limits.maxComputeWorkGroupSize[1];
    out->max_compute_workgroup_size[2] =
        props.limits.maxComputeWorkGroupSize[2];
    out->max_compute_workgroups[0] = props.limits.maxComputeWorkGroupCount[0];
    out->max_compute_workgroups[1] = props.limits.maxComputeWorkGroupCount[1];
    out->max_compute_workgroups[2] = props.limits.maxComputeWorkGroupCount[2];
    out->max_anisotropy = props.limits.maxSamplerAnisotropy;

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device->physical_device, &features);
    out->supports_geometry_shader = features.geometryShader;
    out->supports_tessellation = features.tessellationShader;
    out->supports_compute = true;
  }

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Stub implementations for remaining functions
 ******************************************************************************/

static Candid_Result vulkan_swapchain_resize(Candid_Device *device,
                                             uint32_t width, uint32_t height) {
  if (!device)
    return CANDID_ERROR_INVALID_ARGUMENT;
  device->width = width;
  device->height = height;
  /* TODO: Recreate swapchain */
  return CANDID_SUCCESS;
}

static Candid_Result vulkan_swapchain_present(Candid_Device *device) {
  (void)device;
  return CANDID_SUCCESS;
}

static Candid_Result vulkan_buffer_create(Candid_Device *device,
                                          const Candid_BufferDesc *desc,
                                          Candid_Buffer **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_buffer_destroy(Candid_Device *device,
                                  Candid_Buffer *buffer) {
  (void)device;
  (void)buffer;
}

static Candid_Result vulkan_buffer_update(Candid_Device *device,
                                          Candid_Buffer *buffer, size_t offset,
                                          const void *data, size_t size) {
  (void)device;
  (void)buffer;
  (void)offset;
  (void)data;
  (void)size;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void *vulkan_buffer_map(Candid_Device *device, Candid_Buffer *buffer) {
  (void)device;
  (void)buffer;
  return NULL;
}

static void vulkan_buffer_unmap(Candid_Device *device, Candid_Buffer *buffer) {
  (void)device;
  (void)buffer;
}

static Candid_Result vulkan_texture_create(Candid_Device *device,
                                           const Candid_TextureDesc *desc,
                                           Candid_Texture **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_texture_destroy(Candid_Device *device,
                                   Candid_Texture *texture) {
  (void)device;
  (void)texture;
}

static Candid_Result vulkan_texture_upload(Candid_Device *device,
                                           Candid_Texture *texture,
                                           uint32_t mip_level,
                                           uint32_t array_layer,
                                           const void *data, size_t size) {
  (void)device;
  (void)texture;
  (void)mip_level;
  (void)array_layer;
  (void)data;
  (void)size;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static Candid_Result vulkan_sampler_create(Candid_Device *device,
                                           const Candid_SamplerDesc *desc,
                                           Candid_Sampler **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_sampler_destroy(Candid_Device *device,
                                   Candid_Sampler *sampler) {
  (void)device;
  (void)sampler;
}

static Candid_Result
vulkan_shader_module_create(Candid_Device *device,
                            const Candid_ShaderModuleDesc *desc,
                            Candid_ShaderModule **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_shader_module_destroy(Candid_Device *device,
                                         Candid_ShaderModule *module) {
  (void)device;
  (void)module;
}

static Candid_Result
vulkan_shader_program_create(Candid_Device *device,
                             const Candid_ShaderProgramDesc *desc,
                             Candid_ShaderProgram **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_shader_program_destroy(Candid_Device *device,
                                          Candid_ShaderProgram *program) {
  (void)device;
  (void)program;
}

static Candid_Result vulkan_mesh_create(Candid_Device *device,
                                        const Candid_MeshDesc *desc,
                                        Candid_Mesh **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_mesh_destroy(Candid_Device *device, Candid_Mesh *mesh) {
  (void)device;
  (void)mesh;
}

static Candid_Result vulkan_material_create(Candid_Device *device,
                                            const Candid_MaterialDesc *desc,
                                            Candid_Material **out) {
  (void)device;
  (void)desc;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static void vulkan_material_destroy(Candid_Device *device,
                                    Candid_Material *material) {
  (void)device;
  (void)material;
}

static Candid_Result vulkan_cmd_begin(Candid_Device *device,
                                      Candid_CommandBuffer **out) {
  (void)device;
  (void)out;
  return CANDID_ERROR_RESOURCE_CREATION;
}

static Candid_Result vulkan_cmd_end(Candid_Device *device,
                                    Candid_CommandBuffer *cmd) {
  (void)device;
  (void)cmd;
  return CANDID_SUCCESS;
}

static Candid_Result vulkan_cmd_submit(Candid_Device *device,
                                       Candid_CommandBuffer *cmd) {
  (void)device;
  (void)cmd;
  return CANDID_SUCCESS;
}

static Candid_Result
vulkan_cmd_begin_render_pass(Candid_CommandBuffer *cmd,
                             const Candid_Color *clear_color, float clear_depth,
                             uint8_t clear_stencil) {
  (void)cmd;
  (void)clear_color;
  (void)clear_depth;
  (void)clear_stencil;
  return CANDID_SUCCESS;
}

static void vulkan_cmd_end_render_pass(Candid_CommandBuffer *cmd) { (void)cmd; }

static void vulkan_cmd_set_viewport(Candid_CommandBuffer *cmd, float x, float y,
                                    float width, float height, float min_depth,
                                    float max_depth) {
  (void)cmd;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)min_depth;
  (void)max_depth;
}

static void vulkan_cmd_set_scissor(Candid_CommandBuffer *cmd, int32_t x,
                                   int32_t y, uint32_t width, uint32_t height) {
  (void)cmd;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

static void
vulkan_cmd_bind_pipeline(Candid_CommandBuffer *cmd,
                         Candid_ShaderProgram *program,
                         const Candid_RasterizerState *raster,
                         const Candid_DepthStencilState *depth_stencil,
                         const Candid_BlendState *blend) {
  (void)cmd;
  (void)program;
  (void)raster;
  (void)depth_stencil;
  (void)blend;
}

static void vulkan_cmd_bind_vertex_buffer(Candid_CommandBuffer *cmd,
                                          uint32_t slot, Candid_Buffer *buffer,
                                          size_t offset) {
  (void)cmd;
  (void)slot;
  (void)buffer;
  (void)offset;
}

static void vulkan_cmd_bind_index_buffer(Candid_CommandBuffer *cmd,
                                         Candid_Buffer *buffer, size_t offset,
                                         Candid_IndexFormat format) {
  (void)cmd;
  (void)buffer;
  (void)offset;
  (void)format;
}

static void vulkan_cmd_bind_uniform_buffer(Candid_CommandBuffer *cmd,
                                           uint32_t slot, Candid_Buffer *buffer,
                                           size_t offset, size_t size) {
  (void)cmd;
  (void)slot;
  (void)buffer;
  (void)offset;
  (void)size;
}

static void vulkan_cmd_bind_texture(Candid_CommandBuffer *cmd, uint32_t slot,
                                    Candid_Texture *texture,
                                    Candid_Sampler *sampler) {
  (void)cmd;
  (void)slot;
  (void)texture;
  (void)sampler;
}

static void vulkan_cmd_push_constants(Candid_CommandBuffer *cmd,
                                      Candid_ShaderStage stages,
                                      uint32_t offset, const void *data,
                                      size_t size) {
  (void)cmd;
  (void)stages;
  (void)offset;
  (void)data;
  (void)size;
}

static void vulkan_cmd_draw(Candid_CommandBuffer *cmd, uint32_t vertex_count,
                            uint32_t instance_count, uint32_t first_vertex,
                            uint32_t first_instance) {
  (void)cmd;
  (void)vertex_count;
  (void)instance_count;
  (void)first_vertex;
  (void)first_instance;
}

static void vulkan_cmd_draw_indexed(Candid_CommandBuffer *cmd,
                                    uint32_t index_count,
                                    uint32_t instance_count,
                                    uint32_t first_index, int32_t vertex_offset,
                                    uint32_t first_instance) {
  (void)cmd;
  (void)index_count;
  (void)instance_count;
  (void)first_index;
  (void)vertex_offset;
  (void)first_instance;
}

static void vulkan_cmd_draw_mesh(Candid_CommandBuffer *cmd, Candid_Mesh *mesh,
                                 Candid_Material *material,
                                 const Candid_Mat4 *transform) {
  (void)cmd;
  (void)mesh;
  (void)material;
  (void)transform;
}

static void vulkan_cmd_dispatch(Candid_CommandBuffer *cmd, uint32_t x,
                                uint32_t y, uint32_t z) {
  (void)cmd;
  (void)x;
  (void)y;
  (void)z;
}

/*******************************************************************************
 * Backend Interface Export
 ******************************************************************************/

const Candid_BackendInterface candid_vulkan_backend = {
    .name = "Vulkan",
    .type = CANDID_BACKEND_VULKAN,

    /* Device */
    .device_create = vulkan_device_create,
    .device_destroy = vulkan_device_destroy,
    .device_get_limits = vulkan_device_get_limits,

    /* Swapchain */
    .swapchain_resize = vulkan_swapchain_resize,
    .swapchain_present = vulkan_swapchain_present,

    /* Buffer */
    .buffer_create = vulkan_buffer_create,
    .buffer_destroy = vulkan_buffer_destroy,
    .buffer_update = vulkan_buffer_update,
    .buffer_map = vulkan_buffer_map,
    .buffer_unmap = vulkan_buffer_unmap,

    /* Texture */
    .texture_create = vulkan_texture_create,
    .texture_destroy = vulkan_texture_destroy,
    .texture_upload = vulkan_texture_upload,

    /* Sampler */
    .sampler_create = vulkan_sampler_create,
    .sampler_destroy = vulkan_sampler_destroy,

    /* Shader */
    .shader_module_create = vulkan_shader_module_create,
    .shader_module_destroy = vulkan_shader_module_destroy,
    .shader_program_create = vulkan_shader_program_create,
    .shader_program_destroy = vulkan_shader_program_destroy,

    /* Mesh */
    .mesh_create = vulkan_mesh_create,
    .mesh_destroy = vulkan_mesh_destroy,

    /* Material */
    .material_create = vulkan_material_create,
    .material_destroy = vulkan_material_destroy,

    /* Command buffer */
    .cmd_begin = vulkan_cmd_begin,
    .cmd_end = vulkan_cmd_end,
    .cmd_submit = vulkan_cmd_submit,

    /* Render pass */
    .cmd_begin_render_pass = vulkan_cmd_begin_render_pass,
    .cmd_end_render_pass = vulkan_cmd_end_render_pass,
    .cmd_set_viewport = vulkan_cmd_set_viewport,
    .cmd_set_scissor = vulkan_cmd_set_scissor,

    /* Draw commands */
    .cmd_bind_pipeline = vulkan_cmd_bind_pipeline,
    .cmd_bind_vertex_buffer = vulkan_cmd_bind_vertex_buffer,
    .cmd_bind_index_buffer = vulkan_cmd_bind_index_buffer,
    .cmd_bind_uniform_buffer = vulkan_cmd_bind_uniform_buffer,
    .cmd_bind_texture = vulkan_cmd_bind_texture,
    .cmd_push_constants = vulkan_cmd_push_constants,
    .cmd_draw = vulkan_cmd_draw,
    .cmd_draw_indexed = vulkan_cmd_draw_indexed,
    .cmd_draw_mesh = vulkan_cmd_draw_mesh,

    /* Compute */
    .cmd_dispatch = vulkan_cmd_dispatch,
};

#endif /* CANDID_VULKAN_SUPPORT */
