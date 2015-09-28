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

#include "reb-host.h"
#include "host-view.h"
#include "host-text-api.h"

void rt_block_text(void *richtext, REBSER *block)
{
	REBCEC ctx;

	ctx.envr = richtext;
	ctx.block = block;
	ctx.index = 0;

	RL_DO_COMMANDS(block, 0, &ctx);
}

REBINT rt_gob_text(REBGOB *gob, REBYTE* buf, REBXYI buf_size, REBXYF abs_oft, REBXYI clip_oft, REBXYI clip_siz)
{
	if (GET_GOB_FLAG(gob, GOBF_WINDOW)) return 0; //don't render window title text

#if 0
	agg_graphics::ren_buf rbuf_win(buf, buf_size.x, buf_size.y, buf_size.x << 2);		
	agg_graphics::pixfmt pixf_win(rbuf_win);
	agg_graphics::ren_base rb_win(pixf_win);
	rich_text* rt = (rich_text*)Rich_Text;
	REBINT w = GOB_LOG_W_INT(gob);	
	REBINT h = GOB_LOG_H_INT(gob);

	rt->rt_reset();
	rt->rt_attach_buffer(&rbuf_win, buf_size.x, buf_size.y);
	//note: rt_set_clip() include bottom-right values
	//		rt->rt_set_clip(abs_oft.x, abs_oft.y, abs_oft.x+w, abs_oft.y+h, w, h);
	rt->rt_set_clip(clip_oft.x, clip_oft.y, clip_siz.x, clip_siz.y, w, h);

	if (GOB_TYPE(gob) == GOBT_TEXT)
		rt_block_text(rt, (REBSER *)GOB_CONTENT(gob));
	else {
		REBCHR* str;
#ifdef TO_WIN32
		//Windows uses UTF16 wide chars
		REBOOL dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#else
		//linux, android use UTF32 wide chars
		REBOOL dealloc = As_UTF32_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#endif
		if (str){
			rt->rt_set_text(str, dealloc);
			rt->rt_push(1);
		}
	}

	return rt->rt_draw_text(DRAW_TEXT, &abs_oft);
#endif
}

void* Create_RichText()
{
#if 0
#ifdef AGG_WIN32_FONTS
	return (void*)new rich_text(GetDC( NULL ));
#endif
#ifdef AGG_FREETYPE
	return (void*)new rich_text();
#endif
#endif
}

void Destroy_RichText(void* rt)
{
//	delete (rich_text*)rt;
}

void rt_anti_alias(void* rt, REBINT mode)
{
//	((rich_text*)rt)->rt_text_mode(mode);
}

void rt_bold(void* rt, REBINT state)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->rt_get_font();
	REBFNT->bold = state;
	((rich_text*)rt)->rt_push();
#endif
}

void rt_caret(void* rt, REBXYF* caret, REBXYF* highlightStart, REBXYF highlightEnd)
{
#if 0
	if (highlightStart) ((rich_text*)rt)->rt_set_hinfo(*highlightStart,highlightEnd);
	if (caret) ((rich_text*)rt)->rt_set_caret(*caret);
#endif
}

void rt_center(void* rt)
{
#if 0
	REBPRA* par = ((rich_text*)rt)->rt_get_para();
	par->align = W_TEXT_CENTER;
	((rich_text*)rt)->rt_set_para(par);
	((rich_text*)rt)->rt_push();
#endif
}

void rt_color(void* rt, REBCNT color)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->rt_get_font();
	REBFNT->color[0] = ((REBYTE*)&color)[0];
	REBFNT->color[1] = ((REBYTE*)&color)[1];
	REBFNT->color[2] = ((REBYTE*)&color)[2];
	REBFNT->color[3] = ((REBYTE*)&color)[3];
	((rich_text*)rt)->rt_push();
	((rich_text*)rt)->rt_color_change();
#endif
}

void rt_drop(void* rt, REBINT number)
{
//	((rich_text*)rt)->rt_drop(number);
}

void rt_font(void* rt, REBFNT* REBFNT)
{
#if 0
	((rich_text*)rt)->rt_set_font(REBFNT);
	((rich_text*)rt)->rt_push();
#endif
}

void rt_font_size(void* rt, REBINT size)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->rt_get_font();
	REBFNT->size = size;
	((rich_text*)rt)->rt_push();
#endif
}

void* rt_get_font(void* rt)
{
//	return (void*)((rich_text*)rt)->rt_get_font();
}


void* rt_get_para(void* rt)
{
//	return (void*)((rich_text*)rt)->rt_get_para();
}

void rt_italic(void* rt, REBINT state)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->rt_get_font();
	REBFNT->italic = state;
	((rich_text*)rt)->rt_push();
#endif
}

void rt_left(void* rt)
{
#if 0
	REBPRA* par = ((rich_text*)rt)->rt_get_para();
	par->align = W_TEXT_LEFT;
	((rich_text*)rt)->rt_set_para(par);
	((rich_text*)rt)->rt_push();
#endif
}

void rt_newline(void* rt, REBINT index)
{
#if 0
	((rich_text*)rt)->rt_set_text((REBCHR*)"\n", TRUE);
	((rich_text*)rt)->rt_push(index);
#endif
}

void rt_para(void* rt, REBPRA* REBPRA)
{
#if 0
	((rich_text*)rt)->rt_set_para(REBPRA);
	((rich_text*)rt)->rt_push();
#endif
}

