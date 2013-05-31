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
**  Summary: Java Native Interface support for Android port
**  Module:  host-jni.h
**  Author:  Richard Smolak
**  Note: OS dependent
**
***********************************************************************/
#include <pthread.h>
#include "host-jni.h"

//externs
extern pthread_mutex_t mutex;
extern pthread_cond_t input_cv;


//JNI globals
JNIEnv *jni_env;
jobject jni_obj;
jclass jni_class;
JavaVM *jni_vm;

//exposed Java functions
jmethodID jni_browseURL;
jmethodID jni_putOutput;
jmethodID jni_getInput;
jmethodID jni_getClipboard;
jmethodID jni_setClipboard;
jmethodID jni_getSystemMetric;

jmethodID jni_getWindowGob;
jmethodID jni_updateWindow;
jmethodID jni_windowToFront;
jmethodID jni_createWindow;
jmethodID jni_destroyWindow;

jmethodID jni_getWindowBuffer;
jmethodID jni_blitWindow;
jmethodID jni_drawColor;

jmethodID jni_setWinRegion;
jmethodID jni_resetWindowClip;
jmethodID jni_intersectWindowClip;
jmethodID jni_setWindowClip;
jmethodID jni_getWindowClip;
jmethodID jni_setOldRegion;
jmethodID jni_setNewRegion;
jmethodID jni_combineRegions;

jmethodID jni_pollEvents;

jmethodID jni_showSoftKeyboard;


void jni_init(JNIEnv * env, jobject  obj)
{
	jni_env = env;
	jni_obj = (*env)->NewGlobalRef(env, obj);
	jni_class = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, obj));
	
	jni_browseURL = (*jni_env)->GetMethodID(jni_env, jni_class, "browseURL", "(Ljava/lang/String;)V");
	jni_putOutput = (*jni_env)->GetMethodID(jni_env, jni_class, "putOutput", "(Ljava/lang/String;)V");
	jni_getInput = (*jni_env)->GetMethodID(jni_env, jni_class, "getInput", "()Ljava/lang/String;");
	jni_getClipboard = (*jni_env)->GetMethodID(jni_env, jni_class, "getClipboard", "()Ljava/lang/String;");
	jni_setClipboard = (*jni_env)->GetMethodID(jni_env, jni_class, "setClipboard", "(Ljava/lang/String;)V");	
	jni_getSystemMetric = (*jni_env)->GetMethodID(jni_env, jni_class, "getSystemMetric", "(I)F");	
	jni_getWindowGob = (*jni_env)->GetMethodID(jni_env, jni_class, "getWindowGob", "(I)I");
	jni_updateWindow = (*jni_env)->GetMethodID(jni_env, jni_class, "updateWindow", "(IIIII)V");	
	jni_windowToFront = (*jni_env)->GetMethodID(jni_env, jni_class, "windowToFront", "(I)V");	
	jni_createWindow = (*jni_env)->GetMethodID(jni_env, jni_class, "createWindow", "(IIIIIZ)I");	
	jni_destroyWindow = (*jni_env)->GetMethodID(jni_env, jni_class, "destroyWindow", "(I)V");
	
	jni_getWindowBuffer = (*jni_env)->GetMethodID(jni_env, jni_class, "getWindowBuffer", "(I)Landroid/graphics/Bitmap;");
	jni_blitWindow = (*jni_env)->GetMethodID(jni_env, jni_class, "blitWindow", "(I)V");
	jni_drawColor = (*jni_env)->GetMethodID(jni_env, jni_class, "drawColor", "(II)V");
	jni_setWinRegion = (*jni_env)->GetMethodID(jni_env, jni_class, "setWinRegion", "(IIIII)V");
	jni_setOldRegion = (*jni_env)->GetMethodID(jni_env, jni_class, "setOldRegion", "(IIIII)V");
//	jni_setNewRegion = (*jni_env)->GetMethodID(jni_env, jni_class, "setNewRegion", "(IIIII)V");
	jni_setNewRegion = (*jni_env)->GetMethodID(jni_env, jni_class, "setNewRegion", "(IIIII)Z");
	jni_resetWindowClip = (*jni_env)->GetMethodID(jni_env, jni_class, "resetWindowClip", "(IIIII)V");
	jni_intersectWindowClip = (*jni_env)->GetMethodID(jni_env, jni_class, "intersectWindowClip", "(IIIII)[I");
	jni_setWindowClip = (*jni_env)->GetMethodID(jni_env, jni_class, "setWindowClip", "(II)Z");
	jni_getWindowClip = (*jni_env)->GetMethodID(jni_env, jni_class, "getWindowClip", "(I)[I");
	jni_combineRegions = (*jni_env)->GetMethodID(jni_env, jni_class, "combineRegions", "(I)V");
	
	jni_pollEvents = (*jni_env)->GetMethodID(jni_env, jni_class, "pollEvents", "()Z");
	
	jni_showSoftKeyboard = (*jni_env)->GetMethodID(jni_env, jni_class, "showSoftKeyboard", "(III)V");
}

void jni_destroy()
{
	(*jni_env)->DeleteGlobalRef(jni_env, jni_obj);
	(*jni_env)->DeleteGlobalRef(jni_env, jni_class);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) 
{
	LOGI("JNI OnLoad called!");
	//store java virtual machine reference
    jni_vm = vm; 
	
	//initialize thread related structures
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&input_cv, NULL);
	
    return JNI_VERSION_1_6; 
} 

/*
JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) 
{
	LOGI("ONUNLOAD called!");
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&input_cv);
}
*/
