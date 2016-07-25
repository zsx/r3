#include <stdlib.h>

#include <SDL.h>
#include <GL/glew.h>

#include <nanovg.h>

#define NANOVG_GL3
#include <nanovg_gl.h>

#include "reb-host.h"
#include "host-renderer.h"
#include "host-text-api.h"
#include "host-draw-api-nanovg.h"

#include "nvtx.h"

//#define NO_FRAME_BUFFER //Nsight seems to ignore drawings to the non-default framebuffer

void paint_image(REBDRW_CTX *ctx, int image, REBINT mode, float alpha,
	REBXYF image_oft, REBXYF image_size,
	REBXYF clip_oft, REBXYF clip_size);

#define PAINT_LAYER(ctx, layer, paint_mode, alpha, clip_oft, clip_size) 	\
	do {												\
		REBXYF img_oft = {0, 0};						\
		REBXYF img_size = {ctx->ww, ctx->wh};			\
		paint_image(ctx, (layer)->image, paint_mode, alpha, img_oft, img_size, clip_oft, clip_size);\
	} while (0)

#define PAINT_LAYER_FULL(ctx, layer, paint_mode) 	\
	do {												\
		REBXYF clip_oft = {0, 0};						\
		REBXYF clip_size = {ctx->ww, ctx->wh};			\
		PAINT_LAYER(ctx, layer, paint_mode, 1.0f, clip_oft, clip_size);\
	} while (0)

static int sdl_gl_swap_interval = 0;

static int nanovg_init(REBRDR *renderer)
{
	SDL_Window *dummy_win = NULL;
	SDL_GLContext *gl_ctx = NULL;
	GLenum glew_err;
	const char *s_int = NULL;
	char *s_end = NULL;
	int interval = 0;
	int ver_major = 3;
	int ver_minor = 2;
	int ret = 0;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	dummy_win = SDL_CreateWindow("dummy", 0, 0, 1, 1, SDL_WINDOW_OPENGL);
	gl_ctx = SDL_GL_CreateContext(dummy_win);
	if (gl_ctx == NULL) {
		ret = -1;
		goto destroy_win;
	}
	if (SDL_GL_MakeCurrent(dummy_win, gl_ctx) < 0) {
		ret = -2;
		goto cleanup;
	}

	if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &ver_major) < 0
		|| SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &ver_minor) < 0) {
		ret = -3;
		goto cleanup;
	}

	if (ver_major < 3 || ver_minor < 2) {
		ret = -4;
		goto cleanup;
	}

	s_int = SDL_getenv("R3_VSYNC");
	if (s_int != NULL) {
		interval = strtol(s_int, &s_end, 10);
		if (s_end != s_int
			&& interval >= -1
			&& interval <= 1) {
			sdl_gl_swap_interval = interval;
		}
	}

	glewExperimental = 1; /* try to load every extension */
	glew_err = glewInit();
	if (glew_err != GLEW_OK) {
		ret = -5;
		goto cleanup;
	}

	if (!glewIsSupported("GL_VERSION_3_2")) {
		ret = -6;
		goto cleanup;
	}

	if (renderer->text && renderer->text->init) {
		ret = renderer->text->init(renderer->text);
	}

cleanup:
	SDL_GL_DeleteContext(gl_ctx);
destroy_win:
	SDL_DestroyWindow(dummy_win);
	return ret;
}

static int create_layers(REBDRW_CTX *ctx, REBINT w, REBINT h)
{
	if (ctx == NULL) return -1;
	ctx->win_layer = nvgCreateLayer(ctx->nvg, w, h, 0);
	if (ctx->win_layer == NULL) return -1;
	ctx->gob_layer = nvgCreateLayer(ctx->nvg, w, h, 0);
	if (ctx->gob_layer == NULL) {
		nvgDeleteLayer(ctx->nvg, ctx->win_layer);
		return -1;
	}
	ctx->tmp_layer = NULL;

	//printf("Created a NVG context at: %p, w: %d, h: %d\n", ctx->nvg, w, h);
	ctx->pixel_ratio = 1.0;	/* FIXME */

	/* initialize the GL context */
	glViewport(0, 0, w, h);

	//printf("begin win_layer: %d\n", __LINE__);
	nvgBeginLayer(ctx->nvg, ctx->win_layer);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	//printf("end of win_layer: %d\n", __LINE__);
	nvgEndLayer(ctx->nvg, ctx->win_layer);
    
    return 0;
}

static void delete_layers(REBDRW_CTX *ctx)
{
	nvgDeleteLayer(ctx->nvg, ctx->win_layer);
	ctx->win_layer = NULL;
	nvgDeleteLayer(ctx->nvg, ctx->gob_layer);
	ctx->gob_layer = NULL;
	if (ctx->tmp_layer) {
		nvgDeleteLayer(ctx->nvg, ctx->tmp_layer);
		ctx->tmp_layer = NULL;
	}
}

