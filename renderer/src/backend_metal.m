/**
 * @file backend_metal.m
 * @brief Metal backend implementation for macOS/iOS
 *
 * This implements the Candid_BackendInterface for Apple's Metal API.
 */

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <candid/backend.h>
#include <candid/mesh.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*******************************************************************************
 * Internal Structures
 ******************************************************************************/

struct Candid_Device {
  id<MTLDevice> mtl_device;
  id<MTLCommandQueue> command_queue;
  CAMetalLayer *layer;
  uint32_t width;
  uint32_t height;
  id<MTLTexture> depth_texture;
  id<MTLDepthStencilState> default_depth_state;

  /* Built-in resources */
  id<MTLLibrary> default_library;
  id<MTLRenderPipelineState> default_pipeline;
};

struct Candid_Buffer {
  id<MTLBuffer> mtl_buffer;
  size_t size;
  Candid_BufferMemory memory;
};

struct Candid_Texture {
  id<MTLTexture> mtl_texture;
  Candid_TextureDesc desc;
};

struct Candid_Sampler {
  id<MTLSamplerState> mtl_sampler;
};

struct Candid_ShaderModule {
  id<MTLFunction> mtl_function;
  Candid_ShaderStage stage;
};

struct Candid_ShaderProgram {
  id<MTLRenderPipelineState> pipeline_state;
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
  id<MTLBuffer> uniform_buffer;
};

struct Candid_CommandBuffer {
  id<MTLCommandBuffer> mtl_command_buffer;
  id<MTLRenderCommandEncoder> render_encoder;
  id<CAMetalDrawable> drawable;
  Candid_Device *device;
};

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

static MTLPixelFormat texture_format_to_mtl(Candid_TextureFormat format) {
  switch (format) {
  case CANDID_TEXTURE_FORMAT_RGBA8_UNORM:
    return MTLPixelFormatRGBA8Unorm;
  case CANDID_TEXTURE_FORMAT_RGBA8_SRGB:
    return MTLPixelFormatRGBA8Unorm_sRGB;
  case CANDID_TEXTURE_FORMAT_BGRA8_UNORM:
    return MTLPixelFormatBGRA8Unorm;
  case CANDID_TEXTURE_FORMAT_BGRA8_SRGB:
    return MTLPixelFormatBGRA8Unorm_sRGB;
  case CANDID_TEXTURE_FORMAT_R8_UNORM:
    return MTLPixelFormatR8Unorm;
  case CANDID_TEXTURE_FORMAT_RG8_UNORM:
    return MTLPixelFormatRG8Unorm;
  case CANDID_TEXTURE_FORMAT_RGBA16_FLOAT:
    return MTLPixelFormatRGBA16Float;
  case CANDID_TEXTURE_FORMAT_RGBA32_FLOAT:
    return MTLPixelFormatRGBA32Float;
  case CANDID_TEXTURE_FORMAT_DEPTH32_FLOAT:
    return MTLPixelFormatDepth32Float;
  case CANDID_TEXTURE_FORMAT_DEPTH24_STENCIL8:
    return MTLPixelFormatDepth24Unorm_Stencil8;
  default:
    return MTLPixelFormatInvalid;
  }
}

static MTLSamplerMinMagFilter sampler_filter_to_mtl(Candid_SamplerFilter filter) {
  switch (filter) {
  case CANDID_SAMPLER_FILTER_NEAREST:
    return MTLSamplerMinMagFilterNearest;
  case CANDID_SAMPLER_FILTER_LINEAR:
    return MTLSamplerMinMagFilterLinear;
  default:
    return MTLSamplerMinMagFilterLinear;
  }
}

static MTLSamplerAddressMode sampler_address_to_mtl(Candid_SamplerAddressMode mode) {
  switch (mode) {
  case CANDID_SAMPLER_ADDRESS_REPEAT:
    return MTLSamplerAddressModeRepeat;
  case CANDID_SAMPLER_ADDRESS_MIRROR_REPEAT:
    return MTLSamplerAddressModeMirrorRepeat;
  case CANDID_SAMPLER_ADDRESS_CLAMP_TO_EDGE:
    return MTLSamplerAddressModeClampToEdge;
  case CANDID_SAMPLER_ADDRESS_CLAMP_TO_BORDER:
    return MTLSamplerAddressModeClampToBorderColor;
  default:
    return MTLSamplerAddressModeRepeat;
  }
}

__attribute__((unused))
static MTLCompareFunction compare_func_to_mtl(Candid_CompareFunc func) {
  switch (func) {
  case CANDID_COMPARE_NEVER:
    return MTLCompareFunctionNever;
  case CANDID_COMPARE_LESS:
    return MTLCompareFunctionLess;
  case CANDID_COMPARE_EQUAL:
    return MTLCompareFunctionEqual;
  case CANDID_COMPARE_LESS_EQUAL:
    return MTLCompareFunctionLessEqual;
  case CANDID_COMPARE_GREATER:
    return MTLCompareFunctionGreater;
  case CANDID_COMPARE_NOT_EQUAL:
    return MTLCompareFunctionNotEqual;
  case CANDID_COMPARE_GREATER_EQUAL:
    return MTLCompareFunctionGreaterEqual;
  case CANDID_COMPARE_ALWAYS:
    return MTLCompareFunctionAlways;
  default:
    return MTLCompareFunctionLess;
  }
}

