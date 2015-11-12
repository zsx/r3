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
 **  Title: TEXT dialect API functions
 **  Author: Shixin Zeng
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

#if TO_WINDOWS
#include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "reb-host.h"
#include "host-view.h"
#include "host-renderer.h"
#include "host-text-api.h"
#include "host-ext-text.h"
#include "host-draw-api-nanovg.h"
#include "host-text-api-nanovg.h"

#include "nanovg.h"

#include "host-view.h"
#include "host-draw-api.h"

#include <GL/glew.h>

#include "nanovg_gl.h"

enum rich_text_mode {
	RT_DRAW,
	RT_SIZE
};

typedef struct {
	NVGcontext *nvg;
	REBFNT font_spec;
	REBPRA para_spec;
	REBINT x;
	REBINT y;
	REBINT w; /* -1 for no wrapping or clipping */
	REBINT h;
	REBINT line_height;
	REBINT clip_x;
	REBINT clip_y;
	REBINT clip_w;
	REBINT clip_h;

	enum rich_text_mode mode;
	float size_x;
	float size_y;
} Rich_Text;

unsigned char *find_font_path(
	const unsigned char* family,
	unsigned char bold,
	unsigned char italic,
	unsigned char size);


#define EXTRA_CHAR_FOR_FONT_NAME 16

static void update_font(Rich_Text *rt)
{
	int font_id;
#if TO_WINDOWS
	int len = Strlen_Uni(rt->font_spec.name);
	int utf8_len = RL_Length_As_UTF8(rt->font_spec.name, len, TRUE, FALSE);
	char *full = malloc(utf8_len + EXTRA_CHAR_FOR_FONT_NAME + 1);

	if (!full) return;

	RL_Encode_UTF8(full, utf8_len, rt->font_spec.name, &len, TRUE, FALSE);
#else
	int len = strlen(rt->font_spec.name);
	int utf8_len = len;
	char *full = malloc(utf8_len + EXTRA_CHAR_FOR_FONT_NAME + 1);
	if (!full) return;

	strcpy(full, rt->font_spec.name);
#endif
	len = sprintf(full + utf8_len, ":%s:%s",
		rt->font_spec.italic ? "Italic" : "",
		rt->font_spec.bold ? "Bold" : "");

	full[utf8_len + len] = '\0';

	font_id = nvgFindFont(rt->nvg, full);

	if (font_id == -1) {
		/* not found */
#if TO_WINDOWS
#if 0
		DWORD buf_size = 0;
		void *buf = NULL;
		HFONT hfont;
		HDC hdc = CreateCompatibleDC(NULL);

		SelectObject(hdc, hfont);
		buf_size = GetFontData(hdc, 0, 0, NULL, 0);
		DeleteDC(hdc);
		buf = malloc(buf_size);
		if (!buf) goto done;
		font_id = nvgCreateFontMem(rt->nvg, full, buf, buf_size, TRUE);
#endif
		font_id = nvgCreateFont(rt->nvg, full, "C:\\Users\\user\\work\\zoe.git\\fonts\\DejaVuSans.ttf");
#elif AGG_FONTCONFIG
		char * font_path = find_font_path(rt->font_spec.name, rt->font_spec.bold, rt->font_spec.italic, rt->font_spec.size);
		font_id = nvgCreateFont(rt->nvg, full, font_path);
#endif
	}

	if (font_id == -1) goto done;

	nvgFontFaceId(rt->nvg, font_id);

done:
	free(full);
	return;
}

static void nvg_rt_block_text(void *richtext, void *nvg, REBSER *block)
{
	REBCEC ctx;
	Rich_Text *rt;

	ctx.envr = richtext;
	ctx.block = block;
	ctx.index = 0;

	rt = (Rich_Text*)richtext;
	
	if (nvg != NULL && rt->nvg == NULL) rt->nvg = nvg;

	update_font(rt);
	nvgFontSize(rt->nvg, rt->font_spec.size);
	nvgFillColor(rt->nvg, REBCNT_NVG_COLOR(rt->font_spec.color));

	RL_DO_COMMANDS(block, 0, &ctx);
}

