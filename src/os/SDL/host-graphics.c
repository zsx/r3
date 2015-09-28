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

#ifdef TO_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
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
	return 0;
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

#ifdef TO_LINUX

static int get_work_area(Display *display, METRIC_TYPE type)
{
	   Atom     actual_type;
	   int      actual_format;
	   long     nitems;
	   long     bytes;
	   long     *data = NULL;
	   int 		status;
	   int 		index = 0;
	   int		ret;
	   Atom		XA_NET_WORKAREA = None;
	   int fake_data[] = {0, 0, 1920, 1080};

	   switch(type) {
		   case SM_WORK_X:
			   index = 0;
			   break;
		   case SM_WORK_Y:
			   index = 1;
			   break;
		   case SM_WORK_WIDTH:
			   index = 2;
			   break;
		   case SM_WORK_HEIGHT:
			   index = 3;
			   break;
	   }

	   XA_NET_WORKAREA = XInternAtom(display, "_NET_WORKAREA", True);
	   if (XA_NET_WORKAREA == None) {
		   return fake_data[index];
	   }
	   status = XGetWindowProperty(display,
								   DefaultRootWindow(display),
								   XA_NET_WORKAREA,
								   0,
								   (~0L),
								   False,
								   AnyPropertyType,
								   &actual_type,
								   &actual_format,
								   &nitems,
								   &bytes,
								   (unsigned char**)&data);

	   /*
		  RL_Print("actual_type %d\n", actual_type);
		  RL_Print("actual_format %d\n", actual_format);
		  RL_Print("nitems %d\n", nitems);
		  RL_Print("bytes %d\n", bytes);
		  for (i=0; i < nitems; i++){
		  RL_Print("data[%d] %d\n", i, data[i]);
		  }
		  */
	   if (status != Success
		   || data == NULL
		   || actual_type != XA_CARDINAL
		   || actual_format != 32
		   || nitems < 4) {
		   //RL_Print("Falling back...\n");
		   if (data) {
			   XFree(data);
		   }
		   return fake_data[index];
	   }

	   ret = data[index];
	   XFree(data);
	   return ret;
}

/***********************************************************************
**
*/	REBD32 X11_Get_Metrics(METRIC_TYPE type)
/*
**
***********************************************************************/
{
       Screen *sc = NULL;
	   Window root = 0;
       int dot, mm;
	   Atom     actual_type;
	   int      actual_format;
	   long     nitems;
	   long     bytes;
	   long     *data = NULL;
	   int      status;
	   int 		i;
	   REBD32	ret;
	   Atom		XA_NET_FRAME_EXTENTS = None;
	   Display	*display = NULL;

	   display = XOpenDisplay(NULL);

       if (display == NULL){
               return 0;
       }
	   root = DefaultRootWindow(display);
       sc = XDefaultScreenOfDisplay(display);
       switch(type) {
               case SM_SCREEN_WIDTH:
                       ret = XWidthOfScreen(sc);
					   XCloseDisplay(display);
					   return ret;
               case SM_SCREEN_HEIGHT:
                       ret = XHeightOfScreen(sc);
					   XCloseDisplay(display);
					   return ret;
               case SM_WORK_X:
               case SM_WORK_Y:
               case SM_WORK_WIDTH:
               case SM_WORK_HEIGHT:
					   ret = get_work_area(display, type);
					   XCloseDisplay(display);
					   return ret;
               case SM_TITLE_HEIGHT:
					   XA_NET_FRAME_EXTENTS = XInternAtom(display, "_NET_FRAME_EXTENTS", True);
					   if (XA_NET_FRAME_EXTENTS == None) {
						   XCloseDisplay(display);
						   return 20; //FIXME
					   }
					   status = XGetWindowProperty(display,
												   RootWindowOfScreen(sc),
												   XA_NET_FRAME_EXTENTS,
												   0,
												   (~0L),
												   False,
												   AnyPropertyType,
												   &actual_type,
												   &actual_format,
												   &nitems,
												   &bytes,
												   (unsigned char**)&data);
					   if (status != Success
						   || data == NULL
						   || actual_type != XA_CARDINAL
						   || actual_format != 32
						   || nitems != 4) {
						   //RL_Print("status = %d, nitmes = %d\n", status, nitems);
						   //Host_Crash("XGetWindowProperty failed in OS_Get_Metrics");
						   XCloseDisplay(display);
						   return 20; //FIXME
					   }

                       ret = data[2]; //left, right, top, bottom
					   XFree(data);
					   XCloseDisplay(display);
					   return ret;
               case SM_SCREEN_DPI_X:
                       dot = XWidthOfScreen(sc);
                       mm = XWidthMMOfScreen(sc);
					   XCloseDisplay(display);
                       return round(dot * 25.4 / mm);
               case SM_SCREEN_DPI_Y:
                       dot = XHeightOfScreen(sc);
                       mm = XHeightMMOfScreen(sc);
					   XCloseDisplay(display);
                       return round(dot * 25.4 / mm);
               case SM_BORDER_WIDTH:
               case SM_BORDER_HEIGHT:
               case SM_BORDER_FIXED_WIDTH:
               case SM_BORDER_FIXED_HEIGHT:
					   XCloseDisplay(display);
					   return 5; //FIXME
               case SM_WINDOW_MIN_WIDTH:
					   XCloseDisplay(display);
					   return 132; //FIXME; from windows
               case SM_WINDOW_MIN_HEIGHT:
					   XCloseDisplay(display);
					   return 38; //FIXME; from windows
               default:
					   XCloseDisplay(display);
					   Host_Crash("NOT implemented others in OS_Get_Metrics");
                       return 0; //FIXME, not implemented 
       }
}
#endif

/***********************************************************************
**
*/	REBD32 OS_Get_Metrics(METRIC_TYPE type)
/*
**	Provide OS specific UI related information.
**
***********************************************************************/
{
#ifdef TO_LINUX
	return X11_Get_Metrics(type);
#else
	SDL_Rect rect;
	SDL_DisplayMode mode;
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "number of screens: %d\n", SDL_GetNumVideoDisplays());

	switch (type) {
		case SM_SCREEN_WIDTH:
			//SDL_GetDisplayBounds(0, &rect);
			if (SDL_GetDisplayBounds(0, &rect)) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetDisplayBounds failed: %s", SDL_GetError());
				//return rect.w;
				return 1024;
			}
			//SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "screen dimension: %dx%d, format: 0x%x\n", mode.w, mode.h, mode.format);
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "screen width: %d\n", rect.w);
			return rect.w;
		case SM_SCREEN_HEIGHT:
			SDL_GetDisplayBounds(0, &rect);
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "screen height: %d\n", rect.h);
			return rect.h;
	}
	return 0;
#endif
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
