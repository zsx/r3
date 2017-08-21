//
//  File: %c-path.h
//  Summary: "Core Path Dispatching and Chaining"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
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
// !!! See notes in %sys-path.h regarding the R3-Alpha path dispatch concept
// and regarding areas that need improvement.
//

#include "sys-core.h"


//
//  PD_Fail: C
//
// In order to avoid having to pay for a check for NULL in the path dispatch
// table for types with no path dispatch, a failing handler is in the slot.
//
REBINT PD_Fail(REBPVS *pvs)
{
    DECLARE_LOCAL (specified_orig);
    Derelativize(specified_orig, pvs->orig, pvs->item_specifier);

    DECLARE_LOCAL (specified_item);
    Derelativize(specified_item, pvs->item, pvs->item_specifier);

    fail (Error_Invalid_Path_Raw(specified_orig, specified_item));
}


//
//  PD_Unhooked: C
//
// As a temporary workaround for not having real user-defined types, an
// extension can overtake an "unhooked" type slot to provide behavior.
//
REBINT PD_Unhooked(REBPVS *pvs)
{
    REBVAL *type = Get_Type(VAL_TYPE(pvs->value)); // put in error message?
    UNUSED(type);

    fail ("Datatype is provided by an extension which is not loaded.");
}


//
//  Next_Path_Throws: C
//
// Evaluate next part of a path.
//
// !!! This is done as a recursive function instead of iterating in a loop due
// to the unusual nature of some path dispatches that call Next_Path_Throws()
// inside their implementation.
//
REBOOL Next_Path_Throws(REBPVS *pvs)
{
    if (IS_VOID(pvs->value))
        fail (Error_No_Value_Core(pvs->orig, pvs->item_specifier));

    REBPEF dispatcher = Path_Dispatch[VAL_TYPE(pvs->value)];
    assert(dispatcher != NULL); // &PD_Fail is used instead of NULL

    pvs->item++;

    // Calculate the "picker" into the GC guarded cell.
    //
    assert(pvs->picker == &pvs->picker_cell);

    if (IS_GET_WORD(pvs->item)) { // e.g. object/:field
        Copy_Opt_Var_May_Fail(
            KNOWN(&pvs->picker_cell), pvs->item, pvs->item_specifier
        );

        if (IS_VOID(pvs->picker))
            fail (Error_No_Value_Core(pvs->item, pvs->item_specifier));
    }
    else if (IS_GROUP(pvs->item)) { // object/(expr) case:
        REBSPC *derived = Derive_Specifier(pvs->item_specifier, pvs->item);
        if (Do_At_Throws(
            KNOWN(&pvs->picker_cell),
            VAL_ARRAY(pvs->item),
            VAL_INDEX(pvs->item),
            derived
        )) {
            Move_Value(pvs->store, KNOWN(&pvs->picker_cell));
            return TRUE;
        }
    }
    else { // object/word and object/value case:
        Derelativize(&pvs->picker_cell, pvs->item, pvs->item_specifier);
    }

    // Disallow voids from being used in path dispatch.  This rule seems like
    // common sense for safety, and also corresponds to voids being illegal
    // to use in SELECT.
    //
    if (IS_VOID(pvs->picker))
        fail (Error_No_Value_Core(pvs->item, pvs->item_specifier));

    switch (dispatcher(pvs)) {
    case PE_OK:
        break;

    case PE_SET_IF_END:
        if (pvs->opt_setval && IS_END(pvs->item + 1)) {
            *pvs->value = *pvs->opt_setval;
            pvs->opt_setval = NULL;
        }
        break;

    case PE_NONE:
        Init_Blank(pvs->store);
        // falls through
    case PE_USE_STORE:
        pvs->value = pvs->store;
        pvs->value_specifier = SPECIFIED;
        break;

    default:
        assert(FALSE);
    }

    // A function being refined does not actually update pvs->value with
    // a "more refined" function value, it holds the original function and
    // accumulates refinement state on the stack.  The label should only
    // be captured the first time the function is seen, otherwise it would
    // capture the last refinement's name, so check label for non-NULL.
    //
    if (IS_FUNCTION(pvs->value) && IS_WORD(pvs->item))
        if (pvs->label_out != NULL && *pvs->label_out == NULL)
            *pvs->label_out = VAL_WORD_SPELLING(pvs->item);

    if (NOT_END(pvs->item + 1))
        return Next_Path_Throws(pvs);

    return FALSE;
}