static char * to_utf8(REBCHR *text)
{
	char *utf8;
	int len;
	int utf8_len;

	len = Strlen_Uni(text); 	/* FIXME: do not used unexposed core functions */
	utf8_len = RL_Length_As_UTF8(text, len, TRUE, FALSE);
	if (utf8_len == 0) 	return 0;
	utf8 = malloc(utf8_len + 1);
	RL_Encode_UTF8(utf8, utf8_len, text, &len, TRUE, FALSE);

	return utf8;
}


static REBINT nvg_rt_gob_text(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom)
{

	//printf("%s, abs_oft: (%d, %d), clip_top: (%d, %d), clip_bottom: (%d, %d)\n", 
	//	__FUNCTION__, abs_oft.x, abs_oft.y, clip_top.x, clip_top.y, clip_bottom.x, clip_bottom.y);
	fflush(stdout);
	Rich_Text *rt;
	if (GET_GOB_FLAG(gob, GOBF_WINDOW)) return 0; //don't render window title text
	nvgSave(ctx->nvg);
	nvgReset(ctx->nvg);
//	nvgScissor(ctx->nvg, clip_top.x, clip_top.y, clip_bottom.x - clip_top.x, clip_bottom.y - clip_top.y);

	rt = rebol_renderer->text->rich_text;
	if (!rt) return 0;

	rt->nvg = ctx->nvg;
	rt->x = abs_oft.x;
	rt->y = abs_oft.y;

	rt->w = GOB_W_INT(gob);
	rt->h = GOB_H_INT(gob);

	if (GOB_TYPE(gob) == GOBT_TEXT) {
		rt->mode = RT_DRAW;
		nvg_rt_block_text(rt, NULL, (REBSER *)GOB_CONTENT(gob));
	} else {
		char *utf8 = to_utf8((REBCHR*)GOB_CONTENT(gob));
		nvgText(rt->nvg, rt->x, rt->y, utf8, NULL);
		free(utf8);
	}
	nvgRestore(ctx->nvg);

    return 0;
}

void nvg_text(REBDRW_CTX *draw_ctx, int mode, REBXYF *p1, REBXYF *p2, REBSER *block)
{
	Rich_Text *rt = (Rich_Text*)rebol_renderer->text->rich_text;
	rt->w = p2 ? p2->x - p1->x : -1;
	rt->h = p2 ? p2->y - p1->y : -1;
	rt->x = p1->x;
	rt->y = p1->y;
	rt->mode = RT_DRAW;
	rt->nvg = draw_ctx->nvg;

	if (draw_ctx->fill)
		rt->font_spec.color = draw_ctx->fill_color;
	nvg_rt_block_text(rt, NULL, block);
}

static void* nvg_create_rich_text()
{
	Rich_Text * rt = malloc(sizeof(Rich_Text));
	if (!rt) return NULL;

	memset(rt, 0, sizeof(rt));

	/* default font */
#ifdef TO_WINDOWS
	rt->font_spec.name = L"Arial";
#else
	rt->font_spec.name = "Arial";
#endif
	rt->font_spec.bold = FALSE;
	rt->font_spec.size = 12;
	rt->font_spec.italic = FALSE;
	rt->font_spec.underline = FALSE;
	rt->font_spec.color = 0;

	rt->line_height = 1;

	rt->x = 0;
	rt->y = 0;

	return rt;
}

static void nvg_destroy_rich_text(void* rt)
{
	if (rt == NULL) return;
	free(rt);
}

static int nvg_rt_init(REBRDR_TXT *txt)
{
	txt->rich_text = nvg_create_rich_text();
	if (txt->rich_text) return 0;

	return -1;
}

static void nvg_rt_fini(REBRDR_TXT *txt)
{
	if (!txt) return;
	nvg_destroy_rich_text(txt->rich_text);
}

static void nvg_rt_anti_alias(void* rt, REBINT mode)
{
//	((rich_text*)rt)->nvg_rt_text_mode(mode);
}

static void nvg_rt_bold(void* rt, REBINT state)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->font_spec.bold = state;
	update_font(ctx);
}

