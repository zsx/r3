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

#include "sys-core.h"
#include <assert.h>

static REBI64 profiler_epoch;

#define VEC_AT(t, v, i) \
    cast(t*, &(v)->data[(i) * (v)->width])

struct Reb_Caller {
    REBCNT caller_idx; // index in the PG_Func_Profiler vector
    REBCNT n; // how many times it's called by this caller

	/* time is in microseconds */
	REBI64 min_time;
	REBI64 max_time;
	REBI64 total_time;
    REBI64 partial_time; // if this is on the call chain to dump-runtime
};

struct Reb_Func_Stats {
	REBUPT id; /* function ID */
	REBSYM sym; /* name */
    REBCNT type;

	struct Reb_FS_Vector callers; /* pointers to callers */
};

static
void *vector_alloc_at_end(struct Reb_FS_Vector *v)
{
	if (v->capacity < v->size + 1) {
		void *m;
		REBCNT c = v->capacity * 2;

		if (c == 0)
			c = 1024;

		m = Alloc_Mem(c * v->width);
		if (v->size > 0) {
			memcpy(m, v->data, v->size * v->width);
			Free_Mem(v->data, v->capacity * v->width);
		}
		v->data = m;
		v->capacity = c;
	}
	v->size ++;
	return &v->data[v->width * (v->size - 1)];
}

//
//  Init_Func_Profiler: C
// 
// Initialize the profiler
// 
void Init_Func_Profiler(void)
{
    struct Reb_FS_Vector *v = Alloc_Mem(sizeof(struct Reb_FS_Vector));
    struct Reb_Func_Stats *head;
    v->capacity = 4096;
    v->width = sizeof(struct Reb_Func_Stats);
    v->size = 1;
    v->data = Alloc_Mem(v->capacity * v->width);
    PG_Func_Profiler = v;

    /* reserve the first slot for NULL */
    head = VEC_AT(struct Reb_Func_Stats, PG_Func_Profiler, 0);
    head->id = 0;
    head->sym = SYM_0;
    head->type = 0;

    head->callers.capacity = 0;
    head->callers.size = 0;
    head->callers.data = NULL;

    profiler_epoch = OS_DELTA_TIME(0, 0);
}

static struct Reb_Func_Stats *
find_frame(struct Reb_Frame *f)
{
	struct Reb_Func_Stats *fs = NULL;
    REBCNT i;
	for(i = 0; i < PG_Func_Profiler->size; i ++) {
		struct Reb_Func_Stats *fs;
		fs = VEC_AT(struct Reb_Func_Stats, PG_Func_Profiler, i);
		if (fs->id == f->eval_id) {
            f->profile_idx = i;
            return fs;
		}
	}
    return NULL;
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
}

//
//  Func_Profile_Start: C
// 
// Start profiling a func
// 
void Func_Profile_Start(struct Reb_Frame *f)
{
	REBCNT i;
    struct Reb_Frame *p;
    struct Reb_Func_Stats *fs = NULL;
    REBOOL found;

    f->eval_id = get_frame_id(f);
    fs = find_frame(f);
	if (fs == NULL) {
		fs = vector_alloc_at_end(PG_Func_Profiler);
        f->profile_idx = PG_Func_Profiler->size - 1;

        fs->sym = FRM_LABEL(f);
		fs->id = f->eval_id;
        fs->type = VAL_FUNC_CLASS(FUNC_VALUE(f->func));

		fs->callers.data = NULL;
		fs->callers.capacity = 0;
		fs->callers.size = 0;
		fs->callers.width = sizeof(struct Reb_Caller);
    }

	/* update caller info */
    for (p = f->prior;
        p != NULL && p->mode != CALL_MODE_FUNCTION;
        p = p->prior);

    found = FALSE;
    for (i = 0; i < fs->callers.size; i++) {
        struct Reb_Func_Stats *caller_fs;
        REBCNT caller_index;

        caller_index = VEC_AT(struct Reb_Caller, &fs->callers, i)->caller_idx;
        caller_fs = VEC_AT(struct Reb_Func_Stats, PG_Func_Profiler, caller_index);

        if ((p == NULL && caller_index == 0)
            || (p != NULL && caller_index != 0
                && caller_fs->id == p->eval_id)){
            f->last_caller = i;
            found = TRUE;
            break;
        }
    }
    if (!found) {
	    struct Reb_Caller *caller;
        caller = vector_alloc_at_end(&fs->callers);
        f->last_caller = fs->callers.size - 1;

        caller->caller_idx = (p == NULL)? 0 : p->profile_idx;
        caller->n = 0;
        caller->total_time = caller->max_time = 0;
        caller->min_time = 0;
        caller->partial_time = 0;
    }

    assert(fs->callers.size > 0);
}

