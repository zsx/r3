/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Additional code modifications and improvements Copyright 2012 Saphirion AG
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Title: <platform/backend> Compositor abstraction layer API.
**  Author: Richard Smolak
**  File:  host-compositor.c
**  Purpose: Provides simple example of gfx backend specific compositor.
**  Note: This is not fully working code, see the notes for insertion
**        of your backend specific code. Of course the example can be fully
**        modified according to the specific backend. Only the declarations
**        of compositor API calls must remain consistent.
**
************************************************************************
**
**  NOTE to PROGRAMMERS:
**
**    1. Keep code clear and simple.
**    2. Document unusual code, reasoning, or gotchas.
**    3. Use same style for code, vars, indent(4), comments, etc.
**    4. Keep in mind Linux, OS X, BSD, big/little endian CPUs.
**    5. Test everything, then test it again.
**
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>	//for floor()
#include "reb-host.h"
#include "SDL.h"

#include "host-view.h"
#include "host-draw-api.h"

//#include <GL/glew.h>
//#include "nanovg.h"
//#define NANOVG_GL3_IMPLEMENTATION   // Use GL3 implementation.
//#define NANOVG_FBO_VALID
//#include "nanovg_gl.h"

void* Find_Window(REBGOB *gob);

//***** Macros *****
#define GOB_HWIN(gob)	(Find_Window(gob))

//***** Locals *****

static REBXYF Zero_Pair = {0, 0};

typedef struct {
	REBINT left;
	REBINT top;
	REBINT right;
	REBINT bottom;
} REBRECT;

//NOTE: Following structure holds just basic compositor 'instance' values that
//are used internally by the compositor API.
//None of the values should be accessed directly from external code.
//The structure can be extended/modified according to the specific backend needs.
typedef struct {
	REBYTE *Window_Buffer;
	REBXYI winBufSize;
	REBGOB *Win_Gob;
	REBGOB *Root_Gob;
	REBXYF absOffset;
	SDL_Renderer *renderer;
	SDL_Surface *surface;
	SDL_GLContext *sdl_ctx;
	SDL_Window *win;
	SDL_Rect	clip;
	REBDRW_CTX *draw_ctx;
	int		pixel_pitch;
} REBCMP_CTX;

void rebdrw_gob_draw(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);
void rebdrw_gob_color(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);
void rebdrw_gob_image(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz);

/***********************************************************************
**
*/ REBYTE* rebcmp_get_buffer(REBCMP_CTX* ctx)
/*
**	Provide pointer to window compositing buffer.
**  Return NULL if buffer not available of call failed.
**
**  NOTE: The buffer may be "locked" during this call on some platforms.
**        Always call rebcmp_release_buffer() to be sure it is released.
**
***********************************************************************/
{
#if 0
	if (SDL_MUSTLOCK(ctx->surface)) {
		SDL_LockSurface(ctx->surface);
	}
	return ctx->surface->pixels;
#endif

	return NULL;
}

/***********************************************************************
**
*/ void rebcmp_release_buffer(REBCMP_CTX* ctx)
/*
**	Release the window compositing buffer acquired by rebcmp_get_buffer().
**
**  NOTE: this call can be "no-op" on platforms that don't need locking.
**
***********************************************************************/
{
	return;
#if 0
	if (SDL_MUSTLOCK(ctx->surface)) {
		SDL_UnlockSurface(ctx->surface);
	}
#endif
}

/***********************************************************************
**
*/ REBOOL rebcmp_resize_buffer(REBCMP_CTX* ctx, REBGOB* winGob)
/*
**	Resize the window compositing buffer.
**
**  Returns TRUE if buffer size was really changed, otherwise FALSE.
**
***********************************************************************/
{

	//check if window size really changed or buffer needs to be created
	if ((GOB_LOG_W(winGob) != GOB_WO(winGob))
		|| (GOB_LOG_H(winGob) != GOB_HO(winGob))) {

		REBINT w = GOB_LOG_W_INT(winGob);
		REBINT h = GOB_LOG_H_INT(winGob);

		//------------------------------
		//Put backend specific code here
		//------------------------------

		//update the buffer size values
		ctx->winBufSize.x = w;
		ctx->winBufSize.y = h;

		rebdrw_resize_context(ctx->draw_ctx, w, h);

#if 0
		if (ctx->surface) {
			SDL_FreeSurface(ctx->surface);
		}
		ctx->surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

		if (ctx->surface == NULL) {
			fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError());
			exit(1);
		}
#endif

		//update old gob area
		GOB_XO(winGob) = GOB_LOG_X(winGob);
		GOB_YO(winGob) = GOB_LOG_Y(winGob);
		GOB_WO(winGob) = GOB_LOG_W(winGob);
		GOB_HO(winGob) = GOB_LOG_H(winGob);
		return TRUE;
	}
	return FALSE;
}