//
//  Do_Path_Throws_Core: C
//
// Evaluate an ANY_PATH! REBVAL, starting from the index position of that
// path value and continuing to the end.
//
// The evaluator may throw because GROUP! is evaluated, e.g. `foo/(throw 1020)`
//
// If label_sym is passed in as being non-null, then the caller is implying
// readiness to process a path which may be a function with refinements.
// These refinements will be left in order on the data stack in the case
// that `out` comes back as IS_FUNCTION().
//
// If `opt_setval` is given, the path operation will be done as a "SET-PATH!"
// if the path evaluation did not throw or error.  HOWEVER the set value
// is NOT put into `out`.  This provides more flexibility on performance in
// the evaluator, which may already have the `val` where it wants it, and
// so the extra assignment would just be overhead.
//
// !!! Path evaluation is one of the parts of R3-Alpha that has not been
// vetted very heavily by Ren-C, and needs a review and overhaul.
//
REBOOL Do_Path_Throws_Core(
    REBVAL *out,
    REBSTR **label_out,
    const RELVAL *path,
    REBSPC *specifier,
    const REBVAL *opt_setval
) {
    // The pvs contains a cell for the picker into which evaluations are
    // done, e.g. `foo/(1 + 2)`.  Because Next_Path() doesn't commit to not
    // performing any evaluations this cell must be guarded.  In the case of
    // a fail() this guard will be released automatically, but to return
    // normally use `return_thrown` and `return_not_thrown` which drops guard.
    //
    // !!! There was also a strange requirement in some more quirky path
    // evaluation (GOB!, STRUCT!) that the cell survive between Next_Path()
    // calls, which may still be relevant to why this can't be a C local.
    //
    REBPVS pvs;
    Prep_Global_Cell(&pvs.picker_cell);
    SET_END(&pvs.picker_cell);
    PUSH_GUARD_VALUE(&pvs.picker_cell);
    pvs.picker = KNOWN(&pvs.picker_cell);

    REBDSP dsp_orig = DSP;

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
    if (opt_setval)
        Init_Unreadable_Blank(out);

    // None of the values passed in can live on the data stack, because
    // they might be relocated during the path evaluation process.
    //
    assert(!IN_DATA_STACK_DEBUG(out));
    assert(!IN_DATA_STACK_DEBUG(path));
    assert(!opt_setval || !IN_DATA_STACK_DEBUG(opt_setval));

    // Not currently robust for reusing passed in path or value as the output
    assert(out != path && out != opt_setval);

    assert(!opt_setval || !THROWN(opt_setval));

    // Initialize REBPVS -- see notes in %sys-do.h
    //
    pvs.opt_setval = opt_setval;
    pvs.store = out;
    pvs.orig = path;
    pvs.item = VAL_ARRAY_AT(pvs.orig); // may not be starting at head of PATH!
    pvs.label_out = label_out;
    if (label_out != NULL)
        *label_out = NULL; // initial value if no function label found

    // The path value that's coming in may be relative (in which case it
    // needs to use the specifier passed in).  Or it may be specific already,
    // in which case we should use the specifier in the value to process
    // its array contents.
    //
    pvs.item_specifier = Derive_Specifier(specifier, path);

    // Seed the path evaluation process by looking up the first item (to
    // get a datatype to dispatch on for the later path items)
    //
    if (IS_WORD(pvs.item)) {
        pvs.value = Get_Mutable_Var_May_Fail(pvs.item, pvs.item_specifier);
        pvs.value_specifier = SPECIFIED;

        if (IS_VOID(pvs.value))
            fail (Error_No_Value_Core(pvs.item, pvs.item_specifier));

        if (IS_FUNCTION(pvs.value) && pvs.label_out != NULL)
            *pvs.label_out = VAL_WORD_SPELLING(pvs.item);
    }
    else {
        // !!! Ideally there would be some way to deal with writes to
        // temporary locations, like this pvs.value...if a set-path sets
        // it, then it will be discarded.

        Derelativize(pvs.store, VAL_ARRAY_AT(pvs.orig), pvs.item_specifier);
        pvs.value = pvs.store;
        pvs.value_specifier = SPECIFIED;
    }

    // Start evaluation of path:
    if (IS_END(pvs.item + 1)) {
        // If it was a single element path, return the value rather than
        // try to dispatch it (would cause a crash at time of writing)
        //
        // !!! Is this the desired behavior, or should it be an error?
    }
    else {
        REBOOL threw = Next_Path_Throws(&pvs);

        // !!! See comments about why the initialization of out is necessary.
        // Without it this assertion can change on some things:
        //
        //     t: now
        //     t/time: 10:20:03
        //
        // (It thinks pvs.value has its THROWN bit set when it completed
        // successfully.  It was a PE_USE_STORE case where pvs.value was reset to
        // pvs.store, and pvs.store has its thrown bit set.  Valgrind does not
        // catch any uninitialized variables.)
        //
        // There are other cases that do trip valgrind when omitting the
        // initialization, though not as clearly reproducible.
        //
        assert(threw == THROWN(pvs.value));

        if (threw)
            goto return_thrown;
    }

    if (opt_setval) {
        // If SET then we don't return anything
        assert(IS_END(pvs.item) + 1);
        goto return_not_thrown;
    }

    // If storage was not used, then copy final value back to it:
    if (pvs.value != pvs.store)
        Derelativize(pvs.store, pvs.value, pvs.value_specifier);

    assert(!THROWN(out));

    assert(IS_END(pvs.item) + 1);

    // To make things easier for processing, reverse any refinements
    // pushed to the data stack (we needed to evaluate them
    // in forward order).  This way we can just pop them as we go,
    // and know if they weren't all consumed if it doesn't get
    // back to `dsp_orig` by the end.
    //
    if (dsp_orig != DSP) {
        assert(IS_FUNCTION(pvs.store));

        // !!! It should be technically possible to do something like
        // :append/dup and return a "refined" variant of a function.  That
        // feature is not currently implemented.  So if a label wasn't
        // requested, assume a function is not being run and deliver an
        // error for that case.
        //
        if (label_out == NULL)
            fail (Error_Too_Long_Raw());

        REBVAL *bottom = DS_AT(dsp_orig + 1);
        REBVAL *top = DS_TOP;
        while (top > bottom) {
            DECLARE_LOCAL (temp);
            Move_Value(temp, bottom);
            Move_Value(bottom, top);
            Move_Value(top, temp);

            top--;
            bottom++;
        }
    }

return_not_thrown:
    DROP_GUARD_VALUE(&pvs.picker_cell);
    return FALSE;

return_thrown:
    DROP_GUARD_VALUE(&pvs.picker_cell);
    return TRUE;
}


