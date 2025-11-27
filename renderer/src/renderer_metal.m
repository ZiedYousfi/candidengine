/**
 * @file renderer_metal.m
 * @brief Metal backend implementation for macOS
 */

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <candid/renderer.h>
#include <stdlib.h>
#include <math.h>

struct Renderer {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    CAMetalLayer *layer;
    int width;
    int height;
    id<MTLBuffer> vertexBuffer;
    id<MTLBuffer> indexBuffer;
    uint32_t indexCount;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLDepthStencilState> depthState;
    id<MTLTexture> depthTexture;
    bool hasMesh;
};

static void create_pipeline(Renderer *r);

Renderer *renderer_create(void *native_window_surface) {
    Renderer *r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;

    r->layer = (__bridge CAMetalLayer *)native_window_surface;
    r->device = MTLCreateSystemDefaultDevice();
    if (!r->device) {
        free(r);
        return NULL;
    }

    r->layer.device = r->device;
    r->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

    r->commandQueue = [r->device newCommandQueue];
    if (!r->commandQueue) {
        free(r);
        return NULL;
    }

    // Create default pipeline
    create_pipeline(r);

    // setup default pipeline and buffers
    // We can't fully initialize some resources until we have a layer pixelFormat
    // but we'll lazily create them when first drawing or right after create
    // by calling a helper if needed.

    return r;
}

static void create_pipeline(Renderer *r) {
    if (!r || !r->device) return;
    if (r->pipelineState) return; // already created

    // Shader source
    static const char *shaderSrc =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VertexOut { float4 position [[position]]; float4 color; };\n"
    "vertex VertexOut vertex_main(const device float3 *vertices [[buffer(0)]], constant float4x4 &mvp [[buffer(1)]], uint vid [[vertex_id]]) {\n"
    "    VertexOut out; float4 pos = float4(vertices[vid], 1.0); out.position = mvp * pos; out.color = float4((pos.xyz + float3(1.0))/2.0, 1.0); return out; }\n"
    "fragment float4 fragment_main(VertexOut in [[stage_in]]) { return in.color; }\n";

    NSError *error = nil;
    id<MTLLibrary> library = [r->device newLibraryWithSource:[NSString stringWithUTF8String:shaderSrc] options:nil error:&error];
    if (!library) return;

    id<MTLFunction> vert = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> frag = [library newFunctionWithName:@"fragment_main"];
    if (!vert || !frag) return;

    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vert;
    pipelineDescriptor.fragmentFunction = frag;
    pipelineDescriptor.colorAttachments[0].pixelFormat = r->layer.pixelFormat;
    pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

    // Vertex descriptor is optional since we use vertex_id, but set it for clarity
    MTLVertexDescriptor *vDesc = [[MTLVertexDescriptor alloc] init];
    vDesc.attributes[0].format = MTLVertexFormatFloat3;
    vDesc.attributes[0].offset = 0;
    vDesc.attributes[0].bufferIndex = 0;
    vDesc.layouts[0].stride = sizeof(float) * 3;
    pipelineDescriptor.vertexDescriptor = vDesc;

    r->pipelineState = [r->device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!r->pipelineState) return;

    MTLDepthStencilDescriptor *depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthDesc.depthWriteEnabled = YES;
    r->depthState = [r->device newDepthStencilStateWithDescriptor:depthDesc];
}

void renderer_set_mesh(Renderer *r, const Candid_3D_Mesh *mesh) {
    if (!r || !r->device || !mesh) return;
    if (!mesh->vertices[0] || !mesh->triangle_vertex[0]) return;

    // Clear existing buffers if any
    r->vertexBuffer = nil;
    r->indexBuffer = nil;
    r->indexCount = 0;
    r->hasMesh = false;

    const uint32_t vertexCount = (uint32_t)mesh->vertex_count;
    const uint32_t triangleCount = (uint32_t)mesh->triangle_count;

    // Allocate temporary arrays
    uint16_t *indices = malloc(triangleCount * 3 * sizeof(uint16_t));
    if (!indices) return;

    float *vertexData = malloc(vertexCount * 3 * sizeof(float));
    if (!vertexData) {
        free(indices);
        return;
    }

    // Interleave vertex positions
    for (uint32_t i = 0; i < vertexCount; ++i) {
        vertexData[i * 3 + 0] = mesh->vertices[0][i];
        vertexData[i * 3 + 1] = mesh->vertices[1][i];
        vertexData[i * 3 + 2] = mesh->vertices[2][i];
    }

    // Build index buffer
    for (uint32_t t = 0; t < triangleCount; ++t) {
        indices[t * 3 + 0] = (uint16_t)mesh->triangle_vertex[0][t];
        indices[t * 3 + 1] = (uint16_t)mesh->triangle_vertex[1][t];
        indices[t * 3 + 2] = (uint16_t)mesh->triangle_vertex[2][t];
    }

    // Create GPU buffers
    r->vertexBuffer = [r->device newBufferWithBytes:vertexData
                                             length:vertexCount * 3 * sizeof(float)
                                            options:MTLResourceStorageModeShared];
    r->indexBuffer = [r->device newBufferWithBytes:indices
                                            length:triangleCount * 3 * sizeof(uint16_t)
                                           options:MTLResourceStorageModeShared];
    r->indexCount = triangleCount * 3;
    r->hasMesh = (r->vertexBuffer && r->indexBuffer);

    // Free temporary data
    free(vertexData);
    free(indices);
}

