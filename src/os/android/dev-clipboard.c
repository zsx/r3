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
**  Title: Device: Clipboard access for Android
**  Author: Richard Smolak
**  Purpose:
**      Provides a very simple interface to the clipboard for text.
**      May be expanded in the future for images, etc.
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

#include "reb-host.h"
#include "host-lib.h"
#include "sys-net.h"

#include "host-jni.h"		// JNI support

/***********************************************************************
**
*/	DEVICE_CMD Open_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	SET_OPEN(req);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Close_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	SET_CLOSED(req);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Read_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	REBYTE *data;
	REBYTE *rstr;
	jsize rlen;
	jstring result;
	
	if (jni_env == NULL) return DR_ERROR;
	
	result = (*jni_env)->CallObjectMethod(jni_env, jni_obj, jni_getClipboard);
	rstr = (REBYTE*)(*jni_env)->GetStringUTFChars(jni_env, result, NULL);
	rlen = ((*jni_env)->GetStringUTFLength(jni_env, result) + 1) * sizeof(REBYTE);
	data = (REBYTE*)Make_Mem(rlen);
	COPY_STR(data, rstr, rlen);

	(*jni_env)->ReleaseStringUTFChars(jni_env, result, rstr);
	(*jni_env)->DeleteLocalRef(jni_env, result);
	
	req->actual = 0;
	
	if ((data) == NULL) {
		req->error = 30;
		return DR_ERROR;
	}
	
	//make sure "bytes mode" is set
	CLR_FLAG(req->flags, RRF_WIDE);
	
	req->data = data;
	req->actual = LEN_STR(data);
	
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Write_Clipboard(REBREQ *req)
/*
**		Works for Unicode and ASCII strings.
**		Length is number of bytes passed (not number of chars).
**
***********************************************************************/
{
	jstring jstr;
	
	req->actual = 0;
	
	if (jni_env == NULL) return DR_ERROR;
	
	if (GET_FLAG(req->flags, RRF_WIDE)) //unicode
		jstr = (*jni_env)->NewString(jni_env, (const jchar *)req->data, req->length / sizeof(jchar));
	else
		jstr = (*jni_env)->NewStringUTF(jni_env, req->data);
		
	(*jni_env)->CallVoidMethod(jni_env, jni_obj, jni_setClipboard, jstr);
	(*jni_env)->DeleteLocalRef(jni_env, jstr);
	
	req->actual = req->length;
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Poll_Clipboard(REBREQ *req)
/*
***********************************************************************/
{
	return DR_DONE;
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] =
{
	0,
	0,
	Open_Clipboard,
	Close_Clipboard,
	Read_Clipboard,
	Write_Clipboard,
	Poll_Clipboard,
};

DEFINE_DEV(Dev_Clipboard, "Clipboard", 1, Dev_Cmds, RDC_MAX, 0);