static void nvg_rt_caret(void* rt, REBXYF* caret, REBXYF* highlightStart, REBXYF highlightEnd)
{
#if 0
	if (highlightStart) ((rich_text*)rt)->nvg_rt_set_hinfo(*highlightStart,highlightEnd);
	if (caret) ((rich_text*)rt)->nvg_rt_set_caret(*caret);
#endif
}

static void nvg_rt_center(void* rt)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->para_spec.align = W_TEXT_CENTER;
	nvgTextAlign(ctx->nvg, NVG_ALIGN_CENTER);
}

static void nvg_rt_color(void* rt, REBCNT color)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->font_spec.color = color;
	if (ctx->mode == RT_DRAW) nvgFillColor(ctx->nvg, REBCNT_NVG_COLOR(ctx->font_spec.color));
}

static void nvg_rt_drop(void* rt, REBINT number)
{
//	((rich_text*)rt)->nvg_rt_drop(number);
}

static void nvg_rt_font(void* rt, REBFNT* REBFNT)
{
#if 0
	((rich_text*)rt)->nvg_rt_set_font(REBFNT);
	((rich_text*)rt)->nvg_rt_push();
#endif
}

static void nvg_rt_font_size(void* rt, REBINT size)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->font_spec.size = size;
	nvgFontSize(ctx->nvg, size);
}

static void* nvg_rt_get_font(void* rt)
{
	Rich_Text *ctx = (Rich_Text*) rt;
	return &ctx->font_spec;
}


static void* nvg_rt_get_para(void* rt)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	return &ctx->para_spec;
}

static void nvg_rt_italic(void* rt, REBINT state)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->font_spec.italic = state;
	update_font(ctx);
}

static void nvg_rt_left(void* rt)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->para_spec.align = W_TEXT_LEFT;
	nvgTextAlign(ctx->nvg, NVG_ALIGN_LEFT);
}

static void nvg_rt_newline(void* rt, REBINT index)
{
#if 0
	((rich_text*)rt)->nvg_rt_set_text((REBCHR*)"\n", TRUE);
	((rich_text*)rt)->nvg_rt_push(index);
#endif
}

static void nvg_rt_para(void* rt, REBPRA* REBPRA)
{
#if 0
	((rich_text*)rt)->nvg_rt_set_para(REBPRA);
	((rich_text*)rt)->nvg_rt_push();
#endif
}

static void nvg_rt_right(void* rt)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->para_spec.align = W_TEXT_RIGHT;
	nvgTextAlign(ctx->nvg, NVG_ALIGN_RIGHT);
}

static void nvg_rt_scroll(void* rt, REBXYF offset)
{
#if 0
	REBPRA* par = ((rich_text*)rt)->nvg_rt_get_para();
	par->scroll_x = offset.x;
	par->scroll_y = offset.y;
	((rich_text*)rt)->nvg_rt_set_para(par);
	((rich_text*)rt)->nvg_rt_push();
#endif
}

static void nvg_rt_shadow(void* rt, REBXYF d, REBCNT color, REBINT blur)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->nvg_rt_get_font();

	REBFNT->shadow_x = ROUND_TO_INT(d.x);
	REBFNT->shadow_y = ROUND_TO_INT(d.y);
	REBFNT->shadow_blur = blur;

	memcpy(REBFNT->shadow_color, (REBYTE*)&color, 4);

	((rich_text*)rt)->nvg_rt_push();
#endif
}

static void nvg_rt_set_font_styles(REBFNT* fnt, u32 word){
	switch (word){
		case W_TEXT_BOLD:
			fnt->bold = TRUE;
			break;
		case W_TEXT_ITALIC:
			fnt->italic = TRUE;
			break;
		case W_TEXT_UNDERLINE:
			fnt->underline = TRUE;
			break;

		default:
			fnt->bold = FALSE;
			fnt->italic = FALSE;
			fnt->underline = FALSE;
			break;
	}
}

