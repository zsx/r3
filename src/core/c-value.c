//
//  File: %c-value.c
//  Summary: "Generic REBVAL Support Services and Debug Routines"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
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
// These are routines to support the macros and definitions in %sys-value.h
// which are not specific to any given type.  For the type-specific code,
// see files with names like %t-word.c, %t-logic.c, %t-integer.c...
//
// Largely they are debug-oriented routines, with a couple temporary routines
// that are needed in the release build.
//

#include "sys-core.h"


//
//  COPY_VALUE_Release: C
//
// Temporary function implementation of what should really just defined as
// COPY_VALUE_MACRO.  If GUESSED is passed in, it will use dynamic binding
// to look at the stack and determine what function instance's context to
// use for a word.
//
void COPY_VALUE_Release(
    REBVAL *dest,
    const RELVAL *src,
    REBCTX *specifier
) {
    COPY_VALUE_MACRO(dest, src, specifier);
}


#if !defined(NDEBUG)

//
//  Panic_Value_Debug: C
//
// This is a debug-only "error generator", which will hunt through all the
// series allocations and panic on the series that contains the value (if
// it can find it).  This will allow those using Address Sanitizer or
// Valgrind to know a bit more about where the value came from.
//
// Additionally, if it happens to be a void or trash, LOGIC!, BAR!, or NONE!
// it will dump out where the initialization happened if that information
// was stored.
//
ATTRIBUTE_NO_RETURN void Panic_Value_Debug(
    const RELVAL *value,
    const char *file,
    int line
) {
    REBSER *containing = Try_Find_Containing_Series_Debug(value);

    printf("PANIC VALUE called from %s:%d\n", file, line);
    fflush(stdout);

    switch (value->header.bits & HEADER_TYPE_MASK) {
    case REB_0:
    case REB_BLANK:
    case REB_LOGIC:
    case REB_BAR:
        printf(
            "REBVAL init on tick #%d at %s:%d\n",
            cast(unsigned int, value->payload.track.count),
            value->payload.track.filename,
            value->payload.track.line
        );
        fflush(stdout);
        break;
    }

    printf("Kind=%d\n", cast(int, value->header.bits & HEADER_TYPE_MASK));
    fflush(stdout);

    if (containing) {
        printf("Containing series for value pointer found, panicking it:\n");
        fflush(stdout);
        Panic_Series(containing);
    }

    printf("No containing series for value...panicking to make stack dump:\n");
    fflush(stdout);
    Panic_Array(EMPTY_ARRAY);
}


#if defined(__cplusplus)

//
//  Assert_Cell_Writable: C
//
// The check helps avoid very bad catastrophies that might ensue if "implicit
// end markers" could be overwritten.  These are the ENDs that are actually
// pointers doing double duty inside a data structure, and there is no REBVAL
// storage backing the position.
//
// (A fringe benefit is catching writes to other unanticipated locations.)
//
void Assert_Cell_Writable(const RELVAL *v, const char *file, int line)
{
    // REBVALs should not be written at addresses that do not match the
    // alignment of the processor.  This checks modulo the size of an unsigned
    // integer the same size as a platform pointer (REBUPT => uintptr_t)
    //
    assert(cast(REBUPT, (v)) % sizeof(REBUPT) == 0);

    if (NOT((v)->header.bits & WRITABLE_MASK_DEBUG)) {
        printf("Non-writable value passed to writing routine\n");
        fflush(stdout);
        Panic_Value_Debug(v, file, line);
    }
}

#endif


//
//  VAL_RESET_HEADER_Debug: C
//
void VAL_RESET_HEADER_Debug(
    RELVAL *v,
    enum Reb_Kind kind,
    const char *file,
    int line
) {
    ASSERT_CELL_WRITABLE_IF_DEBUG(v, file, line);
    VAL_RESET_HEADER_CORE(v, kind);
    MARK_CELL_WRITABLE_IF_DEBUG(v);
}



//
//  Sink_Debug: C
//
// !!! Uses proper inlines after specific-binding merge, but written this
// way so that sinks get the right file and line information while the
// old macros are still around.
//
REBVAL *Sink_Debug(RELVAL *v, const char *file, int line) {
    //
    // SINK claims it's okay to cast from RELVAL to REBVAL because the
    // value is just going to be written to.  Verify that claim in the
    // debug build by setting to trash as part of the cast.
    //
    VAL_RESET_HEADER((v), REB_0); /* don't set NOT_TRASH flag */
    (v)->payload.track.filename = file;
    (v)->payload.track.line = line;
    (v)->payload.track.count = TG_Do_Count;
    return cast(REBVAL*, v);
}


