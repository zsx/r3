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
 
#include <jni.h>
#include <android/log.h> 

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "R3Droid", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "R3Droid", __VA_ARGS__)) 

#define JAVA_PATH com_saphirion_rebolapp12345678

#define JNI_FUNC_TMP2(r, n, p, ...) JNIEXPORT r JNICALL Java_##p##_##n(JNIEnv * env, jobject  obj, ##__VA_ARGS__)
#define JNI_FUNC_TMP(r, n, p, ...) JNI_FUNC_TMP2(r, n, p , ##__VA_ARGS__)
#define JNI_FUNC(r, n, ...) JNI_FUNC_TMP(r, n, JAVA_PATH , ##__VA_ARGS__)

extern JNIEnv *jni_env;
extern jobject jni_obj;
extern jclass jni_class;
extern JavaVM *jni_vm;

extern void jni_init(JNIEnv * env, jobject  obj);
extern void jni_destroy();

extern jmethodID jni_browseURL;
extern jmethodID jni_putOutput;
extern jmethodID jni_getInput;
extern jmethodID jni_getClipboard;
extern jmethodID jni_setClipboard;
extern jmethodID jni_getSystemMetric;

extern jmethodID jni_getWindowGob;
extern jmethodID jni_updateWindow;
extern jmethodID jni_windowToFront;
extern jmethodID jni_createWindow;
extern jmethodID jni_destroyWindow;

extern jmethodID jni_getWindowBuffer;
extern jmethodID jni_blitWindow;
extern jmethodID jni_drawColor;

extern jmethodID jni_setWinRegion;
extern jmethodID jni_resetWindowClip;
extern jmethodID jni_intersectWindowClip;
extern jmethodID jni_setWindowClip;
extern jmethodID jni_getWindowClip;
extern jmethodID jni_setOldRegion;
extern jmethodID jni_setNewRegion;
extern jmethodID jni_combineRegions;

extern jmethodID jni_pollEvents;

extern jmethodID jni_showSoftKeyboard;