static void nvg_rt_size_text(void* rt, REBGOB* gob, REBXYF* size)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	int internal = 0;
	if (ctx->nvg) {
		nvgSave(ctx->nvg);
	} else {
		ctx->nvg = nvgCreateInternal(NULL);
		internal = 1;
		update_font(ctx);
	}
	ctx->mode = RT_SIZE;

	if (GOB_TYPE(gob) == GOBT_TEXT) {
		nvg_rt_block_text(rt, NULL, (REBSER *)GOB_CONTENT(gob));
		size->x = ctx->size_x;
		size->y = ctx->size_y;
	} else {
		float bounds[4];
		char *utf8 = to_utf8((REBCHR*)GOB_CONTENT(gob));
		nvgTextBounds(ctx->nvg, ctx->x, ctx->y, utf8, NULL, bounds);
		free(utf8);

		size->x = bounds[2] - bounds[0];
		size->y = bounds[3] - bounds[1];
	}

	if (internal) {
		nvgDeleteInternal(ctx->nvg);
		ctx->nvg = NULL;
	}	else {
		nvgRestore(ctx->nvg);
	}
}

#define UNDERLINE_OFFSET 2
static void nvg_rt_text(void* rt, REBSER* text, REBINT index)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	unsigned char *utf8;
	int utf8_n_char = RL_Get_UTF8_String(text, 0, &utf8);
	if (ctx->mode == RT_DRAW) {
		if (ctx->w > 0) {
			int i = 0;
			int y0 = ctx->y;
			NVGtextRow *rows = NULL;
			int max_nrows = ctx->h / (ctx->line_height * ctx->font_spec.size);
			int nrows;

			if (max_nrows > utf8_n_char) max_nrows = utf8_n_char;

			if (max_nrows <= 0) return;

			rows = malloc(sizeof(NVGtextRow) * max_nrows);
			if (rows == NULL) return;

			nrows = nvgTextBreakLines(ctx->nvg, utf8, NULL, ctx->w, rows, max_nrows);

			for (i = 0; i < nrows; i++) {
				float w = nvgText(ctx->nvg, ctx->x, ctx->y + ctx->font_spec.size, rows[i].start, rows[i].end);
				ctx->y += ctx->line_height * ctx->font_spec.size;
			}

			if (ctx->font_spec.underline) {
				nvgBeginPath(ctx->nvg);
				nvgSave(ctx->nvg);
				nvgStrokeWidth(ctx->nvg, 1);
				nvgStrokeColor(ctx->nvg, REBCNT_NVG_COLOR(ctx->font_spec.color));
				for (i = 0; i < nrows; i++) {
					nvgMoveTo(ctx->nvg, ctx->x, y0 + ctx->font_spec.size + UNDERLINE_OFFSET);
					nvgLineTo(ctx->nvg, ctx->x + rows[i].width, y0 + ctx->font_spec.size + UNDERLINE_OFFSET);

					y0 += ctx->line_height * ctx->font_spec.size;
				}
				nvgStroke(ctx->nvg);
				nvgRestore(ctx->nvg);
			}

			free(rows);
		} else { /* no wrapping or clipping */
			float w;
			
			nvgSave(ctx->nvg);
			nvgResetScissor(ctx->nvg);
			w = nvgText(ctx->nvg, ctx->x, ctx->y + ctx->font_spec.size, utf8, NULL);
			if (ctx->font_spec.underline) {
				nvgBeginPath(ctx->nvg);
				nvgStrokeWidth(ctx->nvg, 1);
				nvgStrokeColor(ctx->nvg, REBCNT_NVG_COLOR(ctx->font_spec.color));

				nvgMoveTo(ctx->nvg, ctx->x, ctx->y + ctx->font_spec.size + UNDERLINE_OFFSET);
				nvgLineTo(ctx->nvg, ctx->x + w, ctx->y + ctx->font_spec.size + UNDERLINE_OFFSET);

				nvgStroke(ctx->nvg);
			}
			nvgRestore(ctx->nvg);

		}
	} else { // SIZE_TEXT
		float bounds[4];
		nvgTextBounds(ctx->nvg, ctx->x, ctx->y, utf8, NULL, bounds);
		ctx->size_x = bounds[2] - bounds[0];
		ctx->size_y = bounds[3] - bounds[1];
	}
}

