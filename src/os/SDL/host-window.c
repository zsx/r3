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
**  Title: <platform> Windowing support
**  Author: Richard Smolak
**  File:  host-window.c
**  Purpose: Provides functions for windowing.
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
#include <math.h>
#include "reb-host.h"
#include "host-view.h"
#include "host-compositor.h"
#include "host-renderer.h"

#include <SDL.h>

// Externs
extern REBGOBWINDOWS *Gob_Windows;
extern void Free_Window(REBGOB *gob);
extern REBINT Alloc_Window(REBGOB *gob);
extern void Draw_Window(REBGOB *wingob, REBGOB *gob);

// Locals

//
//** OSAL Library Functions ********************************************
//

/***********************************************************************
**
*/	void OS_Init_Windows()
/*
**		Initialize special variables of the graphics subsystem.
**
***********************************************************************/
{

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0){
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return;
	}
#ifndef NDEBUG
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#else
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN);
#endif


	rebol_renderer = init_renderer();
}

/***********************************************************************
**
*/	void OS_Update_Window(REBGOB *gob)
/*
**		Update window parameters.
**
***********************************************************************/
{
	SDL_Window *win = GOB_HWIN(gob);
	int x, y;
	if (win == NULL) {
		return;
	}
	if (GET_GOB_FLAG(gob, GOBF_FULLSCREEN)) {
		SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
	} else if (GET_GOB_FLAG(gob, GOBF_MAXIMIZE)) {
		SDL_MaximizeWindow(win);
	} else if (GET_GOB_FLAG(gob, GOBF_MINIMIZE)) {
		SDL_MinimizeWindow(win);
	} else if (GET_GOB_FLAG(gob, GOBF_RESTORE)) {
		SDL_RestoreWindow(win);
	}

	SDL_GetWindowPosition(win, &x, &y);
	if (gob->offset.x != x || gob->offset.y != y) {
		SDL_SetWindowPosition(win, gob->offset.x, gob->offset.y);
	}

	SDL_GetWindowSize(win, &x, &y);
	if (gob->size.x != x || gob->size.y != y) {
		SDL_SetWindowSize(win, gob->size.x, gob->size.y);
	}

	//SDL_UpdateWindowSurface(win);
}

/***********************************************************************
**
*/  void* OS_Open_Window(REBGOB *gob)
/*
**		Initialize the graphics window.
**
**		The window handle is returned, but not expected to be used
**		other than for debugging conditions.
**
***********************************************************************/
{
	REBINT windex;
	REBINT x = GOB_LOG_X_INT(gob);
	REBINT y = GOB_LOG_Y_INT(gob);
	REBINT w = GOB_LOG_W_INT(gob);
	REBINT h = GOB_LOG_H_INT(gob);

	SDL_Window *win = NULL;
	char *title;
	REBYTE os_string = FALSE;
	Uint32 flags;
	REBGOB *parent_gob = GOB_TMP_OWNER(gob);

	if (rebol_renderer == NULL) return NULL;

	flags = rebol_renderer->default_SDL_win_flags;
	windex = Alloc_Window(gob);

	if (IS_GOB_STRING(gob))
		RL_Get_UTF8_String(GOB_CONTENT(gob), 0, cast(REBYTE**, &title));
    else
        title = "REBOL Window";

	if (GET_GOB_FLAG(gob, GOBF_NO_TITLE) && GET_GOB_FLAG(gob, GOBF_NO_BORDER)) {
		flags |= SDL_WINDOW_BORDERLESS;
	} else if (GET_GOB_FLAG(gob, GOBF_FULLSCREEN)) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else if (GET_GOB_FLAG(gob, GOBF_RESIZE)) {
		flags |= SDL_WINDOW_RESIZABLE;
	} else if (GET_GOB_FLAG(gob, GOBF_MAXIMIZE)) {
		flags |= SDL_WINDOW_MAXIMIZED;
	} else if (GET_GOB_FLAG(gob, GOBF_MINIMIZE)) {
		flags |= SDL_WINDOW_MINIMIZED;
	}
	printf("Opening a window at: %dx%d, %dx%d, owner gob: 0x%p\n", x, y, w, h, parent_gob);
	if (parent_gob != NULL) {
		if (!GET_GOB_FLAG(gob, GOBF_POPUP)) {
			/* x, y are in parent window coordinates */
			REBINT max_depth = 1000; // avoid infinite loops
			while (parent_gob != NULL
				   && (max_depth-- > 0)
				   && !GET_GOB_FLAG(parent_gob, GOBF_WINDOW)) {
				x += GOB_LOG_X(parent_gob);
				y += GOB_LOG_Y(parent_gob);
				parent_gob = GOB_TMP_OWNER(parent_gob);
			}
			if (parent_gob && GET_GOB_FLAG(parent_gob, GOBF_WINDOW)) {
				SDL_Window *parent_win = GOB_HWIN(parent_gob);
				if (parent_win != NULL) {
					int tx = 0, ty = 0;
					SDL_GetWindowPosition(win, &tx, &ty);
					x += tx;
					y += ty;
				}
			}
		}
	}
	win = SDL_CreateWindow(title, x, y, w, h, flags);
	if (os_string)
		OS_FREE(title);

	SDL_SetWindowData(win, "GOB", gob);

	if (GET_GOB_FLAG(gob, GOBF_HIDDEN)
		|| (x + w) < 0
		|| (y + h) < 0) {
		/* r3-gui.r3 sets offset to negatives to hide it */
		SDL_HideWindow(win);
	}

	Gob_Windows[windex].win = win;
	Gob_Windows[windex].compositor = rebcmp_create(Gob_Root, gob); /* it updates has_xshm */
	CLEAR_GOB_STATE(gob);
	SET_GOB_STATE(gob, GOBS_NEW);

	SET_GOB_FLAG(gob, GOBF_WINDOW);
	SET_GOB_FLAG(gob, GOBF_ACTIVE);
	SET_GOB_STATE(gob, GOBS_OPEN);
	return win;
}

/***********************************************************************
**
*/  void OS_Close_Window(REBGOB *gob)
/*
**		Close the window.
**
***********************************************************************/
{
	SDL_Window *win = GOB_HWIN(gob);
	if (win) {
		SDL_DestroyWindow(win);
	}
    CLR_GOB_STATE(gob, GOBS_OPEN);
    CLR_GOB_STATE(gob, GOBS_ACTIVE);
	Free_Window(gob);
	printf("window of gob %p is closed\n", gob);
}
