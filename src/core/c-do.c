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
************************************************************************
**
**  Module:  c-do.c
**  Summary: the core interpreter - the heart of REBOL
**  Section: core
**  Author:  Carl Sassenrath
**  Notes:
**    WARNING WARNING WARNING
**    This is highly tuned code that should only be modified by experts
**    who fully understand its design. It is very easy to create odd
**    side effects so please be careful and extensively test all changes!
**
***********************************************************************/

#include "sys-core.h"

#include "tmp-evaltypes.h"

enum Reb_Param_Mode {
    PARAM_MODE_NORMAL, // ordinary arguments before any refinements seen
    PARAM_MODE_REFINE_PENDING, // picking up refinement arguments, none yet
    PARAM_MODE_REFINE_ARGS, // at least one refinement has been found
    PARAM_MODE_SCANNING, // looking for refinements (used out of order)
    PARAM_MODE_SKIPPING, // in the process of skipping an unused refinement
    PARAM_MODE_REVOKING // found an unset and aiming to revoke refinement use
};


//
//  Eval_Depth: C
//
REBINT Eval_Depth(void)
{
    REBINT depth = 0;
    struct Reb_Call *call;

    for (call = DSF; call != NULL; call = PRIOR_DSF(call), depth++);
    return depth;
}


//
//  Stack_Frame: C
//
struct Reb_Call *Stack_Frame(REBCNT n)
{
    struct Reb_Call *call = DSF;

    while (call) {
        if (n == 0) return call;

        --n;
        call = PRIOR_DSF(call);
    }

    return NULL;
}


//
//  trace: native [
//  
//  {Enables and disables evaluation tracing and backtrace.}
//  
//      mode [integer! logic!]
//      /back {Set mode ON to enable or integer for lines to display}
//      /function "Traces functions only (less output)"
//  ]
//
REBNATIVE(trace)
{
    REBVAL *arg = D_ARG(1);

    Check_Security(SYM_DEBUG, POL_READ, 0);

    // The /back option: ON and OFF, or INTEGER! for # of lines:
    if (D_REF(2)) { // /back
        if (IS_LOGIC(arg)) {
            Enable_Backtrace(VAL_LOGIC(arg));
        }
        else if (IS_INTEGER(arg)) {
            REBINT lines = Int32(arg);
            Trace_Flags = 0;
            if (lines < 0) {
                fail (Error_Invalid_Arg(arg));
                return R_UNSET;
            }

            Display_Backtrace(cast(REBCNT, lines));
            return R_UNSET;
        }
    }
    else Enable_Backtrace(FALSE);

    // Set the trace level:
    if (IS_LOGIC(arg)) {
        Trace_Level = VAL_LOGIC(arg) ? 100000 : 0;
    }
    else Trace_Level = Int32(arg);

    if (Trace_Level) {
        Trace_Flags = 1;
        if (D_REF(3)) SET_FLAG(Trace_Flags, 1); // function
        Trace_Depth = Eval_Depth() - 1; // subtract current TRACE frame
    }
    else Trace_Flags = 0;

    return R_UNSET;
}

static REBINT Init_Depth(void)
{
    // Check the trace depth is ok:
    REBINT depth = Eval_Depth() - Trace_Depth;
    if (depth < 0 || depth >= Trace_Level) return -1;
    if (depth > 10) depth = 10;
    Debug_Space(cast(REBCNT, 4 * depth));
    return depth;
}

#define CHECK_DEPTH(d) if ((d = Init_Depth()) < 0) return;\

void Trace_Line(REBSER *block, REBINT index, const REBVAL *value)
{
    int depth;

    if (GET_FLAG(Trace_Flags, 1)) return; // function
    if (ANY_FUNC(value)) return;

    CHECK_DEPTH(depth);

    Debug_Fmt_(cs_cast(BOOT_STR(RS_TRACE,1)), index+1, value);
    if (IS_WORD(value) || IS_GET_WORD(value)) {
        value = GET_VAR(value);
        if (VAL_TYPE(value) < REB_NATIVE)
            Debug_Fmt_(cs_cast(BOOT_STR(RS_TRACE,2)), value);
        else if (VAL_TYPE(value) >= REB_NATIVE && VAL_TYPE(value) <= REB_FUNCTION) {
            REBSER *words = List_Func_Words(value);
            Debug_Fmt_(
                cs_cast(BOOT_STR(RS_TRACE,3)), Get_Type_Name(value), words
            );
            Free_Series(words);
        }
        else
            Debug_Fmt_(cs_cast(BOOT_STR(RS_TRACE,4)), Get_Type_Name(value));
    }
    /*if (ANY_WORD(value)) {
        word = value;
        if (IS_WORD(value)) value = GET_VAR(word);
        Debug_Fmt_(cs_cast(BOOT_STR(RS_TRACE,2)), VAL_WORD_FRAME(word), VAL_WORD_INDEX(word), Get_Type_Name(value));
    }
    if (Trace_Stack) Debug_Fmt(cs_cast(BOOT_STR(RS_TRACE,3)), DSP, DSF);
    else
    */
    Debug_Line();
}

void Trace_Func(const REBVAL *word, const REBVAL *value)
{
    int depth;
    CHECK_DEPTH(depth);
    Debug_Fmt_(cs_cast(BOOT_STR(RS_TRACE,5)), Get_Word_Name(word), Get_Type_Name(value));
    if (GET_FLAG(Trace_Flags, 1))
        Debug_Values(DSF_ARG(DSF, 1), DSF_ARGC(DSF), 20);
    else Debug_Line();
}

void Trace_Return(REBCNT label_sym, const REBVAL *value)
{
    int depth;
    CHECK_DEPTH(depth);
    Debug_Fmt_(cs_cast(BOOT_STR(RS_TRACE,6)), Get_Sym_Name(label_sym));
    Debug_Values(value, 1, 50);
}

void Trace_Arg(REBINT num, const REBVAL *arg, const REBVAL *path)
{
    int depth;
    if (IS_REFINEMENT(arg) && (!path || IS_END(path))) return;
    CHECK_DEPTH(depth);
    Debug_Fmt(cs_cast(BOOT_STR(RS_TRACE,6)), num+1, arg);
}


//
//  Trace_Value: C
//
void Trace_Value(REBINT n, const REBVAL *value)
{
    int depth;
    CHECK_DEPTH(depth);
    Debug_Fmt(cs_cast(BOOT_STR(RS_TRACE,n)), value);
}

//
//  Trace_String: C
//
void Trace_String(REBINT n, const REBYTE *str, REBINT limit)
{
    static char tracebuf[64];
    int depth;
    int len = MIN(60, limit);
    CHECK_DEPTH(depth);
    memcpy(tracebuf, str, len);
    tracebuf[len] = '\0';
    Debug_Fmt(cs_cast(BOOT_STR(RS_TRACE,n)), tracebuf);
}


//
//  Trace_Error: C
//
void Trace_Error(const REBVAL *value)
{
    int depth;
    CHECK_DEPTH(depth);
    Debug_Fmt(cs_cast(BOOT_STR(RS_TRACE, 10)), &VAL_ERR_VALUES(value)->type, &VAL_ERR_VALUES(value)->id);
}


//
//  Next_Path_Throws: C
// 
// Evaluate next part of a path.
//
REBFLG Next_Path_Throws(REBPVS *pvs)
{
    REBVAL *path;
    REBPEF func;
    REBVAL temp;

    // Path must have dispatcher, else return:
    func = Path_Dispatch[VAL_TYPE(pvs->value)];
    if (!func) return FALSE; // unwind, then check for errors

    pvs->path++;

    //Debug_Fmt("Next_Path: %r/%r", pvs->path-1, pvs->path);

    // object/:field case:
    if (IS_GET_WORD(path = pvs->path)) {
        pvs->select = GET_MUTABLE_VAR(path);
        if (IS_UNSET(pvs->select))
            fail (Error(RE_NO_VALUE, path));
    }
    // object/(expr) case:
    else if (IS_PAREN(path)) {

        if (DO_ARRAY_THROWS(&temp, path)) {
            *pvs->value = temp;
            return TRUE;
        }

        pvs->select = &temp;
    }
    else // object/word and object/value case:
        pvs->select = path;

    // Uses selector on the value.
    // .path - must be advanced as path is used (modified by func)
    // .value - holds currently evaluated path value (modified by func)
    // .select - selector on value
    // .store - storage (usually TOS) for constructed values
    // .setval - non-zero for SET-PATH (set to zero after SET is done)
    // .orig - original path for error messages
    switch (func(pvs)) {
    case PE_OK:
        break;
    case PE_SET: // only sets if end of path
        if (pvs->setval && IS_END(pvs->path+1)) {
            *pvs->value = *pvs->setval;
            pvs->setval = 0;
        }
        break;
    case PE_NONE:
        SET_NONE(pvs->store);
    case PE_USE:
        pvs->value = pvs->store;
        break;
    case PE_BAD_SELECT:
        fail (Error(RE_INVALID_PATH, pvs->orig, pvs->path));
    case PE_BAD_SET:
        fail (Error(RE_BAD_PATH_SET, pvs->orig, pvs->path));
    case PE_BAD_RANGE:
        fail (Error_Out_Of_Range(pvs->path));
    case PE_BAD_SET_TYPE:
        fail (Error(RE_BAD_FIELD_SET, pvs->path, Type_Of(pvs->setval)));
    default:
        assert(FALSE);
    }

    if (NOT_END(pvs->path + 1)) return Next_Path_Throws(pvs);

    return FALSE;
}


