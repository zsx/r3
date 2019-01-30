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

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#include <windows.h>
#else
#include <unistd.h> // for sleep
#endif

#define OS_LIB_TABLE		// include the host-lib dispatch table

#include "reb-host.h"		// standard host include files
#include "host-lib.h"		// OS host library (dispatch table)

#ifdef CUSTOM_STARTUP
#include "host-init.h"
#endif

#include "SDL.h"

#ifdef TO_ANDROID
#include <android/log.h>
#define  LOG_TAG    "r3"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#else
#define  LOGD(...)  RL_Print(__VA_ARGS__)
#endif


/**********************************************************************/

#define PROMPT_STR ">> "
#define RESULT_STR "== "

REBARGS Main_Args;

#ifdef TO_WIN32
HINSTANCE App_Instance = 0;
#endif

/* for memory allocation trouble shooting */
unsigned int always_malloc = 0;

#ifndef REB_CORE
extern void Init_Windows(void);
extern void OS_Init_Graphics(void);
extern void OS_Destroy_Graphics(void);
#endif

extern void Init_Core_Ext(void);

//#define TEST_EXTENSIONS
#ifdef TEST_EXTENSIONS
extern void Init_Ext_Test(void);	// see: host-ext-test.c
#endif

// Host bare-bones stdio functs:
extern void Open_StdIO(void);
extern void Put_Str(char *buf);
extern REBYTE *Get_Str();

static int wait_for_debugger = 0;

/* coverity[+kill] */
void Host_Crash(REBYTE *reason) {
	OS_Crash("REBOL Host Failure", reason);
}


extern void rs_draw_enable_trace(int);

/***********************************************************************
**
**  MAIN ENTRY POINT
**
**	Win32 args:
**		inst:  current instance of the application (app handle)
**		prior: always NULL (use a mutex for single inst of app)
**		cmd:   command line string (or use GetCommandLine)
**	    show:  how app window is to be shown (e.g. maximize, minimize, etc.)
**
**	Win32 return:
**		If the function succeeds, terminating when it receives a WM_QUIT
**		message, it should return the exit value contained in that
**		message's wParam parameter. If the function terminates before
**		entering the message loop, it should return zero.
**
**  Posix args: as you would expect in C.
**  Posix return: ditto.
**
***********************************************************************/

#ifdef TO_WIN32
// int WINAPI WinMain(HINSTANCE inst, HINSTANCE prior, LPSTR cmd, int show)
int main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{

	REBYTE vers[8];
	REBYTE *line;
	REBINT n;
	const char *env_always_malloc = NULL;
	REBYTE *embedded_script = NULL;
	REBI64 embedded_size = 0;

#ifdef TO_WIN32  // In Win32 get args manually:
	// Fetch the win32 unicoded program arguments:
	argv = (char **)CommandLineToArgvW(GetCommandLineW(), &argc);
#else
	while (wait_for_debugger) {
		sleep(1);
	}

#endif

	Host_Lib = &Host_Lib_Init;

	env_always_malloc = getenv("R3_ALWAYS_MALLOC");
	if (env_always_malloc != NULL) {
		always_malloc = atoi(env_always_malloc);
	}

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    }

	const char env_log_level = getenv("R3_SDL_LOG_LEVEL");
	if (env_log_level != NULL) {
		SDL_LogSetAllPriority(atoi(env_log_level));
	} else {
		SDL_LogSetAllPriority(SDL_LOG_PRIORITY_CRITICAL);
	}

    const char *rs_trace = getenv("R3_SKIA_TRACE");
    if (rs_trace != NULL && atoi(rs_trace)) {
        rs_draw_enable_trace(TRUE);
    }

#ifdef TO_ANDROID
	SDL_RWops *script = SDL_RWFromFile("main.reb", "r");
	if (script != NULL) {
		embedded_size = SDL_RWsize(script);
		if (embedded_size > 0) {
			embedded_script = OS_Make(embedded_size);
			if (SDL_RWread(script, embedded_script, embedded_size, 1) < 0) {
				LOGD("failed to read the embedded script data: %s\n", SDL_GetError());
				OS_Free(embedded_script);
				embedded_size = 0;
			}
		}
	} else {
		LOGD("failed to open the embedded script: %s\n", SDL_GetError());
	}
#else
	embedded_script = OS_Read_Embedded(&embedded_size);
