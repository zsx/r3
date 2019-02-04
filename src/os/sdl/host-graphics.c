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
**  Title: Graphics Commmands
**  Author: Richard Smolak, Carl Sassenrath
**  Purpose: "View" commands support.
**  Tools: make-host-ext.r
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
#include "SDL.h"

#ifdef TO_WIN32
#include <windows.h>
#endif

//***** Externs *****
RXIEXT int RXD_Graphics(int cmd, RXIFRM *frm, REBCEC *data);
RXIEXT int RXD_Draw(int cmd, RXIFRM *frm, REBCEC *ctx);
RXIEXT int RXD_Shape(int cmd, RXIFRM *frm, REBCEC *ctx);
RXIEXT int RXD_Text(int cmd, RXIFRM *frm, REBCEC *ctx);

extern const unsigned char RX_graphics[];
extern const unsigned char RX_draw[];
extern const unsigned char RX_shape[];
extern const unsigned char RX_text[];

extern void Init_Host_Event();

//**********************************************************************
//** Helper Functions **************************************************
//**********************************************************************


/***********************************************************************
**
*/	void* OS_Image_To_Cursor(REBYTE* image, REBINT width, REBINT height)
/*
**      Converts REBOL image! to Windows CURSOR
**
***********************************************************************/
{
	return 0;
}

/***********************************************************************
**
*/	void OS_Set_Cursor(void *cursor)
/*
**
**
***********************************************************************/
{
}

/***********************************************************************
**
*/	void* OS_Load_Cursor(void *cursor)
/*
**
**
***********************************************************************/
{
	return NULL;
}

/***********************************************************************
**
*/	void OS_Destroy_Cursor(void *cursor)
/*
**
**
***********************************************************************/
{
}


/***********************************************************************
**
*/	REBD32 OS_Get_Metrics(METRIC_TYPE type, REBINT display)
/*
**	Provide OS specific UI related information.
**
***********************************************************************/
{
	SDL_Rect rect;
	SDL_DisplayMode mode;
	float dpi;
    REBD32 ret = 0;

	switch (type) {
		case SM_SCREEN_NUM:
			ret = SDL_GetNumVideoDisplays();
			break;
		case SM_SCREEN_WIDTH:
			//SDL_GetDisplayBounds(0, &rect);
			if (SDL_GetDisplayBounds(display, &rect)) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetDisplayBounds failed: %s", SDL_GetError());
				ret = 0;
			}
			ret = rect.w;
			break;
		case SM_SCREEN_HEIGHT:
			if (SDL_GetDisplayBounds(display, &rect)) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetDisplayBounds failed: %s", SDL_GetError());
				ret = 0;
			}
			//SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "screen height: %d\n", rect.h);
			ret = rect.h;
			break;
		case SM_SCREEN_X:
			SDL_GetDisplayBounds(display, &rect);
			ret = rect.x;
			break;
		case SM_SCREEN_Y:
			SDL_GetDisplayBounds(display, &rect);
			ret = rect.y;
			break;
		case SM_WORK_WIDTH:
			if (SDL_GetDisplayUsableBounds(display, &rect)) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetDisplayUsableBounds failed: %s", SDL_GetError());
				ret = 0;
			}
			ret = rect.w;
			break;
		case SM_WORK_HEIGHT:
			SDL_GetDisplayUsableBounds(display, &rect);
			ret = rect.h;
			break;
		case SM_WORK_X:
			SDL_GetDisplayUsableBounds(display, &rect);
			ret = rect.x;
			break;
		case SM_WORK_Y:
			SDL_GetDisplayUsableBounds(display, &rect);
			ret = rect.y;
			break;
		case SM_SCREEN_DPI_X:
			SDL_GetDisplayDPI(display, NULL, &dpi, NULL);
			ret = dpi;
			break;
		case SM_SCREEN_DPI_Y:
			SDL_GetDisplayDPI(display, NULL, NULL, &dpi);
			ret = dpi;
			break;
		case SM_TITLE_HEIGHT:
#ifdef TO_WIN32
			ret = GetSystemMetrics(SM_CYCAPTION);
#else
			ret = 23;
#endif
			break;

#ifdef TO_WIN32
		case SM_BORDER_WIDTH:
			ret = GetSystemMetrics(SM_CXSIZEFRAME);
			break;
		case SM_BORDER_HEIGHT:
			ret = GetSystemMetrics(SM_CYSIZEFRAME);
			break;
		case SM_BORDER_FIXED_WIDTH:
			ret = GetSystemMetrics(SM_CXFIXEDFRAME);
			break;
		case SM_BORDER_FIXED_HEIGHT:
			ret = GetSystemMetrics(SM_CYFIXEDFRAME);
			break;
		case SM_WINDOW_MIN_WIDTH:
			ret = GetSystemMetrics(SM_CXMIN);
			break;
		case SM_WINDOW_MIN_HEIGHT:
			ret = GetSystemMetrics(SM_CYMIN);
			break;
#else
		case SM_WINDOW_MIN_WIDTH:
		case SM_WINDOW_MIN_HEIGHT:
		{
			SDL_Window *win = SDL_CreateWindow("metric", 0, 0, 0, 0, SDL_WINDOW_HIDDEN);
			int w = 0, h = 0;
			if (win) {
				SDL_GetWindowMinimumSize(win, &w, &h);
				SDL_DestroyWindow(win);
			}
			if (h == 0) h = 39;
			if (w == 0) w = 136;
			ret = (type == SM_WINDOW_MIN_WIDTH)? w : h;
		}
			break;
		case SM_BORDER_WIDTH:
			ret = 4;
			break;
		case SM_BORDER_HEIGHT:
			ret = 4;
			break;

#endif
	}
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Metric: %d = %f\n", type, ret);

	return ret;
//#endif
}

/***********************************************************************
**
*/	void OS_Show_Soft_Keyboard(void* win, REBINT x, REBINT y)
/*
**  Display software/virtual keyboard on the screen.
**  (mainly used on mobile platforms)
**
***********************************************************************/
{
}

/***********************************************************************
**
*/	void OS_Init_Graphics(void)
/*
**	Initialize special variables of the graphics subsystem.
**
***********************************************************************/
{
	RL_Extend((REBYTE *)(&RX_graphics[0]), &RXD_Graphics);
	RL_Extend((REBYTE *)(&RX_draw[0]), &RXD_Draw);
	RL_Extend((REBYTE *)(&RX_shape[0]), &RXD_Shape);
	RL_Extend((REBYTE *)(&RX_text[0]), &RXD_Text);

	Init_Host_Event();
}

/***********************************************************************
**
*/	void OS_Destroy_Graphics(void)
/*
**	Finalize any special variables of the graphics subsystem.
**
***********************************************************************/
{
}
