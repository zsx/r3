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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Should include <mach-o/dyld.h>, but it conflicts with reb-c.h because both defined TRUE and FALSE
#ifdef __cplusplus
extern "C"
#endif
int _NSGetExecutablePath(char* buf, uint32_t* bufsize);

#include "reb-host.h"


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
    assert (sizeof(REBCHR) == sizeof(char));

    uint32_t path_size = 1024;

    *path = OS_ALLOC_N(REBCHR, path_size);
    if (*path == NULL) return -1;
    int r = _NSGetExecutablePath(*path, &path_size);
    if (r == -1) {
        // buffer is too small, path_size is set to the required size
        assert(path_size > 1024);

        OS_FREE(*path);
        *path = OS_ALLOC_N(REBCHR, path_size);
        if (*path == NULL) return -1;
        int r = _NSGetExecutablePath(*path, &path_size);
        if (r != 0) {
            OS_FREE(*path);
            return -1;
        }
    }

    // _NSGetExecutablePath returns "a path" not a "real path", and it could be
    // a symbolic link.
    REBCHR *resolved_path = realpath(*path, NULL);
    if (resolved_path != NULL) {
        // resolved_path needs to be free'd by free, which might be different from OS_FREE,
        // make a copy using memory from OS_ALLOC_N, such that the caller can call OS_FREE.
        OS_FREE(*path);
        REBCNT len = OS_STRLEN(resolved_path);
        *path = OS_ALLOC_N(REBCHR, len + 1);
        OS_STRNCPY(*path, resolved_path, len);
        (*path)[len] = '\0';

        free(resolved_path);

        return len;
    } else {
        // Failed to resolve, just return the unresolved path.
        return OS_STRLEN(*path);
    }
}