//
//  IS_END_Debug: C
//
REBOOL IS_END_Debug(const RELVAL *v, const char *file, int line) {
#ifdef __cplusplus
    if (
        (v->header.bits & WRITABLE_MASK_DEBUG)
        //
        // Note: a non-writable value could have any bit pattern in the
        // type slot, so we only check for trash in writable ones.
        //
        && (v->header.bits & HEADER_TYPE_MASK) == REB_0
        && NOT(v->header.bits & VOID_FLAG_NOT_TRASH)
        && NOT(v->header.bits & VOID_FLAG_SAFE_TRASH)
    ) {
        printf("IS_END() called on value marked as TRASH\n");
        fflush(stdout);
        Panic_Value_Debug(v, file, line);
    }
#endif
    return IS_END_MACRO(v);
}


//
//  IS_CONDITIONAL_FALSE_Debug: C
//
// Variant of IS_CONDITIONAL_FALSE() macro for the debug build which checks to
// ensure you never call it on a void
//
REBOOL IS_CONDITIONAL_FALSE_Debug(const RELVAL *v)
{
    if (IS_VOID(v)) {
        Debug_Fmt("Conditional true/false test on void");
        PANIC_VALUE(v);
    }

    return GET_VAL_FLAG(v, VALUE_FLAG_FALSE);
}


//
//  VAL_TYPE_Debug: C
//
// Variant of VAL_TYPE() macro for the debug build which checks to ensure that
// you never call it on an END marker or on REB_TRASH.
//
enum Reb_Kind VAL_TYPE_Debug(const RELVAL *v, const char *file, int line)
{
    if (IS_END(v)) {
        //
        // Seeing a bit pattern that has the low bit to 0 may be a purposeful
        // end signal, or it could be something that's garbage data and just
        // happens to have its zero bit set.  Since half of all possible
        // bit patterns are even, it's more worth it than usual to point out.
        //
        printf("END marker or garbage (low bit 0) in VAL_TYPE()\n");
        fflush(stdout);
        Panic_Value_Debug(v, file, line);
    }
    if (IS_TRASH_DEBUG(v)) {
        printf("Unexpected TRASH in VAL_TYPE()\n");
        fflush(stdout);
        Panic_Value_Debug(v, file, line);
    }
    return cast(enum Reb_Kind, (v)->header.bits & HEADER_TYPE_MASK);
}


//
//  Assert_Flags_Are_For_Value: C
//
// This check is used by GET_VAL_FLAG, SET_VAL_FLAG, CLEAR_VAL_FLAG to avoid
// accidentally checking or setting a type-specific flag on the wrong type
// of value in the debug build.
//
void Assert_Flags_Are_For_Value(const RELVAL *v, REBUPT f) {
    if ((f & HEADER_TYPE_MASK) == 0)
        return; // flag applies to any value (or trash)

    if ((f & HEADER_TYPE_MASK) == REB_FUNCTION) {
        assert(IS_FUNCTION(v));
    }
    else if ((f & HEADER_TYPE_MASK) == REB_OBJECT) {
        assert(ANY_CONTEXT(v));
    }
    else if ((f & HEADER_TYPE_MASK) == REB_WORD) {
        assert(ANY_WORD(v));
    }
}


//
//  VAL_SPECIFIC_Debug: C
//
REBCTX *VAL_SPECIFIC_Debug(const REBVAL *v)
{
    REBCTX *specific;

    assert(IS_SPECIFIC(v));
    assert(ANY_WORD(v) || ANY_ARRAY(v));

    specific = (v)->payload.any_target.specific;

    if (specific != SPECIFIED) {
        //
        // Basic sanity check: make sure it's a context at all
        //
        if (!GET_ARR_FLAG(CTX_VARLIST(specific), ARRAY_FLAG_CONTEXT_VARLIST)) {
            printf("Non-CONTEXT found as specifier in specific value\n");
            Panic_Series(cast(REBSER*, specific));
        }

        // While an ANY-WORD! can be bound specifically to an arbitrary
        // object, an ANY-ARRAY! only becomes bound specifically to frames.
        // The keylist for a frame's context should come from a function's
        // paramlist, which should have a FUNCTION! value in keylist[0]
        //
        if (ANY_ARRAY(v))
            assert(IS_FUNCTION(CTX_ROOTKEY(specific)));
    }

    return specific;
}


