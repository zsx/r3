//
//  File: %host-error.c
//  Summary: "POSIX Exit and Error Functions"
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
// ...
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_EXECINFO_AVAILABLE
    #include <execinfo.h>
    #include <unistd.h>  // STDERR_FILENO
#endif

#include "reb-host.h"


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
// coverity[+kill]
//
void OS_Crash(const REBYTE *title, const REBYTE *content)
{
    // !!! This said "close echo", but file echoing is no longer in core.
    // Is it still needed?
    //
    OS_Call_Device(RDI_STDIO, RDC_CLOSE);

    // A title tells us we should alert the user:
    if (title) {
        fputs(cs_cast(title), stderr);
        fputs(":\n", stderr);
    }
    fputs(cs_cast(content), stderr);
    fputs("\n\n", stderr);

#ifdef HAVE_EXECINFO_AVAILABLE  // backtrace is a GNU extension.
    {
        void *backtrace_buf[1024];
        int n_backtrace = backtrace(backtrace_buf, sizeof(backtrace_buf)/sizeof(backtrace_buf[0]));
        fputs("Backtrace:\n", stderr);
        backtrace_symbols_fd(backtrace_buf, n_backtrace, STDERR_FILENO);
    }
#endif

    exit(EXIT_FAILURE);
}
