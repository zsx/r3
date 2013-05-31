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
**	Title: Host environment main entry point
**	Note: OS independent
**  Author: Carl Sassenrath
**  Purpose:
**		Provides the outer environment that calls the REBOL lib.
**		This module is more or less just an example and includes
**		a very simple console prompt.
**
************************************************************************
**
**  WARNING to PROGRAMMERS:
**
**		This open source code is strictly managed to maintain
**		source consistency according to our standards, not yours.
**
**		1. Keep code clear and simple.
**		2. Document odd code, your reasoning, or gotchas.
**		3. Use our source style for code, indentation, comments, etc.
**		4. It must work on Win32, Linux, OS X, BSD, big/little endian.
**		5. Test your code really well before submitting it.
**
***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <asm/page.h>		//quick patch Android NDKr8c is missing this include
#include <pthread.h>
#include <sys/resource.h>
#include "host-jni.h"		// JNI support

#define OS_LIB_TABLE		// include the host-lib dispatch table

#include "reb-host.h"		// standard host include files
#include "host-lib.h"		// OS host library (dispatch table)

#ifdef CUSTOM_STARTUP
#include "host-init.h"
#endif 

/**********************************************************************/

#define PROMPT_STR ">> "
#define RESULT_STR "== "
#define R3_THREAD_STACK_SIZE ((((1024 * 1024 * 4) / PAGE_SIZE) + 1) * PAGE_SIZE)

REBARGS Main_Args;
REBYTE *CmdLine;
REBOOL IsR3Running = FALSE;
REBOOL IsR3Created = FALSE;

//threading stuff
pthread_mutex_t mutex;
pthread_cond_t input_cv;

#ifndef REB_CORE
extern void Init_Windows(void);
extern void OS_Init_Graphics(void);
#endif
 
extern void Init_Core_Ext(void);

// Host bare-bones stdio functs:
extern void Open_StdIO(void);
extern void Put_Str(char *buf);

void Host_Crash(REBYTE *reason) {
	OS_Crash("REBOL Host Failure", reason);
} 

pthread_t DoThread(void *fn, void *arg, REBOOL detach)
{
	int err;
	int rc;
	void *status;
//	JNIEnv *t_env;
	pthread_t tid;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setstacksize (&attr, R3_THREAD_STACK_SIZE);

	if (!detach) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

//	t_env = jni_env;

    err = pthread_create(&tid, &attr, fn, arg);
	
	if (err == 0)
	{
		//thread created successfully
		
		if (detach)
			pthread_detach(tid);
		else {
			rc = pthread_join(tid, &status);
			if (rc) {
				LOGE("Error during pthread_join(): %d", rc);
				return -1;
			}
		}

		pthread_attr_destroy(&attr);

	} else {
		 LOGE("Can't create R3 thread");
		return -1;
	}
	
//   	jni_env = t_env;
	return tid;
}

