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

#include "reb-host.h"
#include "host-compositor.h"

#include "SDL.h"

//***** Constants *****

#define GOB_HWIN(gob)	(Find_Window(gob))
#define GOB_COMPOSITOR(gob)	(Find_Compositor(gob)) //gets handle to window's compositor

//***** Externs *****
extern REBGOBWINDOWS *Gob_Windows;
extern void Free_Window(REBGOB *gob);
extern void* Find_Compositor(REBGOB *gob);
extern REBINT Alloc_Window(REBGOB *gob);
extern void Draw_Window(REBGOB *wingob, REBGOB *gob);

//***** Locals *****

static REBXYF Zero_Pair = {0, 0};

//**********************************************************************
//** OSAL Library Functions ********************************************
//**********************************************************************

/***********************************************************************
**
*/	void OS_Init_Windows()
/*
**		Initialize special variables of the graphics subsystem.
**
***********************************************************************/
{
}

/***********************************************************************
**
*/	void OS_Update_Window(REBGOB *gob)
/*
**		Update window parameters.
**
***********************************************************************/
{
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
	REBCHR *title;
	REBYTE os_string = FALSE;
	Uint32 flags = 0;

	windex = Alloc_Window(gob);

	if (IS_GOB_STRING(gob))
        os_string = As_OS_Str(GOB_CONTENT(gob), (REBCHR**)&title);
    else
        title = TXT("REBOL Window");

	if (GET_FLAGS(gob->flags, GOBF_NO_TITLE, GOBF_NO_BORDER)) {
		flags |= SDL_WINDOW_BORDERLESS;
	} else if (GET_FLAG(gob->flags, GOBF_FULLSCREEN)) {
		flags |= SDL_WINDOW_FULLSCREEN;
	} else if (GET_FLAG(gob->flags, GOBF_RESIZE)) {
		flags |= SDL_WINDOW_RESIZABLE;
	} else if (GET_FLAG(gob->flags, GOBF_MAXIMIZE)) {
		flags |= SDL_WINDOW_MAXIMIZED;
	} else if (GET_FLAG(gob->flags, GOBF_MINIMIZE)) {
		flags |= SDL_WINDOW_MINIMIZED;
	}
	win = SDL_CreateWindow(title, x, y, w, h, flags);
	if (os_string)
		OS_Free(title);

	SDL_SetWindowData(win, "GOB", gob);

	SDL_ShowWindow(win);

	Gob_Windows[windex].win = win;
	Gob_Windows[windex].compositor = rebcmp_create(Gob_Root, gob); /* it updates has_xshm */
	CLEAR_GOB_STATE(gob);
	SET_GOB_STATE(gob, GOBS_NEW);

	SET_GOB_FLAG(gob, GOBF_WINDOW);
	SET_GOB_FLAG(gob, GOBF_ACTIVE);
	SET_GOB_STATE(gob, GOBS_OPEN);
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
	CLR_GOB_STATES(gob, GOBS_OPEN, GOBS_ACTIVE);
	Free_Window(gob);
}
