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
#include <SDL.h>

#include "host-view.h"
#include "host-renderer.h"
#include "host-draw-api.h"
#include "host-text-api.h"

void* Find_Window(REBGOB *gob);

//***** Macros *****
#define GOB_HWIN(gob)	(Find_Window(gob))

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
	REBGOB 		*Win_Gob;
	REBGOB 		*Root_Gob;
	REBXYF 		absOffset;
	SDL_Window 	*win;
	SDL_Rect	clip;
	REBDRW_CTX 	*draw_ctx;
} REBCMP_CTX;

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

		rebol_renderer->resize_draw_context(ctx->draw_ctx, w, h);

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

	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "creating ctx %p, gob %p, rootGob: %p (%dx%d)\n", ctx, gob, rootGob, GOB_LOG_W_INT(rootGob), GOB_LOG_H_INT(rootGob));
	ctx->win = GOB_HWIN(gob);

	ctx->draw_ctx = rebol_renderer->create_draw_context(ctx->win, w, h);

	//call resize to init buffer
	rebcmp_resize_buffer(ctx, gob);
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
	rebol_renderer->destroy_draw_context(ctx->draw_ctx);
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
		REBXYI offset = {x, y};
		REBXYI top = {gob_clip.left, gob_clip.top};
		REBXYI bottom = {gob_clip.right, gob_clip.bottom};

		//renderer GOB content
		switch (GOB_TYPE(gob)) {
			case GOBT_COLOR:
				rebol_renderer->draw->rebdrw_gob_color(gob, ctx->draw_ctx, offset, top, bottom);
				break;

			case GOBT_IMAGE:
				rebol_renderer->draw->rebdrw_gob_image(gob, ctx->draw_ctx, offset, top, bottom);
				break;

			case GOBT_DRAW:
				rebol_renderer->draw->rebdrw_gob_draw(gob, ctx->draw_ctx, offset, top, bottom);
				break;

			case GOBT_TEXT:
			case GOBT_STRING:
				rebol_renderer->text->rt_gob_text(gob, ctx->draw_ctx, offset, top, bottom);
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
*/ void rebcmp_compose(REBCMP_CTX* ctx, REBGOB* winGob, REBGOB* gob, char *buf)
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
	if (buf){ /* render to a memory buffer */
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
		rebol_renderer->begin_frame(ctx->draw_ctx);
		process_gobs(ctx, winGob);
		rebol_renderer->end_frame(ctx->draw_ctx);
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
	//printf("%s\n", __FUNCTION__);
	rebol_renderer->blit_frame(ctx->draw_ctx, &ctx->clip);
}
