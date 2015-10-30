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
**  Title: DRAW dialect API functions
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

#include <SDL.h>

typedef struct REBDRW_CTX REBDRW_CTX;
typedef struct REBRDR REBRDR;
typedef struct REBRDR_DRW REBRDR_DRW;
typedef struct REBRDR_TXT REBRDR_TXT;

extern REBRDR *render;

struct REBRDR {
	const char *name;

	/* returns 0 if this render can be used, otherwise negative */
	int (*init) (REBRDR *render);

	void (*fini) (REBRDR *render);

	/* context related function */
	void (*begin_frame)(REBDRW_CTX *ctx);
	void (*end_frame)(REBDRW_CTX *ctx);
	void (*blit_frame)(REBDRW_CTX *ctx, SDL_Rect *clip);
	REBDRW_CTX* (*create_draw_context)(SDL_Window *win, REBINT w, REBINT h);
	void (*destroy_draw_context)(REBDRW_CTX *ctx);
	void (*resize_draw_context)(REBDRW_CTX *ctx, REBINT w, REBINT h);

	REBRDR_DRW *draw;
	REBRDR_TXT *text;

	unsigned int default_SDL_win_flags;

	void *priv; /* private data */
};

extern REBRDR *rebol_renderer;
REBRDR *init_renderer();
