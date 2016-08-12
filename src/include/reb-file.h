//
//  File: %reb-file.h
//  Summary: "Special file device definitions"
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

// RFM - REBOL File Modes
enum {
    RFM_READ = 0,
    RFM_WRITE,
    RFM_APPEND,
    RFM_SEEK,
    RFM_NEW,
    RFM_READONLY,
    RFM_TRUNCATE,
    RFM_RESEEK,         // file index has moved, reseek
    RFM_NAME_MEM,       // converted name allocated in mem
    RFM_DIR = 16,
    RFM_MAX
};

// RFE - REBOL File Error
enum {
    RFE_BAD_PATH = 1,
    RFE_NO_MODES,       // No file modes specified
    RFE_OPEN_FAIL,      // File open failed
    RFE_BAD_SEEK,       // Seek not supported for this file
    RFE_NO_HANDLE,      // File struct has no handle
    RFE_NO_SEEK,        // Seek action failed
    RFE_BAD_READ,       // Read failed (general)
    RFE_BAD_WRITE,      // Write failed (general)
    RFE_DISK_FULL,      // No space on target volume
    RFE_MAX
};

#define MAX_FILE_NAME 1022
