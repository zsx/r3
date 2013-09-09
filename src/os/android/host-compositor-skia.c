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
**  Title: Android OS(Skia backend)based Compositor abstraction layer API.
**  Author: Richard Smolak
**  File:  host-compositor.c
**  Purpose: Provides simple gob compositor code for Android OS.
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

#include <math.h>	//for floor()
#include "reb-host.h"
#include "host-jni.h"
#include <android/bitmap.h>
//***** Macros *****

#define GOB_HWIN(gob)	((REBINT)Find_Window(gob))

//***** Locals *****

static REBXYF Zero_Pair = {0, 0};

typedef struct {
	REBINT left;
	REBINT top;
	REBINT right;
	REBINT bottom;
} REBRECT;

typedef struct {
	REBINT Window;				//window ref ID
	REBYTE *Window_Buffer;
	jobject jnibuffer;
	REBXYI winBufSize;	
	REBGOB *Win_Gob;
	REBGOB *Root_Gob;
	REBXYF absOffset;
} REBCMP_CTX;

enum Region_ops {
	RGN_OP_REPLACE = 0,
	RGN_OP_INTERSECT,
	RGN_OP_UNION
};

/***********************************************************************
**
*/ REBYTE* rebcmp_get_buffer(REBCMP_CTX* ctx)
/*
**	Provide pointer to window compositing buffer.
**  Return NULL if buffer not available of call failed.
**
**  NOTE: The buffer may be "locked" during this call on some platforms.
**        Always call rebcmp_release_buffer() to be sure it is released.
**
***********************************************************************/
{
//	LOGI("rebcmp_get_buffer()");
	void* bitmapPixels;
	REBYTE* bytes;
	REBINT result;
	
	ctx->jnibuffer = (*jni_env)->CallObjectMethod(jni_env, jni_obj, jni_getWindowBuffer, ctx->Window);
	result = AndroidBitmap_lockPixels(jni_env, ctx->jnibuffer, &bitmapPixels);
	
	if (result < 0) return NULL;

	return (REBYTE*)bitmapPixels;
}

/***********************************************************************
**
*/ void rebcmp_release_buffer(REBCMP_CTX* ctx)
/*
**	Release the window compositing buffer acquired by rebcmp_get_buffer().
**
**  NOTE: this call can be "no-op" on platforms that don't need locking.
**
***********************************************************************/
{
//	LOGI("rebcmp_release_buffer()");
	AndroidBitmap_unlockPixels(jni_env, ctx->jnibuffer);
	(*jni_env)->DeleteLocalRef(jni_env, ctx->jnibuffer);
}

/***********************************************************************
**
*/ REBOOL rebcmp_resize_buffer(REBCMP_CTX* ctx, REBGOB* winGob)
/*
**	Resize the window compositing buffer.
**
**  Returns TRUE if buffer size was really changed, otherwise FALSE.
**
***********************************************************************/
{
//	LOGI("rebcmp_resize_buffer()");
	//check if window size really changed
	if ((GOB_LOG_W(winGob) != GOB_WO(winGob)) || (GOB_LOG_H(winGob) != GOB_HO(winGob))) {

		REBINT w = GOB_LOG_W_INT(winGob);
		REBINT h = GOB_LOG_H_INT(winGob);
//		LOGI("resize to: %dx%d %dx%d\n", GOB_LOG_X_INT(winGob), GOB_LOG_Y_INT(winGob), w, h);
		(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_updateWindow, ctx->Window, GOB_LOG_X_INT(winGob), GOB_LOG_Y_INT(winGob), w, h);

		//update the buffer size values
		ctx->winBufSize.x = w;
		ctx->winBufSize.y = h;
		
		//update old gob area
		GOB_XO(winGob) = GOB_LOG_X(winGob);
		GOB_YO(winGob) = GOB_LOG_Y(winGob);
		GOB_WO(winGob) = GOB_LOG_W(winGob);
		GOB_HO(winGob) = GOB_LOG_H(winGob);
		return TRUE;
	}
	return FALSE;
}