//
//  Do_Path_Throws: C
// 
// Evaluate a path value, given the first value in that path's series.  This
// evaluator may throw because parens are evaluated, e.g. `foo/(throw 1020)`
//
// If label_sym is passed in as being non-null, then the caller is implying
// readiness to process a path which may be a function with refinements.
// These refinements will be left in order on the data stack in the case
// that `out` comes back as ANY_FUNC().
//
// If a `val` is provided, it is assumed to be a set-path and is set to that
// value IF the path evaluation did not throw or error.  HOWEVER the set value
// is NOT put into `out`.  This provides more flexibility on performance in
// the evaluator, which may already have the `val` where it wants it, and
// so the extra assignment would just be overhead.
//
REBFLG Do_Path_Throws(REBVAL *out, REBCNT *label_sym, const REBVAL *path, REBVAL *val)
{
    REBPVS pvs;
    REBINT dsp_orig = DSP;

    assert(ANY_PATH(path));

    // !!! There is a bug in the dispatch such that if you are running a
    // set path, it does not always assign the output, because it "thinks you
    // aren't going to look at it".  This presumably originated from before
    // parens were allowed in paths, and neglects cases like:
    //
    //     foo/(throw 1020): value
    //
    // We always have to check to see if a throw occurred.  Until this is
    // streamlined, we have to at minimum set it to something that is *not*
    // thrown so that we aren't testing uninitialized memory.  A safe trash
    // will do, which is unset in release builds.
    //
    if (val)
        SET_TRASH_SAFE(out);

    // None of the values passed in can live on the data stack, because
    // they might be relocated during the path evaluation process.
    //
    assert(!IN_DATA_STACK(out));
    assert(!IN_DATA_STACK(path));
    assert(!val || !IN_DATA_STACK(val));

    // Not currently robust for reusing passed in path or value as the output
    assert(out != path && out != val);

    assert(!val || !THROWN(val));

    pvs.setval = val;       // Set to this new value
    pvs.store = out;        // Space for constructed results

    // Get first block value:
    pvs.orig = path;
    pvs.path = VAL_BLK_DATA(pvs.orig);

    // Lookup the value of the variable:
    if (IS_WORD(pvs.path)) {
        pvs.value = GET_MUTABLE_VAR(pvs.path);
        if (IS_UNSET(pvs.value))
            fail (Error(RE_NO_VALUE, pvs.path));
    }
    else pvs.value = pvs.path;

    // Start evaluation of path:
    if (IS_END(pvs.path + 1)) {
        // If it was a single element path, return the value rather than
        // try to dispatch it (would cause a crash at time of writing)
        //
        // !!! Is this the desired behavior, or should it be an error?
    }
    else if (Path_Dispatch[VAL_TYPE(pvs.value)]) {
        REBFLG threw = Next_Path_Throws(&pvs);

        // !!! See comments about why the initialization of out is necessary.
        // Without it this assertion can change on some things:
        //
        //     t: now
        //     t/time: 10:20:03
        //
        // (It thinks pvs.value has its THROWN bit set when it completed
        // successfully.  It was a PE_USE case where pvs.value was reset to
        // pvs.store, and pvs.store has its thrown bit set.  Valgrind does not
        // catch any uninitialized variables.)
        //
        // There are other cases that do trip valgrind when omitting the
        // initialization, though not as clearly reproducible.
        //
        assert(threw == THROWN(pvs.value));

        if (threw) return TRUE;

        // Check for errors:
        if (NOT_END(pvs.path + 1) && !ANY_FUNC(pvs.value)) {
            // Only function refinements should get by this line:
            fail (Error(RE_INVALID_PATH, pvs.orig, pvs.path));
        }
    }
    else if (!ANY_FUNC(pvs.value))
        fail (Error(RE_BAD_PATH_TYPE, pvs.orig, Type_Of(pvs.value)));

    assert(!THROWN(out));

    if (val) {
        // If SET then we don't return anything
        assert(IS_END(pvs.path) + 1);
        return FALSE;
    }

    // If storage was not used, then copy final value back to it:
    if (pvs.value != pvs.store) *pvs.store = *pvs.value;

    // Return 0 if not function or is :path/word...
    if (!ANY_FUNC(pvs.value)) {
        assert(IS_END(pvs.path) + 1);
        return FALSE;
    }

    if (label_sym) {
        REBVAL refinement;

        // When a function is hit, path processing stops with it sitting on
        // the position of what function to dispatch, usually a word.

        if (IS_WORD(pvs.path)) {
            *label_sym = VAL_WORD_SYM(pvs.path);
        }
        else if (ANY_FUNC(pvs.path)) {
            // You can get an actual function value as a label if you use it
            // literally with a refinement.  Tricky to make it, but possible:
            //
            // do reduce [
            //     to-path reduce [:append 'only] [a] [b]
            // ]
            //

            // !!! When a function was not invoked through looking up a word
            // (or a word in a path) to use as a label, there were once three
            // different alternate labels used.  One was SYM__APPLY_, another
            // was ROOT_NONAME, and another was to be the type of the function
            // being executed.  None are fantastic, we do the type for now.

            *label_sym = SYM_FROM_KIND(VAL_TYPE(pvs.path));
        }
        else
            fail (Error(RE_BAD_REFINE, pvs.path)); // CC#2226

        // Move on to the refinements (if any)
        pvs.path = pvs.path + 1;

        // !!! Currently, the mainline path evaluation "punts" on refinements.
        // When it finds a function, it stops the path evaluation and leaves
        // the position pvs.path before the list of refinements.
        //
        // A more elegant solution would be able to process and notice (for
        // instance) that `:APPEND/ONLY` should yield a function value that
        // has been specialized with a refinement.  Path chaining should thus
        // be able to effectively do this and give the refined function object
        // back to the evaluator or other client.
        //
        // If a label_sym is passed in, we recognize that a function dispatch
        // is going to be happening.  We do not want to pay to generate the
        // new series that would be needed to make a temporary function that
        // will be invoked and immediately GC'd  So we gather the refinements
        // on the data stack.
        //
        // This code simulates that path-processing-to-data-stack, but it
        // should really be something in dispatch iself.  In any case, we put
        // refinements on the data stack...and caller knows refinements are
        // from dsp_orig to DSP (thanks to accounting, all other operations
        // should balance!)

        for (; NOT_END(pvs.path); pvs.path++) { // "the refinements"
            if (IS_NONE(pvs.path)) continue;

            if (IS_PAREN(pvs.path)) {
                // Note it is not legal to use the data stack directly as the
                // output location for a DO (might be resized)

                if (DO_ARRAY_THROWS(&refinement, pvs.path)) {
                    *out = refinement;
                    DS_DROP_TO(dsp_orig);
                    return TRUE;
                }
                if (IS_NONE(&refinement)) continue;
                DS_PUSH(&refinement);
            }
            else if (IS_GET_WORD(pvs.path)) {
                DS_PUSH_TRASH;
                *DS_TOP = *GET_VAR(pvs.path);
                if (IS_NONE(DS_TOP)) {
                    DS_DROP;
                    continue;
                }
            }
            else
                DS_PUSH(pvs.path);

            // Whatever we were trying to use as a refinement should now be
            // on the top of the data stack, and only words are legal ATM
            if (!IS_WORD(DS_TOP)) fail (Error(RE_BAD_REFINE, DS_TOP));

            // Go ahead and canonize the word symbol so we don't have to
            // do it each time in order to get a case-insenstive compare
            VAL_WORD_SYM(DS_TOP) = SYMBOL_TO_CANON(VAL_WORD_SYM(DS_TOP));
        }

        // To make things easier for processing, reverse the refinements on
        // the data stack (we needed to evaluate them in forward order).
        // This way we can just pop them as we go, and know if they weren't
        // all consumed if it doesn't get back to `dsp_orig` by the end.

        if (dsp_orig != DSP) {
            REBVAL *bottom = DS_AT(dsp_orig + 1);
            REBVAL *top = DS_TOP;
            while (top > bottom) {
                refinement = *bottom;
                *bottom = *top;
                *top = refinement;

                top--;
                bottom++;
            }
        }
    }
    else {
        // !!! Historically this just ignores a result indicating this is a
        // function with refinements, e.g. ':append/only'.  However that
        // ignoring seems unwise.  It should presumably create a modified
        // function in that case which acts as if it has the refinement.
        //
        // If the caller did not pass in a label pointer we assume they are
        // likely not ready to process any refinements.
        //
        if (!IS_END(pvs.path + 1))
            fail (Error(RE_TOO_LONG)); // !!! Better error or add feature
    }

    return FALSE;
}


//
//  Pick_Path: C
// 
// Lightweight version of Do_Path used for A_PICK actions.
// Does not do paren evaluation, hence not designed to throw.
//
void Pick_Path(REBVAL *out, REBVAL *value, REBVAL *selector, REBVAL *val)
{
    REBPVS pvs;
    REBPEF func;

    pvs.value = value;
    pvs.path = 0;
    pvs.select = selector;
    pvs.setval = val;
    pvs.store = out;        // Temp space for constructed results

    // Path must have dispatcher, else return:
    func = Path_Dispatch[VAL_TYPE(value)];
    if (!func) return; // unwind, then check for errors

    switch (func(&pvs)) {
    case PE_OK:
        break;
    case PE_SET: // only sets if end of path
        if (pvs.setval) *pvs.value = *pvs.setval;
        break;
    case PE_NONE:
        SET_NONE(pvs.store);
    case PE_USE:
        pvs.value = pvs.store;
        break;
    case PE_BAD_SELECT:
        fail (Error(RE_INVALID_PATH, pvs.value, pvs.select));
    case PE_BAD_SET:
        fail (Error(RE_BAD_PATH_SET, pvs.value, pvs.select));
    default:
        assert(FALSE);
    }
}


//
//  Do_Signals: C
// 
// Special events to process during evaluation.
// Search for SET_SIGNAL to find them.
//
void Do_Signals(void)
{
    REBCNT sigs;
    REBCNT mask;

    // Accumulate evaluation counter and reset countdown:
    if (Eval_Count <= 0) {
        //Debug_Num("Poll:", (REBINT) Eval_Cycles);
        Eval_Cycles += Eval_Dose - Eval_Count;
        Eval_Count = Eval_Dose;
        if (Eval_Limit != 0 && Eval_Cycles > Eval_Limit)
            Check_Security(SYM_EVAL, POL_EXEC, 0);
    }

    if (!(Eval_Signals & Eval_Sigmask)) return;

    // Be careful of signal loops! EG: do not PRINT from here.
    sigs = Eval_Signals & (mask = Eval_Sigmask);
    Eval_Sigmask = 0;   // avoid infinite loop
    //Debug_Num("Signals:", Eval_Signals);

    // Check for recycle signal:
    if (GET_FLAG(sigs, SIG_RECYCLE)) {
        CLR_SIGNAL(SIG_RECYCLE);
        Recycle();
    }

#ifdef NOT_USED_INVESTIGATE
    if (GET_FLAG(sigs, SIG_EVENT_PORT)) {  // !!! Why not used?
        CLR_SIGNAL(SIG_EVENT_PORT);
        Awake_Event_Port();
    }
#endif

    // Escape only allowed after MEZZ boot (no handlers):
    if (GET_FLAG(sigs, SIG_ESCAPE) && PG_Boot_Phase >= BOOT_MEZZ) {
        CLR_SIGNAL(SIG_ESCAPE);
        Eval_Sigmask = mask;
        fail (VAL_ERR_OBJECT(TASK_HALT_ERROR));
    }

    Eval_Sigmask = mask;
}


