//
//  File: %host-process.c
//  Summary: "POSIX Process API"
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
// This was originally the file host-lib.c, providing the entire
// host API.  When the host routines were broken into smaller
// pieces, it made sense that host-lib.c be kept as the largest
// set of related routines.  That turned out to be the process
// related routines and support for CALL.
//

#if !defined( __cplusplus) && defined(TO_LINUX)
    // See feature_test_macros(7)
    // This definition is redundant under C++
    #define _GNU_SOURCE  // Needed for pipe2 on Linux
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <assert.h>

#if !defined(NDEBUG)
    #include <stdio.h>
#endif

#include "reb-host.h"


/***********************************************************************
**
**  OS Library Functions
**
***********************************************************************/


//
//  OS_Get_PID: C
//
// Return the current process ID
//
REBINT OS_Get_PID()
{
    return getpid();
}

//
//  OS_Get_UID: C
//
// Return the real user ID
//
REBINT OS_Get_UID()
{
    return getuid();
}

//
//  OS_Set_UID: C
//
// Set the user ID, see setuid manual for its semantics
//
REBINT OS_Set_UID(REBINT uid)
{
    if (setuid(uid) < 0) {
        switch (errno) {
            case EINVAL:
                return OS_EINVAL;
            case EPERM:
                return OS_EPERM;
            default:
                return -errno;
        }
    } else {
        return 0;
    }
}

//
//  OS_Get_GID: C
//
// Return the real group ID
//
REBINT OS_Get_GID()
{
    return getgid();
}

//
//  OS_Set_GID: C
//
// Set the group ID, see setgid manual for its semantics
//
REBINT OS_Set_GID(REBINT gid)
{
    if (setgid(gid) < 0) {
        switch (errno) {
            case EINVAL:
                return OS_EINVAL;
            case EPERM:
                return OS_EPERM;
            default:
                return -errno;
        }
    } else {
        return 0;
    }
}

//
//  OS_Get_EUID: C
//
// Return the effective user ID
//
REBINT OS_Get_EUID()
{
    return geteuid();
}

//
//  OS_Set_EUID: C
//
// Set the effective user ID
//
REBINT OS_Set_EUID(REBINT uid)
{
    if (seteuid(uid) < 0) {
        switch (errno) {
            case EINVAL:
                return OS_EINVAL;
            case EPERM:
                return OS_EPERM;
            default:
                return -errno;
        }
    } else {
        return 0;
    }
}

//
//  OS_Get_EGID: C
//
// Return the effective group ID
//
REBINT OS_Get_EGID()
{
    return getegid();
}

//
//  OS_Set_EGID: C
//
// Set the effective group ID
//
REBINT OS_Set_EGID(REBINT gid)
{
    if (setegid(gid) < 0) {
        switch (errno) {
            case EINVAL:
                return OS_EINVAL;
            case EPERM:
                return OS_EPERM;
            default:
                return -errno;
        }
    } else {
        return 0;
    }
}

//
//  OS_Send_Signal: C
//
// Send signal to a process
//
REBINT OS_Send_Signal(REBINT pid, REBINT signal)
{
    if (kill(pid, signal) < 0) {
        switch (errno) {
            case EINVAL:
                return OS_EINVAL;
            case EPERM:
                return OS_EPERM;
            case ESRCH:
                return OS_ESRCH;
            default:
                return -errno;
        }
    } else {
        return 0;
    }
}

//
//  OS_Kill: C
//
// Try to kill the process
//
REBINT OS_Kill(REBINT pid)
{
    return OS_Send_Signal(pid, SIGTERM);
}


//
//  OS_Reap_Process: C
//
// pid:
//     > 0, a signle process
//     -1, any child process
//
// flags:
//     0: return immediately
//
// Return -1 on error
//
int OS_Reap_Process(int pid, int *status, int flags)
{
    return waitpid(pid, status, flags == 0? WNOHANG : 0);
}