//
//  Error_Bad_Path_Select: C
//
REBCTX *Error_Bad_Path_Select(REBPVS *pvs)
{
    DECLARE_LOCAL (orig);
    Derelativize(orig, pvs->orig, pvs->item_specifier);

    DECLARE_LOCAL (item);
    Derelativize(item, pvs->item, pvs->item_specifier);

    return Error_Invalid_Path_Raw(orig, item);
}


//
//  Error_Bad_Path_Set: C
//
REBCTX *Error_Bad_Path_Set(REBPVS *pvs)
{
    DECLARE_LOCAL (orig);
    Derelativize(orig, pvs->orig, pvs->item_specifier);

    DECLARE_LOCAL (item);
    Derelativize(item, pvs->item, pvs->item_specifier);

    return Error_Bad_Path_Set_Raw(orig, item);
}


//
//  Error_Bad_Path_Range: C
//
REBCTX *Error_Bad_Path_Range(REBPVS *pvs)
{
    DECLARE_LOCAL (item);
    Derelativize(item, pvs->item, pvs->item_specifier);

    return Error_Out_Of_Range(item);
}


//
//  Error_Bad_Path_Field_Set: C
//
REBCTX *Error_Bad_Path_Field_Set(REBPVS *pvs)
{
    DECLARE_LOCAL (item);
    Derelativize(item, pvs->item, pvs->item_specifier);

    return Error_Bad_Field_Set_Raw(item, Type_Of(pvs->opt_setval));
}