//
//  Dispatch_Call_Throws: C
// 
// Expects call frame to be ready with all arguments fulfilled.
//
REBFLG Dispatch_Call_Throws(struct Reb_Call *call)
{
#if !defined(NDEBUG)
    REBINT dsp_precall = DSP;

    // We keep track of the head of the list of series that are not tracked
    // by garbage collection at the outset of the call.  Then we ensure that
    // when the call is finished, no accumulation has happened.  So all
    // newly allocated series should either be (a) freed or (b) delegated
    // to management by the GC...else they'd represent a leak
    //
    REBCNT manuals_tail = SERIES_TAIL(GC_Manuals);

    REBCNT series_guard_tail = SERIES_TAIL(GC_Series_Guard);
    REBCNT value_guard_tail = SERIES_TAIL(GC_Value_Guard);

    const REBYTE *label_str = Get_Word_Name(DSF_LABEL(call));
#endif

    const REBVAL * const func = DSF_FUNC(call);
    REBVAL *out = DSF_OUT(call);

    REBFLG threw;

    // We need to save what the DSF was prior to our execution, and
    // cannot simply use our frame's prior...because our frame's
    // prior call frame may be a *pending* frame that we do not want
    // to put in effect when we are finished.
    //
    struct Reb_Call *dsf_precall = DSF;
    SET_DSF(call);

    // Write some garbage (that won't crash the GC) into the `out` slot in
    // the debug build.  This helps to catch functions that do not
    // at some point intentionally write an output value into the slot.
    //
    // Note: if they use that slot for temp space, it subverts this check.
    SET_TRASH_SAFE(out);

    if (Trace_Flags) Trace_Func(DSF_LABEL(call), func);

    switch (VAL_TYPE(func)) {
    case REB_NATIVE:
        threw = Do_Native_Throws(func);
        break;
    case REB_ACTION:
        threw = Do_Action_Throws(func);
        break;
    case REB_COMMAND:
        threw = Do_Command_Throws(func);
        break;
    case REB_CLOSURE:
        threw = Do_Closure_Throws(func);
        break;
    case REB_FUNCTION:
        threw = Do_Function_Throws(func);
        break;
    case REB_ROUTINE:
        threw = Do_Routine_Throws(func);
        break;
    default:
        assert(FALSE);
    }

    // Function execution should have written *some* actual output value
    // over the trash that we put in the return slot before the call.
    assert(!IS_TRASH(out));

    assert(VAL_TYPE(out) < REB_MAX); // cheap check

    ASSERT_VALUE_MANAGED(out);

    assert(threw == THROWN(out));

#if !defined(NDEBUG)
    assert(DSP >= dsp_precall);
    if (DSP > dsp_precall) {
        PROBE_MSG(DSF_WHERE(call), "UNBALANCED STACK TRAP!!!");
        panic (Error(RE_MISC));
    }

    MANUALS_LEAK_CHECK(manuals_tail, cs_cast(label_str));

    assert(series_guard_tail == SERIES_TAIL(GC_Series_Guard));
    assert(value_guard_tail == SERIES_TAIL(GC_Value_Guard));
#endif

    SET_DSF(dsf_precall);
    Free_Call(call);

    return threw;
}


