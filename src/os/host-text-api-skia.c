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
**  Author: Richard Smolak
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

#include <stdlib.h>
#include "reb-host.h"
#include <reb-skia.h>
#include <host-text-api.h>
#include "host-ext-text.h"

#include "Remotery.h"

typedef REBFNT font;
typedef REBPRA para;

static REBINT default_font_color = 0x00000000;
static REBINT default_shadow_color = 0x00000000;
static REBFNT Vfont = {
    "Arial",    //REBCHR *name;
    FALSE,      //REBCNT name_free;
    FALSE,      //REBINT bold;
    FALSE,      //REBINT italic;
    FALSE,      //REBINT underline;
    12,         //REBINT size;
    &default_font_color, //REBYTE* color;
    0,          //REBINT offset_x;
    0,          //REBINT offset_y;
    0,          //REBINT space_x;
    0,          //REBINT space_y;
    0,          //REBINT shadow_x;
    0,          //REBINT shadow_y;
    &default_shadow_color, //REBYTE* shadow_color;
    0           //REBINT shadow_blur;
};

static REBPRA Vpara = {
		0,  //REBINT origin_x;
		0,  //REBINT origin_y;
		0,  //REBINT margin_x;
		0,  //REBINT margin_y;
		0,  //REBINT indent_x;
		0,  //REBINT indent_y;
		0,  //REBINT tabs;
		0,  //REBINT wrap;
		0,  //float scroll_x;
		0,  //float scroll_y;
		0,  //REBINT align;
		0,  //REBINT valign;
};

extern void* Rich_Text;

REBINT As_OS_Str(REBSER *series, REBCHR **string);
REBOOL As_UTF8_Str(REBSER *series, REBYTE **string);

void rt_block_text(void *richtext, REBSER *block)
{
	REBCEC ctx;

	ctx.envr = richtext;
	ctx.block = block;
	ctx.index = 0;

	RL_DO_COMMANDS(block, 0, &ctx);
}

void rt_gob_text(REBGOB *gob, REBYTE* ctx, REBXYI abs_oft, REBXYI clip_oft, REBXYI clip_siz)
{
	rmt_BeginCPUSample(rt_gob_text, RMTSF_Aggregate);
	if (GET_GOB_FLAG(gob, GOBF_WINDOW)) goto end; //don't render window title text
	rs_rich_text_t *rt = (rs_rich_text_t *)Rich_Text;
	rs_rt_reset(rt);
	rs_draw_text_pre_setup(ctx, rt);

	rs_draw_push_matrix(ctx);
	rs_draw_reset_matrix(ctx);

	if (GOB_TYPE(gob) == GOBT_TEXT)
		rt_block_text(rt, (REBSER *)GOB_CONTENT(gob));
	else {
		rt_text(rt, GOB_CONTENT(gob), 0, FALSE);
	}

	rs_draw_text(ctx, abs_oft.x, abs_oft.y, clip_siz.x, clip_siz.y, rt);
	rs_draw_pop_matrix(ctx);
end:
	rmt_EndCPUSample();
}

void* Create_RichText()
{
	return rs_create_rich_text();
}

void Destroy_RichText(void* rt)
{
	rs_free_rich_text(rt);
}

void rt_anti_alias(void* rt, REBINT mode)
{
    rs_rt_anti_alias(rt, mode);
}

void rt_bold(void* rt, REBINT state)
{
    rs_rt_bold(rt, state);
}

void rt_caret(void* rt, REBXYF* caret, REBXYF* highlightStart, REBXYF highlightEnd)
{
    if (caret) rs_rt_caret(rt, caret->x, caret->y - 1);
    if (highlightStart) rs_rt_highlight(rt, highlightStart->x, highlightStart->y - 1, highlightEnd.y - 1);
}

void rt_center(void* rt)
{
	rs_rt_center(rt);
}

void rt_color(void* rt, REBCNT color)
{
	rs_rt_color(rt, color);
}

void rt_drop(void* rt, REBINT number)
{
}

