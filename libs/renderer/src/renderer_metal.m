/**
 * @file renderer_metal.m
 * @brief Metal backend implementation for macOS
 */

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <candid/renderer.h>
#include <stdlib.h>

struct Renderer {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    CAMetalLayer *layer;
    int width;
    int height;
};

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

    return r;
}

void renderer_resize(Renderer *r, int width, int height) {
    if (!r) return;
    r->width = width;
    r->height = height;
    r->layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
}

void renderer_draw_frame(Renderer *r, float time) {
    if (!r) return;

    @autoreleasepool {
        id<CAMetalDrawable> drawable = [r->layer nextDrawable];
        if (!drawable) return;

        // Simple clear color animation
        double red = (double)((sinf(time) + 1.0f) * 0.5f);
        double green = (double)((cosf(time * 0.7f) + 1.0f) * 0.5f);
        double blue = (double)((sinf(time * 1.3f) + 1.0f) * 0.5f);

        MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        passDescriptor.colorAttachments[0].texture = drawable.texture;
        passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(red, green, blue, 1.0);

        id<MTLCommandBuffer> commandBuffer = [r->commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
        [encoder endEncoding];

        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];
    }
}

void renderer_destroy(Renderer *r) {
    if (!r) return;
    // ARC handles Metal object cleanup
    free(r);
}