//
//  Do_Core: C
// 
// Evaluate the code block until we have:
//     1. An irreducible value (return next index)
//     2. Reached the end of the block (return END_FLAG)
//     3. Encountered an error
// 
// Index is a zero-based index into the block.
// Op indicates infix operator is being evaluated (precedence);
// The value (or error) is placed on top of the data stack.
//
// The interface includes flexibility for "APPLY-like scenarios" where the
// series contains *arguments* to what you want to run, though the actual
// function or otherwise is not resident in the series.  Hence the caller
// can seed the evaluative process with a `value` that may not be in the
// series at all (often, however, it will be at the head of the series).
//
// LOOKAHEAD:
// When we're in mid-dispatch of an infix function, the precedence
// is such that we don't want to do further infix lookahead while
// getting the arguments.  (e.g. with `1 + 2 * 3` we don't want
// infix `+` to look ahead past the 2 to see the infix `*`)
// 
// !!! IMPORTANT NOTE !!! => Changing the behavior of the function calling
// conventions and parameter fulfillment generally needs to mean changes to
// two other closely-related routines: Apply_Block_Throws() and
// Redo_Func_Throws().
//
void Do_Core(struct Reb_Do_State * const s)
{
    // Capture the data stack pointer on entry (used by debug checks, but
    // also refinements are pushed to stack and need to be checked if there
    // are any that are not processed)
    REBINT dsp_orig = DSP;

#if !defined(NDEBUG)
    static REBCNT count_static = 0;
    REBCNT count;
#endif

    REBFLG infix;

    enum Reb_Param_Mode mode; // before refinements, in refinement, skipping...

    // We use the convention that "param" refers to the word from the spec
    // of the function (a.k.a. the "formal" argument) and "arg" refers to
    // the evaluated value the function sees (a.k.a. the "actual" argument)
    REBVAL *param;
    REBVAL *arg;

    // The refinement currently being processed.  We have to remember its
    // address in case we want to revoke it.  (Note: can probably save on
    // having another pointer by merging with others above later.)
    REBVAL *refine;

#if !defined(NDEBUG)
    // Debug builds that are emulating the old behavior of writing NONE into
    // refinement args need to know when they should be writing these nones
    // or leaving what's there during PARAM_MODE_SCANNING/PARAM_MODE_SKIPPING
    REBFLG write_none;
#endif

    // First, check the input parameters (debug build only)

    // Though we can protect the value written into the target pointer 'out'
    // from GC during the course of evaluation, we can't protect the
    // underlying value from relocation.  Technically this would be a problem
    // for any series which might be modified while this call is running, but
    // most notably it applies to the data stack--where output used to always
    // be returned.
    //
#ifdef STRESS_CHECK_DO_OUT_POINTER
    ASSERT_NOT_IN_SERIES_DATA(s->out);
#else
    assert(!IN_DATA_STACK(s->out));
#endif

    assert(s->value);

    // logical xor: http://stackoverflow.com/a/1596970/211160
    assert(!(s->flags & DO_FLAG_NEXT) != !(s->flags & DO_FLAG_TO_END));
    assert(
        !(s->flags & DO_FLAG_LOOKAHEAD)
        != !(s->flags & DO_FLAG_NO_LOOKAHEAD)
    );

    assert(Is_Array_Series(s->array));

    // Only need to check this once (C stack size would be the same each
    // time this line is run if it were in a loop)
    if (C_STACK_OVERFLOWING(&s)) Trap_Stack_Overflow();

    // !!! We have to compensate for the passed-in index by subtracting 1,
    // as the looped form below needs an addition each time.  This means most
    // of the time we are subtracting one from a value the caller had to add
    // one to before passing in.  Also REBCNT is an unsigned number...and this
    // overlaps the NOT_FOUND value (which Do_Core does not use, but PARSE
    // does).  Not completely ideal, so review.
    --s->index;

do_at_index:
    assert(s->index != END_FLAG && s->index != THROWN_FLAG);
    SET_TRASH_SAFE(s->out);

#ifndef NDEBUG
    // This counter is helpful for tracking a specific invocation.
    // If you notice a crash, look on the stack for the topmost call
    // and read the count...then put that here and recompile with
    // a breakpoint set.  (The 'count_static' value is captured into a
    // local 'count' so you still get the right count after recursion.)
    //
    // We bound it at the max unsigned 32-bit because otherwise it would
    // roll over to zero and print a message that wasn't asked for, which
    // is annoying even in a debug build.
    //
    if (count_static < MAX_U32) {
        count = ++count_static;
        if (count ==
            // *** DON'T COMMIT THIS v-- KEEP IT AT ZERO! ***
                                      0
            // *** DON'T COMMIT THIS --^ KEEP IT AT ZERO! ***
        ) {
            Val_Init_Block_Index(&s->save, s->array, s->index);
            PROBE_MSG(&s->save, "Do_Core() count trap");
        }
    }
#endif

    if (Trace_Flags) Trace_Line(s->array, s->index, s->value);

reevaluate:
    if (--Eval_Count <= 0 || Eval_Signals) Do_Signals();

    assert(!THROWN(s->value));
    assert(!VAL_GET_OPT(s->value, OPT_VALUE_REEVALUATE));
    ASSERT_VALUE_MANAGED(s->value);

    // Someday it may be worth it to micro-optimize these null assignments
    // so they don't happen each time through the loop.
    s->label_sym = SYM_0;

    switch (VAL_TYPE(s->value)) {

    case REB_END:
        // This means evaluation is over, regardless of whether it was a
        // DO/NEXT or not...no need to check infix, etc.
        SET_UNSET(s->out);
        s->index = END_FLAG;
        goto return_index;

    case REB_WORD:
        GET_VAR_INTO(s->out, s->value);

    do_fetched_word:
        if (IS_UNSET(s->out)) fail (Error(RE_NO_VALUE, s->value));

        if (ANY_FUNC(s->out)) {
            // We can only acquire an infix operator's first arg during the
            // "lookahead".  Here we are starting a brand new expression.
            if (VAL_GET_EXT(s->out, EXT_FUNC_INFIX))
                fail (Error(RE_NO_OP_ARG, s->value));

            s->label_sym = VAL_WORD_SYM(s->value);

            // `do_function_args` expects the function to be in `value`
            s->value = s->out;

            if (Trace_Flags) Trace_Line(s->array, s->index, s->value);
            goto do_function_args;
        }

    #if !defined(NDEBUG)
        if (LEGACY(OPTIONS_LIT_WORD_DECAY) && IS_LIT_WORD(s->out))
            VAL_SET(s->out, REB_WORD);
    #endif

        s->index++;
        break;

    case REB_SET_WORD:
        // `index` and `out` are modified
        DO_NEXT_MAY_THROW_CORE(
            s->index, s->out, s->array, s->index + 1, DO_FLAG_LOOKAHEAD
        );

        if (s->index == THROWN_FLAG) goto return_index;

        if (s->index == END_FLAG) {
            // `do [x:]` is not as purposefully an assignment of an unset as
            // something like `do [x: ()]`, so it's an error.
            assert(IS_UNSET(s->out));
            fail (Error(RE_NEED_VALUE, s->value));
        }

        if (IS_UNSET(s->out)) {
            // Treat direct assignments of an unset as unsetting the word
            REBVAL *var;

        #if !defined(NDEBUG)
            if (LEGACY(OPTIONS_CANT_UNSET_SET_WORDS))
                fail (Error(RE_NEED_VALUE, s->value));
        #endif

            if (!HAS_FRAME(s->value)) fail (Error(RE_NOT_BOUND, s->value));

            var = GET_MUTABLE_VAR(s->value);
            SET_UNSET(var);
        }
        else
            Set_Var(s->value, s->out);
        break;

    case REB_NATIVE:
    case REB_ACTION:
    case REB_COMMAND:
    case REB_CLOSURE:
    case REB_FUNCTION:

        // If we come across an infix function from do_at_index in the loop,
        // we can't actually run it.  It only runs after an evaluation has
        // yielded a value as part of a single "atomic" Do/Next step

        if (VAL_GET_EXT(s->value, EXT_FUNC_INFIX))
            fail (Error(RE_NO_OP_ARG, s->value));

        // Note: Because this is a function value being hit literally in
        // a block, it does not have a name.  Use symbol of its VAL_TYPE

        s->label_sym = SYM_FROM_KIND(VAL_TYPE(s->value));

    // Value must be the function when a jump here occurs
    do_function_args:
        assert(ANY_FUNC(s->value));
        s->index++;

        // There may be refinements pushed to the data stack to process, if
        // the call originated from a path dispatch.
        assert(DSP >= dsp_orig);

        // `out` may contain the pending argument for an infix operation,
        // and it could also be the backing store of the `value` pointer
        // to the function.  So Make_Call() shouldn't overwrite it!
        //
        // Note: Although we create the call frame here, we cann't "put
        // it into effect" until all the arguments have been computed.
        // This is because recursive stack-relative bindings would wind
        // up reading variables out of the frame while it is still
        // being built, and that would be bad.

        s->call = Make_Call(
            s->out, s->array, s->index, s->label_sym, s->value
        );

        // Make_Call() put a safe copy of the function value into the
        // call frame.  Refresh our value to point to that one (instead of
        // where it was possibly lingering in the `out` slot).

        s->value = DSF_FUNC(s->call);
        assert(ANY_FUNC(s->value));
        infix = VAL_GET_EXT(s->value, EXT_FUNC_INFIX);

        // If there are no arguments, just skip the next section
        if (DSF_ARGC(s->call) == 0) goto function_ready_to_call;

        // We assume you can enumerate both the formal parameters (in the
        // spec) and the actual arguments (in the call frame) using pointer
        // incrementation, that they are both terminated by REB_END, and
        // that there are an equal number of values in both.
        param = VAL_FUNC_PARAM(s->value, 1);
        arg = DSF_ARG(s->call, 1);

        // Fetch the first argument from output slot before overwriting
        // !!! Redundant check on REB_PATH branch (knows it's not infix)
        if (infix) {
            assert(s->index != 0);

            // If func is being called infix, prior evaluation loop has
            // already computed first argument, so it's sitting in `out`
            *arg = *s->out;
            if (!TYPE_CHECK(param, VAL_TYPE(arg)))
                fail (Error_Arg_Type(s->call, param, Type_Of(arg)));

            param++;
            arg++;
        }

        // This loop goes through the parameter and argument slots, filling in
        // the arguments via recursive calls to the evaluator.
        //
        // Note that Make_Call initialized them all to UNSET.  This is needed
        // in order to allow skipping around, in particular so that a
        // refinement slot can be marked as processed or not processed, but
        // also because the garbage collector has to consider the slots "live"
        // as arguments are progressively fulfilled.

        mode = PARAM_MODE_NORMAL;

    #if !defined(NDEBUG)
        write_none = FALSE;
    #endif

        for (; NOT_END(param); param++, arg++) {
        no_advance:
            assert(IS_TYPESET(param));

            // *** PURE LOCALS => continue ***

            if (VAL_GET_EXT(param, EXT_WORD_HIDE)) {
                // When the spec contained a SET-WORD!, that was a "pure
                // local".  It corresponds to no argument and will not
                // appear in WORDS-OF.  Unlike /local, it cannot be used
                // for "locals injection".  Helpful when writing generators
                // because you don't have to go find /local (!), you can
                // really put it wherever is convenient--no position rule.

                // A special trick for functions marked EXT_FUNC_HAS_RETURN
                // puts a "magic" REBNATIVE(return) value into the arg slot
                // for pure locals named RETURN: ....used by FUNC and CLOS

                assert(SYM_RETURN == SYMBOL_TO_CANON(SYM_RETURN));

                if (
                    VAL_GET_EXT(s->value, EXT_FUNC_HAS_RETURN)
                    && SYMBOL_TO_CANON(VAL_TYPESET_SYM(param)) == SYM_RETURN
                ) {
                    *arg = *ROOT_RETURN_NATIVE;
                    VAL_FUNC_RETURN_TO(arg) = VAL_FUNC_PARAMLIST(s->value);
                }
                // otherwise leave it unset

                continue;
            }

            if (!VAL_GET_EXT(param, EXT_TYPESET_REFINEMENT)) {
                // Hunting a refinement?  Quickly disregard this if we are
                // doing such a scan and it isn't a refinement.
                if (mode == PARAM_MODE_SCANNING) {
                #if !defined(NDEBUG)
                    if (write_none)
                        SET_NONE(arg);
                #endif
                    continue;
                }
            }
            else {
                // *** REFINEMENTS => continue ***

                // Refinements are tricky because users can write:
                //
                //     foo: func [a /b c /d e] [...]
                //
                //     foo/b/d (1 + 2) (3 + 4) (5 + 6)
                //     foo/d/b (1 + 2) (3 + 4) (5 + 6)
                //
                // But we are marching across the parameters in order of their
                // *definition*.  Hence we may have to seek refinements ahead
                // or behind to know where to put the results we evaluate.

                if (mode == PARAM_MODE_SCANNING) {
                    // Note that we have already canonized the path words for
                    // a case-insensitive-comparison to the symbol in the
                    // function's paramlist.  While it might be tempting to
                    // canonize those too, they should retain their original
                    // case for when that symbol is given back to the user to
                    // indicate a used refinement.

                    if (
                        // Already canonized the word on stack when pushing
                        VAL_WORD_SYM(DS_TOP)
                        == SYMBOL_TO_CANON(VAL_TYPESET_SYM(param))
                    ) {
                        // If we seeked backwards to find a refinement and it's
                        // the one we are looking for, "consume" it off the
                        // data stack to say we found it in the frame
                        DS_DROP;

                        // Now as we switch to pending mode, change refine to
                        // point at the arg slot so we can revoke it if needed
                        mode = PARAM_MODE_REFINE_PENDING;
                        refine = arg;

                    #if !defined(NDEBUG)
                        write_none = FALSE;

                        if (TYPE_CHECK(param, REB_LOGIC)) {
                            // OPTIONS_REFINEMENTS_TRUE at function create
                            SET_TRUE(refine);
                        }
                        else
                    #endif
                            Val_Init_Word_Unbound(
                                refine, REB_WORD, VAL_TYPESET_SYM(param)
                            );

                        continue;
                    }

                    // ...else keep scanning, but if it's unset then set it
                    // to none because we *might* not revisit this spot again.
                    if (IS_UNSET(arg)) {
                        SET_NONE(arg);

                    #if !defined(NDEBUG)
                        if (TYPE_CHECK(param, REB_LOGIC))
                            write_none = TRUE;
                    #endif
                    }
                    else {
                    #if !defined(NDEBUG)
                        write_none = FALSE;
                    #endif
                    }
                    continue;
                }

                if (dsp_orig == DSP) {
                    // No refinements are left on the data stack, so if this
                    // refinement slot is still unset, skip the args and leave
                    // them as unsets (or set nones in legacy mode)
                    mode = PARAM_MODE_SKIPPING;
                    if (IS_UNSET(arg)) {
                        SET_NONE(arg);

                    #if !defined(NDEBUG)
                        // We need to know if we need to write nones into args
                        // based on the legacy switch capture.
                        if (TYPE_CHECK(param, REB_LOGIC))
                            write_none = TRUE;
                    #endif
                    }

                    continue;
                }

                // Should have only pushed words to the stack (or there would
                // have been an earlier error)
                assert(IS_WORD(DS_TOP));

                if (
                    // Already canonized the word on the stack when pushing
                    VAL_WORD_SYM(DS_TOP)
                    == SYMBOL_TO_CANON(VAL_TYPESET_SYM(param))
                ) {
                    // We were lucky and the next refinement we wish to
                    // process lines up with this parameter slot.

                    mode = PARAM_MODE_REFINE_PENDING;
                    refine = arg;

                    DS_DROP;

                    #if !defined(NDEBUG)
                        if (TYPE_CHECK(param, REB_LOGIC)) {
                            // OPTIONS_REFINEMENTS_TRUE at function create
                            SET_TRUE(refine);
                        }
                        else
                    #endif
                            Val_Init_Word_Unbound(
                                refine, REB_WORD, VAL_TYPESET_SYM(param)
                            );

                    continue;
                }

                // We weren't lucky and need to scan
                mode = PARAM_MODE_SCANNING;

                assert(IS_WORD(DS_TOP));

                // We have to reset to the beginning if we are going to scan,
                // because we might have gone past the refinement on a prior
                // scan.  (Review if a bit might inform us if we've ever done
                // a scan before to get us started going out of order.  If not,
                // we would only need to look ahead.)

                param = VAL_FUNC_PARAM(s->value, 1);
                arg = DSF_ARG(s->call, 1);

            #if !defined(NDEBUG)
                write_none = FALSE;
            #endif

                // We might have a function with no normal args, where a
                // refinement is the first parameter...and we don't want to
                // run the loop's arg++/param++ we get if we `continue`
                goto no_advance;
            }

            if (mode == PARAM_MODE_SKIPPING) {
            #if !defined(NDEBUG)
                // In release builds we just skip because the args are already
                // UNSET.  But in debug builds we may need to overwrite the
                // unset default with none if this function was created while
                // legacy mode was on.
                if (write_none)
                    SET_NONE(arg);
            #endif
                continue;
            }

            assert(
                mode == PARAM_MODE_NORMAL
                || mode == PARAM_MODE_REFINE_PENDING
                || mode == PARAM_MODE_REFINE_ARGS
                || mode == PARAM_MODE_REVOKING
            );

            // *** QUOTED OR EVALUATED ITEMS ***

            if (VAL_GET_EXT(param, EXT_TYPESET_QUOTE)) {
                // Using a GET-WORD! in the function spec indicates that you
                // would like that argument to be EXT_TYPESET_QUOTE, e.g.
                // not evaluated at the callsite:
                //
                //     >> foo: function [:a] [print [{a is} a]
                //
                //     >> foo 1 + 2
                //     a is 1
                //
                //     >> foo (1 + 2)
                //     a is (1 + 2)
                //
                // Using a LIT-WORD in the function spec indicates that args
                // should be EXT_TYPESET_QUOTE but also EXT_TYPESET_EVALUATE
                // so that if they are "gets" or parens they still run:
                //
                //     >> foo: function ['a] [print [{a is} a]
                //
                //     >> foo 1 + 2
                //     a is 1
                //
                //     >> foo (1 + 2)
                //     a is 3
                //
                // This provides a convenient escape mechanism for the caller
                // to subvert quote-like behavior (which is an option that
                // one generally would like to give in a quote-like API).
                //
                // A special allowance is made that if a function quotes its
                // argument and the parameter is at the end of a series,
                // it will be treated as an UNSET!  (This is how HELP manages
                // to act as an arity 1 function as well as an arity 0 one.)
                // But to use this feature it must also explicitly accept
                // the UNSET! type (checked after the switch).
                //
                if (s->index < BLK_LEN(s->array)) {
                    REBVAL * const quoted = BLK_SKIP(s->array, s->index);
                    if (
                        VAL_GET_EXT(param, EXT_TYPESET_EVALUATE)
                        && (
                            IS_PAREN(quoted)
                            || IS_GET_WORD(quoted)
                            || IS_GET_PATH(quoted)
                        )
                    ) {
                        DO_NEXT_MAY_THROW_CORE(
                            s->index,
                            arg,
                            s->array,
                            s->index,
                            infix ? DO_FLAG_NO_LOOKAHEAD : DO_FLAG_LOOKAHEAD
                        );
                        if (s->index == THROWN_FLAG) {
                            *s->out = *arg;
                            Free_Call(s->call);
                            // If we have refinements pending on the data
                            // stack we need to balance those...
                            DS_DROP_TO(dsp_orig);
                            goto return_index;
                        }
                        if (s->index == END_FLAG) {
                            // This is legal due to the series end UNSET! trick
                            // (we will type check if unset is actually legal
                            // in a moment below)
                            assert(IS_UNSET(arg));
                        }
                    }
                    else {
                        s->index++;
                        *arg = *quoted;
                    }
                }
                else {
                    // series end UNSET! trick; already unset...
                }
            }
            else {
                // !!! Note: ROUTINE! does not set any bits on the symbols
                // and will need to be made to...
                //
                // assert(VAL_GET_EXT(param, EXT_TYPESET_EVALUATE));

                // An ordinary WORD! in the function spec indicates that you
                // would like that argument to be evaluated normally.
                //
                //     >> foo: function [a] [print [{a is} a]
                //
                //     >> foo 1 + 2
                //     a is 3
                //
                DO_NEXT_MAY_THROW_CORE(
                    s->index,
                    arg,
                    s->array,
                    s->index,
                    infix ? DO_FLAG_NO_LOOKAHEAD : DO_FLAG_LOOKAHEAD
                );
                if (s->index == THROWN_FLAG) {
                    *s->out = *arg;
                    Free_Call(s->call);
                    // If we have refinements pending on the data
                    // stack we need to balance those...
                    DS_DROP_TO(dsp_orig);
                    goto return_index;
                }
                if (s->index == END_FLAG)
                    fail (Error_No_Arg(DSF_LABEL(s->call), param));
            }

            ASSERT_VALUE_MANAGED(arg);

            if (IS_UNSET(arg)) {
                if (mode == PARAM_MODE_REFINE_ARGS)
                    fail (Error(RE_BAD_REFINE_REVOKE));
                else if (mode == PARAM_MODE_REFINE_PENDING) {
                    mode = PARAM_MODE_REVOKING;

                #if !defined(NDEBUG)
                    // We captured the "true-valued-refinements" legacy switch
                    // and decided whether to make the refinement true or the
                    // word value of the refinement.  We use the same switch
                    // to cover whether the unused args are NONE or UNSET...
                    // but decide which it is by looking at what was filled
                    // in for the refinement.  UNSET is the default.

                    if (!IS_WORD(refine)) {
                        assert(IS_LOGIC(refine));
                        SET_NONE(arg);
                    }
                #endif

                    // Revoke the refinement.
                    SET_NONE(refine);
                }
                else if (mode == PARAM_MODE_REVOKING) {
                #ifdef NDEBUG
                    // Don't have to do anything, it's unset
                #else
                    // We have overwritten the refinement so we don't know if
                    // it was LOGIC! or WORD!, but we don't need to know that
                    // because we can just copy whatever the previous arg
                    // was set to...
                    *arg = *(arg - 1);
                #endif
                }
            }
            else {
                if (mode == PARAM_MODE_REVOKING)
                    fail (Error(RE_BAD_REFINE_REVOKE));
                else if (mode == PARAM_MODE_REFINE_PENDING)
                    mode = PARAM_MODE_REFINE_ARGS;
            }

            // If word is typed, verify correct argument datatype:
            if (mode != PARAM_MODE_REVOKING)
                if (!TYPE_CHECK(param, VAL_TYPE(arg)))
                    fail (Error_Arg_Type(s->call, param, Type_Of(arg)));
        }

        // If we were scanning and didn't find the refinement we were looking
        // for, then complain with an error.
        //
        // !!! This will complain differently than a proper "REFINED!" strategy
        // would complain, because if you do:
        //
        //     append/(second [only asdhjas])/(print "hi") [a b c] [d]
        //
        // ...it would never make it to the print.  Here we do all the path
        // and paren evals up front and check that things are words or NONE,
        // not knowing if a refinement isn't on the function until the end.

        if (mode == PARAM_MODE_SCANNING)
            fail (Error(RE_BAD_REFINE, DS_TOP));

        // In the case where the user has said foo/bar/baz, and bar was later
        // in the spec than baz, then we will have passed it.  We need to
        // restart the scan (which may wind up failing)

        if (DSP != dsp_orig) {
            mode = PARAM_MODE_SCANNING;
            param = VAL_FUNC_PARAM(s->value, 1);
            arg = DSF_ARG(s->call, 1);

        #if !defined(NDEBUG)
            write_none = FALSE;
        #endif

            goto no_advance;
        }

    function_ready_to_call:
        // Execute the function with all arguments ready
        if (Dispatch_Call_Throws(s->call)) {
            s->index = THROWN_FLAG;
            goto return_index;
        }

        if (Trace_Flags) Trace_Return(s->label_sym, s->out);

        if (VAL_GET_OPT(s->out, OPT_VALUE_REEVALUATE)) {
            // The return value came from EVAL and we need to "activate" it.
            //
            // !!! As EVAL is the only way this can ever happen, the test
            // could be if the function is a NATIVE! and holds the C
            // function pointer to REBNATIVE(eval)--then reclaim the bit.

            VAL_CLR_OPT(s->out, OPT_VALUE_REEVALUATE);

            // The next evaluation we invoke expects to be able to write into
            // `out` (and not have `value` living in there), so move it!
            s->save = *s->out;
            s->value = &s->save;

            // act "as if" value had been in the last position of the last
            // function argument evaluated (or the function itself if no args)
            s->index--;

            goto reevaluate;
        }
        break;

    case REB_PATH:
        if (Do_Path_Throws(s->out, &s->label_sym, s->value, 0)) {
            s->index = THROWN_FLAG;
            goto return_index;
        }

        if (ANY_FUNC(s->out)) {
            // object/func or func/refinements or object/func/refinement

            // Because we passed in a label symbol, the path evaluator was
            // willing to assume we are going to invoke a function if it
            // is one.  Hence it left any potential refinements on data stack.

            assert(DSP >= dsp_orig);

            s->value = s->out;

            // Cannot handle infix because prior value is wiped out above
            // (Theoretically we could save it if we are DO-ing a chain of
            // values, and make it work.  But then, a loop of DO/NEXT
            // may not behave the same as DO-ing the whole block.  Bad.)

            if (VAL_GET_EXT(s->value, EXT_FUNC_INFIX))
                fail (Error_Has_Bad_Type(s->value));

            goto do_function_args;
        }
        else {
            // Path should have been fully processed, no refinements on stack
            assert(DSP == dsp_orig);

            s->index++;
        }
        break;

    case REB_GET_PATH:
        // returns in word the path item, DS_TOP has value
        if (Do_Path_Throws(s->out, NULL, s->value, NULL)) {
            s->index = THROWN_FLAG;
            goto return_index;
        }

        // We did not pass in a symbol ID
        assert(DSP == dsp_orig);

        s->index++;
        break;

    case REB_SET_PATH:
        // We want the result of the set path to wind up in `out`, so go
        // ahead and put the result of the evaluation there.  Do_Path_Throws
        // will *not* put this value in the output when it is making the
        // variable assignment!
        DO_NEXT_MAY_THROW_CORE(
            s->index, s->out, s->array, s->index + 1, DO_FLAG_LOOKAHEAD
        );

        assert(s->index != END_FLAG || IS_UNSET(s->out)); // unset if END_FLAG
        if (IS_UNSET(s->out)) fail (Error(RE_NEED_VALUE, s->value));
        if (s->index == THROWN_FLAG) goto return_index;

        if (Do_Path_Throws(&s->save, NULL, s->value, s->out)) {
            s->index = THROWN_FLAG;
            goto return_index;
        }

        // We did not pass in a symbol, so not a call... hence we cannot
        // process refinements.  Should not get any back.
        assert(DSP == dsp_orig);
        break;

    case REB_PAREN:
        if (DO_ARRAY_THROWS(s->out, s->value)) {
            s->index = THROWN_FLAG;
            goto return_index;
        }
        s->index++;
        break;

    case REB_LIT_WORD:
        *s->out = *s->value;
        VAL_SET(s->out, REB_WORD);
        s->index++;
        break;

    case REB_GET_WORD:
        GET_VAR_INTO(s->out, s->value);
        s->index++;
        break;

    case REB_LIT_PATH:
        // !!! Aliases a REBSER under two value types, likely bad, see CC#2233
        *s->out = *s->value;
        VAL_SET(s->out, REB_PATH);
        s->index++;
        break;

    case REB_FRAME:
        // !!! Frame should be hidden from user visibility
        panic (Error(RE_BAD_EVALTYPE, Get_Type(VAL_TYPE(s->value))));

    default:
        // Most things just evaluate to themselves
        assert(!IS_TRASH(s->value));
        *s->out = *s->value;
        s->index++;
        break;
    }

    if (s->index >= BLK_LEN(s->array)) {
        // When running a DO/NEXT, clients may wish to distinguish between
        // running a step that evaluated to an unset vs. one that was actually
        // at the end of a block (e.g. implementing `[x:]` vs. `[x: ()]`).
        // But if running to the end, it's best to go ahead and flag
        // completion as soon as possible.

        if (s->flags & DO_FLAG_TO_END)
            s->index = END_FLAG;
        goto return_index;
    }

    // Should not have accumulated any net data stack during the evaluation
    assert(DSP == dsp_orig);

    // Should not have a THROWN value if we got here
    assert(s->index != THROWN_FLAG && !THROWN(s->out));

    if (s->flags & DO_FLAG_LOOKAHEAD) {
        s->value = BLK_SKIP(s->array, s->index);

        // Literal infix function values may occur.
        if (VAL_GET_EXT(s->value, EXT_FUNC_INFIX)) {
            // !!! Not true, should be OP! when it is reinstated
            s->label_sym = SYM_NATIVE;

            if (Trace_Flags) Trace_Line(s->array, s->index, s->value);
            goto do_function_args;
        }

        if (IS_WORD(s->value)) {
            // WORD! values may look up to an infix function

            GET_VAR_INTO(&s->save, s->value);
            if (VAL_GET_EXT(&s->save, EXT_FUNC_INFIX)) {
                s->label_sym = VAL_WORD_SYM(s->value);
                s->value = &s->save;
                if (Trace_Flags) Trace_Line(s->array, s->index, s->value);
                goto do_function_args;
            }

            // Perhaps not an infix function, but we just paid for a variable
            // lookup.  If this isn't just a DO/NEXT, use the work!
            if (s->flags & DO_FLAG_TO_END) {
                *s->out = s->save;
                goto do_fetched_word;
            }
        }
    }
    else {
        // We do not look ahead for infix dispatch if we are currently
        // processing an infix operation with higher precedence
    }

    // Continue evaluating rest of block if not just a DO/NEXT
    if (s->flags & DO_FLAG_TO_END) goto do_at_index;

return_index:
    assert(DSP == dsp_orig);

#if !defined(NDEBUG)
    if (s->index < BLK_LEN(s->array))
        assert(s->index != END_FLAG);

    if (s->flags & DO_FLAG_TO_END)
        assert(s->index == THROWN_FLAG || s->index == END_FLAG);
#endif

    assert((s->index == THROWN_FLAG) == THROWN(s->out));

    assert(!IS_TRASH(s->out));
    assert(VAL_TYPE(s->out) < REB_MAX); // cheap check

    // Caller needs to inspect `index`, at minimum to know if it's THROWN_FLAG
}