void rt_right(void* rt)
{
#if 0
	REBPRA* par = ((rich_text*)rt)->rt_get_para();
	par->align = W_TEXT_RIGHT;
	((rich_text*)rt)->rt_set_para(par);
	((rich_text*)rt)->rt_push();
#endif
}

void rt_scroll(void* rt, REBXYF offset)
{
#if 0
	REBPRA* par = ((rich_text*)rt)->rt_get_para();
	par->scroll_x = offset.x;
	par->scroll_y = offset.y;
	((rich_text*)rt)->rt_set_para(par);
	((rich_text*)rt)->rt_push();
#endif
}

void rt_shadow(void* rt, REBXYF d, REBCNT color, REBINT blur)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->rt_get_font();

	REBFNT->shadow_x = ROUND_TO_INT(d.x);
	REBFNT->shadow_y = ROUND_TO_INT(d.y);
	REBFNT->shadow_blur = blur;

	memcpy(REBFNT->shadow_color, (REBYTE*)&color, 4);

	((rich_text*)rt)->rt_push();
#endif
}

void rt_set_font_styles(REBFNT* REBFNT, u32 word){
#if 0
	switch (word){
		case W_TEXT_BOLD:
			REBFNT->bold = TRUE;
			break;
		case W_TEXT_ITALIC:
			REBFNT->italic = TRUE;
			break;
		case W_TEXT_UNDERLINE:
			REBFNT->underline = TRUE;
			break;

		default:
			REBFNT->bold = FALSE;
			REBFNT->italic = FALSE;
			REBFNT->underline = FALSE;
			break;
	}
#endif
}

void rt_size_text(void* rt, REBGOB* gob, REBXYF* size)
{
#if 0
	REBCHR* str;
	REBOOL dealloc;
	((rich_text*)rt)->rt_reset();
	((rich_text*)rt)->rt_set_clip(0,0, GOB_LOG_W_INT(gob),GOB_LOG_H_INT(gob));
	if (GOB_TYPE(gob) == GOBT_TEXT){
		rt_block_text(rt, (REBSER *)GOB_CONTENT(gob));
	} else if (GOB_TYPE(gob) == GOBT_STRING) {
#ifdef TO_WIN32
		//Windows uses UTF16 wide chars
		dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#else
		//linux, android use UTF32 wide chars
		dealloc = As_UTF32_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#endif

		((rich_text*)rt)->rt_set_text(str, dealloc);
		((rich_text*)rt)->rt_push(1);
	} else {
		size->x = 0;
		size->y = 0;
		return;
	}

	((rich_text*)rt)->rt_size_text(size);
#endif
}

void rt_text(void* rt, REBCHR* text, REBINT index, REBCNT dealloc)
{
#if 0
	((rich_text*)rt)->rt_set_text(text, dealloc);
	((rich_text*)rt)->rt_push(index);
#endif
}

void rt_underline(void* rt, REBINT state)
{
#if 0
	REBFNT* REBFNT = ((rich_text*)rt)->rt_get_font();
	REBFNT->underline = state;
	((rich_text*)rt)->rt_push();
#endif
}


















void rt_offset_to_caret(void* rt, REBGOB *gob, REBXYF xy, REBINT *element, REBINT *position)
{
#if 0
	REBCHR* str;
	REBOOL dealloc;

	((rich_text*)rt)->rt_reset();
	((rich_text*)rt)->rt_set_clip(0,0, GOB_LOG_W_INT(gob),GOB_LOG_H_INT(gob));
	if (GOB_TYPE(gob) == GOBT_TEXT){
		rt_block_text(rt, (REBSER *)GOB_CONTENT(gob));
	} else if (GOB_TYPE(gob) == GOBT_STRING) {
#ifdef TO_WIN32
		//Windows uses UTF16 wide chars
		dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#else
		//linux, android use UTF32 wide chars
		dealloc = As_UTF32_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#endif
		((rich_text*)rt)->rt_set_text(str, dealloc);
		((rich_text*)rt)->rt_push(1);
	} else {
		*element = 0;
		*position = 0;
		return;
	}

	((rich_text*)rt)->rt_offset_to_caret(xy, element, position);
#endif
}

void rt_caret_to_offset(void* rt, REBGOB *gob, REBXYF* xy, REBINT element, REBINT position)
{
#if 0
	REBCHR* str;
	REBOOL dealloc;
	((rich_text*)rt)->rt_reset();
	((rich_text*)rt)->rt_set_clip(0,0, GOB_LOG_W_INT(gob),GOB_LOG_H_INT(gob));
	if (GOB_TYPE(gob) == GOBT_TEXT){
		rt_block_text(rt, (REBSER *)GOB_CONTENT(gob));
	} else if (GOB_TYPE(gob) == GOBT_STRING) {
#ifdef TO_WIN32
		//Windows uses UTF16 wide chars
		dealloc = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#else
		//linux, android use UTF32 wide chars
		dealloc = As_UTF32_Str(GOB_CONTENT(gob), (REBCHR**)&str);
#endif

		((rich_text*)rt)->rt_set_text(str, dealloc);
		((rich_text*)rt)->rt_push(1);
	} else {
		xy->x = 0;
		xy->y = 0;
		return;
	}

	((rich_text*)rt)->rt_caret_to_offset(xy, element, position);
#endif
}

