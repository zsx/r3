/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************/

#include <assert.h>
#include <time.h>
#include <stdio.h>

#include <signal.h>

#define REN_C_STDIO_OK
#include "sys-core.h"

static FILE* pfile;

#pragma pack (push, 1)
struct Prof_Entry {
    const void *block_addr;
    REBUPT index;
    REBUPT val_addr;
    i64 wc_time;
    i64 CPU_TIME;
    i32 func_class;
    i32 record_type;
    i32 n_len;
};
#pragma pack (pop)

REBUPT stop_id = 0;

enum Prof_Entry_Type {
    RT_BOF,
    RT_BOC,
    RT_EOC,
    RT_EOF
};

static void write_prof_data(struct Prof_Entry *entry, const REBYTE *name)
{
    //if (entry->block_addr == (void*)stop_id) {
    //   raise(SIGTRAP);
    //}
    entry->n_len = strlen(cast(char*, name));
    if (fwrite(entry, sizeof(*entry), 1, pfile) == 0) {
        fclose(pfile);
        pfile = NULL;
    }
    if (fprintf(pfile, "%s", cast(char*, name)) == 0) {
        fclose(pfile);
        pfile = NULL;
    }
}
//
//  Init_Func_Profiler: C
// 
// Initialize the profiler
// 
void Init_Func_Profiler(const REBCHR *path)
{
    if (path == NULL) return;
    pfile = fopen(cast(char*, path), "w");
    if (pfile != NULL) {
        struct Prof_Entry entry = {
            NULL,
            CLOCKS_PER_SEC, /* abuse indix */
            0, /* value addr */
            OS_DELTA_TIME(0, 0),
            clock(),
            0, /* func_class */
            RT_BOF, /* record type */
            0 
        };

        write_prof_data(&entry, cast(REBYTE*, "prof-data"));
    }
}

static REBUPT 
get_frame_id(struct Reb_Frame *f)
{
    switch (VAL_FUNC_CLASS(FUNC_VALUE(FRM_FUNC(f)))) {
    case FUNC_CLASS_NATIVE:
        return cast(REBUPT, FUNC_CODE(FRM_FUNC(f)));

    case FUNC_CLASS_ACTION:
        return cast(REBUPT, Value_Dispatch[TO_0_FROM_KIND(VAL_TYPE(FRM_ARG(f, 1)))])
            + FUNC_ACT(f->func);
    case FUNC_CLASS_COMMAND:
    {
        REBVAL *val = ARR_HEAD(FUNC_BODY(FRM_FUNC(f)));
        void *ext = Find_Command_Extension(f);
        REBCNT cmd = cast(REBCNT, Int32(val + 1));
        return cast(REBUPT, ext) + cmd;
    }

    case FUNC_CLASS_CALLBACK:
    case FUNC_CLASS_ROUTINE:
        return cast(REBUPT, FRM_FUNC(f));

    case FUNC_CLASS_USER:
        return cast(REBUPT, FRM_FUNC(f));

    case FUNC_CLASS_SPECIALIZED:
        //
        // Shouldn't get here--the specific function type should have been
        // extracted from the frame to use.
        //
        assert(FALSE);
        break;

    default:
        fail(Error(RE_MISC));
    }
    return 0;
}

static void Func_Profile(struct Reb_Frame *f, enum Prof_Entry_Type rt)
{
    //1, addr, name, type, time
    if (pfile) {
        const REBYTE *name = Get_Sym_Name(FRM_LABEL(f));
        const void *addr = (f->indexor == VALIST_FLAG) ?
            cast(const void *, f->source.vaptr)
            : f->source.array;
        struct Prof_Entry entry = {
            addr,
            f->expr_index,
            get_frame_id(f),
            OS_DELTA_TIME(0, 0),
            clock(),
            VAL_FUNC_CLASS(FUNC_VALUE(FRM_FUNC(f))), /* func_class */
            rt, /* record type */
            0, /* n-len, set later */
        };
        write_prof_data(&entry, name);
    }
}
//
//  Func_Profile_Start: C
// 
// Start profiling a func
// 
void Func_Profile_Start(struct Reb_Frame *f)
{
    assert(f->mode == CALL_MODE_FUNCTION);

    Func_Profile(f, RT_BOC);
}

//
//  Func_Profile_End: C
// 
// Finish profiling a func
// 
void Func_Profile_End(struct Reb_Frame *f)
{
    // f->mode could be CALL_MODE_THROW_PENDING
    Func_Profile(f, RT_EOC);
}

//
//  Shutdown_Func_Profiler: C
// 
// Free all resources for the profiler
// 
void Shutdown_Func_Profiler(void)
{
    if (pfile) {
        struct Prof_Entry entry = {
            NULL, /* addr */
            0,
            0, /* value addr */
            OS_DELTA_TIME(0, 0),
            clock(),
            0,      /* func_class */
            RT_EOF, /* record type */
            0,  /* n_len */
        };
        write_prof_data(&entry, cast(REBYTE*, ""));
        fclose(pfile);
    }
    pfile = NULL;
}