//
//  Get_Simple_Value_Into: C
//
// Does easy lookup, else just returns the value as is.
//
void Get_Simple_Value_Into(REBVAL *out, const RELVAL *val, REBSPC *specifier)
{
    if (IS_WORD(val) || IS_GET_WORD(val)) {
        Copy_Opt_Var_May_Fail(out, val, specifier);
    }
    else if (IS_PATH(val) || IS_GET_PATH(val)) {
        if (Do_Path_Throws_Core(out, NULL, val, specifier, NULL))
            fail (Error_No_Catch_For_Throw(out));
    }
    else {
        Derelativize(out, val, specifier);
    }
}


//
//  Resolve_Path: C
//
// Given a path, determine if it is ultimately specifying a selection out
// of a context...and if it is, return that context.  So `a/obj/key` would
// return the object assocated with obj, while `a/str/1` would return
// NULL if `str` were a string as it's not an object selection.
//
// !!! This routine overlaps the logic of Do_Path, and should potentially
// be a mode of that instead.  It is not very complete, considering that it
// does not execute GROUP! (and perhaps shouldn't?) and only supports a
// path that picks contexts out of other contexts, via word selection.
//
REBCTX *Resolve_Path(const REBVAL *path, REBCNT *index_out)
{
    REBARR *array = VAL_ARRAY(path);
    RELVAL *picker = ARR_HEAD(array);

    if (IS_END(picker) || !ANY_WORD(picker))
        return NULL; // !!! only handles heads of paths that are ANY-WORD!

    const RELVAL *var = Get_Opt_Var_May_Fail(picker, VAL_SPECIFIER(path));

    ++picker;
    if (IS_END(picker))
        return NULL; // !!! does not handle single-element paths

    while (ANY_CONTEXT(var) && IS_WORD(picker)) {
        REBCNT i = Find_Canon_In_Context(
            VAL_CONTEXT(var), VAL_WORD_CANON(picker), FALSE
        );
        ++picker;
        if (IS_END(picker)) {
            *index_out = i;
            return VAL_CONTEXT(var);
        }

        var = CTX_VAR(VAL_CONTEXT(var), i);
    }

    return NULL;
}


//
//  pick*: native [
//
//  {Perform a path picking operation, same as `:(:location)/(:picker)`}
//
//      return: [<opt> any-value!]
//          {Picked value, or void if picker can't fulfill the request}
//      location [any-value!]
//      picker [any-value!]
//          {Index offset, symbol, or other value to use as index}
//  ]
//
REBNATIVE(pick_p)
//
// In R3-Alpha, PICK was an "action", which dispatched on types through the
// "action mechanic" for the following types:
//
//     [any-series! map! gob! pair! date! time! tuple! bitset! port! varargs!]
//
// In Ren-C, PICK is rethought to use the same dispatch mechanic as paths,
// to cut down on the total number of operations the system has to define.
{
    INCLUDE_PARAMS_OF_PICK_P;

    REBVAL *location = ARG(location);
    REBVAL *picker = ARG(picker);

    // PORT!s are kind of a "user defined type" which historically could
    // react to PICK and POKE, but which could not override path dispatch.
    // Use a symbol-based call to bounce the frame to the port, which should
    // be a compatible frame with the historical "action".
    //
    if (IS_PORT(location))
        return Do_Port_Action(frame_, VAL_CONTEXT(location), SYM_PICK_P);

    REBPVS pvs_decl;
    REBPVS *pvs = &pvs_decl;

    Prep_Global_Cell(&pvs->picker_cell);
    TRASH_CELL_IF_DEBUG(&pvs->picker_cell); // not used
    pvs->picker = picker;
    pvs->store = D_OUT;

    // !!! Sometimes path dispatchers check the item to see if it's at the
    // end of the path.  The entire thing needs review.  In the meantime,
    // take advantage of the implicit termination of the frame cell.
    //
    Move_Value(D_CELL, picker);
    assert(IS_END(D_CELL + 1));

    pvs->item = D_CELL;
    pvs->item_specifier = SPECIFIED;
    pvs->value = location;
    pvs->value_specifier = SPECIFIED;

    pvs->label_out = NULL; // applies to e.g. :append/only returning APPEND
    pvs->orig = location; // expected to be a PATH! for errors, but tolerant
    pvs->opt_setval = NULL;

    REBPEF dispatcher = Path_Dispatch[VAL_TYPE(location)];
    assert(dispatcher != NULL); // &PD_Fail is used instead of NULL
    switch (dispatcher(pvs)) {
    case PE_OK:
        break;

    case PE_SET_IF_END:
        break;

    case PE_NONE:
        Init_Blank(pvs->store);
        // falls through
    case PE_USE_STORE:
        pvs->value = pvs->store;
        pvs->value_specifier = SPECIFIED;
        break;

    default:
        assert(FALSE);
    }

    if (pvs->value != pvs->store)
        Derelativize(D_OUT, pvs->value, pvs->value_specifier);

    return R_OUT;
}