void renderer_resize(Renderer *r, int width, int height) {
    if (!r) return;
    r->width = width;
    r->height = height;
    r->layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
    // (Re)create depth texture
    if (r->depthTexture) {
        r->depthTexture = nil;
    }
    if (width > 0 && height > 0) {
        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:(NSUInteger)width height:(NSUInteger)height mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;
        r->depthTexture = [r->device newTextureWithDescriptor:desc];
    }
}

void renderer_draw_frame(Renderer *r, float time) {
    if (!r) return;

    @autoreleasepool {
        // Ensure pipeline is created
        if (!r->pipelineState) {
            create_pipeline(r);
        }

        // Skip rendering if no mesh has been set
        if (!r->hasMesh || !r->vertexBuffer || !r->indexBuffer) {
            return;
        }

        id<CAMetalDrawable> drawable = [r->layer nextDrawable];
        if (!drawable) return;
        if (!r->pipelineState) {
            return;
        }
        // Ensure we have the pipeline and buffers (already checked above)

        MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        passDescriptor.colorAttachments[0].texture = drawable.texture;
        passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.2, 0.2, 0.2, 1.0);
        if (r->depthTexture) {
            passDescriptor.depthAttachment.texture = r->depthTexture;
            passDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
            passDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
            passDescriptor.depthAttachment.clearDepth = 1.0;
        }

        id<MTLCommandBuffer> commandBuffer = [r->commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
        [encoder setRenderPipelineState:r->pipelineState];
        if (r->depthState) {
            [encoder setDepthStencilState:r->depthState];
        }

        // Vertex buffer (positions)
        [encoder setVertexBuffer:r->vertexBuffer offset:0 atIndex:0];

        // Compute a simple MVP matrix
        const float aspect = (r->width > 0 && r->height > 0) ? ((float)r->width / (float)r->height) : 1.0f;
        const float fovy = 65.0f * (float)(M_PI / 180.0);
        float near = 0.1f;
        float far = 100.0f;
        float f = 1.0f / tanf(fovy * 0.5f);
        float proj[16] = {0};
        proj[0] = f / aspect;
        proj[5] = f;
        proj[10] = (far + near) / (near - far);
        proj[11] = -1.0f;
        proj[14] = (2.0f * far * near) / (near - far);

        float rotY = time * 0.8f;
        float rotX = time * 0.4f;
        float cosY = cosf(rotY);
        float sinY = sinf(rotY);
        float cosX = cosf(rotX);
        float sinX = sinf(rotX);

        float model[16] = {0};
        // rotation X then Y
        model[0] = cosY;
        model[2] = sinY;
        model[5] = cosX;
        model[6] = -sinX;
        model[8] = -sinY;
        model[10] = cosY * cosX;
        model[12] = 0.0f;
        model[13] = 0.0f;
        model[14] = -3.0f; // translate back
        model[15] = 1.0f;

        // Multiply proj * model (column major)
        float mvp[16];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += proj[k * 4 + i] * model[j * 4 + k];
                }
                mvp[j * 4 + i] = sum;
            }
        }

        // Create/update uniform buffer
        id<MTLBuffer> uniformBuffer = [r->device newBufferWithBytes:mvp length:sizeof(mvp) options:MTLResourceStorageModeShared];
        [encoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];

        // Draw
        [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
        [encoder setCullMode:MTLCullModeBack];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:r->indexCount indexType:MTLIndexTypeUInt16 indexBuffer:r->indexBuffer indexBufferOffset:0];

        [encoder endEncoding];
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
    }
}

void renderer_destroy(Renderer *r) {
    if (!r) return;
    // Release Metal objects (ARC handles Objective-C refs)
    r->vertexBuffer = nil;
    r->indexBuffer = nil;
    r->pipelineState = nil;
    r->depthState = nil;
    r->depthTexture = nil;
    r->commandQueue = nil;
    r->device = nil;
    r->layer = nil;
    free(r);
}