/***********************************************************************
**
*/ void* rebcmp_create(REBGOB* rootGob, REBGOB* gob)
/*
**	Create new Compositor instance.
**
***********************************************************************/
{
//	LOGI("rebcmp_create()");
	//new compositor struct
	REBCMP_CTX *ctx = (REBCMP_CTX*)OS_Make(sizeof(REBCMP_CTX));
	REBINT windex;
	
	//shortcuts
	ctx->Root_Gob = rootGob;
	ctx->Win_Gob = gob;
	ctx->Window = GOB_HWIN(gob); //an "id" of the Android WindowWiew class
	
	if (ctx->Window == 0) {	//no physical window, use "offscreen buffer" only
		(*jni_env)->CallIntMethod(jni_env, jni_obj, jni_createWindow, (REBINT)gob, GOB_LOG_X_INT(gob), GOB_LOG_Y_INT(gob), GOB_LOG_W_INT(gob), GOB_LOG_H_INT(gob), TRUE);
	}
	
	//call resize to init buffer
	rebcmp_resize_buffer(ctx, gob);
	return ctx;
}

/***********************************************************************
**
*/ void rebcmp_destroy(REBCMP_CTX* ctx)
/*
**	Destroy existing Compositor instance.
**
***********************************************************************/
{
//	LOGI("rebcmp_destroy()");
	if (ctx->Window == 0) //destroy "offscreen buffer"
		(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_destroyWindow, 0);
	OS_Free(ctx);
}

