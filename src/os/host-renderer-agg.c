#include <stdlib.h>

#include "reb-host.h"
#include "host-renderer.h"
#include "host-text-api.h"
#include "host-draw-api-agg.h"
#include <SDL.h>

static int agg_init(REBRDR *renderer)
{
	int ret = 0;

	if (renderer->text && renderer->text->init) {
		ret = renderer->text->init(renderer->text);
	}

	return ret;
}

static REBDRW_CTX* agg_create_draw_context(SDL_Window *win, REBINT w, REBINT h)
{
	REBDRW_CTX *ctx = (REBDRW_CTX*)malloc(sizeof(REBDRW_CTX));
	if (ctx == NULL) return NULL;
	ctx->surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (ctx->surface == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
	if (ctx->renderer == NULL) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a render\n");
		SDL_FreeSurface(ctx->surface);
		free(ctx);
		return NULL;
	}
	SDL_RenderClear(ctx->renderer);
	return ctx;
}

static void agg_resize_draw_context(REBDRW_CTX *ctx, REBINT w, REBINT h)
{
	if (ctx == NULL) return;
	if (ctx->surface) {
		SDL_FreeSurface(ctx->surface);
	}
	ctx->surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

	if (ctx->surface == NULL) {
		//fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
		return;
	}
}

static void agg_destroy_draw_context(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;
	SDL_FreeSurface(ctx->surface);
	SDL_DestroyRenderer(ctx->renderer);

	free(ctx);
}

static void agg_begin_frame(REBDRW_CTX *ctx)
{
	//printf("begin frame: %d\n", __LINE__);
	if (ctx == NULL) return;
	if (SDL_MUSTLOCK(ctx->surface)) {
		SDL_LockSurface(ctx->surface);
	}
}

static void agg_end_frame(REBDRW_CTX *ctx)
{
	if (ctx == NULL) return;
	if (SDL_MUSTLOCK(ctx->surface)) {
		SDL_UnlockSurface(ctx->surface);
	}
}

static void agg_blit_frame(REBDRW_CTX *ctx, SDL_Rect *clip)
{
	SDL_Texture *texture = NULL;

	if (ctx == NULL) return;

	//SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "bliting ctx %p\n", ctx);

	texture = SDL_CreateTextureFromSurface(ctx->renderer, ctx->surface);

	SDL_RenderCopy(ctx->renderer, texture, clip, clip);
	//SDL_RenderCopy(ctx->renderer, texture, NULL, NULL);
	SDL_RenderPresent(ctx->renderer);

	SDL_DestroyTexture(texture);
}

extern REBRDR_DRW draw_agg;
extern REBRDR_TXT text_agg;

REBRDR rebrdr_agg = {
	.name = "AGG",
	.init = agg_init,
	.fini = NULL,
	.begin_frame = agg_begin_frame,
	.end_frame = agg_end_frame,
	.blit_frame = agg_blit_frame,
	.create_draw_context = agg_create_draw_context,
	.resize_draw_context = agg_resize_draw_context,
	.destroy_draw_context = agg_destroy_draw_context,

	.draw = &draw_agg,
	.text = &text_agg
};