#endif
	//LOGD("embedded_size: %lld\n", embedded_size);
	Parse_Args(argc, (REBCHR **)argv, &Main_Args);

	vers[0] = 5; // len
	RL_Version(&vers[0]);

	// Must be done before an console I/O can occur. Does not use reb-lib,
	// so this device should open even if there are other problems.
	Open_StdIO();  // also sets up interrupt handler

	// Initialize the REBOL library (reb-lib):
	//if (!CHECK_STRUCT_ALIGN) Host_Crash("Incompatible struct alignment");
	if (!Host_Lib) Host_Crash("Missing host lib");
	// !!! Second part will become vers[2] < RL_REV on release!!!
	if (vers[1] != RL_VER || vers[2] != RL_REV) Host_Crash("Incompatible reb-lib DLL");
	n = RL_Init(&Main_Args, Host_Lib);
	if (n == 1) Host_Crash("Host-lib wrong size");
	if (n == 2) Host_Crash("Host-lib wrong version/checksum");

	//Initialize core extension commands
	//LOGD("Initializing core ext\n");
	Init_Core_Ext();
	//LOGD("core ext Initialized\n");
#ifdef EXT_LICENSING
	Init_Licensing_Ext();
#endif //EXT_LICENSING

#ifdef TEST_EXTENSIONS
	Init_Ext_Test();
#endif

#ifdef TO_WIN32
	// no console, we must be the child process
	if (GetStdHandle(STD_OUTPUT_HANDLE) == 0)
	{
		App_Instance = GetModuleHandle(NULL);
	}
#ifdef REB_CORE	
	else //use always the console for R3/core
	{
		// GetWindowsLongPtr support 32 & 64 bit windows
		App_Instance = (HINSTANCE)GetWindowLongPtr(GetConsoleWindow(), GWLP_HINSTANCE);
	}
#else
	//followinng R3/view code behaviors when compiled as:
	//-"console app" mode: stdio redirection works but blinking console window during start
	//-"GUI app" mode stdio redirection doesn't work properly, no blinking console window during start
	else if (argc > 1) // we have command line args
	{
		// GetWindowsLongPtr support 32 & 64 bit windows
		App_Instance = (HINSTANCE)GetWindowLongPtr(GetConsoleWindow(), GWLP_HINSTANCE);
	}
	else // no command line args but a console - launch child process so GUI is initialized and exit
	{
		DWORD dwCreationFlags = CREATE_DEFAULT_ERROR_MODE | DETACHED_PROCESS;
		STARTUPINFO startinfo;
		PROCESS_INFORMATION procinfo;
		ZeroMemory(&startinfo, sizeof(startinfo));
		startinfo.cb = sizeof(startinfo);
		if (!CreateProcess(NULL, (LPTSTR)argv[0], NULL, NULL, FALSE, dwCreationFlags, NULL, NULL, &startinfo, &procinfo))
			MessageBox(0, L"CreateProcess() failed :(", L"", 0);
		exit(0);
	}
#endif //REB_CORE	
#endif //TO_WIN32

	// Common code for console & GUI version
#ifndef REB_CORE
	Init_Windows();
	OS_Init_Graphics();
#endif // REB_CORE

#ifdef TO_WIN32
#ifdef ENCAP
	Console_Output(FALSE);
#else
	if (Main_Args.script) Console_Output(FALSE);
#endif // ENCAP
#endif // TO_WIN32

	// Call sys/start function. If a compressed script is provided, it will be
	// decompressed, stored in system/options/boot-host, loaded, and evaluated.
	// Returns: 0: ok, -1: error, 1: bad data.
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Starting up...\n");
#ifdef CUSTOM_STARTUP
	// For custom startup, you can provide compressed script code here:
	n = RL_Start((REBYTE *)(&Reb_Init_Code[0]), REB_INIT_SIZE, embedded_script, (REBINT)embedded_size, 0); // TRUE on halt
#else
	n = RL_Start(0, 0, embedded_script, (REBINT)embedded_size, 0);
#endif
	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Started up...\n");

#ifdef TO_WIN32
#ifdef ENCAP
	Console_Output(TRUE);
#else
	if (Main_Args.script) Console_Output(TRUE);
#endif // TO_WIN32
#endif // ENCAP

#ifndef ENCAP
	// Console line input loop (just an example, can be improved):
	if (
		!(Main_Args.options & RO_CGI)
		&& (
			!Main_Args.script // no script was provided
			|| n  < 0         // script halted or had error
			|| Main_Args.options & RO_HALT  // --halt option
		)
	){
		n = 0;  // reset error code (but should be able to set it below too!)
		while (TRUE) {
			Put_Str(PROMPT_STR);
			if ((line = Get_Str())) {
				RL_Do_String(line, 0, 0);
				RL_Print_TOS(0, RESULT_STR);
				OS_Free(line);
			}
			else break; // EOS
		}
	}
#endif //!ENCAP
	OS_Quit_Devices(0);
#ifndef REB_CORE	
	OS_Destroy_Graphics();
#endif

	// A QUIT does not exit this way, so the only valid return code is zero.
	return 0;
}