void rt_font(void* rt, font* font)
{
	if (font) {
		if (*(rs_argb_t*)font->color & 0x00FFFFFF) {
			rt_color(rt, *(rs_argb_t*)font->color);
		}
		rt_font_size(rt, font->size);
		rt_italic(rt, font->italic);
		rt_bold(rt, font->bold);
		rs_rt_set_font_name(rt, font->name);
		if (font->name_free) {
			OS_Free(font->name);
			font->name_free = FALSE;
		}
        rs_rt_font_offset(rt, font->offset_x, font->offset_y);
        rs_rt_font_space(rt, font->space_x, font->space_y);
	}
}

void rt_font_size(void* rt, REBINT size)
{
	rs_rt_font_size(rt, size);
}

void* rt_get_font(void* rt)
{
	return &Vfont;
}

void* rt_get_para(void* rt)
{
	return &Vpara;
}

void rt_italic(void* rt, REBINT state)
{
	rs_rt_italic(rt, state != 0);
}

void rt_left(void* rt)
{
	rs_rt_left(rt);
}

void rt_newline(void* rt, REBINT index)
{
    rs_rt_newline(rt);
}

void rt_para(void* rt, para* para)
{
	if (para) {
		switch (para->align) {
		case W_TEXT_CENTER:
			rt_center(rt);
			break;
		case W_TEXT_LEFT:
			rt_left(rt);
			break;
		case W_TEXT_RIGHT:
			rt_right(rt);
			break;
		default:
			rt_left(rt);
			break;
		}
        switch (para->valign) {
            case W_TEXT_TOP:
                rs_rt_top(rt);
                break;
            case W_TEXT_MIDDLE:
                rs_rt_middle(rt);
                break;
            case W_TEXT_BOTTOM:
                rs_rt_bottom(rt);
                break;
        }

        rs_rt_para_origin(rt, para->origin_x, para->origin_y);
        rs_rt_para_margin(rt, para->margin_x, para->margin_y);
        rs_rt_para_indent(rt, para->indent_x, para->indent_y);
        rs_rt_para_scroll(rt, para->scroll_x, para->scroll_y);
	}
}

void rt_right(void* rt)
{
	rs_rt_right(rt);
}

void rt_scroll(void* rt, REBXYF offset)
{
}

void rt_shadow(void* rt, REBXYF d, REBCNT color, REBINT blur)
{
}

void rt_set_font_styles(font* font, u32 word){
}

void rt_size_text(void* rt, REBGOB* gob, REBXYF* size)
{
	rmt_BeginCPUSample(size_text, RMTSF_Aggregate);
	REBCHR* str;
	REBOOL dealloc;
	rs_rt_reset(rt);
	//((rich_text*)rt)->rt_set_clip(0, 0, GOB_LOG_W_INT(gob), GOB_LOG_H_INT(gob));
	if (GOB_TYPE(gob) == GOBT_TEXT) {
		rt_block_text(rt, (REBSER *)GOB_CONTENT(gob));
	}
	else if (GOB_TYPE(gob) == GOBT_STRING) {
		dealloc = As_UTF8_Str(GOB_CONTENT(gob), (REBCHR**)&str);
		rs_rt_text(rt, 0, str);
		if (dealloc) {
			OS_Free(str);
		}
	} else {
		size->x = 0;
		size->y = 0;
		return;
	}

	rs_rt_size_text(rt, GOB_LOG_W(gob), GOB_LOG_H(gob), &size->x, &size->y);
	rmt_EndCPUSample();
}

void rt_text(void* rt, REBCHR* text, REBINT index, REBCNT dealloc)
{
	char * utf8 = NULL;
	REBOOL needs_free = As_UTF8_Str(text + index, &utf8);
	rs_rt_text(rt, index, utf8);
	if (needs_free) {
		OS_Free(utf8);
	}
	if (dealloc) {
		free(text);
	}
}

void rt_text_utf8(void *rt, REBYTE *text, REBINT index, REBCNT dealloc)
{
	rs_rt_text(rt, index, text);
	if (dealloc) {
		free(text);
	}
}

void rt_underline(void* rt, REBINT state)
{
	rs_rt_underline(rt, state);
}

void rt_offset_to_caret(void* rt, REBGOB *gob, REBXYF xy, REBINT *element, REBINT *position)
{
}

void rt_caret_to_offset(void* rt, REBGOB *gob, REBXYF* xy, REBINT element, REBINT position)
{
}