static void nvg_rt_underline(void* rt, REBINT state)
{
	Rich_Text *ctx = (Rich_Text*)rt;
	ctx->font_spec.underline = state;
}

static void nvg_rt_offset_to_caret(void* rt, REBGOB *gob, REBXYF xy, REBINT *element, REBINT *position)
{
#if 0
	REBCHR* str;
	REBOOL dealloc;

	((rich_text*)rt)->nvg_rt_reset();
	((rich_text*)rt)->nvg_rt_set_clip(0,0, GOB_LOG_W_INT(gob),GOB_LOG_H_INT(gob));
	if (GOB_TYPE(gob) == GOBT_TEXT){
		nvg_rt_block_text(rt, NULL, (REBSER *)GOB_CONTENT(gob));
	} else if (GOB_TYPE(gob) == GOBT_STRING) {
#ifdef TO_WIN32
		//Windows uses UTF16 wide chars
		dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#else
		//linux, android use UTF32 wide chars
		dealloc = As_UTF32_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#endif
		((rich_text*)rt)->nvg_rt_set_text(str, dealloc);
		((rich_text*)rt)->nvg_rt_push(1);
	} else {
		*element = 0;
		*position = 0;
		return;
	}

	((rich_text*)rt)->nvg_rt_offset_to_caret(xy, element, position);
#endif
}

static void nvg_rt_caret_to_offset(void* rt, REBGOB *gob, REBXYF* xy, REBINT element, REBINT position)
{
#if 0
	REBCHR* str;
	REBOOL dealloc;
	((rich_text*)rt)->nvg_rt_reset();
	((rich_text*)rt)->nvg_rt_set_clip(0,0, GOB_LOG_W_INT(gob),GOB_LOG_H_INT(gob));
	if (GOB_TYPE(gob) == GOBT_TEXT){
		nvg_rt_block_text(rt, NULL, (REBSER *)GOB_CONTENT(gob));
	} else if (GOB_TYPE(gob) == GOBT_STRING) {
#ifdef TO_WIN32
		//Windows uses UTF16 wide chars
		dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#else
		//linux, android use UTF32 wide chars
		dealloc = As_UTF32_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#endif

		((rich_text*)rt)->nvg_rt_set_text(str, dealloc);
		((rich_text*)rt)->nvg_rt_push(1);
	} else {
		xy->x = 0;
		xy->y = 0;
		return;
	}

	((rich_text*)rt)->nvg_rt_caret_to_offset(xy, element, position);
#endif
}

struct REBRDR_TXT text_nanovg = {
	.init = nvg_rt_init,
	.fini = nvg_rt_fini,
	.create_rich_text = nvg_create_rich_text,
	.destroy_rich_text = nvg_destroy_rich_text,
	.rt_anti_alias = nvg_rt_anti_alias,
	.rt_bold = nvg_rt_bold,
	.rt_caret = nvg_rt_caret,
	.rt_center = nvg_rt_center,
	.rt_color = nvg_rt_color,
	.rt_drop = nvg_rt_drop,
	.rt_font = nvg_rt_font,
	.rt_font_size = nvg_rt_font_size,
	.rt_get_font = nvg_rt_get_font,
	.rt_get_para = nvg_rt_get_para,
	.rt_italic = nvg_rt_italic,
	.rt_left = nvg_rt_left,
	.rt_newline = nvg_rt_newline,
	.rt_para = nvg_rt_para,
	.rt_right = nvg_rt_right,
	.rt_scroll = nvg_rt_scroll,
	.rt_shadow = nvg_rt_shadow,
	.rt_set_font_styles = nvg_rt_set_font_styles,
	.rt_size_text = nvg_rt_size_text,
	.rt_text = nvg_rt_text,
	.rt_underline = nvg_rt_underline,
	.rt_offset_to_caret = nvg_rt_offset_to_caret,
	.rt_caret_to_offset = nvg_rt_caret_to_offset,
	.rt_gob_text = nvg_rt_gob_text,
	.rt_block_text = nvg_rt_block_text
};
