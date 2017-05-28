//
//  File: %host-exec-path.c
//  Summary: "Executable Path"
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
//

#ifndef __cplusplus
    // See feature_test_macros(7)
    // This definition is redundant under C++
    #define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#if defined(HAVE_PROC_PATHNAME)
#include <sys/sysctl.h>
#endif

#include "reb-host.h"

#ifndef PATH_MAX
#define PATH_MAX 4096  // generally lacking in Posix
#endif

//
//  OS_Get_Current_Exec: C
//
// Return the current executable path as a string and
// its length in chars (not bytes).
//
// The result should be freed after copy/conversion.
//
// See
// https://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe 
//
int OS_Get_Current_Exec(REBCHR **path)
{
    assert(sizeof(REBCHR) == sizeof(char));

#if !defined(PROC_EXEC_PATH) && !defined(HAVE_PROC_PATHNAME)
    UNUSED(path);
    return -1;
#else
#if defined(PROC_EXEC_PATH)
    const char *self = PROC_EXEC_PATH;
#else //HAVE_PROC_PATHNAME
    int mib[4] = {
        CTL_KERN,
        KERN_PROC,
        KERN_PROC_PATHNAME,
        -1 //current process
    };
    char *self = OS_ALLOC_N(REBCHR, PATH_MAX + 1);
    size_t len = PATH_MAX + 1;
    if (sysctl(mib, sizeof(mib), self, &len, NULL, 0) != 0) {
        OS_FREE(self);
        return -1;
    }
#endif

    *path = NULL;
    int r = 0;
    *path = OS_ALLOC_N(REBCHR, PATH_MAX);
    if (*path == NULL) return -1;

    r = readlink(self, *path, PATH_MAX);

#if defined(HAVE_PROC_PATHNAME)
    OS_FREE(self);
#endif

    if (r < 0) {
        OS_FREE(*path);
        return -1;
    }
    (*path)[r] = '\0';

    return r;
#endif
}
