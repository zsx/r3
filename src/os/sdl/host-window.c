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

#if defined(WITH_OPENGLES)
#include "GLES/gl.h"
#include "GLES/glext.h"
#else
#if defined(SK_BUILD_FOR_ANDROID)
#include <GLES/gl.h>
#elif defined(SK_BUILD_FOR_UNIX)
#include <GL/gl.h>
#elif defined(SK_BUILD_FOR_MAC)
#include <OpenGL/gl.h>
#elif defined(SK_BUILD_FOR_IOS)
#include <OpenGLES/ES2/gl.h>
#include "SDL_opengl.h"
#endif
#endif // WITH_OPENGLES

#include "reb-skia.h"

//***** Constants *****

void* Find_Window(REBGOB *gob);
#define GOB_HWIN(gob)	(Find_Window(gob))
#define GOB_COMPOSITOR(gob)	(Find_Compositor(gob)) //gets handle to window's compositor

//***** Externs *****
extern REBGOBWINDOWS *Gob_Windows;
extern void Free_Window(REBGOB *gob);
extern void* Find_Compositor(REBGOB *gob);
extern REBINT Alloc_Window(REBGOB *gob);
extern void Draw_Window(REBGOB *wingob, REBGOB *gob);
extern REBINT As_UTF8_Str(REBSER *series, REBYTE **string);

//***** Locals *****

static REBXYF Zero_Pair = {0, 0};

//**********************************************************************
//** OSAL Library Functions ********************************************
//**********************************************************************

static int prepare_gl_env()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

#if defined(WITH_OPENGLES)
	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1"); // required for ANGLE
#endif

#if defined(SK_BUILD_FOR_ANDROID) || defined(SK_BUILD_FOR_IOS) || defined(WITH_OPENGLES)
    // For Android/iOS we need to set up for OpenGL ES and we make the window hi res & full screen
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	/*
    param->windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                  SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN_DESKTOP |
                  SDL_WINDOW_ALLOW_HIGHDPI;
				  */
#else
    // For all other clients we use the core profile and operate in a window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);  // Skia needs 8 stencil bits

    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
}


/***********************************************************************
**
*/	void OS_Init_Windows()
/*
**		Initialize special variables of the graphics subsystem.
**
***********************************************************************/
{
	//rs_draw_enable_skia_trace();

	prepare_gl_env();
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

	SDL_UpdateWindowSurface(win);
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
	REBYTE title_needs_free = FALSE;
	Uint32 flags = SDL_WINDOW_OPENGL;
	REBGOB *parent_gob = GOB_PARENT(gob);

	windex = Alloc_Window(gob);

	if (IS_GOB_STRING(gob))
        title_needs_free = As_UTF8_Str(GOB_CONTENT(gob), (REBCHR**)&title);
    else
        title = "REBOL Window";

	if (GET_FLAGS(gob->flags, GOBF_NO_TITLE, GOBF_NO_BORDER)) {
		flags |= SDL_WINDOW_BORDERLESS;
	}

	if (GET_FLAG(gob->flags, GOBF_RESIZE)) {
		flags |= SDL_WINDOW_RESIZABLE;
	}

	if (GET_FLAG(gob->flags, GOBF_FULLSCREEN)) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else if (GET_FLAG(gob->flags, GOBF_MAXIMIZE)) {
		flags |= SDL_WINDOW_MAXIMIZED;
	} else if (GET_FLAG(gob->flags, GOBF_MINIMIZE)) {
		flags |= SDL_WINDOW_MINIMIZED;
	}

	printf("Opening a window at: %dx%d, %dx%d, owner gob: 0x%p\n", x, y, w, h, parent_gob);
	if (parent_gob != NULL) {
		if (!GET_GOB_FLAG(gob, GOBF_POPUP)) {
			/* x, y are in parent gob coordinates */
			REBINT max_depth = 1000; // avoid infinite loops
			while (parent_gob != NULL
				   && (max_depth-- > 0)
				   && !GET_GOB_FLAG(parent_gob, GOBF_WINDOW)) {
				x += GOB_LOG_X(parent_gob);
				y += GOB_LOG_Y(parent_gob);
				parent_gob = GOB_PARENT(parent_gob);
			}
			if (parent_gob != NULL
				&& GET_GOB_FLAG(parent_gob, GOBF_WINDOW)) {
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
	if (title_needs_free)
		OS_Free(title);

	SDL_SetWindowData(win, "GOB", gob);

	if (GET_GOB_FLAG(gob, GOBF_HIDDEN)
		|| (x + w) < 0
		|| (y + h) < 0) {
		/* r3-gui.r3 sets offset to negatives to hide it */
		SDL_HideWindow(win);
	}

	Gob_Windows[windex].win = win;
	Gob_Windows[windex].compositor = rebcmp_create(Gob_Root, gob, win);
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