/***********************************************************************
**
*/ void* rebcmp_create(REBGOB* rootGob, REBGOB* gob)
/*
**	Create new Compositor instance.
**
***********************************************************************/
{
	SDL_Window *win = NULL;

	//new compositor struct
	REBCMP_CTX *ctx = (REBCMP_CTX*)OS_ALLOC_ARRAY(REBCMP_CTX, 1);
	int w = GOB_LOG_W_INT(gob);
	int h = GOB_LOG_H_INT(gob);

	memset(ctx, 0, sizeof(REBCMP_CTX));

	//shortcuts
	ctx->Root_Gob = rootGob;
	ctx->Win_Gob = gob;
	ctx->pixel_pitch = 4;

	//------------------------------
	//Put backend specific code here
	//------------------------------
	
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "creating ctx %p, gob %p, rootGob: %p (%ldx%ld)\n", ctx, gob, rootGob, GOB_LOG_W_INT(rootGob), GOB_LOG_H_INT(rootGob));
	ctx->win = GOB_HWIN(gob);
	//ctx->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	//ctx->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_SOFTWARE);
#if 0
	ctx->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

	if (!ctx->renderer) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a render\n");
	}
	SDL_RenderClear(ctx->renderer);
#endif

	ctx->sdl_ctx = SDL_GL_CreateContext(ctx->win);
	SDL_GL_MakeCurrent(ctx->win, ctx->sdl_ctx);

	printf("Graphic system is initialized.\n");
	ctx->draw_ctx = rebdrw_create_context(w, h);

	//call resize to init buffer
	rebcmp_resize_buffer(ctx, gob);
	//ctx->winBufSize.x = w;
	//ctx->winBufSize.y = h;
	return ctx;
}

/***********************************************************************
**
*/ void rebcmp_destroy(REBCMP_CTX* ctx)
/*
**	Destroy existing Compositor instance.
**
***********************************************************************/
{
	//------------------------------
	//Put backend specific code here
	//------------------------------
	//SDL_FreeSurface(ctx->surface);
	//OS_Free(ctx->Window_Buffer);
	
	rebdrw_destroy_context(ctx->draw_ctx);
	SDL_DestroyRenderer(ctx->renderer);
	OS_FREE(ctx);
}