//
//  Do_At_Throws: C
//
// Do_At_Throws behaves "as if" it is performing iterated calls to DO_NEXT.
// (Under the hood it is actually more efficient than doing so.)
//
// It is named the way it is because it's expected to usually be used in an
// 'if' statement.  It cues you into realizing that it returns TRUE if a
// THROW interrupts this current DO_BLOCK execution--not asking about a
// "THROWN" that happened as part of a prior statement.
//
// If it returns FALSE, then the DO completed successfully to end of input
// without a throw...and the output contains the last value evaluated in the
// block (empty blocks give UNSET!).  If it returns TRUE then it will be the
// THROWN() value.
//
// A raised error via a `fail` will longjmp and not return to the caller
// normally.  If interested in catching these, use PUSH_TRAP().
//
REBFLG Do_At_Throws(REBVAL *out, REBSER *array, REBCNT index)
{
    struct Reb_Do_State s;
    s.out = out;
    s.array = array;

    // don't suppress the infix lookahead
    s.flags = DO_FLAG_LOOKAHEAD | DO_FLAG_TO_END;

    // We always seed the evaluator with an initial value.  It isn't required
    // to be resident in the same series, in order to facilitate an APPLY-like
    // situation of putting a path or otherwise at the head.  We aren't doing
    // that, so we just grab the pointer and advance.
    s.value = BLK_SKIP(array, index);
    s.index = index + 1;

    Do_Core(&s);
    assert(s.index == THROWN_FLAG || s.index == END_FLAG);

    return THROWN_FLAG == s.index;
}


