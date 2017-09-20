//
//  File: %host-lib.c
//  Summary: {OS API function library called by REBOL interpreter}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This module is parsed for function declarations used to
// build prototypes, tables, and other definitions. To change
// function arguments requires a rebuild of the REBOL library.
//
// This module provides the functions that REBOL calls
// to interface to the native (host) operating system.
// REBOL accesses these functions through the structure
// defined in host-lib.h (auto-generated, do not modify).
//
// compile with -DUNICODE for Win32 wide char API
//
//=////////////////////////////////////////////////////////////////////////=//
//
// WARNING: The function declarations here cannot be modified without also
// modifying those found in the other OS host-lib files!  Do not even modify
// the argument names.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <assert.h>

#include "reb-host.h"

#ifndef REB_CORE
REBSER* Gob_To_Image(REBGOB *gob);
#endif

//used to detect non-modal OS dialogs
BOOL osDialogOpen = FALSE;


//
//  Convert_Date: C
//
// Convert local format of system time into standard date
// and time structure.
//
void Convert_Date(REBVAL *out, long zone, const SYSTEMTIME *stime)
{
    RL_Init_Date(
        out,
        stime->wYear, // year
        stime->wMonth, // month
        stime->wDay, // day
        stime->wHour * 3600 + stime->wMinute * 60 + stime->wSecond, // "time"
        1000000 * stime->wMilliseconds, // nano
        zone
    );
}


/***********************************************************************
**
**  OS Library Functions
**
***********************************************************************/


//
//  OS_Config: C
//
// Return a specific runtime configuration parameter.
//
REBINT OS_Config(int id, REBYTE *result)
{
    UNUSED(result);

#define OCID_STACK_SIZE 1  // needs to move to .h file

    switch (id) {
    case OCID_STACK_SIZE:
        return 0;  // (size in bytes should be returned here)
    }

    return 0;
}


//
//  OS_Exit: C
//
// Called in cases where REBOL needs to quit immediately
// without returning from the main() function.
//
void OS_Exit(int code)
{
    //OS_Call_Device(RDI_STDIO, RDC_CLOSE); // close echo
    OS_Quit_Devices(0);
#ifndef REB_CORE
    OS_Destroy_Graphics();
#endif
    exit(code);
}


//
//  OS_Crash: C
//
// Tell user that REBOL has crashed. This function must use
// the most obvious and reliable method of displaying the
// crash message.
//
// If the title is NULL, then REBOL is running in a server mode.
// In that case, we do not want the crash message to appear on
// the screen, because the system may be unattended.
//
// On some systems, the error may be recorded in the system log.
//
void OS_Crash(const REBYTE *title, const REBYTE *content)
{
    // Echo crash message if echo file is open:
    ///PUTE(content);
    OS_Call_Device(RDI_STDIO, RDC_CLOSE); // close echo

    // A title tells us we should alert the user:
    if (title) {
    //  OS_Put_Str(title);
    //  OS_Put_Str(":\n");
        // Use ASCII only
        MessageBoxA(NULL, cs_cast(content), cs_cast(title), MB_ICONHAND);
    }
    //  OS_Put_Str(content);
    exit(100);
}


//
//  OS_Form_Error: C
//
// Translate OS error into a string. The str is the string
// buffer and the len is the length of the buffer.
//
REBCHR *OS_Form_Error(int errnum, REBCHR *str, int len)
{
    wchar_t *lpMsgBuf;
    int ok;

    if (!errnum) errnum = GetLastError();

    // !!! Why does this allocate a buffer when FormatMessage takes a
    // buffer and a size...exactly the interface we're implementing?
    ok = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errnum,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            cast(wchar_t*, &lpMsgBuf), // see FORMAT_MESSAGE_ALLOCATE_BUFFER
            0,
            NULL);

    len--; // termination

    if (!ok) wcsncpy(str, L"unknown error", len);
    else {
        wcsncpy(str, lpMsgBuf, len);
        LocalFree(lpMsgBuf);
    }
    return str;
}


//
//  OS_Get_Env: C
//
// Get a value from the environment.
// Returns size of retrieved value for success or zero if missing.
//
// If return size is greater than capacity then value contents
// are undefined, and size includes null terminator of needed buf
//
REBINT OS_Get_Env(REBCHR* buffer, const REBCHR *key, REBINT capacity)
{
    // Note: The Windows variant of this API is NOT case-sensitive

    REBINT result = GetEnvironmentVariable(key, buffer, capacity);
    if (result == 0) { // some failure...
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            return -1; // not found
        }
        return -2; // other error... fail?
    }
    return result;
}