//
//  INIT_WORD_INDEX_Debug: C
//
void INIT_WORD_INDEX_Debug(RELVAL *v, REBCNT i)
{
    assert(ANY_WORD(v));
    assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND));
    if (IS_RELATIVE(v))
        assert(
            SAME_SYM(VAL_WORD_SYM(v), FUNC_PARAM_SYM(VAL_WORD_FUNC(v), i))
        );
    else
        assert(SAME_SYM(
            VAL_WORD_SYM(v), CTX_KEY_SYM(VAL_WORD_CONTEXT(KNOWN(v)), i))
        );
    (v)->payload.any_word.place.binding.index = (i);
}


//
//  IS_RELATIVE_Debug: C
//
// One should only be testing relvals for their relativeness or specificness,
// because all REBVAL* should be guaranteed to be speciic!
//
REBOOL IS_RELATIVE_Debug(const RELVAL *value)
{
    return GET_VAL_FLAG(value, VALUE_FLAG_RELATIVE);
}


//
//  Assert_No_Relative: C
//
// Check to make sure there are no relative values in an array, maybe deeply.
//
// !!! Should this pay attention to indices?
//
void Assert_No_Relative(REBARR *array, REBOOL deep)
{
    RELVAL *item = ARR_HEAD(array);
    while (NOT_END(item)) {
        if (IS_RELATIVE(item)) {
            Debug_Fmt("Array contained relative item and wasn't supposed to.");
            PROBE_MSG(item, "relative item");
            Panic_Array(array);
        }
        if (!IS_VOID_OR_SAFE_TRASH(item) && ANY_ARRAY(item) && deep)
             Assert_No_Relative(VAL_ARRAY(item), deep);
        ++item;
    }
}


//
//  ENSURE_C_REBVAL_Debug: C
//
// NOOP for type check in the debug build.
//
const REBVAL *ENSURE_C_REBVAL_Debug(const REBVAL *value)
{
    assert(!GET_VAL_FLAG(value, VALUE_FLAG_RELATIVE));
    return value;
}


//
//  ENSURE_REBVAL_Debug: C
//
// NOOP for type check in the debug build.
//
REBVAL *ENSURE_REBVAL_Debug(REBVAL *value)
{
    assert(!GET_VAL_FLAG(value, VALUE_FLAG_RELATIVE));
    return value;
}


//
//  SINK_Debug: C
//
// Checked version of casting operation (to restrict input to only RELVAL).
// This is used when the cast is legal because the slot is to be written to
// only, and hence it doesn't matter if it contains relative data.
//
REBVAL *SINK_Debug(RELVAL *v)
{
    return cast(REBVAL*, v);
}


//
//  const_KNOWN_Debug: C
//
const REBVAL *const_KNOWN_Debug(const RELVAL *value)
{
    assert(IS_SPECIFIC(value));
    return cast(const REBVAL*, value);
}


//
//  KNOWN_Debug: C
//
REBVAL *KNOWN_Debug(RELVAL *value)
{
    assert(IS_SPECIFIC(value));
    return cast(REBVAL*, value);
}



//
//  COPY_VALUE_Debug: C
//
// A function in debug build for compile-time type check (vs. blind casting)
//
void COPY_VALUE_Debug(
    REBVAL *dest,
    const RELVAL *src,
    REBCTX *specifier
) {
    if (IS_RELATIVE(src)) {
        if (specifier == SPECIFIED) {
            Debug_Fmt("Internal Error: Relative word used with SPECIFIED");
            PROBE_MSG(src, "word or array");
            PROBE_MSG(FUNC_VALUE(VAL_WORD_FUNC(src)), "func");
            assert(FALSE);
        }
        else if (
            VAL_RELATIVE(src)
            != VAL_FUNC(CTX_FRAME_FUNC_VALUE(specifier))
        ) {
            Debug_Fmt("Internal Error: Function mismatch in specific binding");
            PROBE_MSG(src, "word or array");
            PROBE_MSG(FUNC_VALUE(VAL_RELATIVE(src)), "expected func");
            PROBE_MSG(CTX_FRAME_FUNC_VALUE(specifier), "actual func");
            assert(FALSE);
        }
    }
    COPY_VALUE_MACRO(dest, src, specifier);
}


//
//  Probe_Core_Debug: C
//
// Debug function for outputting a value.  Done as a function instead of just
// a macro due to how easy it is with va_lists to order the types of the
// parameters wrong.  :-/
//
void Probe_Core_Debug(
    const char *msg,
    const char *file,
    int line,
    const RELVAL *val
) {
    if (msg)
        printf("\n** PROBE_MSG(\"%s\") ", msg);
    else
        printf("\n** PROBE() ");

    printf("tick %d %s:%d\n", cast(int, TG_Do_Count), file, line);

    fflush(stdout);

    Debug_Fmt("%r\n", val);
}

#endif