//
//  Reduce_Block_Throws: C
// 
// Reduce block from the index position specified in the value.
// Collect all values from stack and make them a block.
//
REBFLG Reduce_Block_Throws(REBVAL *out, REBSER *block, REBCNT index, REBOOL into)
{
    REBINT dsp_orig = DSP;

    while (index < BLK_LEN(block)) {
        REBVAL reduced;
        DO_NEXT_MAY_THROW(index, &reduced, block, index);
        if (index == THROWN_FLAG) {
            *out = reduced;
            DS_DROP_TO(dsp_orig);
            return TRUE;
        }
        DS_PUSH(&reduced);
    }

    Pop_Stack_Values(out, dsp_orig, into);
    return FALSE;
}


//
//  Reduce_Only: C
// 
// Reduce only words and paths not found in word list.
//
void Reduce_Only(REBVAL *out, REBSER *block, REBCNT index, REBVAL *words, REBOOL into)
{
    REBINT dsp_orig = DSP;
    REBVAL *val;
    const REBVAL *v;
    REBSER *ser = 0;
    REBCNT idx = 0;

    if (IS_BLOCK(words)) {
        ser = VAL_SERIES(words);
        idx = VAL_INDEX(words);
    }

    for (val = BLK_SKIP(block, index); NOT_END(val); val++) {
        if (IS_WORD(val)) {
            // Check for keyword:
            if (ser && NOT_FOUND != Find_Word(ser, idx, VAL_WORD_CANON(val))) {
                DS_PUSH(val);
                continue;
            }
            v = GET_VAR(val);
            DS_PUSH(v);
        }
        else if (IS_PATH(val)) {
            if (ser) {
                // Check for keyword/path:
                v = VAL_BLK_DATA(val);
                if (IS_WORD(v)) {
                    if (NOT_FOUND != Find_Word(ser, idx, VAL_WORD_CANON(v))) {
                        DS_PUSH(val);
                        continue;
                    }
                }
            }

            // pushes val on stack
            DS_PUSH_TRASH_SAFE;
            if (Do_Path_Throws(DS_TOP, NULL, val, NULL))
                fail (Error_No_Catch_For_Throw(DS_TOP));
        }
        else DS_PUSH(val);
        // No need to check for unwinds (THROWN) here, because unwinds should
        // never be accessible via words or paths.
    }

    Pop_Stack_Values(out, dsp_orig, into);

    assert(DSP == dsp_orig);
}


//
//  Reduce_Block_No_Set_Throws: C
//
REBFLG Reduce_Block_No_Set_Throws(REBVAL *out, REBSER *block, REBCNT index, REBOOL into)
{
    REBINT dsp_orig = DSP;

    while (index < BLK_LEN(block)) {
        REBVAL *value = BLK_SKIP(block, index);
        if (IS_SET_WORD(value)) {
            DS_PUSH(value);
            index++;
        }
        else {
            REBVAL reduced;
            DO_NEXT_MAY_THROW(index, &reduced, block, index);
            if (index == THROWN_FLAG) {
                *out = reduced;
                DS_DROP_TO(dsp_orig);
                return TRUE;
            }
            DS_PUSH(&reduced);
        }
    }

    Pop_Stack_Values(out, dsp_orig, into);

    return FALSE;
}