//
//  Func_Profile_End: C
// 
// Finish profiling a func
// 
void Func_Profile_End(struct Reb_Frame *f)
{
    REBI64 time = f->eval_time;

    struct Reb_Func_Stats *fs = NULL;
    struct Reb_Caller *caller;
    
    assert(f->profile_idx < PG_Func_Profiler->size);

    fs = VEC_AT(struct Reb_Func_Stats, PG_Func_Profiler, f->profile_idx);
    caller = VEC_AT(struct Reb_Caller, &fs->callers, f->last_caller);

    assert(caller != NULL);

    if (caller->n == 0) {
        caller->max_time = caller->min_time = caller->total_time = time;
    }
    else {
        if (caller->min_time > time) caller->min_time = time;
        if (caller->max_time < time) caller->max_time = time;
        caller->total_time += time;
    }
    caller->n++;
}

//
//  Shutdown_Func_Profiler: C
// 
// Free all resources for the profiler
// 
void Shutdown_Func_Profiler(void)
{
    REBCNT i;
    struct Reb_FS_Vector *v = PG_Func_Profiler;
    for (i = 0; i < v->size; i++) {
        struct Reb_Func_Stats *fs = VEC_AT(struct Reb_Func_Stats, v, i);
        if (fs->callers.capacity > 0) {
            Free_Mem(fs->callers.data,
                fs->callers.capacity * fs->callers.width);
        }
    }
    Free_Mem(v->data, v->capacity * v->width);
    FREE(struct Reb_FS_Vector, v);
    PG_Func_Profiler = NULL;
}

static void
set_partial_time(struct Reb_Frame *f, REBI64 time)
{
    struct Reb_Func_Stats *fs = NULL;
    struct Reb_Caller *caller;

    if (f->mode != CALL_MODE_FUNCTION) return;
    
    assert(f->profile_idx < PG_Func_Profiler->size);

    fs = VEC_AT(struct Reb_Func_Stats, PG_Func_Profiler, f->profile_idx);
    caller = VEC_AT(struct Reb_Caller, &fs->callers, f->last_caller);

    assert(caller != NULL);

    caller->partial_time = time;
}

//
//  Dump_Func_Stats: C
// 
// Dump the function statistics to a file
// 
void Dump_Func_Stats(REBVAL *path)
{
    REBCNT i;
    REBSER *ser;
    FILE *dest;
    struct Reb_FS_Vector *v = PG_Func_Profiler;
    struct Reb_Frame *f;

    if (v == NULL) {
        return;
    }
    ser = Value_To_OS_Path(path, TRUE);
    dest = fopen(cast(char*, SER_HEAD(REBCHR, ser)), "w");
    if (dest == NULL) {
        Free_Series(ser);
        return;
    }
    Free_Series(ser);

    // Update partial_time
    for (f = FS_TOP; f != NULL; f = f->prior) {
        set_partial_time(f, OS_DELTA_TIME(f->eval_time, 0));
    }

    fprintf(dest, "#Total Time,%lld\n", OS_DELTA_TIME(profiler_epoch, 0));
    fprintf(dest, "#ID,Name,TYPE,Caller_ID,Caller_Name,Count,"
        "Min_Time,Max_Time,Total_Time,Average_Time\n");
    for (i = 0; i < v->size; i++) {
        REBCNT j; 
        struct Reb_Func_Stats *fs = VEC_AT(struct Reb_Func_Stats, v, i);
        assert(i == 0 || fs->callers.size > 0);
        for(j = 0; j < fs->callers.size; j ++) {
            struct Reb_Caller *caller;
            struct Reb_Func_Stats *caller_fs;

            caller = VEC_AT(struct Reb_Caller, &fs->callers, j);
            assert(caller->caller_idx < PG_Func_Profiler->size);

            caller_fs = VEC_AT(struct Reb_Func_Stats, PG_Func_Profiler,
                caller->caller_idx);

            fprintf(dest, "%llx,%s,%d,%llx,%s,%d,%lld,%lld,%lld,%.2f\n",
                fs->id, Get_Sym_Name(fs->sym),
                fs->type,
                caller_fs->id,
                cast(char*, Get_Sym_Name(caller_fs->sym)),
                caller->n, caller->min_time, caller->max_time,
                caller->total_time + caller->partial_time,
                caller->n == 0 ? 0: ((double)caller->total_time) / caller->n);
        }
    }
    fclose(dest);

    // clear partial_time
    for (f = FS_TOP; f != NULL; f = f->prior) {
        set_partial_time(f, 0);
    }
}