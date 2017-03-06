//
//  File: %host-gob.c
//  Summary: "GOB Hostkit Facilities"
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
// @HostileFork doesn't particularly like the way GOB! is done,
// and feels it's an instance of a more general need for external
// types that participate in Rebol's type system and garbage
// collector.  For now these routines are kept together here.
//

#ifndef __cplusplus
    // See feature_test_macros(7)
    // This definition is redundant under C++
    #define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "reb-host.h"


#ifndef REB_CORE
REBSER* Gob_To_Image(REBGOB *gob);
#endif

//
//  OS_GOB_To_Image: C
//
// Render a GOB into an image. Returns an image or zero if
// it cannot be done.
//
REBVAL *OS_GOB_To_Image(REBGOB *gob)
{
#if (defined REB_CORE)
    return 0;
#else
    return Gob_To_Image(gob);
#endif
}