//
//  Compose_Block_Throws: C
// 
// Compose a block from a block of un-evaluated values and
// paren blocks that are evaluated.  Performs evaluations, so
// if 'into' is provided, then its series must be protected from
// garbage collection.
// 
//     deep - recurse into sub-blocks
//     only - parens that return blocks are kept as blocks
// 
// Writes result value at address pointed to by out.
//
REBFLG Compose_Block_Throws(REBVAL *out, REBVAL *block, REBFLG deep, REBFLG only, REBOOL into)
{
    REBVAL *value;
    REBINT dsp_orig = DSP;

    for (value = VAL_BLK_DATA(block); NOT_END(value); value++) {
        if (IS_PAREN(value)) {
            REBVAL evaluated;

            if (DO_ARRAY_THROWS(&evaluated, value)) {
                *out = evaluated;
                DS_DROP_TO(dsp_orig);
                return TRUE;
            }

            if (IS_BLOCK(&evaluated) && !only) {
                // compose [blocks ([a b c]) merge] => [blocks a b c merge]
                Push_Stack_Values(
                    cast(REBVAL*, VAL_BLK_DATA(&evaluated)),
                    VAL_BLK_LEN(&evaluated)
                );
            }
            else if (!IS_UNSET(&evaluated)) {
                // compose [(1 + 2) inserts as-is] => [3 inserts as-is]
                // compose/only [([a b c]) unmerged] => [[a b c] unmerged]
                DS_PUSH(&evaluated);
            }
            else {
                // compose [(print "Unsets *vanish*!")] => []
            }
        }
        else if (deep) {
            if (IS_BLOCK(value)) {
                // compose/deep [does [(1 + 2)] nested] => [does [3] nested]
                REBVAL composed;

                if (Compose_Block_Throws(&composed, value, TRUE, only, into)) {
                    *out = composed;
                    DS_DROP_TO(dsp_orig);
                    return TRUE;
                }

                DS_PUSH(&composed);
            }
            else {
                DS_PUSH(value);
                if (ANY_ARRAY(value)) {
                    // compose [copy/(orig) (copy)] => [copy/(orig) (copy)]
                    // !!! path and second paren are copies, first paren isn't
                    VAL_SERIES(DS_TOP) = Copy_Array_Shallow(VAL_SERIES(value));
                    MANAGE_SERIES(VAL_SERIES(DS_TOP));
                }
            }
        }
        else {
            // compose [[(1 + 2)] (reverse "wollahs")] => [[(1 + 2)] "shallow"]
            DS_PUSH(value);
        }
    }

    Pop_Stack_Values(out, dsp_orig, into);

    return FALSE;
}


//
//  Apply_Func_Core: C
//
// (va_list by pointer: http://stackoverflow.com/a/3369762/211160)
//
// Applies function from args provided by C call.  Although the
// function knows how many total args + refinements + refinement
// args it takes, it's desirable to be able to pass fewer and
// have the remainder padded with NONE! + UNSET! as appropriate.
// So the convention used is that the varargs must be NULL-terminated.
//
// Type checking is performed.  This is the mechanism for calling Rebol
// functions from C (such as system support functions).
// 
// Refinements are processed according to the positions in which they
// were defined in the function spec.  So for instance, /ONLY is in
// the 5th spot of APPEND (after /PART and its LIMIT).
//
// Any conditionally true value in a refinement position means the
// refinement will be considered present, while conditional
// falsehood means absent.  When the function runs, a present
// refinement's variable will hold its WORD! name unbound
// (for chaining, while still testing as TRUE?).  Absent refinements
// will be seen as UNSET! from within the function body.
// 
// The varargs must fulfill the required args, and cannot cut off in the
// middle of refinement args to anything it has started.  But it may stop
// supplying arguments at the boundary of any transition to another
// refinement.
// 
// * If there are more processed values in the block than arguments,
// that will trigger an error.
// 
// * Infix functions are allowed, with the first item in the block
// presumed to be the left-hand side of the call.
// 
// The boolean result will be TRUE if an argument eval or the call
// created a THROWN() value, with the thrown value in `out`.
// Otheriwse it returns FALSE and `out` will hold the evaluation.
// 
// !!! NOTE: This code currently does not process revoking of
// refinements via unset as the path-based dispatch does in the
// Do_Core evaluator.
//
REBFLG Apply_Func_Core(REBVAL *out, const REBVAL *func, va_list *varargs)
{
    struct Reb_Call *call;

    REBVAL *param; // typeset parameter (w/symbol) in function words list
    REBVAL *arg; // value argument slot to fill in call frame for `param`

    // before refinements, in refinement, skipping...
    enum Reb_Param_Mode mode = PARAM_MODE_NORMAL;

    REBVAL *refine; // the refinement we may need to go back and revoke

    REBSER *block;
    REBCNT index;

    // For debug backtracing, the evaluator wants to know what our
    // block and position are.  We have to make something up, because
    // this call is originating from C code (not Rebol code).

    if (DSF) {
        // Some function is on the stack, so fabricate our execution
        // position by copying the block and position it was at.

        block = VAL_SERIES(DSF_WHERE(DSF));
        index = VAL_INDEX(DSF_WHERE(DSF));
    }
    else if (IS_FUNCTION(func) || IS_CLOSURE(func)) {
        // Stack is empty, so offer up the body of the function itself
        // (if it has a body!)

        block = VAL_FUNC_BODY(func);
        index = 0;
    }
    else {
        // We got nothin'.  Give back the specially marked "top level"
        // empty block just to provide something in the slot
        // !!! Could use more sophisticated backtracing in general

        block = EMPTY_ARRAY;
        index = 0;
    }

    assert(ANY_FUNC(func));
    assert(index < SERIES_TAIL(block));

    // !!! Better symbol to use?
    call = Make_Call(out, block, index, SYM_NATIVE, func);

    assert(VAL_FUNC_NUM_PARAMS(func) == DSF_ARGC(call));

    // Get first parameter (or a REB_END if no parameters)
    if (VAL_FUNC_NUM_PARAMS(func) > 0)
        param = VAL_FUNC_PARAM(func, 1);
    else
        param = END_VALUE; // triggers `too_many` if loop is entered

    // Get slot to write actual argument for first parameter into (or NULL)
    arg = (DSF_ARGC(call) > 0) ? DSF_ARG(call, 1) : NULL;

    for (; varargs || index < BLK_LEN(block); param++, arg++) {
        REBVAL* value = va_arg(*varargs, REBVAL*);
        if (!value) break; // our convention is "NULL signals no more"

        *arg = *value;

        // *** PURE LOCALS => continue ***

        while (VAL_GET_EXT(param, EXT_WORD_HIDE)) {
            // We need to skip over "pure locals", e.g. those created in
            // the spec with a SET-WORD!.  (They are useful for generators)
            //
            // The special exception is a RETURN: local, if it is "magic"
            // (e.g. FUNC and CLOS made it).  Then it gets a REBNATIVE(return)
            // that is "magic" and knows to return to *this* function...

            if (
                VAL_GET_EXT(func, EXT_FUNC_HAS_RETURN)
                && SAME_SYM(VAL_TYPESET_SYM(param), SYM_RETURN)
            ) {
                *arg = *ROOT_RETURN_NATIVE;
                VAL_FUNC_RETURN_TO(arg) = VAL_FUNC_PARAMLIST(func);
            }
            else {
                // Leave as unset.
            }

            // We aren't actually consuming the evaluated item in the block,
            // so keep advancing the parameter as long as it's a pure local.
            param++;
            if (IS_END(param))
                fail (Error(RE_APPLY_TOO_MANY));
        }

        // *** REFINEMENT => continue ***

        if (VAL_GET_EXT(param, EXT_TYPESET_REFINEMENT)) {
            // If we've found a refinement, this resets our ignore state
            // based on whether or not the arg suggests it is enabled

            refine = arg;

            if (IS_CONDITIONAL_TRUE(arg)) {
                mode = PARAM_MODE_REFINE_PENDING;

                Val_Init_Word_Unbound(arg, REB_WORD, VAL_TYPESET_SYM(param));
            }
            else {
                mode = PARAM_MODE_SKIPPING;
                SET_NONE(arg);
            }
            continue;
        }

        // *** QUOTED OR EVALUATED ITEMS ***
        // We are passing the value literally so it doesn't matter if it was
        // quoted or not in the spec...the value is taken verbatim

        if (mode == PARAM_MODE_SKIPPING) {
            // Leave as unset
            continue;
        }

        // Verify allowed argument datatype:
        if (!TYPE_CHECK(param, VAL_TYPE(arg)))
            fail (Error_Arg_Type(call, param, Type_Of(arg)));
    }

    // Pad out any remaining parameters with unset or none, depending

    while (!IS_END(param)) {
        if (VAL_GET_EXT(param, EXT_WORD_HIDE)) {
            // A true local...to be ignored as far as block args go.
            // Very likely to hit them at the end of the paramlist because
            // that's where the function generators tack on RETURN:

            if (
                VAL_GET_EXT(func, EXT_FUNC_HAS_RETURN)
                && SAME_SYM(VAL_TYPESET_SYM(param), SYM_RETURN)
            ) {
                *arg = *ROOT_RETURN_NATIVE;
                VAL_FUNC_RETURN_TO(arg) = VAL_FUNC_PARAMLIST(func);
            }
            else {
                // Leave as unset
            }
        }
        else if (VAL_GET_EXT(param, EXT_TYPESET_REFINEMENT)) {
            mode = PARAM_MODE_SKIPPING;
            SET_NONE(arg);
            refine = arg;
        }
        else {
            if (mode != PARAM_MODE_SKIPPING) {
                // If we aren't in ignore mode and we are dealing with
                // a non-refinement, then it's a situation of a required
                // argument missing or not all the args to a refinement given

                fail (Error_No_Arg(DSF_LABEL(call), param));
            }

            assert(IS_NONE(refine));
            assert(IS_UNSET(arg));
        }

        arg++;
        param++;
    }

    // With the arguments processed and proxied into the call frame, invoke
    // the function body.

    return Dispatch_Call_Throws(call);
}


//
//  Apply_Func_Throws: C
// 
// Applies function from args provided by C call. NULL terminated.
// 
// returns TRUE if out is THROWN()
//
REBFLG Apply_Func_Throws(REBVAL *out, REBVAL *func, ...)
{
    REBFLG result;
    va_list args;

    va_start(args, func);
    result = Apply_Func_Core(out, func, &args);

    // !!! An issue was uncovered here that there can be problems if a
    // failure is caused inside the Apply_Func_Core() which does
    // a longjmp() and never returns.  The C standard is explicit that
    // you cannot dodge the call to va_end(), and that call can only be
    // in *this* function (as it passes a local variable directly, and
    // its implementation is opaque):
    //
    //    http://stackoverflow.com/a/32259710/211160
    //
    // This means either the entire Apply logic has to be duplicated
    // inside of this function with shutdowns -OR- the varargs need to
    // be unpacked into a series managed by GC or the data stack -OR-
    // the Apply logic has to be parameterized to return errors to be
    // raised and a call frame to be executed, without failing or
    // calling itself.
    //
    // On most architectures this will not be a problem, but most does
    // not mean all... so it should be tended to at some point.
    //
    va_end(args);

    return result;
}


