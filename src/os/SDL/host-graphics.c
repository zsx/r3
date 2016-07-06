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

#include <math.h>
#include "reb-host.h"
#include "host-view.h"
#include "host-ext-graphics.h"
#include "SDL.h"

#ifdef TO_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#elif defined(TO_WINDOWS)
#include <Windows.h>
#endif

//** Externs *****
RXIEXT int RXD_Graphics(int cmd, RXIFRM *frm, REBCEC *data);
RXIEXT int RXD_Draw(int cmd, RXIFRM *frm, REBCEC *ctx);
RXIEXT int RXD_Shape(int cmd, RXIFRM *frm, REBCEC *ctx);
RXIEXT int RXD_Text(int cmd, RXIFRM *frm, REBCEC *ctx);

extern const unsigned char RX_graphics[];
extern const unsigned char RX_draw[];
extern const unsigned char RX_shape[];
extern const unsigned char RX_text[];

extern void Init_Host_Event();
extern void Host_Crash(const char *reason);

////////////////////////////////////////////////////////////////////////
//** Helper Functions **************************************************
///////////////////////////////////////////////////////////////////////


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
       Screen *sc = XDefaultScreenOfDisplay(global_x_info->display);
	   int virtual_width = XWidthOfScreen(sc);
	   int virtual_height = XHeightOfScreen(sc);

	   XRRScreenResources *res;
	   RROutput primary_output;
	   XRROutputInfo *info;
	   XRRCrtcInfo *crtc;

	   Window root = DefaultRootWindow(global_x_info->display);
	   res = XRRGetScreenResourcesCurrent(global_x_info->display, root);
	   primary_output = XRRGetOutputPrimary(global_x_info->display, root);
	   info = XRRGetOutputInfo(global_x_info->display, res, primary_output);

	   crtc = XRRGetCrtcInfo(global_x_info->display, res, info->crtc);

	   /* adjust width/height for primary output */
	   data [2] += crtc->width - virtual_width;
	   data [3] += crtc->height - virtual_height;

	   XRRFreeCrtcInfo(crtc);
	   XRRFreeOutputInfo(info);
	   XRRFreeScreenResources(res);

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
       switch (type) {
       case SM_VIRTUAL_SCREEN_WIDTH:
           return XWidthOfScreen(sc);
       case SM_VIRTUAL_SCREEN_HEIGHT:
           return XHeightOfScreen(sc);
       case SM_SCREEN_WIDTH:
       case SM_SCREEN_HEIGHT:
       {
           XRRScreenResources *res;
           RROutput primary_output;
           XRROutputInfo *info;
           XRRCrtcInfo *crtc;

           res = XRRGetScreenResourcesCurrent(global_x_info->display, root);
           primary_output = XRRGetOutputPrimary(global_x_info->display, root);
           info = XRRGetOutputInfo(global_x_info->display, res, primary_output);

           crtc = XRRGetCrtcInfo(global_x_info->display, res, info->crtc);

           ret = (type == SM_SCREEN_WIDTH) ? crtc->width : crtc->height;
           XRRFreeCrtcInfo(crtc);
           XRRFreeOutputInfo(info);
           XRRFreeScreenResources(res);

           return ret;
       }
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
#elif defined(TO_WINDOWS)
static REBD32 Windows_Get_Metrics(METRIC_TYPE type)
{
	REBD32 result = 0;
	switch(type){
		case SM_VIRTUAL_SCREEN_WIDTH:
			result = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			break;
		case SM_VIRTUAL_SCREEN_HEIGHT:
			result = GetSystemMetrics(SM_CYVIRTUALSCREEN);
			break;
		case SM_SCREEN_WIDTH:
			result = GetSystemMetrics(SM_CXSCREEN);
			break;
		case SM_SCREEN_HEIGHT:
			result = GetSystemMetrics(SM_CYSCREEN);
			break;
		case SM_WORK_WIDTH:
			{
				RECT rect;
				SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
				result = rect.right;
			}
			break;
		case SM_WORK_HEIGHT:
			{
				RECT rect;
				SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
				result = rect.bottom;
			}
			break;
		case SM_TITLE_HEIGHT:
			result = GetSystemMetrics(SM_CYCAPTION);
			break;
		case SM_SCREEN_DPI_X:
			{
				HDC hDC = GetDC(NULL);
				result = GetDeviceCaps(hDC, LOGPIXELSX);
				ReleaseDC(NULL, hDC);
			}
			break;
		case SM_SCREEN_DPI_Y:
			{
				HDC hDC = GetDC(NULL);
				result = GetDeviceCaps(hDC, LOGPIXELSY);
				ReleaseDC(NULL, hDC);
			}
			break;
		case SM_BORDER_WIDTH:
			result = GetSystemMetrics(SM_CXSIZEFRAME);
			break;
		case SM_BORDER_HEIGHT:
			result = GetSystemMetrics(SM_CYSIZEFRAME);
			break;
		case SM_BORDER_FIXED_WIDTH:
			result = GetSystemMetrics(SM_CXFIXEDFRAME);
			break;
		case SM_BORDER_FIXED_HEIGHT:
			result = GetSystemMetrics(SM_CYFIXEDFRAME);
			break;
		case SM_WINDOW_MIN_WIDTH:
			result = GetSystemMetrics(SM_CXMIN);
			break;
		case SM_WINDOW_MIN_HEIGHT:
			result = GetSystemMetrics(SM_CYMIN);
			break;
		case SM_WORK_X:
			{
				RECT rect;
				SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
				result = rect.left;
			}
			break;
		case SM_WORK_Y:
			{
				RECT rect;
				SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
				result = rect.top;
			}
			break;
	}
	return result;
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
#elif defined(TO_WINDOWS)
    return Windows_Get_Metrics(type);
#else
	SDL_Rect rect;
	SDL_DisplayMode mode;
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "number of screens: %d\n", SDL_GetNumVideoDisplays());
	float ddpi, vdpi, hdpi;

	switch (type) {
		case SM_SCREEN_WIDTH:
		case SM_WORK_WIDTH:
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
		case SM_WORK_HEIGHT:
			SDL_GetDisplayBounds(0, &rect);
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "screen height: %d\n", rect.h);
			return rect.h;

		case SM_SCREEN_DPI_X:
			if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) < 0) return 0;
			return hdpi;

		case SM_SCREEN_DPI_Y:
			if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) < 0) return 0;
			return vdpi;
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