/***********************************************************************
**
*/ static void process_gobs(REBCMP_CTX* ctx, REBGOB* gob)
/*
**	Recursively process and compose gob and its children.
**
** NOTE: this function is used internally by rebcmp_compose() call only.
**
***********************************************************************/
{

	REBINT x = ROUND_TO_INT(ctx->absOffset.x);
	REBINT y = ROUND_TO_INT(ctx->absOffset.y);
	REBRECT gob_clip;
	SDL_Rect saved_clip = ctx->clip;
	SDL_Rect gob_rect = {x, y, GOB_LOG_W(gob), GOB_LOG_H(gob)};

	//SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "processing ctx %p, gob %p\n", ctx, gob);
	if (GET_GOB_STATE(gob, GOBS_NEW)){
		//reset old-offset and old-size if newly added
		GOB_XO(gob) = GOB_LOG_X(gob);
		GOB_YO(gob) = GOB_LOG_Y(gob);
		GOB_WO(gob) = GOB_LOG_W(gob);
		GOB_HO(gob) = GOB_LOG_H(gob);

		CLR_GOB_STATE(gob, GOBS_NEW);
	}

	//printf("src ctx->clip: top-left, (%d, %d) -> (w, h), (%d, %d)\n", ctx->clip.x, ctx->clip.y, ctx->clip.w, ctx->clip.h);
	//printf("src gob rect: top-left, (%d, %d) -> (w, h), (%d, %d)\n", gob_rect.x, gob_rect.y, gob_rect.w, gob_rect.h);
	SDL_IntersectRect(&ctx->clip, &gob_rect, &ctx->clip);

	gob_clip.left = ctx->clip.x;
	gob_clip.top = ctx->clip.y;
	gob_clip.right = ctx->clip.w + ctx->clip.x;
	gob_clip.bottom = ctx->clip.h + ctx->clip.y;

	//printf("ctx->clip: top-left, (%d, %d) -> (w, h), (%d, %d)\n", ctx->clip.x, ctx->clip.y, ctx->clip.w, ctx->clip.h);
	if (!SDL_RectEmpty(&ctx->clip))
	{
		//renderer GOB content
		switch (GOB_TYPE(gob)) {
			case GOBT_COLOR:
				//------------------------------
				//Put backend specific code here
				//------------------------------
				// or use the similar draw api call:
				rebdrw_gob_color(gob, ctx->draw_ctx, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
				break;

			case GOBT_IMAGE:
				{
					//------------------------------
					//Put backend specific code here
					//------------------------------
					// or use the similar draw api call:
					rebdrw_gob_image(gob, ctx->draw_ctx, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
				}
				break;

			case GOBT_DRAW:
				{
					//------------------------------
					//Put backend specific code here
					//------------------------------
					// or use the similar draw api call:
					rebdrw_gob_draw(gob, ctx->draw_ctx, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
				}
				break;

			case GOBT_TEXT:
			case GOBT_STRING:
				//------------------------------
				//Put backend specific code here
				//------------------------------
				// or use the similar draw api call:
				//rt_gob_text(gob, ctx->Window_Buffer ,ctx->winBufSize,ctx->absOffset, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
				break;

			case GOBT_EFFECT:
				//not yet implemented
				break;
		}

		//recursively process sub GOBs
		if (GOB_PANE(gob)) {
			REBINT n;
			REBINT len = GOB_TAIL(gob);
			REBGOB **gp = GOB_HEAD(gob);

			for (n = 0; n < len; n++, gp++) {
				REBINT g_x = GOB_LOG_X(*gp);
				REBINT g_y = GOB_LOG_Y(*gp);

				//restore the "parent gob" clip region
				//------------------------------
				//Put backend specific code here
				//------------------------------

				ctx->absOffset.x += g_x;
				ctx->absOffset.y += g_y;

				process_gobs(ctx, *gp);

				ctx->absOffset.x -= g_x;
				ctx->absOffset.y -= g_y;
			}
		}
	} else {
	//	printf("Skipping gob %p because of the empty clip\n", gob);
	}

	ctx->clip = saved_clip;
}

/***********************************************************************
**
*/ void rebcmp_compose(REBCMP_CTX* ctx, REBGOB* winGob, REBGOB* gob, REBOOL only)
/*
**	Compose content of the specified gob. Main compositing function.
**
**  If the ONLY arg is TRUE then the specified gob area will be
**  renderered to the buffer at 0x0 offset.(used by TO-IMAGE)
**
***********************************************************************/
{
	REBINT max_depth = 1000; // avoid infinite loops
	REBD32 abs_x = 0;
	REBD32 abs_y = 0;
	REBD32 abs_ox;
	REBD32 abs_oy;
	REBGOB* parent_gob = gob;

	SDL_Rect old_clip;

	//reset clip region to window area
	//------------------------------
	//Put backend specific code here
	//------------------------------
	//SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "composing ctx: %p, win_gob: %p, gob: %p, size: %dx%d\n",
	//			 ctx, winGob, gob, GOB_LOG_W_INT(gob), GOB_LOG_H_INT(gob));

	//calculate absolute offset of the gob
	while (GOB_PARENT(parent_gob) && (max_depth-- > 0) && !GET_GOB_FLAG(parent_gob, GOBF_WINDOW))
	{
		abs_x += GOB_LOG_X(parent_gob);
		abs_y += GOB_LOG_Y(parent_gob);
		parent_gob = GOB_PARENT(parent_gob);
	}

	//the offset is shifted to renderer given gob at offset 0x0 (used by TO-IMAGE)
	if (only){
		ctx->absOffset.x = -abs_x;
		ctx->absOffset.y = -abs_y;
		abs_x = 0;
		abs_y = 0;
	} else {
		ctx->absOffset.x = 0;
		ctx->absOffset.y = 0;
	}

	ctx->clip.x = abs_x;
	ctx->clip.y = abs_y;
	ctx->clip.w = GOB_LOG_W_INT(gob);
	ctx->clip.h = GOB_LOG_H_INT(gob);

	//handle newly added gob case
	if (!GET_GOB_STATE(gob, GOBS_NEW)){
		//calculate absolute old offset of the gob
		abs_ox = abs_x + (GOB_XO(gob) - GOB_LOG_X(gob));
		abs_oy = abs_y + (GOB_YO(gob) - GOB_LOG_Y(gob));

		//set region with old gob location and dimensions
		old_clip.x = abs_ox;
		old_clip.y = abs_oy;
		old_clip.w = GOB_WO_INT(gob);
		old_clip.h = GOB_HO_INT(gob);

		SDL_UnionRect (&ctx->clip, &old_clip, &ctx->clip);
	}

	if (!SDL_RectEmpty(&ctx->clip))
	{
		ctx->Window_Buffer = rebcmp_get_buffer(ctx);

		SDL_GL_MakeCurrent(ctx->win, ctx->sdl_ctx);
		rebdrw_begin_frame(ctx->draw_ctx);
		//draw(vg);
		printf("abs oft: (%f, %f)\n", ctx->absOffset.x, ctx->absOffset.y);
		process_gobs(ctx, winGob);

		//redraw gobs
		rebdrw_end_frame(ctx->draw_ctx);

		rebcmp_release_buffer(ctx);

		ctx->Window_Buffer = NULL;
	}

	//update old GOB area
	GOB_XO(gob) = GOB_LOG_X(gob);
	GOB_YO(gob) = GOB_LOG_Y(gob);
	GOB_WO(gob) = GOB_LOG_W(gob);
	GOB_HO(gob) = GOB_LOG_H(gob);
}

/***********************************************************************
**
*/ void rebcmp_blit(REBCMP_CTX* ctx)
/*
**	Blit window content on the screen.
**
***********************************************************************/
{
	rebdrw_blit_frame(ctx->draw_ctx);
	SDL_GL_SwapWindow(ctx->win);
}