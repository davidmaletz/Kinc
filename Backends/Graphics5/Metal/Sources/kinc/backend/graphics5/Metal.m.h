#include "Metal.h"

#include <kinc/color.h>
#include <kinc/system.h>
#include <kinc/window.h>

#include <kinc/graphics5/commandlist.h>
#include <kinc/graphics5/rendertarget.h>

#import <Metal/Metal.h>
#import <MetalKit/MTKView.h>

id getMetalLayer(void);
id getMetalDevice(void);
id getMetalQueue(void);

int renderTargetWidth;
int renderTargetHeight;
int newRenderTargetWidth;
int newRenderTargetHeight;

id<CAMetalDrawable> drawable;
id<MTLCommandBuffer> commandBuffer;
id<MTLRenderCommandEncoder> commandEncoder;
id<MTLTexture> depthTexture;
int depthBits;
int stencilBits;

static kinc_g5_render_target_t fallback_render_target;

id getMetalEncoder(void) {
	return commandEncoder;
}

void kinc_g5_internal_destroy_window(int window) {}

void kinc_g5_internal_destroy() {}

extern void kinc_g4_on_g5_internal_resize(int, int, int);

void kinc_internal_resize(int window, int width, int height) {
	kinc_g4_on_g5_internal_resize(window, width, height);
}

extern void kinc_internal_init_samplers(void);

void kinc_g5_internal_init() {}

void kinc_g5_internal_init_window(int window, int depthBufferBits, int stencilBufferBits, bool vsync) {
	depthBits = depthBufferBits;
	stencilBits = stencilBufferBits;
	kinc_internal_init_samplers();
	kinc_g5_render_target_init(&fallback_render_target, 32, 32, 0, false, KINC_G5_RENDER_TARGET_FORMAT_32BIT, 0, 0);
}

void kinc_g5_flush() {}

void kinc_g5_draw_indexed_vertices_instanced(int instanceCount) {}

void kinc_g5_draw_indexed_vertices_instanced_from_to(int instanceCount, int start, int count) {}

bool kinc_internal_metal_has_depth = false;

bool kinc_internal_current_render_target_has_depth(void) {
	return kinc_internal_metal_has_depth;
}

void kinc_g5_begin(kinc_g5_render_target_t *renderTarget, int window) {
	CAMetalLayer *metalLayer = getMetalLayer();
	drawable = [metalLayer nextDrawable];

	if (depthBits > 0 && (depthTexture == nil || depthTexture.width != drawable.texture.width || depthTexture.height != drawable.texture.height)) {
		MTLTextureDescriptor *descriptor = [MTLTextureDescriptor new];
		descriptor.textureType = MTLTextureType2D;
		descriptor.width = drawable.texture.width;
		descriptor.height = drawable.texture.height;
		descriptor.depth = 1;
		descriptor.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
		descriptor.arrayLength = 1;
		descriptor.mipmapLevelCount = 1;
		descriptor.resourceOptions = MTLResourceStorageModePrivate;
		descriptor.usage = MTLTextureUsageRenderTarget;
		id<MTLDevice> device = getMetalDevice();
		depthTexture = [device newTextureWithDescriptor:descriptor];
		kinc_internal_metal_has_depth = true;
	}
	else {
		kinc_internal_metal_has_depth = false;
	}

	id<MTLTexture> texture = drawable.texture;
	MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	renderPassDescriptor.colorAttachments[0].texture = texture;
	renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
	renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
	renderPassDescriptor.depthAttachment.clearDepth = 1;
	renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
	renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
	renderPassDescriptor.depthAttachment.texture = depthTexture;
	renderPassDescriptor.stencilAttachment.clearStencil = 0;
	renderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionDontCare;
	renderPassDescriptor.stencilAttachment.storeAction = MTLStoreActionDontCare;
	renderPassDescriptor.stencilAttachment.texture = depthTexture;

	if (commandBuffer != nil && commandEncoder != nil) {
		[commandEncoder endEncoding];
		[commandBuffer commit];
	}
	
	id<MTLCommandQueue> commandQueue = getMetalQueue();
	commandBuffer = [commandQueue commandBuffer];
	commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
}