/***********************************************************************
**
*/ static void process_gobs(REBCMP_CTX* ctx, REBGOB* gob)
/*
**	Recursively process and compose gob and its children.
**
** NOTE: this function is used internally by rebcmp_compose() call only.
**
***********************************************************************/
{
	REBINT x = ROUND_TO_INT(ctx->absOffset.x);
	REBINT y = ROUND_TO_INT(ctx->absOffset.y);
//	jboolean r = TRUE;
//	REBRECT gob_clip = {x, y, x + GOB_W_INT(gob), y + GOB_H_INT(gob)};
//	jintArray array;
//	jint *coords;

	if (GET_GOB_STATE(gob, GOBS_NEW)){
		//reset old-offset and old-size if newly added
		GOB_XO(gob) = GOB_LOG_X(gob);
		GOB_YO(gob) = GOB_LOG_Y(gob);
		GOB_WO(gob) = GOB_LOG_W(gob);
		GOB_HO(gob) = GOB_LOG_H(gob);

		CLR_GOB_STATE(gob, GOBS_NEW);
	}

//	RL_Print("oft: %dx%d siz: %dx%d abs_oft: %dx%d \n", GOB_X_INT(gob), GOB_Y_INT(gob), GOB_W_INT(gob), GOB_H_INT(gob), x, y);

	//intersect gob dimensions with actual window clip region
//	(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_setWinRegion, ctx->Window, x, y, x + GOB_W_INT(gob), y + GOB_H_INT(gob));	
//	jboolean r = (*jni_env)->CallBooleanMethod(jni_env, jni_obj, jni_setWindowClip, ctx->Window, RGN_OP_INTERSECT);

	//get the current Window clip box
//	jintArray array = (*jni_env)->CallObjectMethod(jni_env, jni_obj, jni_getWindowClip, ctx->Window);
	jintArray array = (*jni_env)->CallObjectMethod(jni_env, jni_obj, jni_intersectWindowClip, ctx->Window, x, y, x + GOB_LOG_W_INT(gob), y + GOB_LOG_H_INT(gob));
	jint *coords = (*jni_env)->GetIntArrayElements(jni_env, array, NULL);
	REBRECT gob_clip = {coords[0],coords[1],coords[2],coords[3]};
	REBOOL valid_intersection = (REBOOL)coords[4];
	(*jni_env)->ReleaseIntArrayElements(jni_env, array, coords, 0);
	(*jni_env)->DeleteLocalRef(jni_env, array);
//	RL_Print("clip: %dx%d %dx%d\n", gob_clip.left, gob_clip.top, gob_clip.right, gob_clip.bottom);
//	LOGI("clip: %dx%d %dx%d\n", gob_clip.left, gob_clip.top, gob_clip.right, gob_clip.bottom);
	
	if (valid_intersection)
	{
//		RL_Print("clip OK %d %d\n", r, GOB_TYPE(gob));
		
//		if (!GET_GOB_FLAG(gob, GOBF_WINDOW))
		//render GOB content
		switch (GOB_TYPE(gob)) {
			case GOBT_COLOR:
//				(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_drawColor, ctx->Window, (REBCNT)GOB_CONTENT(gob));
				rebdrw_gob_color(gob, ctx->Window_Buffer, ctx->winBufSize, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
				break;
			
			case GOBT_IMAGE:
				{
//					REBYTE* buf = rebcmp_get_buffer(ctx);
//					rebdrw_gob_image(gob, buf, ctx->winBufSize, (REBXYI){x,y});
					rebdrw_gob_image(gob, ctx->Window_Buffer, ctx->winBufSize, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
//					rebcmp_release_buffer(ctx);
				}
				break;

			case GOBT_DRAW:
				{
//					REBYTE* buf = rebcmp_get_buffer(ctx);
//					rebdrw_gob_draw(gob, buf ,ctx->winBufSize, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
					rebdrw_gob_draw(gob, ctx->Window_Buffer ,ctx->winBufSize, (REBXYI){x,y}, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
//					rebcmp_release_buffer(ctx);
				}
				break;

			case GOBT_TEXT:
			case GOBT_STRING:
				rt_gob_text(gob, ctx->Window_Buffer ,ctx->winBufSize,ctx->absOffset, (REBXYI){gob_clip.left, gob_clip.top}, (REBXYI){gob_clip.right, gob_clip.bottom});
				break;
				
			case GOBT_EFFECT:
				break;
		}

		//recursively process sub GOBs
		if (GOB_PANE(gob)) {
			REBINT n;
			REBINT len = GOB_TAIL(gob);
			REBGOB **gp = GOB_HEAD(gob);
			
			for (n = 0; n < len; n++, gp++) {
				REBINT g_x = GOB_LOG_X(*gp);
				REBINT g_y = GOB_LOG_Y(*gp);

				//restore the "parent gob" clip region
//				(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_setWinRegion, ctx->Window, gob_clip.left, gob_clip.top, gob_clip.right, gob_clip.bottom);
//				(*jni_env)->CallBooleanMethod(jni_env, jni_obj, jni_setWindowClip, ctx->Window, RGN_OP_REPLACE);
				(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_resetWindowClip, ctx->Window, gob_clip.left, gob_clip.top, gob_clip.right, gob_clip.bottom);
				
				ctx->absOffset.x += g_x;
				ctx->absOffset.y += g_y;
				
				process_gobs(ctx, *gp);

				ctx->absOffset.x -= g_x;
				ctx->absOffset.y -= g_y;
			}
		}
	}
}

/***********************************************************************
**
*/ void rebcmp_compose(REBCMP_CTX* ctx, REBGOB* winGob, REBGOB* gob, REBOOL only)
/*
**	Compose content of the specified gob. Main compositing function.
**
**  If the ONLY arg is TRUE then the specified gob area will be
**  rendered to the buffer at 0x0 offset.(used by TO-IMAGE)
**
***********************************************************************/
{
//	LOGI("rebcmp_compose()");
	REBINT max_depth = 1000; // avoid infinite loops
	jboolean valid_intersection = TRUE;
	REBD32 abs_x;
	REBD32 abs_y;
	REBD32 abs_ox;
	REBD32 abs_oy;
	REBGOB* parent_gob = gob;
	
//	RL_Print("COMPOSE %d %d\n", GetDeviceCaps(ctx->backDC, SHADEBLENDCAPS), GetDeviceCaps(ctx->winDC, SHADEBLENDCAPS));
	
	abs_x = 0;
	abs_y = 0;

	//reset clip region to window area
//	(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_setWinRegion, ctx->Window, 0, 0, GOB_W_INT(winGob), GOB_H_INT(winGob));	
//	(*jni_env)->CallBooleanMethod(jni_env, jni_obj, jni_setWindowClip, ctx->Window, RGN_OP_REPLACE);
	
	//calculate absolute offset of the gob
	while (GOB_PARENT(parent_gob) && (max_depth-- > 0) && !GET_GOB_FLAG(parent_gob, GOBF_WINDOW))
	{
		abs_x += GOB_LOG_X(parent_gob);
		abs_y += GOB_LOG_Y(parent_gob);
		parent_gob = GOB_PARENT(parent_gob);
	} 

	//the offset is shifted to render given gob at offset 0x0 (used by TO-IMAGE)
	if (only){
		ctx->absOffset.x = -abs_x;
		ctx->absOffset.y = -abs_y;
		abs_x = 0;
		abs_y = 0;
	} else {
		ctx->absOffset.x = 0;
		ctx->absOffset.y = 0;
	}

	if (!GET_GOB_STATE(gob, GOBS_NEW)){
		//calculate absolute old offset of the gob
		abs_ox = abs_x + (GOB_XO(gob) - GOB_LOG_X(gob));
		abs_oy = abs_y + (GOB_YO(gob) - GOB_LOG_Y(gob));
		
//		RL_Print("OLD: %dx%d %dx%d\n",(REBINT)abs_ox, (REBINT)abs_oy, (REBINT)abs_ox + GOB_WO_INT(gob), (REBINT)abs_oy + GOB_HO_INT(gob));
		
		//set region with old gob location and dimensions
		(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_setOldRegion, ctx->Window, (REBINT)abs_ox, (REBINT)abs_oy, (REBINT)abs_ox + GOB_WO_INT(gob), (REBINT)abs_oy + GOB_HO_INT(gob));
	}
	
//	RL_Print("NEW: %dx%d %dx%d\n",(REBINT)abs_x, (REBINT)abs_y, (REBINT)abs_x + GOB_W_INT(gob), (REBINT)abs_y + GOB_H_INT(gob));
//LOGI("NEW: %dx%d %dx%d\n",(REBINT)abs_x, (REBINT)abs_y, (REBINT)abs_x + GOB_W_INT(gob), (REBINT)abs_y + GOB_H_INT(gob));	
	//Create union of "new" and "old" gob location
//	(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_setNewRegion, ctx->Window, (REBINT)abs_x, (REBINT)abs_y, (REBINT)abs_x + GOB_W_INT(gob), (REBINT)abs_y + GOB_H_INT(gob));
//	(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_combineRegions, ctx->Window);

	
	//intersect resulting region with window clip region
//	r = (*jni_env)->CallBooleanMethod(jni_env, jni_obj, jni_setWindowClip, ctx->Window, RGN_OP_INTERSECT);

	valid_intersection = (*jni_env)->CallBooleanMethod(jni_env, jni_obj, jni_setNewRegion, ctx->Window, (REBINT)abs_x, (REBINT)abs_y, (REBINT)abs_x + GOB_LOG_W_INT(gob), (REBINT)abs_y + GOB_LOG_H_INT(gob));

	if (valid_intersection)
	{
		ctx->Window_Buffer = rebcmp_get_buffer(ctx);
		
		//redraw gobs
		process_gobs(ctx, winGob);

		rebcmp_release_buffer(ctx);
		ctx->Window_Buffer = NULL;		
	}
	
	//update old GOB area
	GOB_XO(gob) = GOB_LOG_X(gob);
	GOB_YO(gob) = GOB_LOG_Y(gob);
	GOB_WO(gob) = GOB_LOG_W(gob);
	GOB_HO(gob) = GOB_LOG_H(gob);
}

/***********************************************************************
**
*/ void rebcmp_blit(REBCMP_CTX* ctx)
/*
**	Blit window content on the screen.
**
***********************************************************************/
{
//	LOGI("rebcmp_blit()");
	(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_blitWindow, ctx->Window);
//	LOGI("rebcmp_blitEND()");
}
