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

typedef struct REBOL_FONT {
	REBCHR *name;
	REBCNT name_free;
	REBINT bold;
	REBINT italic;
	REBINT underline;
	REBINT size;
	REBCNT color;
	REBINT offset_x;
	REBINT offset_y;
	REBINT space_x;
	REBINT space_y;
	REBINT shadow_x;
	REBINT shadow_y;
	REBYTE* shadow_color;
	REBINT shadow_blur;
} REBFNT;

typedef struct REBOL_PARA {
		REBINT origin_x;
		REBINT origin_y;
		REBINT margin_x;
		REBINT margin_y;
		REBINT indent_x;
		REBINT indent_y;
		REBINT tabs;
		REBINT wrap;
		float scroll_x;
		float scroll_y;
		REBINT align;
		REBINT valign;
} REBPRA;

struct REBDRW_CTX;

struct REBRDR_TXT {
	void *rich_text;
	int (*init) (REBRDR_TXT *);
	void (*fini) (REBRDR_TXT *);
	void* (*create_rich_text)();
	void (*destroy_rich_text)(void *rt);
	void (*rt_anti_alias)(void* rt, REBINT mode);
	void (*rt_bold)(void* rt, REBINT state);
	void (*rt_caret)(void* rt, REBXYF* caret, REBXYF* highlightStart, REBXYF highlightEnd);
	void (*rt_center)(void* rt);
	void (*rt_color)(void* rt, REBCNT col);
	void (*rt_drop)(void* rt, REBINT number);
	void (*rt_font)(void* rt, REBFNT* font);
	void (*rt_font_size)(void* rt, REBINT size);
	REBFNT* (*rt_get_font)(void* rt);
	REBPRA* (*rt_get_para)(void* rt);
	void (*rt_italic)(void* rt, REBINT state);
	void (*rt_left)(void* rt);
	void (*rt_newline)(void* rt, REBINT index);
	void (*rt_para)(void* rt, REBPRA* para);
	void (*rt_right)(void* rt);
	void (*rt_scroll)(void* rt, REBXYF offset);
	void (*rt_shadow)(void* rt, REBXYF d, REBCNT color, REBINT blur);
	void (*rt_set_font_styles)(REBFNT* font, u32 word);
	void (*rt_size_text)(void* rt, REBGOB* gob, REBXYF* size);
	void (*rt_text)(void* gr, REBSER* text, REBINT index);
	void (*rt_underline)(void* rt, REBINT state);

	void (*rt_offset_to_caret)(void* rt, REBGOB *gob, REBXYF xy, REBINT *element, REBINT *position);
	void (*rt_caret_to_offset)(void* rt, REBGOB *gob, REBXYF* xy, REBINT element, REBINT position);
	REBINT (*rt_gob_text)(REBGOB *gob, REBDRW_CTX *ctx, REBXYI abs_oft, REBXYI clip_top, REBXYI clip_bottom);
	void (*rt_block_text)(void *rt, void * draw_ctx, REBSER *block);
};