void kinc_g5_end(int window) {}

bool kinc_g5_swap_buffers() {
	if (commandBuffer != nil && commandEncoder != nil) {
		[commandEncoder endEncoding];
		[commandBuffer presentDrawable:drawable];
		[commandBuffer commit];
	}
	drawable = nil;
	commandBuffer = nil;
	commandEncoder = nil;

	return true;
}

int kinc_g5_max_bound_textures(void) {
	return 16;
}

bool kinc_g5_render_targets_inverted_y() {
	return false;
}

bool kinc_g5_non_pow2_textures_qupported() {
	return true;
}

bool kinc_window_vsynced(int window) {
	return true;
}

void kinc_g5_internal_new_render_pass(kinc_g5_render_target_t **renderTargets, int count, bool wait, unsigned clear_flags, unsigned color, float depth,
                                      int stencil) {
	if (commandBuffer != nil && commandEncoder != nil) {
		[commandEncoder endEncoding];
		[commandBuffer commit];
		if (wait) {
			[commandBuffer waitUntilCompleted];
		}
	}

	MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	for (int i = 0; i < count; ++i) {
		if (renderTargets == NULL) {
			if (drawable == nil) {
				renderPassDescriptor.colorAttachments[i].texture = (__bridge id<MTLTexture>)fallback_render_target.impl._tex;
				renderPassDescriptor.depthAttachment.texture = nil;
				renderPassDescriptor.stencilAttachment.texture = nil;
				kinc_internal_metal_has_depth = false;
			}
			else {
				renderPassDescriptor.colorAttachments[i].texture = drawable.texture;
				renderPassDescriptor.depthAttachment.texture = depthTexture;
				renderPassDescriptor.stencilAttachment.texture = depthTexture;
				kinc_internal_metal_has_depth = depthTexture != nil;
			}
		}
		else {
			renderPassDescriptor.colorAttachments[i].texture = (__bridge id<MTLTexture>)renderTargets[i]->impl._tex;
			renderPassDescriptor.depthAttachment.texture = (__bridge id<MTLTexture>)renderTargets[0]->impl._depthTex;
			renderPassDescriptor.stencilAttachment.texture = (__bridge id<MTLTexture>)renderTargets[0]->impl._depthTex;
			kinc_internal_metal_has_depth = renderTargets[0]->impl._depthTex != nil;
		}
		if (clear_flags & KINC_G5_CLEAR_COLOR) {
			float red, green, blue, alpha;
			kinc_color_components(color, &red, &green, &blue, &alpha);
			renderPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionClear;
			renderPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
			renderPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(red, green, blue, alpha);
		}
		else {
			renderPassDescriptor.colorAttachments[i].loadAction = MTLLoadActionLoad;
			renderPassDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
			renderPassDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
		}
	}

	if (clear_flags & KINC_G5_CLEAR_DEPTH) {
		renderPassDescriptor.depthAttachment.clearDepth = depth;
		renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
		renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
	}
	else {
		renderPassDescriptor.depthAttachment.clearDepth = 1;
		renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
		renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
	}
	
	if (clear_flags & KINC_G5_CLEAR_STENCIL) {
		renderPassDescriptor.stencilAttachment.clearStencil = stencil;
		renderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionClear;
		renderPassDescriptor.stencilAttachment.storeAction = MTLStoreActionStore;
	}
	else {
		renderPassDescriptor.stencilAttachment.clearStencil = 0;
		renderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionDontCare;
		renderPassDescriptor.stencilAttachment.storeAction = MTLStoreActionDontCare;
	}
		
	id<MTLCommandQueue> commandQueue = getMetalQueue();
	commandBuffer = [commandQueue commandBuffer];
	commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
}
