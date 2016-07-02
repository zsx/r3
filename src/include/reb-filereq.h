//
//  File: %reb-filereq.h
//  Summary: "File requestor definitions"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
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

#define MAX_FILE_REQ_BUF (16*1024)

typedef struct Reb_File_Requestor {
    REBCNT flags;   // multi, load/save, unicode
    REBCHR *title;  // title of requestor
    REBCHR *button; // button name
    REBCHR *dir;    // dir path
    REBCHR *files;  // buffer to hold results
    REBCHR *filter; // buffer to hold results
    REBINT len;     // length of buffer
} REBRFR;

// File Request Flags:
enum {
    FRF_MULTI,
    FRF_SAVE,
    FRF_KEEP,
    FRF_MAX
};