static void RebolThread()
{
	REBINT n;
	REBYTE vers[8];
	int status = 0;
	
	status = (*jni_vm)->AttachCurrentThread( jni_vm, &jni_env, NULL );
	
	if (status != 0) {
		LOGE("R3 thread: failed to attach current thread");
		jni_destroy();
		return;
	}

	pthread_mutex_lock (&mutex);
	LOGI("R3->lock  ENTER");

	LOGI("R3 thread attached");
	
	Host_Lib = &Host_Lib_Init;
	
	vers[0] = 5; // len
	RL_Version(&vers[0]);
	LOGI("opening stdio");
	// Must be done before an console I/O can occur. Does not use reb-lib,
	// so this device should open even if there are other problems.
	Open_StdIO();  // also sets up interrupt handler

	
	n = RL_Init(&Main_Args, Host_Lib);
	
	if (n == 0)
	{
		LOGI("loading core ext");
		Init_Core_Ext();

#ifndef REB_CORE
		Init_Windows();
		OS_Init_Graphics();
#endif

		LOGI("RL_Start() called");
#ifdef CUSTOM_STARTUP
		// For custom startup, you can provide compressed script code here:
		n = RL_Start((REBYTE *)(&Reb_Init_Code[0]), REB_INIT_SIZE, 0); // TRUE on halt
#else
		n = RL_Start(0, 0, 0);
#endif

		if (n < 0) {
			LOGE("RL_Start(): script halted or had error (%d)", n);
		} else LOGI("RL_Start(): %d", n);

		IsR3Running = TRUE;

		//send signal to rebolCreate() we are ready to enter console loop (will unlock mutex)
		LOGI("R3-> signal");
		pthread_cond_signal(&input_cv); 

		while (TRUE)
		{
			Put_Str(PROMPT_STR);
			LOGI("R3->unlock, wait");
			pthread_cond_wait(&input_cv, &mutex); //(will lock mutex when done)
			LOGI("R3->lock, awake");
			if (!IsR3Running) {
				pthread_mutex_unlock (&mutex);
				LOGI("R3->unlock");
				break;
			}
			LOGI("R3 prompt do: %s", (char *)CmdLine);
			Put_Str((char *)CmdLine);
			Put_Str("\n");
			
			RL_Do_String(CmdLine, 0, 0);
			RL_Print_TOS(0, RESULT_STR);
			
			//send signal to rebolDo() we are fnished (will unlock mutex)
			LOGI("R3-> signal");
			pthread_cond_signal(&input_cv);
		}
	}
	else {
		LOGE("RL_Init() failed: ", n);
//		RL_Print_TOS(0, RESULT_STR);
	}

	jni_destroy();
	
	(*jni_vm)->DetachCurrentThread(jni_vm);
	
	LOGI("R3 thread dettached");
	
	OS_Exit(0);
}

JNI_FUNC(void, MainActivity_rebolCreate, jstring str)
{
	jni_init(env, obj);

	if (IsR3Created) return;

	pthread_mutex_lock (&mutex);
	LOGI("RC->lock, ENTER");
	
	Parse_Args(0, 0, &Main_Args);
	
	if (str != NULL)
		Main_Args.script = (REBCHR *)(*env)->GetStringUTFChars( env, str , NULL );

	DoThread(RebolThread, NULL, TRUE);

	//wait for the R3 console thread to initialize(locks mutex when done)
	LOGI("RC->unlock, wait");
	pthread_cond_wait(&input_cv, &mutex);
	LOGI("RC->lock, awake");

	IsR3Created = TRUE;
	
	pthread_mutex_unlock (&mutex);
	LOGI("RC->unlock, EXIT");
}

JNI_FUNC(void, MainActivity_rebolDestroy)
{
	pthread_mutex_lock (&mutex);
	LOGI("DES->lock, ENTER");

	if (IsR3Running)
	{
		IsR3Running = FALSE; //kill the R3 console loop task
		LOGI("DES-> signal");
		pthread_cond_signal(&input_cv);
	}

	IsR3Created = FALSE;
	
	pthread_mutex_unlock (&mutex);
	LOGI("DES->unlock, EXIT");

	dlclose(); //force OS to unload lib (does it really work?)
}

JNI_FUNC(void, MainActivity_rebolDo, jstring str)
{
	pthread_mutex_lock (&mutex);
	LOGI("RD->lock, ENTER");

	if (IsR3Running)
	{
		CmdLine = (REBYTE *)(*env)->GetStringUTFChars(env, str , NULL);


		//send signal to rebolCreate() loop (will unlock mutex)
		LOGI("RD-> signal");
		pthread_cond_signal(&input_cv);

		//wait for the RebolThread() loop to execute R3 expression (locks mutex when done)
		LOGI("RD->unlock, wait");
		pthread_cond_wait(&input_cv, &mutex);
		LOGI("RD->lock, awake");

	}
	pthread_mutex_unlock (&mutex);
	LOGI("RD->unlock, EXIT");
}

JNI_FUNC(void, MainActivity_rebolEscape)
{
	RL_Escape(0);
}