//
//  poke: native [
//
//  {Perform a path poking operation, same as `(:location)/(:picker): :value`}
//
//      return: [<opt> any-value!]
//          {Same as value}
//      location [any-value!]
//          {(modified)}
//      picker
//          {Index offset, symbol, or other value to use as index}
//      value [<opt> any-value!]
//          {The new value}
//  ]
//
REBNATIVE(poke)
//
// As with PICK*, POKE is changed in Ren-C from its own action to "whatever
// path-setting (now path-poking) would do".
{
    INCLUDE_PARAMS_OF_POKE;

    REBVAL *location = ARG(location);
    REBVAL *picker = ARG(picker);
    REBVAL *value = ARG(value);

    // PORT!s are kind of a "user defined type" which historically could
    // react to PICK and POKE, but which could not override path dispatch.
    // Use a symbol-based call to bounce the frame to the port, which should
    // be a compatible frame with the historical "action".
    //
    if (IS_PORT(location))
        return Do_Port_Action(frame_, VAL_CONTEXT(location), SYM_POKE);

    REBPVS pvs_decl;
    REBPVS *pvs = &pvs_decl;

    Prep_Global_Cell(&pvs->picker_cell);
    TRASH_CELL_IF_DEBUG(&pvs->picker_cell); // not used
    pvs->picker = picker;
    pvs->store = D_OUT;

    // !!! Sometimes the path mechanics do the writes for a poke inside their
    // dispatcher, vs. delegating via PE_SET_IF_END.  They check to see if
    // the current pvs->item is at the end.  All of path dispatch was ad hoc
    // and needs a review.  In the meantime, take advantage of the implicit
    // termination of the frame cell.
    //
    Move_Value(D_CELL, picker);
    assert(IS_END(D_CELL + 1));

    pvs->item = D_CELL;
    pvs->item_specifier = SPECIFIED;
    pvs->value = location;
    pvs->value_specifier = SPECIFIED;

    pvs->label_out = NULL; // applies to e.g. :append/only returning APPEND
    pvs->orig = location; // expected to be a PATH! for errors, but tolerant
    pvs->opt_setval = value;

    REBPEF dispatcher = Path_Dispatch[VAL_TYPE(location)];
    assert(dispatcher != NULL); // &PD_Fail is used instead of NULL
    switch (dispatcher(pvs)) {
    case PE_SET_IF_END:
        *pvs->value = *pvs->opt_setval;
        break;

    case PE_OK:
        // !!! Trust that it wrote?  See above notes about D_CELL.
        break;

    case PE_NONE:
    case PE_USE_STORE:
        fail (picker); // Invalid argument

    default:
        assert(FALSE);
    }

    Move_Value(D_OUT, value);
    return R_OUT;
}