//
//  OS_Set_Env: C
//
// Set a value from the environment.
// Returns >0 for success and 0 for errors.
//
REBOOL OS_Set_Env(REBCHR *envname, REBCHR *envval)
{
    return SetEnvironmentVariable(envname, envval);
}


//
//  OS_List_Env: C
//
REBCHR *OS_List_Env(void)
{
    wchar_t *env = GetEnvironmentStrings();
    REBCNT n, len = 0;
    wchar_t *str;

    str = env;
    while ((n = wcslen(str))) {
        len += n + 1;
        str = env + len; // next
    }
    len++;

    str = OS_ALLOC_N(wchar_t, len);
    memmove(str, env, len * sizeof(wchar_t));

    FreeEnvironmentStrings(env);

    return str;
}


//
//  OS_Get_Time: C
//
// Get the current system date/time in UTC plus zone offset (mins).
//
void OS_Get_Time(REBVAL *out)
{
    SYSTEMTIME stime;
    TIME_ZONE_INFORMATION tzone;

    GetSystemTime(&stime);

    if (TIME_ZONE_ID_DAYLIGHT == GetTimeZoneInformation(&tzone))
        tzone.Bias += tzone.DaylightBias;

    Convert_Date(out, -tzone.Bias, &stime);
}


//
//  OS_Delta_Time: C
//
// Return time difference in microseconds. If base = 0, then
// return the counter. If base != 0, compute the time difference.
//
// Note: Requires high performance timer.
//      Q: If not found, use timeGetTime() instead ?!
//
i64 OS_Delta_Time(i64 base)
{
    LARGE_INTEGER time;
    if (!QueryPerformanceCounter(&time))
        OS_Crash(cb_cast("Missing resource"), cb_cast("High performance timer"));

    if (base == 0) return time.QuadPart; // counter (may not be time)

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    return ((time.QuadPart - base) * 1000) / (freq.QuadPart / 1000);
}


//
//  OS_Get_Current_Dir: C
//
// Return the current directory path as a string and
// its length in chars (not bytes).
//
// The result should be freed after copy/conversion.
//
int OS_Get_Current_Dir(REBCHR **path)
{
    int len;

    len = GetCurrentDirectory(0, NULL); // length, incl terminator.
    *path = OS_ALLOC_N(wchar_t, len);
    GetCurrentDirectory(len, *path);
    len--; // less terminator

    return len;
}


//
//  OS_Set_Current_Dir: C
//
// Set the current directory to local path. Return FALSE
// on failure.
//
REBOOL OS_Set_Current_Dir(REBCHR *path)
{
    return SetCurrentDirectory(path);
}


//
//  OS_File_Time: C
//
// Convert file.time to REBOL date/time format.
// Time zone is UTC.
//
void OS_File_Time(REBVAL *out, struct devreq_file *file)
{
    SYSTEMTIME stime;
    TIME_ZONE_INFORMATION tzone;

    if (TIME_ZONE_ID_DAYLIGHT == GetTimeZoneInformation(&tzone))
        tzone.Bias += tzone.DaylightBias;

    FileTimeToSystemTime(cast(FILETIME *, &file->time), &stime);
    Convert_Date(out, -tzone.Bias, &stime);
}


//
//  OS_Open_Library: C
//
// Load a DLL library and return the handle to it.
// If zero is returned, error indicates the reason.
//
void *OS_Open_Library(const REBCHR *path, REBCNT *error)
{
    void *dll = LoadLibraryW(path);
    *error = GetLastError();

    return dll;
}


//
//  OS_Close_Library: C
//
// Free a DLL library opened earlier.
//
void OS_Close_Library(void *dll)
{
    FreeLibrary((HINSTANCE)dll);
}


//
//  OS_Find_Function: C
//
// Get a DLL function address from its string name.
//
CFUNC *OS_Find_Function(void *dll, const char *funcname)
{
    // !!! See notes about data pointers vs. function pointers in the
    // definition of CFUNC.  This is trying to stay on the right side
    // of the specification, but OS APIs often are not standard C.  So
    // this implementation is not guaranteed to work, just to suppress
    // compiler warnings.  See:
    //
    //      http://stackoverflow.com/a/1096349/211160

    FARPROC fp = GetProcAddress((HMODULE)dll, funcname);

    //DWORD err = GetLastError();

    return cast(CFUNC*, fp);
}