static void create_depth_texture(Candid_Device *device) {
  if (device->depth_texture) {
    device->depth_texture = nil;
  }

  if (device->width > 0 && device->height > 0) {
    MTLTextureDescriptor *desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                     width:device->width
                                    height:device->height
                                 mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget;
    desc.storageMode = MTLStorageModePrivate;
    device->depth_texture = [device->mtl_device newTextureWithDescriptor:desc];
  }
}

static void create_default_pipeline(Candid_Device *device) {
  /* Default shader for basic 3D rendering */
  static const char *shader_source =
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "\n"
      "struct Vertex {\n"
      "    float3 position [[attribute(0)]];\n"
      "    float3 normal [[attribute(1)]];\n"
      "    float4 tangent [[attribute(2)]];\n"
      "    float2 texcoord0 [[attribute(3)]];\n"
      "    float2 texcoord1 [[attribute(4)]];\n"
      "    float4 color [[attribute(5)]];\n"
      "};\n"
      "\n"
      "struct VertexOut {\n"
      "    float4 position [[position]];\n"
      "    float3 world_pos;\n"
      "    float3 normal;\n"
      "    float2 texcoord;\n"
      "    float4 color;\n"
      "};\n"
      "\n"
      "struct Uniforms {\n"
      "    float4x4 model;\n"
      "    float4x4 view_projection;\n"
      "    float time;\n"
      "};\n"
      "\n"
      "vertex VertexOut vertex_main(Vertex in [[stage_in]],\n"
      "                             constant Uniforms &uniforms [[buffer(1)]]) {\n"
      "    VertexOut out;\n"
      "    float4 world_pos = uniforms.model * float4(in.position, 1.0);\n"
      "    out.position = uniforms.view_projection * world_pos;\n"
      "    out.world_pos = world_pos.xyz;\n"
      "    out.normal = (uniforms.model * float4(in.normal, 0.0)).xyz;\n"
      "    out.texcoord = in.texcoord0;\n"
      "    out.color = in.color;\n"
      "    return out;\n"
      "}\n"
      "\n"
      "fragment float4 fragment_main(VertexOut in [[stage_in]]) {\n"
      "    float3 N = normalize(in.normal);\n"
      "    float3 L = normalize(float3(1.0, 1.0, 0.5));\n"
      "    float NdotL = max(dot(N, L), 0.0);\n"
      "    float3 ambient = float3(0.1);\n"
      "    float3 diffuse = in.color.rgb * NdotL;\n"
      "    return float4(ambient + diffuse, in.color.a);\n"
      "}\n";

  NSError *error = nil;
  device->default_library = [device->mtl_device
      newLibraryWithSource:[NSString stringWithUTF8String:shader_source]
                   options:nil
                     error:&error];

  if (!device->default_library) {
    NSLog(@"Failed to create default library: %@", error);
    return;
  }

  id<MTLFunction> vert = [device->default_library newFunctionWithName:@"vertex_main"];
  id<MTLFunction> frag = [device->default_library newFunctionWithName:@"fragment_main"];

  if (!vert || !frag) {
    NSLog(@"Failed to create default shader functions");
    return;
  }

  MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
  desc.vertexFunction = vert;
  desc.fragmentFunction = frag;
  desc.colorAttachments[0].pixelFormat = device->layer.pixelFormat;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

  /* Vertex descriptor matching Candid_Vertex layout */
  MTLVertexDescriptor *vdesc = [[MTLVertexDescriptor alloc] init];

  /* Position */
  vdesc.attributes[0].format = MTLVertexFormatFloat3;
  vdesc.attributes[0].offset = offsetof(Candid_Vertex, position);
  vdesc.attributes[0].bufferIndex = 0;

  /* Normal */
  vdesc.attributes[1].format = MTLVertexFormatFloat3;
  vdesc.attributes[1].offset = offsetof(Candid_Vertex, normal);
  vdesc.attributes[1].bufferIndex = 0;

  /* Tangent */
  vdesc.attributes[2].format = MTLVertexFormatFloat4;
  vdesc.attributes[2].offset = offsetof(Candid_Vertex, tangent);
  vdesc.attributes[2].bufferIndex = 0;

  /* TexCoord0 */
  vdesc.attributes[3].format = MTLVertexFormatFloat2;
  vdesc.attributes[3].offset = offsetof(Candid_Vertex, texcoord0);
  vdesc.attributes[3].bufferIndex = 0;

  /* TexCoord1 */
  vdesc.attributes[4].format = MTLVertexFormatFloat2;
  vdesc.attributes[4].offset = offsetof(Candid_Vertex, texcoord1);
  vdesc.attributes[4].bufferIndex = 0;

  /* Color */
  vdesc.attributes[5].format = MTLVertexFormatFloat4;
  vdesc.attributes[5].offset = offsetof(Candid_Vertex, color);
  vdesc.attributes[5].bufferIndex = 0;

  vdesc.layouts[0].stride = sizeof(Candid_Vertex);
  vdesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

  desc.vertexDescriptor = vdesc;

  device->default_pipeline = [device->mtl_device
      newRenderPipelineStateWithDescriptor:desc
                                     error:&error];

  if (!device->default_pipeline) {
    NSLog(@"Failed to create default pipeline: %@", error);
  }

  /* Default depth state */
  MTLDepthStencilDescriptor *depth_desc = [[MTLDepthStencilDescriptor alloc] init];
  depth_desc.depthCompareFunction = MTLCompareFunctionLess;
  depth_desc.depthWriteEnabled = YES;
  device->default_depth_state = [device->mtl_device
      newDepthStencilStateWithDescriptor:depth_desc];
}

