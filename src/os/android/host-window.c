/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
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
**  Title: Android OS Windowing support
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

#include <math.h>

#include "reb-host.h"
//#include "host-lib.h"
#include "host-jni.h"

#include "host-compositor.h"

//***** Constants *****

#define GOB_HWIN(gob)	((REBINT)Find_Window(gob))
#define GOB_COMPOSITOR(gob)	(Find_Compositor(gob)) //gets handle to window's compositor

//***** Externs *****
extern REBGOBWINDOWS *Gob_Windows;
extern void Free_Window(REBGOB *gob);
extern void* Find_Compositor(REBGOB *gob);
extern REBINT Alloc_Window(REBGOB *gob);
extern void Draw_Window(REBGOB *wingob, REBGOB *gob);

//***** Locals *****

//static const REBCHR *Window_Class_Name = TXT("REBOLWindow");
static REBXYF Zero_Pair = {0, 0};

//HCURSOR Cursor;					// active cursor image object

//**********************************************************************
//** Helper Functions **************************************************
//**********************************************************************

/***********************************************************************
**
*/	void Paint_Window(void *window)
/*
**		Repaint the window by redrawing all the gobs.
**		It just blits the whole window buffer.
**
***********************************************************************/
{
	REBGOB *gob = (REBGOB*)(*jni_env)->CallIntMethod(jni_env, jni_obj, jni_getWindowGob, window);
	if (gob)
		rebcmp_blit(GOB_COMPOSITOR(gob));
}

//**********************************************************************
//** OSAL Library Functions ********************************************
//**********************************************************************

/***********************************************************************
**
*/	void OS_Init_Windows()
/*
**  Initialize special variables of the graphics subsystem.
**
***********************************************************************/
{
//	LOGI("OS_Init_Windows()");
}

/***********************************************************************
**
*/	void OS_Update_Window(REBGOB *gob)
/*
**		Update window parameters.
**
***********************************************************************/
{
//	LOGI("OS_Update_Window(): %d", GOB_HWIN(gob));
	REBINT x = GOB_PX_INT(gob);
	REBINT y = GOB_PY_INT(gob);
	REBINT w = GOB_PW_INT(gob);
	REBINT h = GOB_PH_INT(gob);

	REBOOL moved = ((x != GOB_XO_INT(gob)) || (y != GOB_YO_INT(gob)));
	REBOOL resized = Resize_Window(gob, FALSE);
	
	if (moved || resized)
		(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_updateWindow, GOB_HWIN(gob), x, y, w, h);
	
	if (GET_GOB_FLAG(gob, GOBF_ACTIVE)){
		CLR_GOB_FLAG(gob, GOBF_ACTIVE);
		(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_windowToFront, GOB_HWIN(gob));
	}
}

/***********************************************************************
**
*/  void* OS_Open_Window(REBGOB *gob)
/*
**      Initialize the graphics window.
**
**		The window handle is returned, but not expected to be used
**		other than for debugging conditions.
**
***********************************************************************/
{
	REBINT windex;
	REBINT x = GOB_PX_INT(gob);
	REBINT y = GOB_PY_INT(gob);
	REBINT w = GOB_PW_INT(gob);
	REBINT h = GOB_PH_INT(gob);
	void* window;
	
	windex = Alloc_Window(gob);
	
	if (windex < 0) Host_Crash("Too many windows");

	CLEAR_GOB_STATE(gob);
	SET_GOB_STATE(gob, GOBS_NEW);
//	LOGI("jni_createWindow: %dx%d %dx%d %dx%d\n", GOB_W_INT(gob),GOB_H_INT(gob),w,h, GOB_W(gob),GOB_H(gob));
	window = (void*)(*jni_env)->CallIntMethod(jni_env, jni_obj, jni_createWindow, (REBINT)gob, x, y, w, h, FALSE);

//	LOGI("OS_Open_Window(): %d", window);

	Gob_Windows[windex].win = window;
	Gob_Windows[windex].compositor = rebcmp_create(Gob_Root, gob);
	
	SET_GOB_FLAG(gob, GOBF_WINDOW);
	SET_GOB_FLAG(gob, GOBF_ACTIVE);	
	SET_GOB_STATE(gob, GOBS_OPEN); 

    if (!GET_GOB_FLAG(gob, GOBF_HIDDEN)) {
        OS_Update_Window(gob);
    }
	
	return NULL;
}

/***********************************************************************
**
*/  void OS_Close_Window(REBGOB *gob)
/*
**		Close the window.
**
***********************************************************************/
{
//	LOGI("OS_Close_Window(): %d", GOB_HWIN(gob));
	if (GET_GOB_FLAG(gob, GOBF_WINDOW) && Find_Window(gob)) {
		(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_destroyWindow, GOB_HWIN(gob));
		CLR_GOB_FLAG(gob, GOBF_WINDOW);
		CLEAR_GOB_STATE(gob); // set here or in the destroy?
		Free_Window(gob);
	}
}