//
//  OS_Reap_Process: C
//
// pid:
//      > 0, a single process
//      -1, any child process
// flags:
//      0: return immediately
//
//      Return -1 on error
//
int OS_Reap_Process(int pid, int *status, int flags)
{
    UNUSED(pid);
    UNUSED(status);
    UNUSED(flags);

    // !!! It seems that process doesn't need to be reaped on Windows
    return 0;
}


//
//  OS_Request_File: C
//
REBOOL OS_Request_File(REBRFR *fr)
{
    OPENFILENAME ofn;
    BOOL ret;
    //int err;
    const wchar_t *filters = L"All files\0*.*\0REBOL scripts\0*.r\0Text files\0*.txt\0";

    memset(&ofn, '\0', sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);

    // ofn.hwndOwner = WIN_WIN(win); // Must find a way to set this

    ofn.lpstrTitle = fr->title;
    ofn.lpstrInitialDir = fr->dir;
    ofn.lpstrFile = fr->files;
    ofn.lpstrFilter = fr->filter ? fr->filter : filters;
    ofn.nMaxFile = fr->len;
    ofn.lpstrFileTitle = 0;
    ofn.nMaxFileTitle = 0;

    ofn.Flags = OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR; //|OFN_NONETWORKBUTTON; //;

    if (GET_FLAG(fr->flags, FRF_MULTI)) ofn.Flags |= OFN_ALLOWMULTISELECT;

    osDialogOpen = TRUE;

    if (GET_FLAG(fr->flags, FRF_SAVE))
        ret = GetSaveFileName(&ofn);
    else
        ret = GetOpenFileName(&ofn);

    osDialogOpen = FALSE;

    //if (!ret)
    //  err = CommDlgExtendedError(); // CDERR_FINDRESFAILURE

    return ret;
}

int CALLBACK ReqDirCallbackProc( HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
{
    UNUSED(lParam);

    static REBOOL inited = FALSE;
    switch (uMsg) {
        case BFFM_INITIALIZED:
            if (lpData) SendMessage(hWnd,BFFM_SETSELECTION,TRUE,lpData);
            SetForegroundWindow(hWnd);
            inited = TRUE;
            break;
        case BFFM_SELCHANGED:
            if (inited && lpData) {
                SendMessage(hWnd,BFFM_SETSELECTION,TRUE,lpData);
                inited = FALSE;
            }
            break;
    }
    return 0;
}


//
//  OS_Request_Dir: C
//
// WARNING: TEMPORARY implementation! Used only by host-core.c
// Will be most probably changed in future.
//
REBOOL OS_Request_Dir(REBCHR* title, REBCHR** folder, REBCHR* path)
{
    BROWSEINFO bi;
    wchar_t buffer[MAX_PATH];
    LPCITEMIDLIST pFolder;
    ZeroMemory(buffer, MAX_PATH);
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = NULL;
    bi.pszDisplayName = buffer;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS | BIF_SHAREABLE;
    bi.lpfn = ReqDirCallbackProc;
    bi.lParam = (LPARAM)path;

    osDialogOpen = TRUE;
    pFolder = SHBrowseForFolder(&bi);
    osDialogOpen = FALSE;
    if (pFolder == NULL) return FALSE;
    if (!SHGetPathFromIDList(pFolder, buffer) ) return FALSE;
    wcscpy(*folder, buffer);
    return TRUE;
}

//
//  OS_GOB_To_Image: C
//
// Render a GOB into an image. Returns an image or zero if
// it cannot be done.
//
REBVAL *OS_GOB_To_Image(REBGOB *gob)
{
#if (defined REB_CORE)
    UNUSED(gob);
    return 0;
#else
    return Gob_To_Image(gob);
#endif
}

//
//  OS_Get_Current_Exec: C
//
// Return the current executable path as a string and
// its length in chars (not bytes).
//
// The result should be freed after copy/conversion.
//
int OS_Get_Current_Exec(REBCHR **path)
{
    DWORD r = 0;
    *path = NULL;
    *path = OS_ALLOC_N(REBCHR, MAX_PATH);
    if (*path == NULL) return -1;

    r = GetModuleFileName(NULL, *path, MAX_PATH);
    if (r == 0) {
        OS_FREE(*path);
        return -1;
    }
    (*path)[r] = '\0'; //It might not be NULL-terminated if buffer is not big enough

    return r;
}