/*******************************************************************************
 * Device Functions
 ******************************************************************************/

static Candid_Result metal_device_create(const Candid_DeviceDesc *desc,
                                         Candid_Device **out) {
  if (!desc || !out || !desc->native_surface)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Device *device = calloc(1, sizeof(Candid_Device));
  if (!device)
    return CANDID_ERROR_OUT_OF_MEMORY;

  device->layer = (__bridge CAMetalLayer *)desc->native_surface;
  device->mtl_device = MTLCreateSystemDefaultDevice();

  if (!device->mtl_device) {
    free(device);
    return CANDID_ERROR_BACKEND_NOT_SUPPORTED;
  }

  device->layer.device = device->mtl_device;
  device->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

  device->command_queue = [device->mtl_device newCommandQueue];
  if (!device->command_queue) {
    free(device);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  device->width = desc->width;
  device->height = desc->height;

  if (device->width > 0 && device->height > 0) {
    device->layer.drawableSize = CGSizeMake(device->width, device->height);
    create_depth_texture(device);
  }

  create_default_pipeline(device);

  *out = device;
  return CANDID_SUCCESS;
}

static void metal_device_destroy(Candid_Device *device) {
  if (!device)
    return;

  device->default_pipeline = nil;
  device->default_library = nil;
  device->default_depth_state = nil;
  device->depth_texture = nil;
  device->command_queue = nil;
  device->mtl_device = nil;
  device->layer = nil;

  free(device);
}

static Candid_Result metal_device_get_limits(Candid_Device *device,
                                             Candid_DeviceLimits *out) {
  if (!device || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  memset(out, 0, sizeof(*out));

  /* Query Metal device limits */
  out->max_texture_size = 16384;
  out->max_cube_map_size = 16384;
  out->max_texture_array_layers = 2048;
  out->max_vertex_attributes = 31;
  out->max_vertex_buffers = 31;
  out->max_uniform_buffer_size = 256 * 1024 * 1024;
  out->max_storage_buffer_size = (uint32_t)device->mtl_device.maxBufferLength;
  out->max_compute_workgroup_size[0] = 1024;
  out->max_compute_workgroup_size[1] = 1024;
  out->max_compute_workgroup_size[2] = 64;
  out->max_compute_workgroups[0] = 65535;
  out->max_compute_workgroups[1] = 65535;
  out->max_compute_workgroups[2] = 65535;
  out->max_anisotropy = 16.0f;
  out->supports_geometry_shader = NO;
  out->supports_tessellation = YES;
  out->supports_compute = YES;
  out->supports_ray_tracing = device->mtl_device.supportsRaytracing;

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Swapchain Functions
 ******************************************************************************/

static Candid_Result metal_swapchain_resize(Candid_Device *device, uint32_t width,
                                            uint32_t height) {
  if (!device)
    return CANDID_ERROR_INVALID_ARGUMENT;

  device->width = width;
  device->height = height;
  device->layer.drawableSize = CGSizeMake(width, height);
  create_depth_texture(device);

  return CANDID_SUCCESS;
}

static Candid_Result metal_swapchain_present(Candid_Device *device) {
  (void)device;
  /* Presentation is handled in cmd_submit */
  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Buffer Functions
 ******************************************************************************/

static Candid_Result metal_buffer_create(Candid_Device *device,
                                         const Candid_BufferDesc *desc,
                                         Candid_Buffer **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Buffer *buffer = calloc(1, sizeof(Candid_Buffer));
  if (!buffer)
    return CANDID_ERROR_OUT_OF_MEMORY;

  MTLResourceOptions options = MTLResourceStorageModeShared;
  if (desc->memory == CANDID_BUFFER_MEMORY_GPU_ONLY) {
    options = MTLResourceStorageModePrivate;
  }

  if (desc->initial_data) {
    buffer->mtl_buffer = [device->mtl_device newBufferWithBytes:desc->initial_data
                                                         length:desc->size
                                                        options:options];
  } else {
    buffer->mtl_buffer = [device->mtl_device newBufferWithLength:desc->size
                                                         options:options];
  }

  if (!buffer->mtl_buffer) {
    free(buffer);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  buffer->size = desc->size;
  buffer->memory = desc->memory;

  if (desc->label) {
    buffer->mtl_buffer.label = [NSString stringWithUTF8String:desc->label];
  }

  *out = buffer;
  return CANDID_SUCCESS;
}

static void metal_buffer_destroy(Candid_Device *device, Candid_Buffer *buffer) {
  (void)device;
  if (!buffer)
    return;
  buffer->mtl_buffer = nil;
  free(buffer);
}

static Candid_Result metal_buffer_update(Candid_Device *device,
                                         Candid_Buffer *buffer, size_t offset,
                                         const void *data, size_t size) {
  (void)device;
  if (!buffer || !data)
    return CANDID_ERROR_INVALID_ARGUMENT;

  if (buffer->memory == CANDID_BUFFER_MEMORY_GPU_ONLY) {
    return CANDID_ERROR_INVALID_ARGUMENT; /* Need staging buffer */
  }

  memcpy((char *)buffer->mtl_buffer.contents + offset, data, size);
  return CANDID_SUCCESS;
}

static void *metal_buffer_map(Candid_Device *device, Candid_Buffer *buffer) {
  (void)device;
  if (!buffer || buffer->memory == CANDID_BUFFER_MEMORY_GPU_ONLY)
    return NULL;
  return buffer->mtl_buffer.contents;
}

static void metal_buffer_unmap(Candid_Device *device, Candid_Buffer *buffer) {
  (void)device;
  (void)buffer;
  /* No-op for shared memory */
}

/*******************************************************************************
 * Texture Functions
 ******************************************************************************/

static Candid_Result metal_texture_create(Candid_Device *device,
                                          const Candid_TextureDesc *desc,
                                          Candid_Texture **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Texture *texture = calloc(1, sizeof(Candid_Texture));
  if (!texture)
    return CANDID_ERROR_OUT_OF_MEMORY;

  MTLTextureDescriptor *mtl_desc = [[MTLTextureDescriptor alloc] init];
  mtl_desc.width = desc->width;
  mtl_desc.height = desc->height;
  mtl_desc.depth = desc->depth > 0 ? desc->depth : 1;
  mtl_desc.pixelFormat = texture_format_to_mtl(desc->format);
  mtl_desc.mipmapLevelCount = desc->mip_levels > 0 ? desc->mip_levels : 1;
  mtl_desc.arrayLength = desc->array_layers > 0 ? desc->array_layers : 1;

  MTLTextureUsage usage = 0;
  if (desc->usage & CANDID_TEXTURE_USAGE_SAMPLED)
    usage |= MTLTextureUsageShaderRead;
  if (desc->usage & CANDID_TEXTURE_USAGE_STORAGE)
    usage |= MTLTextureUsageShaderWrite;
  if (desc->usage & CANDID_TEXTURE_USAGE_RENDER_TARGET)
    usage |= MTLTextureUsageRenderTarget;
  if (desc->usage & CANDID_TEXTURE_USAGE_DEPTH_STENCIL)
    usage |= MTLTextureUsageRenderTarget;
  mtl_desc.usage = usage;

  texture->mtl_texture = [device->mtl_device newTextureWithDescriptor:mtl_desc];
  if (!texture->mtl_texture) {
    free(texture);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  texture->desc = *desc;

  if (desc->label) {
    texture->mtl_texture.label = [NSString stringWithUTF8String:desc->label];
  }

  *out = texture;
  return CANDID_SUCCESS;
}

static void metal_texture_destroy(Candid_Device *device, Candid_Texture *texture) {
  (void)device;
  if (!texture)
    return;
  texture->mtl_texture = nil;
  free(texture);
}

static Candid_Result metal_texture_upload(Candid_Device *device,
                                          Candid_Texture *texture,
                                          uint32_t mip_level,
                                          uint32_t array_layer,
                                          const void *data, size_t size) {
  (void)device;
  (void)size;
  if (!texture || !data)
    return CANDID_ERROR_INVALID_ARGUMENT;

  uint32_t width = texture->desc.width >> mip_level;
  uint32_t height = texture->desc.height >> mip_level;
  if (width == 0)
    width = 1;
  if (height == 0)
    height = 1;

  size_t bytes_per_row = width * 4; /* Assuming RGBA8 */

  MTLRegion region = MTLRegionMake2D(0, 0, width, height);
  [texture->mtl_texture replaceRegion:region
                          mipmapLevel:mip_level
                                slice:array_layer
                            withBytes:data
                          bytesPerRow:bytes_per_row
                        bytesPerImage:0];

  return CANDID_SUCCESS;
}

/*******************************************************************************
 * Sampler Functions
 ******************************************************************************/

static Candid_Result metal_sampler_create(Candid_Device *device,
                                          const Candid_SamplerDesc *desc,
                                          Candid_Sampler **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Sampler *sampler = calloc(1, sizeof(Candid_Sampler));
  if (!sampler)
    return CANDID_ERROR_OUT_OF_MEMORY;

  MTLSamplerDescriptor *mtl_desc = [[MTLSamplerDescriptor alloc] init];
  mtl_desc.minFilter = sampler_filter_to_mtl(desc->min_filter);
  mtl_desc.magFilter = sampler_filter_to_mtl(desc->mag_filter);
  mtl_desc.mipFilter = (desc->mip_filter == CANDID_SAMPLER_FILTER_NEAREST)
                           ? MTLSamplerMipFilterNearest
                           : MTLSamplerMipFilterLinear;
  mtl_desc.sAddressMode = sampler_address_to_mtl(desc->address_u);
  mtl_desc.tAddressMode = sampler_address_to_mtl(desc->address_v);
  mtl_desc.rAddressMode = sampler_address_to_mtl(desc->address_w);
  mtl_desc.maxAnisotropy = (NSUInteger)desc->max_anisotropy;

  sampler->mtl_sampler = [device->mtl_device newSamplerStateWithDescriptor:mtl_desc];
  if (!sampler->mtl_sampler) {
    free(sampler);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  *out = sampler;
  return CANDID_SUCCESS;
}

static void metal_sampler_destroy(Candid_Device *device, Candid_Sampler *sampler) {
  (void)device;
  if (!sampler)
    return;
  sampler->mtl_sampler = nil;
  free(sampler);
}

/*******************************************************************************
 * Shader Functions
 ******************************************************************************/

static Candid_Result metal_shader_module_create(Candid_Device *device,
                                                const Candid_ShaderModuleDesc *desc,
                                                Candid_ShaderModule **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_ShaderModule *module = calloc(1, sizeof(Candid_ShaderModule));
  if (!module)
    return CANDID_ERROR_OUT_OF_MEMORY;

  NSError *error = nil;
  id<MTLLibrary> library = nil;

  if (desc->source_type == CANDID_SHADER_SOURCE_MSL) {
    library = [device->mtl_device
        newLibraryWithSource:[NSString stringWithUTF8String:desc->source]
                     options:nil
                       error:&error];
  } else if (desc->source_type == CANDID_SHADER_SOURCE_METALLIB) {
    dispatch_data_t data =
        dispatch_data_create(desc->bytecode, desc->bytecode_size,
                             dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    library = [device->mtl_device newLibraryWithData:data error:&error];
  } else {
    free(module);
    return CANDID_ERROR_INVALID_ARGUMENT;
  }

  if (!library) {
    NSLog(@"Shader compilation failed: %@", error);
    free(module);
    return CANDID_ERROR_SHADER_COMPILATION;
  }

  const char *entry = desc->entry_point ? desc->entry_point : "main";
  module->mtl_function = [library newFunctionWithName:[NSString stringWithUTF8String:entry]];

  if (!module->mtl_function) {
    free(module);
    return CANDID_ERROR_SHADER_COMPILATION;
  }

  module->stage = desc->stage;
  *out = module;
  return CANDID_SUCCESS;
}

static void metal_shader_module_destroy(Candid_Device *device,
                                        Candid_ShaderModule *module) {
  (void)device;
  if (!module)
    return;
  module->mtl_function = nil;
  free(module);
}

static Candid_Result metal_shader_program_create(Candid_Device *device,
                                                 const Candid_ShaderProgramDesc *desc,
                                                 Candid_ShaderProgram **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  if (!desc->vertex || !desc->fragment)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_ShaderProgram *program = calloc(1, sizeof(Candid_ShaderProgram));
  if (!program)
    return CANDID_ERROR_OUT_OF_MEMORY;

  MTLRenderPipelineDescriptor *pipeline_desc =
      [[MTLRenderPipelineDescriptor alloc] init];
  pipeline_desc.vertexFunction = desc->vertex->mtl_function;
  pipeline_desc.fragmentFunction = desc->fragment->mtl_function;
  pipeline_desc.colorAttachments[0].pixelFormat = device->layer.pixelFormat;
  pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

  NSError *error = nil;
  program->pipeline_state = [device->mtl_device
      newRenderPipelineStateWithDescriptor:pipeline_desc
                                     error:&error];

  if (!program->pipeline_state) {
    NSLog(@"Pipeline creation failed: %@", error);
    free(program);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  program->vertex = desc->vertex;
  program->fragment = desc->fragment;

  *out = program;
  return CANDID_SUCCESS;
}

static void metal_shader_program_destroy(Candid_Device *device,
                                         Candid_ShaderProgram *program) {
  (void)device;
  if (!program)
    return;
  program->pipeline_state = nil;
  free(program);
}

/*******************************************************************************
 * Mesh Functions
 ******************************************************************************/

static Candid_Result metal_mesh_create(Candid_Device *device,
                                       const Candid_MeshDesc *desc,
                                       Candid_Mesh **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Mesh *mesh = calloc(1, sizeof(Candid_Mesh));
  if (!mesh)
    return CANDID_ERROR_OUT_OF_MEMORY;

  /* Create vertex buffer */
  Candid_BufferDesc vb_desc = {
      .size = desc->data.vertex_count * desc->data.vertex_stride,
      .usage = CANDID_BUFFER_USAGE_VERTEX,
      .memory = CANDID_BUFFER_MEMORY_CPU_TO_GPU,
      .initial_data = desc->data.vertices,
      .label = desc->label,
  };

  Candid_Result result = metal_buffer_create(device, &vb_desc, &mesh->vertex_buffer);
  if (result != CANDID_SUCCESS) {
    free(mesh);
    return result;
  }

  /* Create index buffer */
  size_t index_size = (desc->data.index_format == CANDID_INDEX_FORMAT_UINT16)
                          ? sizeof(uint16_t)
                          : sizeof(uint32_t);
  Candid_BufferDesc ib_desc = {
      .size = desc->data.index_count * index_size,
      .usage = CANDID_BUFFER_USAGE_INDEX,
      .memory = CANDID_BUFFER_MEMORY_CPU_TO_GPU,
      .initial_data = desc->data.indices,
      .label = desc->label,
  };

  result = metal_buffer_create(device, &ib_desc, &mesh->index_buffer);
  if (result != CANDID_SUCCESS) {
    metal_buffer_destroy(device, mesh->vertex_buffer);
    free(mesh);
    return result;
  }

  mesh->vertex_count = (uint32_t)desc->data.vertex_count;
  mesh->index_count = (uint32_t)desc->data.index_count;
  mesh->index_format = desc->data.index_format;
  mesh->layout = desc->data.layout;
  mesh->bounds = desc->bounds;

  *out = mesh;
  return CANDID_SUCCESS;
}

static void metal_mesh_destroy(Candid_Device *device, Candid_Mesh *mesh) {
  if (!mesh)
    return;
  metal_buffer_destroy(device, mesh->vertex_buffer);
  metal_buffer_destroy(device, mesh->index_buffer);
  free(mesh);
}

/*******************************************************************************
 * Material Functions
 ******************************************************************************/

static Candid_Result metal_material_create(Candid_Device *device,
                                           const Candid_MaterialDesc *desc,
                                           Candid_Material **out) {
  if (!device || !desc || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_Material *material = calloc(1, sizeof(Candid_Material));
  if (!material)
    return CANDID_ERROR_OUT_OF_MEMORY;

  material->desc = *desc;
  material->shader = desc->shader;

  *out = material;
  return CANDID_SUCCESS;
}

static void metal_material_destroy(Candid_Device *device,
                                   Candid_Material *material) {
  (void)device;
  if (!material)
    return;
  material->uniform_buffer = nil;
  free(material);
}

/*******************************************************************************
 * Command Buffer Functions
 ******************************************************************************/

static Candid_Result metal_cmd_begin(Candid_Device *device,
                                     Candid_CommandBuffer **out) {
  if (!device || !out)
    return CANDID_ERROR_INVALID_ARGUMENT;

  Candid_CommandBuffer *cmd = calloc(1, sizeof(Candid_CommandBuffer));
  if (!cmd)
    return CANDID_ERROR_OUT_OF_MEMORY;

  cmd->device = device;
  cmd->mtl_command_buffer = [device->command_queue commandBuffer];

  if (!cmd->mtl_command_buffer) {
    free(cmd);
    return CANDID_ERROR_RESOURCE_CREATION;
  }

  *out = cmd;
  return CANDID_SUCCESS;
}

static Candid_Result metal_cmd_end(Candid_Device *device,
                                   Candid_CommandBuffer *cmd) {
  (void)device;
  if (!cmd)
    return CANDID_ERROR_INVALID_ARGUMENT;

  if (cmd->render_encoder) {
    [cmd->render_encoder endEncoding];
    cmd->render_encoder = nil;
  }

  return CANDID_SUCCESS;
}

static Candid_Result metal_cmd_submit(Candid_Device *device,
                                      Candid_CommandBuffer *cmd) {
  (void)device;
  if (!cmd)
    return CANDID_ERROR_INVALID_ARGUMENT;

  if (cmd->drawable) {
    [cmd->mtl_command_buffer presentDrawable:cmd->drawable];
  }

  [cmd->mtl_command_buffer commit];

  /* Clean up */
  cmd->mtl_command_buffer = nil;
  cmd->drawable = nil;
  free(cmd);

  return CANDID_SUCCESS;
}

static Candid_Result metal_cmd_begin_render_pass(Candid_CommandBuffer *cmd,
                                                 const Candid_Color *clear_color,
                                                 float clear_depth,
                                                 uint8_t clear_stencil) {
  (void)clear_stencil;
  if (!cmd || !cmd->device)
    return CANDID_ERROR_INVALID_ARGUMENT;

  @autoreleasepool {
    cmd->drawable = [cmd->device->layer nextDrawable];
    if (!cmd->drawable)
      return CANDID_ERROR_RESOURCE_CREATION;

    MTLRenderPassDescriptor *pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
    pass_desc.colorAttachments[0].texture = cmd->drawable.texture;
    pass_desc.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;

    if (clear_color) {
      pass_desc.colorAttachments[0].clearColor =
          MTLClearColorMake((double)clear_color->r, (double)clear_color->g, (double)clear_color->b,
                            (double)clear_color->a);
    } else {
      pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(0.2, 0.2, 0.2, 1.0);
    }

    if (cmd->device->depth_texture) {
      pass_desc.depthAttachment.texture = cmd->device->depth_texture;
      pass_desc.depthAttachment.loadAction = MTLLoadActionClear;
      pass_desc.depthAttachment.storeAction = MTLStoreActionDontCare;
      pass_desc.depthAttachment.clearDepth = (double)clear_depth;
    }    cmd->render_encoder =
        [cmd->mtl_command_buffer renderCommandEncoderWithDescriptor:pass_desc];

    if (!cmd->render_encoder)
      return CANDID_ERROR_RESOURCE_CREATION;
  }

  return CANDID_SUCCESS;
}

static void metal_cmd_end_render_pass(Candid_CommandBuffer *cmd) {
  if (!cmd || !cmd->render_encoder)
    return;
  [cmd->render_encoder endEncoding];
  cmd->render_encoder = nil;
}

static void metal_cmd_set_viewport(Candid_CommandBuffer *cmd, float x, float y,
                                   float width, float height, float min_depth,
                                   float max_depth) {
  if (!cmd || !cmd->render_encoder)
    return;

  MTLViewport viewport = {(double)x, (double)y, (double)width, (double)height, (double)min_depth, (double)max_depth};
  [cmd->render_encoder setViewport:viewport];
}static void metal_cmd_set_scissor(Candid_CommandBuffer *cmd, int32_t x,
                                  int32_t y, uint32_t width, uint32_t height) {
  if (!cmd || !cmd->render_encoder)
    return;

  MTLScissorRect scissor = {(NSUInteger)x, (NSUInteger)y, width, height};
  [cmd->render_encoder setScissorRect:scissor];
}

static void metal_cmd_bind_pipeline(Candid_CommandBuffer *cmd,
                                    Candid_ShaderProgram *program,
                                    const Candid_RasterizerState *raster,
                                    const Candid_DepthStencilState *depth_stencil,
                                    const Candid_BlendState *blend) {
  (void)blend; /* Blend state is part of pipeline in Metal */
  if (!cmd || !cmd->render_encoder)
    return;

  if (program && program->pipeline_state) {
    [cmd->render_encoder setRenderPipelineState:program->pipeline_state];
  } else if (cmd->device->default_pipeline) {
    [cmd->render_encoder setRenderPipelineState:cmd->device->default_pipeline];
  }

  if (cmd->device->default_depth_state) {
    [cmd->render_encoder setDepthStencilState:cmd->device->default_depth_state];
  }

  MTLCullMode cull_mode = MTLCullModeNone;
  if (raster) {
    switch (raster->cull_mode) {
    case CANDID_CULL_FRONT:
      cull_mode = MTLCullModeFront;
      break;
    case CANDID_CULL_BACK:
      cull_mode = MTLCullModeBack;
      break;
    default:
      cull_mode = MTLCullModeNone;
      break;
    }
  } else {
    cull_mode = MTLCullModeBack;
  }
  [cmd->render_encoder setCullMode:cull_mode];

  MTLWinding winding = MTLWindingCounterClockwise;
  if (raster && raster->front_face == CANDID_FRONT_FACE_CW) {
    winding = MTLWindingClockwise;
  }
  [cmd->render_encoder setFrontFacingWinding:winding];

  if (raster && raster->wireframe) {
    [cmd->render_encoder setTriangleFillMode:MTLTriangleFillModeLines];
  } else {
    [cmd->render_encoder setTriangleFillMode:MTLTriangleFillModeFill];
  }

  (void)depth_stencil; /* Depth state created at device init */
}

static void metal_cmd_bind_vertex_buffer(Candid_CommandBuffer *cmd, uint32_t slot,
                                         Candid_Buffer *buffer, size_t offset) {
  if (!cmd || !cmd->render_encoder || !buffer)
    return;
  [cmd->render_encoder setVertexBuffer:buffer->mtl_buffer
                                offset:offset
                               atIndex:slot];
}

static void metal_cmd_bind_index_buffer(Candid_CommandBuffer *cmd,
                                        Candid_Buffer *buffer, size_t offset,
                                        Candid_IndexFormat format) {
  (void)cmd;
  (void)buffer;
  (void)offset;
  (void)format;
  /* Index buffer is bound at draw time in Metal */
}

static void metal_cmd_bind_uniform_buffer(Candid_CommandBuffer *cmd,
                                          uint32_t slot, Candid_Buffer *buffer,
                                          size_t offset, size_t size) {
  (void)size;
  if (!cmd || !cmd->render_encoder || !buffer)
    return;
  [cmd->render_encoder setVertexBuffer:buffer->mtl_buffer offset:offset atIndex:slot];
  [cmd->render_encoder setFragmentBuffer:buffer->mtl_buffer offset:offset atIndex:slot];
}

static void metal_cmd_bind_texture(Candid_CommandBuffer *cmd, uint32_t slot,
                                   Candid_Texture *texture,
                                   Candid_Sampler *sampler) {
  if (!cmd || !cmd->render_encoder)
    return;

  if (texture) {
    [cmd->render_encoder setFragmentTexture:texture->mtl_texture atIndex:slot];
  }
  if (sampler) {
    [cmd->render_encoder setFragmentSamplerState:sampler->mtl_sampler atIndex:slot];
  }
}

static void metal_cmd_push_constants(Candid_CommandBuffer *cmd,
                                     Candid_ShaderStage stages, uint32_t offset,
                                     const void *data, size_t size) {
  if (!cmd || !cmd->render_encoder || !data)
    return;

  if (stages & CANDID_SHADER_STAGE_VERTEX) {
    [cmd->render_encoder setVertexBytes:data length:size atIndex:31];
  }
  if (stages & CANDID_SHADER_STAGE_FRAGMENT) {
    [cmd->render_encoder setFragmentBytes:data length:size atIndex:31];
  }
  (void)offset;
}

static void metal_cmd_draw(Candid_CommandBuffer *cmd, uint32_t vertex_count,
                           uint32_t instance_count, uint32_t first_vertex,
                           uint32_t first_instance) {
  if (!cmd || !cmd->render_encoder)
    return;

  [cmd->render_encoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:first_vertex
                          vertexCount:vertex_count
                        instanceCount:instance_count
                         baseInstance:first_instance];
}

static void metal_cmd_draw_indexed(Candid_CommandBuffer *cmd, uint32_t index_count,
                                   uint32_t instance_count, uint32_t first_index,
                                   int32_t vertex_offset,
                                   uint32_t first_instance) {
  (void)cmd;
  (void)index_count;
  (void)instance_count;
  (void)first_index;
  (void)vertex_offset;
  (void)first_instance;
  /* Requires bound index buffer - see draw_mesh for complete implementation */
}

static void metal_cmd_draw_mesh(Candid_CommandBuffer *cmd, Candid_Mesh *mesh,
                                Candid_Material *material,
                                const Candid_Mat4 *transform) {
  if (!cmd || !cmd->render_encoder || !mesh)
    return;

  /* Bind vertex buffer */
  [cmd->render_encoder setVertexBuffer:mesh->vertex_buffer->mtl_buffer
                                offset:0
                               atIndex:0];

  /* Bind uniforms with transform */
  struct {
    float model[16];
    float view_projection[16];
    float time;
    float padding[3];
  } uniforms;

  if (transform) {
    memcpy(uniforms.model, transform->m, sizeof(uniforms.model));
  } else {
    /* Identity matrix */
    memset(uniforms.model, 0, sizeof(uniforms.model));
    uniforms.model[0] = uniforms.model[5] = uniforms.model[10] = uniforms.model[15] = 1.0f;
  }

  /* Simple view-projection (this should come from camera in real usage) */
  memset(uniforms.view_projection, 0, sizeof(uniforms.view_projection));
  float aspect = (cmd->device->width > 0 && cmd->device->height > 0)
                     ? (float)cmd->device->width / (float)cmd->device->height
                     : 1.0f;
  float fov = 65.0f * (float)M_PI / 180.0f;
  float near = 0.1f;
  float far = 100.0f;
  float f = 1.0f / tanf(fov * 0.5f);

  uniforms.view_projection[0] = f / aspect;
  uniforms.view_projection[5] = f;
  uniforms.view_projection[10] = (far + near) / (near - far);
  uniforms.view_projection[11] = -1.0f;
  uniforms.view_projection[14] = (2.0f * far * near) / (near - far);

  uniforms.time = 0.0f;

  [cmd->render_encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];

  /* Apply material settings */
  (void)material;

  /* Draw indexed */
  MTLIndexType index_type = (mesh->index_format == CANDID_INDEX_FORMAT_UINT16)
                                ? MTLIndexTypeUInt16
                                : MTLIndexTypeUInt32;

  [cmd->render_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                  indexCount:mesh->index_count
                                   indexType:index_type
                                 indexBuffer:mesh->index_buffer->mtl_buffer
                           indexBufferOffset:0];
}

static void metal_cmd_dispatch(Candid_CommandBuffer *cmd, uint32_t x, uint32_t y,
                               uint32_t z) {
  (void)cmd;
  (void)x;
  (void)y;
  (void)z;
  /* Compute shader dispatch - to be implemented */
}

/*******************************************************************************
 * Backend Interface Export
 ******************************************************************************/

const Candid_BackendInterface candid_metal_backend = {
    .name = "Metal",
    .type = CANDID_BACKEND_METAL,

    /* Device */
    .device_create = metal_device_create,
    .device_destroy = metal_device_destroy,
    .device_get_limits = metal_device_get_limits,

    /* Swapchain */
    .swapchain_resize = metal_swapchain_resize,
    .swapchain_present = metal_swapchain_present,

    /* Buffer */
    .buffer_create = metal_buffer_create,
    .buffer_destroy = metal_buffer_destroy,
    .buffer_update = metal_buffer_update,
    .buffer_map = metal_buffer_map,
    .buffer_unmap = metal_buffer_unmap,

    /* Texture */
    .texture_create = metal_texture_create,
    .texture_destroy = metal_texture_destroy,
    .texture_upload = metal_texture_upload,

    /* Sampler */
    .sampler_create = metal_sampler_create,
    .sampler_destroy = metal_sampler_destroy,

    /* Shader */
    .shader_module_create = metal_shader_module_create,
    .shader_module_destroy = metal_shader_module_destroy,
    .shader_program_create = metal_shader_program_create,
    .shader_program_destroy = metal_shader_program_destroy,

    /* Mesh */
    .mesh_create = metal_mesh_create,
    .mesh_destroy = metal_mesh_destroy,

    /* Material */
    .material_create = metal_material_create,
    .material_destroy = metal_material_destroy,

    /* Command buffer */
    .cmd_begin = metal_cmd_begin,
    .cmd_end = metal_cmd_end,
    .cmd_submit = metal_cmd_submit,

    /* Render pass */
    .cmd_begin_render_pass = metal_cmd_begin_render_pass,
    .cmd_end_render_pass = metal_cmd_end_render_pass,
    .cmd_set_viewport = metal_cmd_set_viewport,
    .cmd_set_scissor = metal_cmd_set_scissor,

    /* Draw commands */
    .cmd_bind_pipeline = metal_cmd_bind_pipeline,
    .cmd_bind_vertex_buffer = metal_cmd_bind_vertex_buffer,
    .cmd_bind_index_buffer = metal_cmd_bind_index_buffer,
    .cmd_bind_uniform_buffer = metal_cmd_bind_uniform_buffer,
    .cmd_bind_texture = metal_cmd_bind_texture,
    .cmd_push_constants = metal_cmd_push_constants,
    .cmd_draw = metal_cmd_draw,
    .cmd_draw_indexed = metal_cmd_draw_indexed,
    .cmd_draw_mesh = metal_cmd_draw_mesh,

    /* Compute */
    .cmd_dispatch = metal_cmd_dispatch,
};
