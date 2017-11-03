//
//  File: %host-config.c
//  Summary: "POSIX Host Configuration Routines"
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
// This file is for situations where there is some kind of
// configuration information (e.g. environment variables, boot
// paths) that Rebol wants to get at from the host.
//

#include <stdlib.h>
#include <string.h>

#include "reb-host.h"



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
