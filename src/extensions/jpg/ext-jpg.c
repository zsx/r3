//
//  File: %ext-jpg.c
//  Summary: "JPG codec"
//  Section: Extension
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
// ==================================================================
//
#include "sys-core.h"
#include "sys-ext.h"

static const char script_bytes[] =
"REBOL ["
    "Title: \"JPG Codec Extension\"\n"
    "name: 'JPG\n"
    "type: 'Extension\n"
    "version: 1.0.0\n"
    "license: {Apache 2.0}\n"
"]\n"
"sys/register-codec* 'jpeg [%.jpg %jpeg]\n"
    "get in import 'jpg 'identify-jpeg?\n"
    "get in import 'jpg 'decode-jpeg\n"
    "_" // currently no JPG encoder
;

#define MODULE_INCLUDE_DECLARATION_ONLY
#include "tmp-mod-jpg-last.h"

DEFINE_EXT_INIT(JPG, //name of the extension
    script_bytes, // REBOL script for the extension in the source form
    {
        // init all modules in this extension
        int init = CALL_MODULE_INIT(JPG);
        if (init < 0) return init;
    }
)

DEFINE_EXT_QUIT(JPG,
{
    return CALL_MODULE_QUIT(JPG);
}
)