static REBDRW_CTX* nanovg_create_draw_context(SDL_Window *win, REBINT w, REBINT h)
{
	REBDRW_CTX *ctx = OS_ALLOC(REBDRW_CTX);
	if (ctx == NULL) return NULL;

	ctx->win = win;
	ctx->sdl = SDL_GL_CreateContext(win);
	if (ctx->sdl == NULL) {
		OS_FREE(ctx);
		return NULL;
	}

	SDL_GL_MakeCurrent(ctx->win, ctx->sdl);
	SDL_GL_SetSwapInterval(sdl_gl_swap_interval);

	ctx->ww = w;
	ctx->wh = h;
#ifdef DEBUG
	ctx->nvg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
#else
	ctx->nvg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
#endif
	ctx->fill_image = 0;
	ctx->stroke_image = 0;
	ctx->fill = FALSE;
	ctx->stroke = TRUE;
	ctx->last_x = 0;
	ctx->last_y = 0;

	if (ctx->nvg == NULL) {
		OS_FREE(ctx);
		return NULL;
	}
	if (create_layers(ctx, w, h) < 0){
		nvgDeleteGL3(ctx->nvg);
		OS_FREE(ctx);
		return NULL;
	}

	return ctx;
}

static void nanovg_resize_draw_context(REBDRW_CTX *ctx, REBINT w, REBINT h)
{
	if (ctx == NULL) return;
	ctx->ww = w;
	ctx->wh = h;
	delete_layers(ctx);
	create_layers(ctx, w, h);
}

static void nanovg_destroy_draw_context(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;

	delete_layers(ctx);

	if (ctx->fill_image != 0) {
		nvgDeleteImage(ctx->nvg, ctx->fill_image);
	}
	if (ctx->stroke_image != 0) {
		nvgDeleteImage(ctx->nvg, ctx->stroke_image);
	}

	SDL_GL_DeleteContext(ctx->sdl);

	nvgDeleteGL3(ctx->nvg);
	ctx->nvg = NULL;

	//printf("Destroyed a NVG context at: %p\n", ctx->nvg);

	OS_FREE(ctx);
}

static void nanovg_begin_frame(REBDRW_CTX *ctx)
{
	//printf("begin frame: %d\n", __LINE__);
	if (ctx == NULL) return;
    NVTX_MARK_FUNC_START();
	SDL_GL_MakeCurrent(ctx->win, ctx->sdl);
	nvgBeginFrame(ctx->nvg, ctx->ww, ctx->wh, ctx->pixel_ratio);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	//printf("begin win_layer: %d\n", __LINE__);
#ifndef NO_FRAME_BUFFER
	nvgBeginLayer(ctx->nvg, ctx->win_layer);
#endif
	/* Do NOT clear the win_layer, whose content will be reused, as it might only update part of the screen */
    NVTX_MARK_FUNC_END();
}

static void nanovg_end_frame(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;
    NVTX_MARK_FUNC_START();
#ifndef NO_FRAME_BUFFER
	nvgEndLayer(ctx->nvg, ctx->win_layer);
	nvgEndFrame(ctx->nvg);
#endif
#ifdef WITH_NVTX
    nvtxRangePop();
#endif
    NVTX_MARK_FUNC_END();
	//printf("End frame: %d\n", __LINE__);
}

static void nanovg_blit_frame(REBDRW_CTX *ctx, SDL_Rect *clip)
{
	if (ctx == NULL) return;
    NVTX_MARK_FUNC_START();
#ifndef NO_FRAME_BUFFER
	nvgBeginFrame(ctx->nvg, ctx->ww, ctx->wh, ctx->pixel_ratio);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	PAINT_LAYER_FULL(ctx, ctx->win_layer, NVG_SOURCE_OVER);
#endif
	//printf("End frame: %d\n", __LINE__);
	nvgEndFrame(ctx->nvg);
	SDL_GL_SwapWindow(ctx->win);
    NVTX_MARK_FUNC_END();
}

extern REBRDR_DRW draw_nanovg;
extern REBRDR_TXT text_nanovg;

REBRDR rebrdr_nanovg = {
	.name = "NANOVG",
	.init = nanovg_init,
	.fini = NULL,
	.begin_frame = nanovg_begin_frame,
	.end_frame = nanovg_end_frame,
	.blit_frame = nanovg_blit_frame,
	.create_draw_context = nanovg_create_draw_context,
	.resize_draw_context = nanovg_resize_draw_context,
	.destroy_draw_context = nanovg_destroy_draw_context,

	.draw = &draw_nanovg,
	.text = &text_nanovg,
	.default_SDL_win_flags = SDL_WINDOW_OPENGL
};