//
//  Do_Sys_Func_Throws: C
// 
// Evaluates a SYS function and out contains the result.
//
REBFLG Do_Sys_Func_Throws(REBVAL *out, REBCNT inum, ...)
{
    REBFLG result;
    va_list args;
    REBVAL *value = FRM_VALUE(Sys_Context, inum);

    if (!ANY_FUNC(value)) fail (Error(RE_BAD_SYS_FUNC, value));

    va_start(args, inum);
    result = Apply_Func_Core(out, value, &args);

    // !!! See notes in Apply_Func_Throws about va_end() and longjmp()
    va_end(args);

    return result;
}


//
//  Do_Construct: C
// 
// Do a block with minimal evaluation and no evaluation of
// functions. Used for things like script headers where security
// is important.
// 
// Handles cascading set words:  word1: word2: value
//
void Do_Construct(REBVAL value[])
{
    REBVAL *temp;
    REBINT ssp;  // starting stack pointer

    DS_PUSH_NONE;
    temp = DS_TOP;
    ssp = DSP;

    for (; NOT_END(value); value++) {
        if (IS_SET_WORD(value)) {
            // Next line not needed, because SET words are ALWAYS in frame.
            //if (VAL_WORD_INDEX(value) > 0 && VAL_WORD_FRAME(value) == frame)
                DS_PUSH(value);
        } else {
            // Get value:
            if (IS_WORD(value)) {
                switch (VAL_WORD_CANON(value)) {
                case SYM_NONE:
                    SET_NONE(temp);
                    break;
                case SYM_TRUE:
                case SYM_ON:
                case SYM_YES:
                    SET_TRUE(temp);
                    break;
                case SYM_FALSE:
                case SYM_OFF:
                case SYM_NO:
                    SET_FALSE(temp);
                    break;
                default:
                    *temp = *value;
                    VAL_SET(temp, REB_WORD);
                }
            }
            else if (IS_LIT_WORD(value)) {
                *temp = *value;
                VAL_SET(temp, REB_WORD);
            }
            else if (IS_LIT_PATH(value)) {
                *temp = *value;
                VAL_SET(temp, REB_PATH);
            }
            else if (VAL_TYPE(value) >= REB_NONE) { // all valid values
                *temp = *value;
            }
            else
                SET_NONE(temp);

            // Set prior set-words:
            while (DSP > ssp) {
                Set_Var(DS_TOP, temp);
                DS_DROP;
            }
        }
    }
    DS_DROP; // temp
}


//
//  Do_Min_Construct: C
// 
// Do no evaluation of the set values.
//
void Do_Min_Construct(REBVAL value[])
{
    REBVAL *temp;
    REBINT ssp;  // starting stack pointer

    DS_PUSH_NONE;
    temp = DS_TOP;
    ssp = DSP;

    for (; NOT_END(value); value++) {
        if (IS_SET_WORD(value)) {
            // Next line not needed, because SET words are ALWAYS in frame.
            //if (VAL_WORD_INDEX(value) > 0 && VAL_WORD_FRAME(value) == frame)
                DS_PUSH(value);
        } else {
            // Get value:
            *temp = *value;
            // Set prior set-words:
            while (DSP > ssp) {
                Set_Var(DS_TOP, temp);
                DS_DROP;
            }
        }
    }
    DS_DROP; // temp
}


//
//  Redo_Func_Throws: C
// 
// This code takes a call frame that has been built for one function and
// then uses it to build a call frame to call another.  (The source call
// frame is implicitly the currently running one.)
// 
// This function is currently used only by Do_Port_Action, where ACTION! (an
// archetypal function spec with no implementation) is "bounced out" of
// the C code into user code.  All the other ACTION!-handling code is
// implemented in C with switch statements.
// 
// The archetypal frame cannot be used directly, because it can be different.
// That difference could be as simple as `foo: action [port [port!]]` vs.
// `foo-impl: func [port [port!] /local x y z]`.  However, other tricks
// are allowed--such as for the implementation to offer refinements which
// were not present in the archetype.
// 
// Returns TRUE if result is THROWN()
//
REBFLG Redo_Func_Throws(REBVAL *func_val)
{
    REBSER *paramlist_src = VAL_FUNC_PARAMLIST(DSF_FUNC(DSF));
    REBSER *paramlist_new = VAL_FUNC_PARAMLIST(func_val);
    REBCNT isrc;        // index position in source frame
    REBCNT inew;        // index position in target frame
    REBVAL *param;
    REBVAL *param2;

    struct Reb_Call *call;
    REBVAL *arg;

    // As part of the "Redo" we are not adding a new function location,
    // label, or place to write the output.  We are substituting new code
    // and perhaps adjusting the arguments in our re-doing call.

    call = Make_Call(
        DSF_OUT(DSF),
        VAL_SERIES(DSF_WHERE(DSF)),
        VAL_INDEX(DSF_WHERE(DSF)),
        VAL_WORD_SYM(DSF_LABEL(DSF)),
        func_val
    );

    // Foreach arg of the target, copy to source until refinement.
    arg = DSF_ARG(call, 1);
    isrc = inew = FIRST_PARAM_INDEX;

    for (; inew < BLK_LEN(paramlist_new); inew++, isrc++, arg++) {
        param = BLK_SKIP(paramlist_new, inew);
        assert(IS_TYPESET(param));

        if (VAL_GET_EXT(param, EXT_WORD_HIDE)) {
            if (
                VAL_GET_EXT(func_val, EXT_FUNC_HAS_RETURN)
                && SAME_SYM(VAL_TYPESET_SYM(param), SYM_RETURN)
            ) {
                // This pure local is a special magic "definitional return"
                // (see comments on VAL_FUNC_RETURN_TO)
                *arg = *ROOT_RETURN_NATIVE;
                VAL_FUNC_RETURN_TO(arg) = VAL_FUNC_PARAMLIST(func_val);
            }
            else {
                // This pure local is not special, so leave as UNSET
            }
            continue;
        }

        if (isrc >= BLK_LEN(paramlist_src)) {
            isrc = BLK_LEN(paramlist_src);
            param2 = NULL;
        }
        else {
            param2 = BLK_SKIP(paramlist_src, isrc);
            assert(IS_TYPESET(param2));
        }

        if (VAL_GET_EXT(param, EXT_TYPESET_REFINEMENT)) {
            // At refinement, search for it in source, then continue with words.

            // Are we aligned on the refinement already? (a common case)
            if (
                param2
                && VAL_GET_EXT(param2, EXT_TYPESET_REFINEMENT)
                && (
                    VAL_TYPESET_CANON(param2)
                    == VAL_TYPESET_CANON(param)
                )
            ) {
                *arg = *DSF_ARG(DSF, isrc);
            }
            else {
                // No, we need to search for it:
                isrc = FIRST_PARAM_INDEX;
                for (; isrc < BLK_LEN(paramlist_src); isrc++) {
                    param2 = BLK_SKIP(paramlist_src, isrc);
                    if (
                        VAL_GET_EXT(param2, EXT_TYPESET_REFINEMENT)
                        && (
                            VAL_TYPESET_CANON(param2)
                            == VAL_TYPESET_CANON(param)
                        )
                    ) {
                        *arg = *DSF_ARG(DSF, isrc);
                        break;
                    }
                }
                // !!! The function didn't have the refinement so skip
                // it and leave as unset.
                // But what will happen now with the arguments?
                /* if (isrc >= BLK_LEN(wsrc)) fail (Error_Invalid_Arg(word)); */
            }
        }
        else {
            if (
                param2
                && (
                    VAL_GET_EXT(param, EXT_TYPESET_QUOTE)
                    == VAL_GET_EXT(param2, EXT_TYPESET_QUOTE)
                ) && (
                    VAL_GET_EXT(param, EXT_TYPESET_EVALUATE)
                    == VAL_GET_EXT(param2, EXT_TYPESET_EVALUATE)
                )
            ) {
                *arg = *DSF_ARG(DSF, isrc);
                // !!! Should check datatypes for new arg passing!
            }
            else {
                // !!! Why does this allow the bounced-to function to have
                // a different type, and be unset instead of erroring?

                // !!! Technically speaking this should honor the
                // LEGACY(OPTIONS_REFINEMENTS_TRUE) switch and SET_NONE if so,
                // but that would apply to a very small amount of "third
                // party actor code" from R3-Alpha.  Both the actor code and
                // this branch are suspect in terms of long term value, so
                // we just leave it as unset.
            }
        }
    }

    return Dispatch_Call_Throws(call);
}


//
//  Get_Simple_Value_Into: C
// 
// Does easy lookup, else just returns the value as is.
//
void Get_Simple_Value_Into(REBVAL *out, const REBVAL *val)
{
    if (IS_WORD(val) || IS_GET_WORD(val)) {
        GET_VAR_INTO(out, val);
    }
    else if (IS_PATH(val) || IS_GET_PATH(val)) {
        if (Do_Path_Throws(out, NULL, val, NULL))
            fail (Error_No_Catch_For_Throw(out));
    }
    else {
        *out = *val;
    }
}


//
//  Resolve_Path: C
// 
// Given a path, return a context and index for its terminal.
//
REBSER *Resolve_Path(REBVAL *path, REBCNT *index)
{
    REBVAL *sel; // selector
    const REBVAL *val;
    REBSER *blk;
    REBCNT i;

    if (VAL_TAIL(path) < 2) return 0;
    blk = VAL_SERIES(path);
    sel = BLK_HEAD(blk);
    if (!ANY_WORD(sel)) return 0;
    val = GET_VAR(sel);

    sel = BLK_SKIP(blk, 1);
    while (TRUE) {
        if (!ANY_OBJECT(val) || !IS_WORD(sel)) return 0;
        i = Find_Word_Index(VAL_OBJ_FRAME(val), VAL_WORD_SYM(sel), FALSE);
        sel++;
        if (IS_END(sel)) {
            *index = i;
            return VAL_OBJ_FRAME(val);
        }
    }

    return 0; // never happens
}
