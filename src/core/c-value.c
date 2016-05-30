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

#include "sys-core.h"


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
void Assert_Cell_Writable(
    const RELVAL *v,
    const char *file,
    int line
) {
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
REBOOL IS_CONDITIONAL_FALSE_Debug(const REBVAL *v)
{
    if (IS_END(v) || IS_VOID(v) || IS_TRASH_DEBUG(v)) {
        Debug_Fmt("Conditional true/false test on END or void or trash");
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
enum Reb_Kind VAL_TYPE_Debug(const REBVAL *v, const char *file, int line)
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
void Assert_Flags_Are_For_Value(const REBVAL *v, REBUPT f) {
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
    assert(IS_SPECIFIC(v));
    return (v)->payload.any_target.specific;
}


//
//  INIT_WORD_INDEX_Debug: C
//
void INIT_WORD_INDEX_Debug(REBVAL *v, REBCNT i)
{
    assert(ANY_WORD(v));
    assert(GET_VAL_FLAG((v), WORD_FLAG_BOUND));
    if (IS_RELATIVE(v))
        assert(
            SAME_SYM(VAL_WORD_SYM(v), FUNC_PARAM_SYM(VAL_WORD_FUNC(v), i))
        );
    else
        assert(SAME_SYM(
            VAL_WORD_SYM(v), CTX_KEY_SYM(VAL_WORD_CONTEXT(v), i))
        );
    (v)->payload.any_word.place.binding.index = (i);
}


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
    const REBVAL *val
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
