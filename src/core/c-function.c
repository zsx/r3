//
//  File: %c-function.c
//  Summary: "support for functions, actions, and routines"
//  Section: core
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
/*
    Structure of functions:

        spec - interface spec block
        body - body code
        args - args list (see below)

    Args list is a block of word+values:

        word - word, 'word, :word, /word
        value - typeset! or blank (valid datatypes)

    Args list provides:

        1. specifies arg order, arg kind (e.g. 'word)
        2. specifies valid datatypes (typesets)
        3. used for word and type in error output
        4. used for debugging tools (stack dumps)
        5. not used for MOLD (spec is used)
        6. used as a (pseudo) frame of function variables

*/

#include "sys-core.h"

//
//  List_Func_Words: C
// 
// Return a block of function words, unbound.
// Note: skips 0th entry.
//
REBARR *List_Func_Words(const REBVAL *func, REBOOL pure_locals)
{
    REBARR *array = Make_Array(VAL_FUNC_NUM_PARAMS(func));
    REBVAL *param = VAL_FUNC_PARAMS_HEAD(func);

    for (; !IS_END(param); param++) {
        enum Reb_Kind kind;

        switch (VAL_PARAM_CLASS(param)) {
        case PARAM_CLASS_NORMAL:
            kind = REB_WORD;
            break;

        case PARAM_CLASS_REFINEMENT:
            kind = REB_REFINEMENT;
            break;

        case PARAM_CLASS_HARD_QUOTE:
            kind = REB_GET_WORD;
            break;

        case PARAM_CLASS_SOFT_QUOTE:
            kind = REB_LIT_WORD;
            break;

        case PARAM_CLASS_PURE_LOCAL:
            if (!pure_locals)
                continue; // treat as invisible, e.g. for WORDS-OF

            kind = REB_SET_WORD;
            break;

        default:
            assert(FALSE);
            DEAD_END;
        }

        Val_Init_Word(Alloc_Tail_Array(array), kind, VAL_TYPESET_SYM(param));
    }

    return array;
}


//
//  List_Func_Typesets: C
// 
// Return a block of function arg typesets.
// Note: skips 0th entry.
//
REBARR *List_Func_Typesets(REBVAL *func)
{
    REBARR *array = Make_Array(VAL_FUNC_NUM_PARAMS(func));
    REBVAL *typeset = VAL_FUNC_PARAMS_HEAD(func);

    for (; !IS_END(typeset); typeset++) {
        REBVAL *value = Alloc_Tail_Array(array);
        *value = *typeset;

        // !!! It's already a typeset, but this will clear out the header
        // bits.  This may not be desirable over the long run (what if
        // a typeset wishes to encode hiddenness, protectedness, etc?)

        VAL_RESET_HEADER(value, REB_TYPESET);
    }

    return array;
}


//
//  Make_Paramlist_Managed: C
// 
// Check function spec of the form:
// 
//     ["description" arg "notes" [type! type2! ...] /ref ...]
// 
// !!! The spec language was not formalized in R3-Alpha.  Strings were left
// in and it was HELP's job (and any other clients) to make sense of it, e.g.:
//
//     [foo [type!] {doc string :-)}]
//     [foo {doc string :-/} [type!]]
//     [foo {doc string1 :-/} {doc string2 :-(} [type!]]
//
// Ren-C breaks this into two parts: one is the mechanical understanding of
// MAKE FUNCTION! for parameters in the evaluator.  Then it is the job
// of a generator to tag the resulting function with a "meta object" with any
// descriptions.  As a proxy for the work of a usermode generator, this
// routine tries to fill in FUNCTION-META (see %sysobj.r) as well as to
// produce a paramlist suitable for the function.
//
REBFUN *Make_Paramlist_Managed(
    REBARR *spec,
    REBCNT opt_sym_last,
    REBARR *body,
    REBNAT dispatch
) {
    ASSERT_ARRAY_MANAGED(body);

    REBUPT func_flags = 0;

    // We want to be able to notice when words are duplicated, and the bind
    // table can be used for that purpose.
    //
    REBINT *binds = WORDS_HEAD(Bind_Table);
    ASSERT_BIND_TABLE_EMPTY;

    REBDSP dsp_orig = DSP;
    assert(DS_TOP == DS_AT(dsp_orig));

    // Watch for the point in the parameter gathering that we want to be moved
    // to the tail of the serious for the optimization of definitional return.
    //
    REBVAL *to_be_moved = NULL;

    REBVAL empty_string;
    REBVAL *EMPTY_STRING = &empty_string;
    REBSER *empty_series = Make_Binary(1);
    *BIN_AT(empty_series, 0) = '\0';
    Val_Init_String(&empty_string, empty_series);

    // As we go through the spec block, we push TYPESET! BLOCK! STRING! triples.
    // These will be split out into separate arrays after the process is done.
    // The first slot of the paramlist needs to be the function canon value,
    // while the other two first slots need to be rootkeys.  Get the process
    // started right after a BLOCK! so it's willing to take a string for
    // the function description--it will be extracted from the slot before
    // it is turned into a rootkey for param_notes.
    //
    DS_PUSH_TRASH; // paramlist[0] (will become FUNCTION! canon value)
    Val_Init_Typeset(DS_TOP, 0, REB_0);
    DS_PUSH(EMPTY_BLOCK); // param_types[0] (to be OBJECT! canon value, if any)
    DS_PUSH(EMPTY_STRING); // param_notes[0] (holds description, then canon)

    REBOOL has_description = FALSE;
    REBOOL has_types = FALSE;
    REBOOL has_notes = FALSE;

    REBVAL *item;
    for (item = ARR_HEAD(spec); NOT_END(item); item++) {

    //=//// STRING! FOR FUNCTION DESCRIPTION OR PARAMETER NOTE ////////////=//

        if (IS_STRING(item)) {
            if (IS_TYPESET(DS_TOP))
                DS_PUSH(EMPTY_BLOCK); // need a block to be in position

            if (IS_BLOCK(DS_TOP)) { // we're in right spot to push notes/title
                DS_PUSH_TRASH;
                Val_Init_String(
                    DS_TOP,
                    Copy_String_Slimming(VAL_SERIES(item), VAL_INDEX(item), -1)
                );
            }
            else if (IS_STRING(DS_TOP)) {
                //
                // !!! A string was already pushed.  Should we append?
                //
                Val_Init_String(
                    DS_TOP,
                    Copy_String_Slimming(VAL_SERIES(item), VAL_INDEX(item), -1)
                );
            }
            else
                fail (Error(RE_MISC)); // should not be possible.

            if (DS_TOP == DS_AT(dsp_orig + 3))
                has_description = TRUE;
            else
                has_notes = TRUE;

            continue;
        }

    //=//// TAGS LIKE <local>, <no-return>, <punctuates>, etc. ////////////=//

        if (IS_TAG(item))
            continue; // leave it be for now (MAKE FUNCTION! vs FUNC differ)

    //=//// BLOCK! OF TYPES TO MAKE TYPESET FROM (PLUS PARAMETER TAGS) ////=//

        if (IS_BLOCK(item)) {
            if (IS_BLOCK(DS_TOP)) {
                //
                // Tried to give two blocks of types.  !!! Better error here
                //
                if (NOT(IS_BLANK(DS_TOP - 1))) // not [0] slot
                    fail (Error(RE_BAD_FUNC_DEF, item));

                // !!! Rebol2 had the ability to put a block in the first
                // slot before any parameters, in which you could put words.
                // This is deprecated in favor of the use of tags.  We permit
                // [catch] and [throw] during Rebol2 => Rebol3 migration,
                // but ignore them.
                //
                REBVAL *attribute = VAL_ARRAY_AT(item);
                for (; NOT_END(attribute); attribute++) {
                    if (IS_WORD(attribute)) {
                        if (VAL_WORD_SYM(attribute) == SYM_CATCH)
                            continue; // ignore it
                        if (VAL_WORD_SYM(attribute) == SYM_THROW) {
                            continue; // ignore it
                        }
                    }
                    fail (Error(RE_BAD_FUNC_DEF, item));
                }

                continue;
            }

            // Save the block for parameter types.
            //
            REBVAL *typeset;
            if (IS_TYPESET(DS_TOP)) {
                typeset = DS_TOP;
                DS_PUSH_TRASH;
                Val_Init_Block(
                    DS_TOP,
                    Copy_Array_At_Deep_Managed(VAL_ARRAY(item), VAL_INDEX(item))
                );
            }
            else if (IS_STRING(DS_TOP)) { // !!! are blocks after notes good?
                assert(IS_TYPESET(DS_TOP - 2));
                typeset = DS_TOP - 2;

                assert(IS_BLOCK(DS_TOP - 1));
                if (VAL_ARRAY(DS_TOP - 1) != EMPTY_ARRAY)
                    fail (Error(RE_BAD_FUNC_DEF, item));

                Val_Init_Block(
                    DS_TOP - 1,
                    Copy_Array_At_Deep_Managed(VAL_ARRAY(item), VAL_INDEX(item))
                );
            }
            else
                fail (Error(RE_MISC)); // shouldn't be possible

            // Turn block into typeset for parameter at current index.
            // Leaves VAL_TYPESET_SYM as-is.
            //
            Update_Typeset_Bits_Core(
                typeset,
                VAL_ARRAY_HEAD(item),
                FALSE // `trap`: false means fail vs. return FALSE if error
            );

            // A hard quote can only get a void if it is an <end>.
            //
            if (VAL_PARAM_CLASS(typeset) == PARAM_CLASS_HARD_QUOTE) {
                if (TYPE_CHECK(typeset, REB_0)) {
                    REBVAL param_name;
                    Val_Init_Word(
                        &param_name, REB_WORD, VAL_TYPESET_SYM(typeset)
                    );
                    fail (Error(RE_HARD_QUOTE_VOID, &param_name));
                }
            }

            has_types = TRUE;
            continue;
        }

    //=//// BAR! AS LOW-LEVEL MAKE FUNCTION! SIGNAL FOR <punctuates> //////=//

        if (IS_BAR(item)) { // !!! Review this notational choice
            func_flags |= FUNC_FLAG_PUNCTUATES;
            continue;
        }

    //=//// ANY-WORD! PARAMETERS THEMSELVES (MAKE TYPESETS w/SYMBOL) //////=//

        if (!ANY_WORD(item))
            fail (Error(RE_BAD_FUNC_DEF, item));

        // Make sure symbol not already in the parameter list, and then mark
        // in the hash table that it is present.  Any non-zero value is ok.
        //
        REBSYM canon = VAL_WORD_CANON(item);
        if (binds[canon] != 0)
            fail (Error(RE_DUP_VARS, item));
        binds[canon] = 1020;

        // In rhythm of TYPESET! BLOCK! STRING! we want to be on a string spot
        // at the time of the push of each new typeset.
        //
        if (IS_TYPESET(DS_TOP))
            DS_PUSH(EMPTY_BLOCK);
        if (IS_BLOCK(DS_TOP))
            DS_PUSH(EMPTY_STRING);
        assert(IS_STRING(DS_TOP));

        // Allow "all datatypes but void".  Note that this is the <opt> sense
        // of void signal--not the <end> sense, which is controlled by a flag.
        // We do not canonize the saved symbol in the paramlist, see #2258.
        //
        DS_PUSH_TRASH;
        REBVAL *typeset = DS_TOP;
        Val_Init_Typeset(typeset, ~FLAGIT_KIND(REB_0), VAL_WORD_SYM(item));
        switch (VAL_TYPE(item)) {
        case REB_WORD:
            INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_NORMAL);
            break;

        case REB_GET_WORD:
            INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_HARD_QUOTE);
            break;

        case REB_LIT_WORD:
            INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_SOFT_QUOTE);
            break;

        case REB_REFINEMENT:
            INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_REFINEMENT);
            //
            // !!! The typeset bits of a refinement are not currently used.
            // They are checked for TRUE or FALSE but this is done literally
            // by the code.  This means that every refinement has some spare
            // bits available in it for another purpose.
            //
            break;

        case REB_SET_WORD:
            INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_PURE_LOCAL);
            break;

        default:
            fail (Error(RE_BAD_FUNC_DEF, item));
        }
        assert(VAL_PARAM_CLASS(typeset) != PARAM_CLASS_0);

        // If this is the symbol we were asked to strategically position last
        // in the parameter list, keep track of its pointer.
        //
        if (VAL_TYPESET_CANON(typeset) == opt_sym_last) {
            assert(to_be_moved == NULL && opt_sym_last);
            to_be_moved = typeset;
        }
    }

    // Go ahead and flesh out the TYPESET! BLOCK! STRING! triples.
    //
    if (IS_TYPESET(DS_TOP))
        DS_PUSH(EMPTY_BLOCK);
    if (IS_BLOCK(DS_TOP))
        DS_PUSH(EMPTY_STRING);
    assert((DSP - dsp_orig) % 3 == 0); // must be a multiple of 3

    // Slots, which is length +1 for including the rootvar/rootparam
    //
    REBCNT num_slots = (DSP - dsp_orig) / 3;

    // If we were looking for something to send to the end, assert we've
    // found it...and swap the three triples at the tail with its three.
    // (it might already be the last slot, but don't worry about it).
    //
    if (opt_sym_last != SYM_0) {
        assert(to_be_moved != NULL);

        REBVAL temp;

        temp = *(DS_TOP - 2);
        *(DS_TOP - 2) = *to_be_moved;
        *to_be_moved = temp;

        temp = *(DS_TOP - 1);
        *(DS_TOP - 1) = *(to_be_moved + 1);
        *(to_be_moved + 1) = temp;

        temp = *(DS_TOP);
        *(DS_TOP) = *(to_be_moved + 2);
        *(to_be_moved + 2) = temp;

        // !!! For now we set the typeset of the element to ALL_64, because
        // this is where the definitional return will hide its type info.
        // Until a notation is picked for the spec this capability isn't
        // enabled, but will be.
        //
        VAL_TYPESET_BITS(DS_TOP - 2) = ALL_64;
    }

    // Must make the function "paramlist" even if "empty", for identity
    //
    REBARR *paramlist = Make_Array(num_slots);
    if (TRUE) {
        REBVAL *dest = ARR_HEAD(paramlist); // canon function value
        VAL_RESET_HEADER(dest, REB_FUNCTION);
        SET_VAL_FLAGS(dest, func_flags);
        dest->payload.function.func = AS_FUNC(paramlist);
        dest->payload.function.exit_from = NULL;
        dest->payload.function.body = body;
        VAL_FUNC_DISPATCH(dest) = dispatch;
        ++dest;

        REBVAL *src = DS_AT(dsp_orig + 1);
        src += 3;
        for (; src <= DS_TOP; src += 3, ++dest) {
            assert(IS_TYPESET(src));
            *dest = *src;
            binds[VAL_TYPESET_CANON(dest)] = 0;
        }
        SET_END(dest);

        SET_ARRAY_LEN(paramlist, num_slots);
        MANAGE_ARRAY(paramlist);

        // Make sure the parameter list does not expand.
        //
        // !!! Should more precautions be taken, at some point locking and
        // protecting the whole array?  (It will be changed more by the
        // caller, but after that.)
        //
        SET_ARR_FLAG(paramlist, SERIES_FLAG_FIXED_SIZE);
    }

    ASSERT_BIND_TABLE_EMPTY;

    //=///////////////////////////////////////////////////////////////////=//
    //
    // BUILD META INFORMATION OBJECT (IF NEEDED)
    //
    //=///////////////////////////////////////////////////////////////////=//

    // !!! See notes on FUNCTION-META in %sysobj.r
    const REBCNT description = 1;
    const REBCNT parameter_types = 2;
    const REBCNT parameter_notes = 3;
    REBCTX *meta;

    if (has_description || has_types || has_notes) {
        meta = Copy_Context_Shallow(VAL_CONTEXT(ROOT_FUNCTION_META));
        MANAGE_ARRAY(CTX_VARLIST(meta));
        VAL_FUNC_META(ARR_HEAD(paramlist)) = meta;
    }
    else
        VAL_FUNC_META(ARR_HEAD(paramlist)) = NULL;

    // If a description string was gathered, it's sitting in the first string
    // slot, the third cell we pushed onto the stack.  Extract it if so.
    //
    if (has_description) {
        assert(IS_STRING(DS_AT(dsp_orig + 3)));
        *CTX_VAR(meta, 1) = *DS_AT(dsp_orig + 3);
    }

    // Only make `parameter-types` if there were blocks in the spec
    //
    if (has_types) {
        REBARR *types_varlist = Make_Array(num_slots);
        SET_ARR_FLAG(types_varlist, ARRAY_FLAG_CONTEXT_VARLIST);
        INIT_CTX_KEYLIST_SHARED(AS_CONTEXT(types_varlist), paramlist);

        REBVAL *dest = ARR_HEAD(types_varlist); // rootvar: canon FRAME! value
        VAL_RESET_HEADER(dest, REB_FRAME);
        dest->payload.any_context.context = AS_CONTEXT(types_varlist);
        dest->payload.any_context.exit_from = NULL;
        ++dest;

        REBVAL *src = DS_AT(dsp_orig + 2);
        src += 3;
        for (; src <= DS_TOP; src += 3, ++dest) {
            assert(IS_BLOCK(src));
            if (VAL_ARRAY_LEN_AT(src) == 0)
                SET_VOID(dest);
            else
                *dest = *src;
        }
        SET_END(dest);

        SET_ARRAY_LEN(types_varlist, num_slots);
        MANAGE_ARRAY(types_varlist);

        Val_Init_Context(
            CTX_VAR(meta, 2), REB_FRAME, AS_CONTEXT(types_varlist)
        );
    }

    // Only make `parameter-notes` if there were strings (besides description)
    //
    if (has_notes) {
        REBARR *notes_varlist = Make_Array(num_slots);
        SET_ARR_FLAG(notes_varlist, ARRAY_FLAG_CONTEXT_VARLIST);
        INIT_CTX_KEYLIST_SHARED(AS_CONTEXT(notes_varlist), paramlist);

        REBVAL *dest = ARR_HEAD(notes_varlist); // rootvar: canon FRAME! value
        VAL_RESET_HEADER(dest, REB_FRAME);
        dest->payload.any_context.context = AS_CONTEXT(notes_varlist);
        dest->payload.any_context.exit_from = NULL;
        ++dest;

        REBVAL *src = DS_AT(dsp_orig + 3);
        src += 3;
        for (; src <= DS_TOP; src += 3, ++dest) {
            assert(IS_STRING(src));
            if (SER_LEN(VAL_SERIES(src)) == 0)
                SET_VOID(dest);
            else
                *dest = *src;
        }
        SET_END(dest);

        SET_ARRAY_LEN(notes_varlist, num_slots);
        MANAGE_ARRAY(notes_varlist);

        Val_Init_Context(
            CTX_VAR(meta, 3), REB_FRAME, AS_CONTEXT(notes_varlist)
        );
    }

    // With all the values extracted from stack to array, restore stack pointer
    //
    DS_DROP_TO(dsp_orig);

    return AS_FUNC(paramlist);
}



//
//  Find_Param_Index: C
//
// Find function param word in function "frame".
//
// !!! This is semi-redundant with similar functions for Find_Word_In_Array
// and key finding for objects, review...
//
REBCNT Find_Param_Index(REBARR *paramlist, REBSYM sym)
{
    REBVAL *params = ARR_AT(paramlist, 1);
    REBCNT len = ARR_LEN(paramlist);

    REBCNT canon = SYMBOL_TO_CANON(sym); // don't recalculate each time

    REBCNT n;
    for (n = 1; n < len; n++, params++) {
        if (
            sym == VAL_TYPESET_SYM(params)
            || canon == VAL_TYPESET_CANON(params)
        ) {
            return n;
        }
    }

    return 0;
}


//
//  Make_Native: C
//
void Make_Native(
    REBVAL *out,
    REBARR *spec,
    REBNAT dispatch
) {
    //Print("Make_Native: %s spec %d", Get_Sym_Name(type+1), SER_LEN(spec));

    REBARR *body = Make_Singular_Array(BLANK_VALUE); // no body by default
    MANAGE_ARRAY(body);

    REBFUN *fun = Make_Paramlist_Managed(spec, SYM_0, body, dispatch);

    *out = *FUNC_VALUE(fun);

    // Note: used to set the keys of natives as read-only so that the debugger
    // couldn't manipulate the values in a native frame out from under it,
    // potentially crashing C code (vs. just causing userspace code to
    // error).  That protection is now done to the frame series on reification
    // in order to be able to MAKE FRAME! and reuse the native's paramlist.
}


//
//  Get_Maybe_Fake_Func_Body: C
// 
// The FUNC_FLAG_LEAVE_OR_RETURN tricks used for definitional scoping
// make it seem like a generator authored more code in the function's
// body...but the code isn't *actually* there and an optimized internal
// trick is used.
// 
// If the body is fake, it needs to be freed by the caller with
// Free_Series.  This means that the body must currently be shallow
// copied, and the splicing slot must be in the topmost series.
//
REBARR *Get_Maybe_Fake_Func_Body(REBOOL *is_fake, const REBVAL *func)
{
    REBARR *fake_body;
    REBVAL *example = NULL;

    assert(IS_FUNCTION(func) && IS_FUNCTION_PLAIN(func));

    if (GET_VAL_FLAG(func, FUNC_FLAG_LEAVE_OR_RETURN)) {
        REBVAL *last_param = VAL_FUNC_PARAM(func, VAL_FUNC_NUM_PARAMS(func));

        if (SYM_RETURN == VAL_TYPESET_CANON(last_param))
            example = Get_System(SYS_STANDARD, STD_FUNC_BODY);
        else {
            assert(SYM_LEAVE == VAL_TYPESET_CANON(last_param));
            example = Get_System(SYS_STANDARD, STD_PROC_BODY);
        }
        *is_fake = TRUE;
    }
    else {
        *is_fake = FALSE;
        return VAL_FUNC_BODY(func);
    }

    // See comments in sysobj.r on standard/func-body and standard/proc-body
    //
    fake_body = Copy_Array_Shallow(VAL_ARRAY(example));

    // Index 5 (or 4 in zero-based C) should be #BODY, a "real" body
    //
    assert(IS_ISSUE(ARR_AT(fake_body, 4))); // #BODY
    Val_Init_Array(ARR_AT(fake_body, 4), REB_GROUP, VAL_FUNC_BODY(func));
    SET_VAL_FLAG(ARR_AT(fake_body, 4), VALUE_FLAG_LINE);

    return fake_body;
}


//
//  Make_Function_May_Fail: C
// 
// This is the support routine behind `MAKE FUNCTION!` (or CLOSURE!), the
// basic building block of creating functions in Rebol.
// 
// If `has_return` is passed in as TRUE, then is also the optimized native
// implementation for the function generators FUNC and CLOS.  Ren/C's
// schematic for these generators is *very* different from R3-Alpha, whose
// definition of FUNC was simply:
// 
//     make function! copy/deep reduce [spec body]
// 
// Not only does Ren/C's `make function!` already copy the spec and body,
// but FUNC and CLOS "use the internals to cheat".  They analyze and edit
// the spec, then potentially build an entity whose full "body" acts like:
// 
//     return: make function! [
//         [{Returns a value from a function.} value [<opt> any-value!]]
//         [exit/from/with (context-of 'return) :value]
//     ]
//     (body goes here)
// 
// This pattern addresses "Definitional Return" in a way that does not
// technically require building RETURN in as a language keyword in any
// specific form.  FUNC and CLOS optimize by not internally building
// or executing the equivalent body, but giving it back from BODY-OF.
// 
// NOTES:
// 
// The spec and body are copied--even for MAKE FUNCTION!--because:
// 
//    (a) It prevents tampering with the spec after it has been analyzed
//        by Make_Paramlist_Managed().  Such changes to the spec will not be
//        reflected in the actual behavior of the function.
// 
//    (b) The BLOCK! values inside the make-spec may actually be imaging
//        series at an index position besides the series head.  However,
//        the REBVAL for a FUNCTION! contains only three REBSER slots--
//        all in use, with no space for offsets.  A copy must be made
//        to truncate to the intended spec and body start (unless one
//        is willing to raise errors on non-head position series :-/)
// 
//    (c) Copying the root of the series into a series the user cannot
//        access makes it possible to "lie" about what the body "above"
//        is.  This gives FUNC and CLOS the edge to pretend to add
//        containing code and simulate its effects, while really only
//        holding onto the body the caller provided.  This trick may
//        prove useful for other optimizing generators.
// 
// While MAKE FUNCTION! has no RETURN, all functions still have EXIT as a
// non-definitional alternative.  Ren/C adds a /WITH refinement so it can
// behave equivalently to old-non-definitonal return.  There is even a way to
// identify specific points up the call stack to exit from via EXIT/FROM, so
// not having definitional return has several alternate options for generators
// that wish to use them.
// 
// This function will either successfully place a function value into
// `out` or not return...as a failed check on a function spec is
// raised as an error.
//
REBFUN *Make_Function_May_Fail(
    REBOOL is_procedure,
    const REBVAL *spec_val,
    const REBVAL *body_val,
    REBOOL has_return
) {
    REBOOL durable = FALSE;

    if (!IS_BLOCK(spec_val) || !IS_BLOCK(body_val))
        fail (Error_Bad_Func_Def(spec_val, body_val));

    REBARR *spec;

    if (VAL_LEN_AT(spec_val) == 0) {
        //
        // Empty specs are semi-common (e.g. DOES [...] is FUNC [] [...]).
        // Since the spec is read-only once put into the function value,
        // re-use an appropriate instance of [], [return:], or [leave:] based
        // on whether the "effective spec" needs a definitional exit or not.
        //
        if (has_return) {
            spec = is_procedure
                ? VAL_ARRAY(ROOT_LEAVE_BLOCK)
                : VAL_ARRAY(ROOT_RETURN_BLOCK);
        } else
            spec = EMPTY_ARRAY;
    }
    else if (!has_return) {
        //
        // If has_return is FALSE upon entry, then nothing in the spec disabled
        // a definitional exit...and this was called by `make function!`.  So
        // there are no bells and whistles (including <opt> or <...> tag
        // conversion).  It is "effectively <no-return>", though the
        // non-definitional EXIT and EXIT/WITH will still be available.
        //
        spec = Copy_Array_At_Deep_Managed(
            VAL_ARRAY(spec_val), VAL_INDEX(spec_val)
        );
    }
    else {
        // Trickier case: when the `func` or `clos` natives are used, they
        // must read the given spec the way a user-space generator might.
        // They must decide whether to add a specially handled RETURN
        // local, which will be given a tricky "native" definitional return

        REBCNT index = 0;
        REBOOL convert_local = FALSE;
        REBVAL *item;

        // We may add a return or leave, so avoid a later expansion by asking
        // for the capacity of the copy to have an extra value.  (May be a
        // waste if unused, but would require two passes to avoid it.)
        //
        spec = Copy_Array_At_Extra_Deep_Managed(
            VAL_ARRAY(spec_val),
            VAL_INDEX(spec_val),
            1 // +1 capacity hint
        );

        item = ARR_HEAD(spec);
        for (; NOT_END(item); index++, item++) {
            if (IS_SET_WORD(item)) {
                //
                // Note a "true local" (indicated by a set-word) is considered
                // to be tacit approval of wanting a definitional return
                // by the generator.  This helps because Red's model
                // for specifying returns uses a SET-WORD!
                //
                //     func [return: [integer!] {returns an integer}]
                //
                // In Ren/C's case it just means you want a local called
                // return, but the generator will be "initializing it
                // with a definitional return" for you.  You don't have
                // to use it if you don't want to...

                // !!! TBD: make type checking work (not yet implemented in
                // Red, either).  Will only be available as a generator
                // feature, by way of ENSURE-TYPE wrapping the body and the
                // argument typing on the return function.

                continue;
            }

            if (IS_TAG(item)) {
                if (
                    0 == Compare_String_Vals(item, ROOT_NO_RETURN_TAG, TRUE)
                ) {
                    // The <no-return> tag is a way to cue FUNC and PROC that
                    // you do not want a definitional return:
                    //
                    //     foo: func [<no-return> a] [return a]
                    //     foo 10 ;-- ERROR!
                    //
                    // This is redundant with the default for `make function!`.
                    // But having an option to use the familiar arity-2 form
                    // will probably appeal to more users.  Also, having two
                    // independent parameters can save the need for a REDUCE
                    // or COMPOSE that is generally required to composite a
                    // single block parameter that MAKE FUNCTION! requires.
                    //
                    has_return = FALSE;

                    // We *could* remove the <no-return> tag, or check to
                    // see if there's more than one, etc.  But Check_Func_Spec
                    // is tolerant of any strings that we leave in the spec.
                }
                else if (
                    0 == Compare_String_Vals(item, ROOT_PUNCTUATES_TAG, TRUE)
                ) {
                    // !!! Right now a BAR! in the top level is what is read
                    // as meaning punctuates by MAKE FUNCTION!, though this
                    // is perhaps not permanent.

                    SET_BAR(item);
                }
                else if (
                    0 == Compare_String_Vals(item, ROOT_LOCAL_TAG, TRUE)
                ) {
                    // While using x: and y: for pure locals is one option,
                    // it has two downsides.  One downside is that it makes
                    // the spec look too much "like everything else", so
                    // all the code kind of bleeds together.  Another is that
                    // if you nest one function within another then the outer
                    // function will wind up locals-gathering the locals of
                    // the inner function.  (It will anyway if you put the
                    // whole literal body there, but if you're adding the
                    // locals in a generator to be picked up by code that
                    // rebinds to them then it makes a difference.)
                    //
                    // Having a tag that lets you mark a run of locals is
                    // useful.  It will convert WORD! to SET-WORD! in the
                    // spec, and stop at the next refinement.
                    //
                    convert_local = TRUE;

                    // See notes about how we *could* remove ANY-STRING!s like
                    // the <local> tag from the spec, but Check_Func_Spec
                    // doesn't mind...it might be useful for HELP...and it's
                    // cheaper not to.
                }
                else if (
                    0 == Compare_String_Vals(item, ROOT_DURABLE_TAG, TRUE)
                ) {
                    // <durable> is currently a lesser version of what it
                    // hopes to be, but signals what R3-Alpha called CLOSURE!
                    // semantics.  Indicating that a typeset is durable in
                    // the low-level will need to be done with some notation
                    // that doesn't use "keywords"--perhaps a #[true] or a
                    // #[false] picked up on by the typeset.
                    //
                    // !!! Enforce only at the head, if it's going to be
                    // applying to everything??
                    //
                    durable = TRUE;
                }
                else
                    fail (Error(RE_BAD_FUNC_DEF, item));
            }
            else if (ANY_WORD(item)) {
                if (convert_local) {
                    if (IS_WORD(item)) {
                        //
                        // We convert words to set-words for pure local status
                        //
                        VAL_SET_TYPE_BITS(item, REB_SET_WORD);
                    }
                    else if (IS_REFINEMENT(item)) {
                        //
                        // A refinement signals us to stop doing the locals
                        // conversion.  Historically, help hides any
                        // refinements that appear behind a /local, so
                        // presumably it would do the same with <local>...
                        // but mechanically there is no way to tell
                        // Check_Func_Spec to hide a refinement.
                        //
                        convert_local = FALSE;
                    }
                    else {
                        // We've already ruled out pure locals, so this means
                        // they wrote something like:
                        //
                        //     func [a b <local> 'c #d :e]
                        //
                        // Consider that an error.
                        //
                        fail (Error(RE_BAD_FUNC_DEF, item));
                    }
                }

                if (SAME_SYM(VAL_WORD_SYM(item), SYM_RETURN)) {
                    //
                    // Although return: is explicitly tolerated,  all these
                    // would cancel a definitional return:
                    //
                    //     func [return [integer!]]
                    //     func [/value return]
                    //     func [/local return]
                    //
                    // The last one because /local is actually "just an ordinary
                    // refinement".  The choice of HELP to omit it could be
                    // a configuration setting.
                    //
                    has_return = FALSE;
                }
            }
            else if (IS_BLOCK(item)) {
                //
                // Blocks representing typesets must be inspected for
                // extension signifiers too, as MAKE TYPESET! doesn't know
                // any keywords either.
                //
                REBVAL *subitem = VAL_ARRAY_AT(item);
                for (; NOT_END(subitem); ++subitem) {
                    if (!IS_TAG(subitem))
                        continue;

                    if (
                        0 ==
                        Compare_String_Vals(subitem, ROOT_ELLIPSIS_TAG, TRUE)
                    ) {
                        // Notational convenience for variadic.
                        // func [x [<...> integer!]] => func [x [[integer!]]]
                        //
                        REBARR *array = Make_Singular_Array(item);
                        Remove_Series(
                            ARR_SERIES(VAL_ARRAY(item)),
                            subitem - VAL_ARRAY_AT(item),
                            1
                        );
                        Val_Init_Block(item, array);
                    }
                    else if (
                        0 == Compare_String_Vals(
                            subitem, ROOT_OPT_TAG, TRUE
                        )
                    ) {
                        // Notational convenience for optional.
                        // func [x [<opt> integer!]] => func [x [_ integer!]]
                        //
                        SET_BLANK(subitem);
                    }
                    else if (
                        0 == Compare_String_Vals(
                            subitem, ROOT_END_TAG, TRUE
                        )
                    ) {
                        // Notational convenience for endable.
                        // func [x [<end> integer!]] => func [x [| integer!]]
                        //
                        SET_BAR(subitem);
                    }
                }
            }
        }

        if (has_return) {
            //
            // No prior RETURN (or other issue) stopping definitional return!
            // Add the "true local" RETURN: to the spec.  +1 capacity was
            // reserved in anticipation of this possibility, so it should not
            // need to expand the array.
            //
            Append_Value(
                spec,
                is_procedure
                    ? ROOT_LEAVE_SET_WORD
                    : ROOT_RETURN_SET_WORD
            );
        }
    }

    // We copy the body.  It doesn't necessarily need to be unique, but if it
    // used the EMPTY_ARRAY then it would have to have the user function
    // dispatcher in its misc field (or be another similar array that did)
    //
    REBARR *body = Copy_Array_At_Deep_Managed(
        VAL_ARRAY(body_val), VAL_INDEX(body_val)
    );

    // Spec checking will longjmp out with an error if the spec is bad.
    // For efficiency, we tell the paramlist what symbol we would like to
    // have located in the final slot if its symbol is found (so SYM_RETURN
    // if the function has a optimized definitional return).
    //
    REBFUN *fun = Make_Paramlist_Managed(
        spec,
        has_return ? (is_procedure ? SYM_LEAVE : SYM_RETURN) : SYM_0,
        body,
        &Plain_Dispatcher
    );

    if (is_procedure)
        SET_VAL_FLAG(FUNC_VALUE(fun), FUNC_FLAG_PUNCTUATES);

    // Even if `has_return` was passed in true, the FUNC or CLOS generator
    // may have seen something to turn it off and turned it false.  But if
    // it's still on, then signal we want the fancy fake return!
    //
    if (has_return) {
        //
        // Make_Paramlist above should have ensured it's in the last slot.
        //
    #if !defined(NDEBUG)
        REBVAL *param = ARR_LAST(AS_ARRAY(fun));

        assert(is_procedure
            ? VAL_TYPESET_CANON(param) == SYM_LEAVE
            : VAL_TYPESET_CANON(param) == SYM_RETURN);

        assert(VAL_PARAM_CLASS(param) == PARAM_CLASS_PURE_LOCAL);
    #endif

        // Flag that this function has a definitional return, so Dispatch_Call
        // knows to write the "hacked" function in that final local.  (Arg
        // fulfillment should leave the hidden parameter unset)
        //
        SET_VAL_FLAG(FUNC_VALUE(fun), FUNC_FLAG_LEAVE_OR_RETURN);
    }

#if !defined(NDEBUG)
    //
    // If FUNC or MAKE FUNCTION! are being invoked from an array of code that
    // has been flagged "legacy" (e.g. the body of a function created after
    // `do <r3-legacy>` has been run) then mark the function with the setting
    // to make refinements and args blank instead of FALSE/void...if that
    // option is on.
    //
    if (
        LEGACY_RUNNING(OPTIONS_REFINEMENTS_BLANK)
        || GET_ARR_FLAG(VAL_ARRAY(spec_val), SERIES_FLAG_LEGACY)
        || GET_ARR_FLAG(VAL_ARRAY(body_val), SERIES_FLAG_LEGACY)
    ) {
        SET_VAL_FLAG(FUNC_VALUE(fun), FUNC_FLAG_LEGACY);
    }
#endif

    // !!! This is a lame way of setting the durability, because it means
    // that there's no way a user with just `make function!` could do it.
    // However, it's a step closer to the solution and eliminating the
    // FUNCTION!/CLOSURE! distinction.
    //
    if (durable) {
        REBVAL *param;
        param = FUNC_PARAMS_HEAD(fun);
        for (; NOT_END(param); ++param)
            SET_VAL_FLAG(param, TYPESET_FLAG_DURABLE);
    }

    // The argument and local symbols have been arranged in the function's
    // "frame" and are now in index order.  These numbers are put
    // into the binding as *negative* versions of the index, in order
    // to indicate that they are in a function and not an object frame.
    //
    // (This is done for durables body even though each call is associated
    // with an object frame.  The reason is that this is only the "archetype"
    // body of the durable...it is copied each time and the real numbers
    // filled in.  Having the indexes already done speeds the copying.)
    //
    Bind_Relative_Deep(fun, ARR_HEAD(body), TS_ANY_WORD);

#if !defined(NDEBUG)
    if (LEGACY(OPTIONS_MUTABLE_FUNCTION_BODIES))
        return fun; // don't run protection code below
#endif

    // All the series inside of a function body are "relatively bound".  This
    // means that there's only one copy of the body, but the series handle
    // is "viewed" differently based on which call it represents.  Though
    // each of these views compares uniquely, there's only one series behind
    // it...hence the series must be read only to keep modifying a view
    // that seems to have one identity but then affecting another.
    //
    // !!! The above is true in the specific-binding branch, but the rule
    // is applied to pre-specific-binding to prepare it for that future.
    //
    // !!! This protection needs to be system level, as the user is able to
    // unprotect conventional protection via UNPROTECT.
    //
    // !!! The protect interface is based on REBVALs at the moment, which
    // is used by the mandatory Unmark() routine as well.  Easier to use
    // than to figure out how to modify it to take series for this ATM.
    //
    REBVAL new_body;
    Val_Init_Block(&new_body, FUNC_BODY(fun));

    Protect_Series(&new_body, FLAGIT(PROT_DEEP) | FLAGIT(PROT_SET));
    assert(GET_ARR_FLAG(VAL_ARRAY(&new_body), SERIES_FLAG_LOCKED));
    Unmark(&new_body);

    return fun;
}


//
//  Make_Frame_For_Function: C
//
// This creates a *non-stack-allocated* FRAME!, which can be used in function
// applications or specializations.  It reuses the keylist of the function
// but makes a new varlist.
//
REBCTX *Make_Frame_For_Function(REBVAL *value) {
    //
    // Note that this cannot take just a REBFUN* directly, because definitional
    // RETURN and LEAVE only have their unique `exit_from` bits in the REBVAL.
    //
    REBFUN *func = VAL_FUNC(value);

    // In order to have the frame survive the call to MAKE and be
    // returned to the user it can't be stack allocated, because it
    // would immediately become useless.  Allocate dynamically.
    //
    REBARR *varlist = Make_Array(ARR_LEN(FUNC_PARAMLIST(func)));
    SET_ARR_FLAG(varlist, ARRAY_FLAG_CONTEXT_VARLIST);
    SET_ARR_FLAG(varlist, SERIES_FLAG_FIXED_SIZE);

    // Fill in the rootvar information for the context canon REBVAL
    //
    REBVAL *var = ARR_HEAD(varlist);
    VAL_RESET_HEADER(var, REB_FRAME);
    INIT_VAL_CONTEXT(var, AS_CONTEXT(varlist));

    // We can reuse the paramlist we're given, but note in the case of
    // definitional RETURN and LEAVE we have to stow the `exit_from` field
    // in the context, since the single archetype paramlist does not hold
    // enough information to know where to return *to*.
    //
    INIT_CTX_KEYLIST_SHARED(AS_CONTEXT(varlist), FUNC_PARAMLIST(func));
    ASSERT_ARRAY_MANAGED(CTX_KEYLIST(AS_CONTEXT(varlist)));

    // !!! The frame will never have stack storage if created this
    // way, because we return it...and it would be of no use if the
    // stackvars were empty--they could not be filled.  However it
    // will have an associated call if it is run.  We don't know what
    // that call pointer will be so NULL is put in for now--but any
    // extant FRAME! values of this type will have to use stack
    // walks to find the pointer (possibly recaching in values.)
    //
    INIT_CONTEXT_FRAME(AS_CONTEXT(varlist), NULL);
    VAL_CONTEXT_EXIT_FROM(CTX_VALUE(AS_CONTEXT(varlist)))
        = VAL_FUNC_EXIT_FROM(value);
    ++var;

    // A FRAME! defaults all args and locals to not being set.  If the frame
    // is then used as the storage for a function specialization, unset
    // vars indicate *unspecialized* arguments...not <opt> ones.  (This is
    // a good argument for not making <opt> have meaning that is interesting
    // to APPLY or SPECIALIZE cases, but to revoke the function's effects.
    //
    REBCNT n;
    for (n = 1; n <= FUNC_NUM_PARAMS(func); ++n, ++var)
        SET_VOID(var);

    SET_END(var);
    SET_ARRAY_LEN(varlist, ARR_LEN(FUNC_PARAMLIST(func)));

    return AS_CONTEXT(varlist);
}


//
//  Specialize_Function_Throws: C
//
// This produces a new REBVAL for a function that specializes another.  It
// uses a FRAME! to do this, where the frame intrinsically stores the
// reference to the function it is specializing.
//
REBOOL Specialize_Function_Throws(
    REBVAL *out,
    REBVAL *specializee,
    REBSYM opt_specializee_sym, // can be SYM_0
    REBVAL *block // !!! REVIEW: gets binding modified directly (not copied)
) {
    assert(out != specializee);

    REBCTX *exemplar;
    REBARR *exit_from;

    REBFUN *func = Find_Underlying_Func(&exit_from, &exemplar, specializee);

    if (exemplar) {
        //
        // Specializing a specialization is ultimately just a specialization
        // of the innermost function being specialized.  (Imagine specializing
        // a specialization of APPEND, to the point where it no longer takes
        // any parameters.  Nevertheless, the frame being stored and invoked
        // needs to have as many parameters as APPEND has.  The frame must be
        // be built for the code ultimately being called--and specializations
        // have no code of their own.)

        REBARR *varlist = Copy_Array_Deep_Managed(CTX_VARLIST(exemplar));
        SET_ARR_FLAG(varlist, ARRAY_FLAG_CONTEXT_VARLIST);
        INIT_CTX_KEYLIST_SHARED(AS_CONTEXT(varlist), CTX_KEYLIST(exemplar));

        exemplar = AS_CONTEXT(varlist); // okay, now make exemplar our copy
        INIT_VAL_CONTEXT(CTX_VALUE(exemplar), exemplar);
        // exit_from should be good to go
    }
    else {
        // An initial specialization is responsible for making a frame out
        // of the function's paramlist.  Frame vars default void.
        //
        exemplar = Make_Frame_For_Function(specializee);
        MANAGE_ARRAY(CTX_VARLIST(exemplar));
        exit_from = VAL_FUNC_EXIT_FROM(specializee);
    }

    // Archetypal frame values can't have exit_froms (would write paramlist)
    //
    assert(VAL_CONTEXT_EXIT_FROM(CTX_VALUE(exemplar)) == NULL);

    // Bind all the SET-WORD! in the body that match params in the frame
    // into the frame.  This means `value: value` can very likely have
    // `value:` bound for assignments into the frame while `value` refers
    // to whatever value was in the context the specialization is running
    // in, but this is likely the more useful behavior.  Review.
    //
    // !!! This binds the actual arg data, not a copy of it--following
    // OBJECT!'s lead.  However, ordinary functions make a copy of the body
    // they are passed before rebinding.  Rethink.
    //
    Bind_Values_Core(
        VAL_ARRAY_AT(block),
        exemplar,
        FLAGIT_KIND(REB_SET_WORD), // types to bind (just set-word!)
        0, // types to "add midstream" to binding as we go (nothing)
        BIND_DEEP
    );

    // Do the block into scratch space--we ignore the result (unless it is
    // thrown, in which case it must be returned.)
    {
        PUSH_GUARD_ARRAY(CTX_VARLIST(exemplar));

        if (DO_VAL_ARRAY_AT_THROWS(out, block)) {
            DROP_GUARD_ARRAY(CTX_VARLIST(exemplar));
            return TRUE;
        }

        DROP_GUARD_ARRAY(CTX_VARLIST(exemplar));
    }

    // Begin initializing the returned function value
    //
    VAL_RESET_HEADER(out, REB_FUNCTION);

    // The "body" is the FRAME! value of the specialization.  Though we may
    // not be able to touch the keylist of that frame to update the "archetype"
    // exit_from, we can patch this cell in the "body array" to hold it.
    //
    VAL_FUNC_BODY(out) = Make_Singular_Array(CTX_VALUE(exemplar));
    MANAGE_ARRAY(VAL_FUNC_BODY(out));
    VAL_CONTEXT_EXIT_FROM(VAL_FUNC_EXEMPLAR(out)) = exit_from;

    // Currently specializations never actually run their own dispatch, but
    // it may be useful to recognize the function category by a dispatcher
    //
    VAL_FUNC_DISPATCH(out) = &Specializer_Dispatcher;

    // Generate paramlist by way of the data stack.  Push empty value (to
    // become the function value afterward), then all the args that remain
    // unspecialized (indicated by being void...<opt> is not supported)
    //
    REBDSP dsp_orig = DSP;
    DS_PUSH_TRASH_SAFE; // later initialized as [0] canon value

    REBVAL *param = CTX_KEYS_HEAD(exemplar);
    REBVAL *arg = CTX_VARS_HEAD(exemplar);
    for (; NOT_END(param); ++param, ++arg) {
        if (IS_VOID(arg))
            DS_PUSH(param);
    }

    REBARR *paramlist = Pop_Stack_Values(dsp_orig);
    MANAGE_ARRAY(paramlist);
    out->payload.function.func = AS_FUNC(paramlist);

    // See %sysobj.r for `specialized-meta:` object template

    REBVAL *std_meta = Get_System(SYS_STANDARD, STD_SPECIALIZED_META);
    REBCTX *meta = Copy_Context_Shallow(VAL_CONTEXT(std_meta));

    assert(IS_VOID(CTX_VAR(meta, SELFISH(1)))); // no description by default
    *CTX_VAR(meta, SELFISH(2)) = *specializee;
    if (opt_specializee_sym != SYM_0)
        Val_Init_Word(CTX_VAR(meta, SELFISH(3)), REB_WORD, opt_specializee_sym);

    MANAGE_ARRAY(CTX_VARLIST(meta));
    VAL_FUNC_META(out) = meta;
    VAL_FUNC_EXIT_FROM(out) = NULL;

    // Update canon value's bits to match what we're giving back in out.
    //
    *ARR_HEAD(paramlist) = *out;

    return FALSE;
}


//
//  Clonify_Function: C
// 
// The "Clonify" interface takes in a raw duplicate value that one
// wishes to mutate in-place into a full-fledged copy of the value
// it is a clone of.  This interface can be more efficient than a
// "source in, dest out" copy...and clarifies the dangers when the
// source and destination are the same.
//
void Clonify_Function(REBVAL *value)
{
    REBFUN *func_orig;
    REBARR *paramlist_copy;

    // !!! Conceptually the only types it currently makes sense to speak of
    // copying are functions and closures.  Though the concept is a little
    // bit "fuzzy"...the idea is that the series which are reachable from
    // their body series by a deep copy would be their "state".  Hence
    // as a function runs, its "state" can change.  One can thus define
    // a copy as snapshotting that "state".  This has been the classic
    // interpretation that Rebol has taken.

    // !!! However, in R3-Alpha a closure's "archetype" (e.g. the one made
    // by `clos [a] [print a]`) never operates on its body directly... it
    // is copied each time.  And there is no way at present to get a
    // reference to a closure "instance" (an ANY-FUNCTION value with the
    // copied body in it).  This has carried over to <durable> for now.

    // !!! This leaves only one function type that is mechanically
    // clonable at all... the non-durable FUNCTION!.  While the behavior is
    // questionable, for now we will suspend disbelief and preserve what
    // R3-Alpha did until a clear resolution.

    if (!IS_FUNCTION(value) || !IS_FUNCTION_PLAIN(value))
        return;

    if (IS_FUNC_DURABLE(value))
        return;

    // No need to modify the spec or header.  But we do need to copy the
    // identifying parameter series, so that the copied function has a
    // unique identity on the stack from the one it is copying.  Otherwise
    // two calls on the stack would be seen as recursions of the same
    // function, sharing each others "stack relative locals".

    func_orig = VAL_FUNC(value);
    paramlist_copy = Copy_Array_Shallow(FUNC_PARAMLIST(func_orig));

    value->payload.function.func = AS_FUNC(paramlist_copy);

    VAL_FUNC_META(value) = FUNC_META(func_orig);

    VAL_FUNC_BODY(value) = Copy_Array_Deep_Managed(VAL_FUNC_BODY(value));
    VAL_FUNC_DISPATCH(value) = &Plain_Dispatcher;

    // Remap references in the body from paramlist_orig to our new copied
    // word list we saved in VAL_FUNC_PARAMLIST(value)

    Rebind_Values_Relative_Deep(
        func_orig,
        value->payload.function.func,
        ARR_HEAD(VAL_FUNC_BODY(value))
    );

    // The above phrasing came from deep cloning code, while the below was
    // in the Copy_Function code.  Evaluate if there is now "dead code"
    // relating to the difference.
/*
    Bind_Relative_Deep(
        VAL_FUNC_PARAMLIST(out),
        ARR_HEAD(VAL_FUNC_BODY(out)),
        TS_ANY_WORD
    );
*/

    // The first element in the paramlist is the identity of the function
    // value itself.  So we must update this value if we make a copy,
    // so the paramlist does not indicate the original.
    //
    *FUNC_VALUE(value->payload.function.func) = *value;

    MANAGE_ARRAY(VAL_FUNC_PARAMLIST(value));
}


//
//  Action_Dispatcher: C
//
REB_R Action_Dispatcher(struct Reb_Frame *f)
{
    Eval_Natives++;

    enum Reb_Kind type = VAL_TYPE(FRM_ARG(f, 1));
    assert(type < REB_MAX);

    // Handle special datatype test cases (eg. integer?).
    //
    if (FUNC_ACT(f->func) < REB_MAX_0) {
        if (TO_0_FROM_KIND(type) == FUNC_ACT(f->func))
            return R_TRUE;

        return R_FALSE;
    }

    REBACT action = Value_Dispatch[TO_0_FROM_KIND(type)];
    if (!action) fail (Error_Illegal_Action(type, FUNC_ACT(f->func)));

    return action(f, FUNC_ACT(f->func));
}


//
//  Plain_Dispatcher: C
//
REB_R Plain_Dispatcher(struct Reb_Frame *f)
{
    // In specific binding, we must always reify the frame and get it handed
    // over to the GC when calling user functions.  This is "costly" but
    // essential.  It is not technically necessary except for "closures"
    // prior to specific binding, but this helps exercise the code path
    // in the non-specific-binding branch.
    //
    REBCTX *frame_ctx = Context_For_Frame_May_Reify_Managed(f);

    Eval_Functions++;

    if (!IS_FUNC_DURABLE(FUNC_VALUE(f->func))) {
        //
        // Simple model with no deep copying or rebinding of the body on
        // a per-call basis.  Long-term this is planned to be able to handle
        // specific binding and durability as well, but for now it means
        // that words embedded in the shared blocks may only look up relative
        // to the currently running function.
        //
        if (Do_At_Throws(f->out, FUNC_BODY(f->func), 0))
            return R_OUT_IS_THROWN;
        else
            return R_OUT;
    }
    else {
        assert(f->varlist);

        // Clone the body of the closure to allow us to rebind words inside
        // of it so that they point specifically to the instances for this
        // invocation.  (Costly, but that is the mechanics of words at the
        // present time, until true relative binding is implemented.)
        //
        REBVAL body;
        VAL_RESET_HEADER(&body, REB_BLOCK);
        INIT_VAL_ARRAY(&body, Copy_Array_Deep_Managed(FUNC_BODY(f->func)));
        VAL_INDEX(&body) = 0;

        Rebind_Values_Specifically_Deep(
            f->func, frame_ctx, VAL_ARRAY_AT(&body)
        );

        // Protect the body from garbage collection during the course of the
        // execution.  (This is inexpensive...it just points `f->param` to it.)
        //
        PROTECT_FRM_X(f, &body);

        if (DO_VAL_ARRAY_AT_THROWS(f->out, &body))
            return R_OUT_IS_THROWN;
        else
            return R_OUT;

        // References to parts of this function's copied body may still be
        // extant, but we no longer need to hold it from GC.  Fortunately the
        // PROTECT_FRM_X will be implicitly dropped when the call ends.
    }
}


//
//  Specializer_Dispatcher: C
//
// The evaluator does not do any special "running" of a specialized frame.
// All of the contribution that the specialization has to make was taken care
// of at the time of generating the arguments to the underlying function.
//
// Though an attempt is made to use the work of "digging" past specialized
// frames, some exist deep as chains of specializations etc.  These have
// to just be peeled off when the chain runs.
//
REB_R Specializer_Dispatcher(struct Reb_Frame *f)
{
    REBVAL *exemplar = FUNC_EXEMPLAR(f->func);
    f->func = VAL_FUNC(CTX_FRAME_FUNC_VALUE(VAL_CONTEXT(exemplar)));
    f->exit_from = VAL_CONTEXT_EXIT_FROM(exemplar);

    return R_REDO;
}


//
//  Hooked_Dispatcher: C
//
// A hooked dispatch is based on poking a function value into the body of
// another function and just running that.  It destroys the existing body,
// but the hooking routine gives a new function back that can be substituted.
//
REB_R Hooked_Dispatcher(struct Reb_Frame *f)
{
    // Whatever was initially in the body of the function
    REBVAL *hook = KNOWN(ARR_HEAD(FUNC_BODY(f->func)));
    assert(IS_FUNCTION(hook));

    REBNAT dispatch = VAL_FUNC_DISPATCH(hook);

    f->func = VAL_FUNC(hook);
    f->exit_from = VAL_FUNC_EXIT_FROM(hook);

    return dispatch(f);
}


//
//  Routine_Dispatcher: C
//
REB_R Routine_Dispatcher(struct Reb_Frame *f)
{
    REBARR *args = Copy_Values_Len_Shallow(
        FRM_NUM_ARGS(f) > 0 ? FRM_ARG(f, 1) : NULL,
        FRM_NUM_ARGS(f)
    );

    Call_Routine(f->func, args, f->out);

    Free_Array(args);
    return R_OUT;
}


//
//  Adapter_Dispatcher: C
//
REB_R Adapter_Dispatcher(struct Reb_Frame *f)
{
    REBARR* adaptation = FUNC_BODY(f->func);
    assert(ARR_LEN(adaptation) == 2);
    REBARR* prelude = VAL_ARRAY(ARR_AT(adaptation, 0));
    REBVAL* adaptee = KNOWN(ARR_AT(adaptation, 1));

    // The first thing to do is run the prelude code, which may throw.  If it
    // does throw--including a RETURN--that means the adapted function will
    // not be run.
    //
    if (THROWN_FLAG == Do_Array_At_Core(
        f->out,
        NULL, // no virtual first element
        prelude,
        0, // index
        DO_FLAG_TO_END | DO_FLAG_LOOKAHEAD | DO_FLAG_ARGS_EVALUATE
    )) {
        return R_OUT_IS_THROWN;
    }

    // Next the adapted function needs to be run, but it could be any kind of
    // function.  Rather than repeat the logic and checks, this dispatcher
    // will ask Do_Core to "REDO" after changing the f->func.
    //
    // But the f->func we want isn't going to be a specialization.  Any top
    // level specializations have already contributed their part, merely
    // by virtue of setting up the frame.  Dig beneath them.
    //
    do {
        f->func = VAL_FUNC(adaptee);
        f->exit_from = VAL_FUNC_EXIT_FROM(adaptee);
    } while (IS_FUNCTION_SPECIALIZER(FUNC_VALUE(f->func)));

    // We have to run a type-checking sweep, to make sure the state of the
    // arguments is legal for the function.  Note that in particular,
    // a native function makes assumptions that the bit patterns are correct
    // for the set of types it takes...and most would crash the interpreter
    // if given a cell with an unexpected type in it.
    //
    REBVAL *test = f->arg;
    f->param = FUNC_PARAMS_HEAD(f->func);
    for (; NOT_END(test); ++test, ++f->param) {
        switch (VAL_PARAM_CLASS(f->param)) {
        case PARAM_CLASS_PURE_LOCAL:
            SET_VOID(test); // cheaper than checking
            break;

        case PARAM_CLASS_REFINEMENT:
            if (!IS_LOGIC(test))
                assert(FALSE);
            break;

        case PARAM_CLASS_HARD_QUOTE:
        case PARAM_CLASS_SOFT_QUOTE:
        case PARAM_CLASS_NORMAL:
            if (!TYPE_CHECK(f->param, VAL_TYPE(f->arg)))
                assert(FALSE);
            break;

        default:
            assert(FALSE);
        }
    }

    return R_REDO; // Have Do_Core run the adaptee updated into f->func
}


//
//  Chainer_Dispatcher: C
//
REB_R Chainer_Dispatcher(struct Reb_Frame *f)
{
    REBARR *pipeline = FUNC_BODY(f->func); // is the "pipeline" of functions

    // Before skipping off to find the underlying non-chained function
    // to kick off the execution, the post-processing pipeline has to
    // be "pushed" so it is not forgotten.  Go in reverse order so
    // the function to apply last is at the bottom of the stack.
    //
    REBVAL *value = KNOWN(ARR_LAST(pipeline));
    while (value != ARR_HEAD(pipeline)) {
        assert(IS_FUNCTION(value));
        DS_PUSH(KNOWN(value));
        --value;
    }

    // Extract the first function, itself which might be a chain.
    //
    f->func = VAL_FUNC(value);
    f->exit_from = VAL_FUNC_EXIT_FROM(value);

    return R_REDO;
}


//
//  func: native [
//  
//  "Defines a user function with given spec and body."
//  
//      spec [block!]
//          {Help string (opt) followed by arg words (and opt type + string)}
//      body [block!]
//          "The body block of the function"
//  ]
//
REBNATIVE(func)
//
// Native optimized implementation of a "definitional return" function
// generator.  See comments on Make_Function_May_Fail for full notes.
{
    PARAM(1, spec);
    PARAM(2, body);

    const REBOOL has_return = TRUE;
    const REBOOL is_procedure = FALSE;

    REBFUN *fun = Make_Function_May_Fail(
        is_procedure, ARG(spec), ARG(body), has_return
    );

    *D_OUT = *FUNC_VALUE(fun);
    return R_OUT;
}


//
//  proc: native [
//
//  "Defines a user function with given spec and body and no return result."
//
//      spec [block!]
//          {Help string (opt) followed by arg words (and opt type + string)}
//      body [block!]
//          "The body block of the function, use LEAVE to exit"
//  ]
//
REBNATIVE(proc)
//
// Short for "PROCedure"; inspired by the Pascal language's discernment in
// terminology of a routine that returns a value vs. one that does not.
// Provides convenient interface similar to FUNC that will not accidentally
// leak values to the caller.
{
    PARAM(1, spec);
    PARAM(2, body);

    const REBOOL has_return = TRUE;
    const REBOOL is_procedure = TRUE;

    REBFUN *fun = Make_Function_May_Fail(
        is_procedure, ARG(spec), ARG(body), has_return
    );

    *D_OUT = *FUNC_VALUE(fun);
    return R_OUT;
}


//
//  Get_If_Word_Or_Path_Arg: C
//
// Some routines like APPLY and SPECIALIZE are willing to take a WORD! or
// PATH! instead of just the value type they are looking for, and perform
// the GET for you.  By doing the GET inside the function, they are able
// to preserve the symbol:
//
//     >> apply 'append [value: 'c]
//     ** Script error: append is missing its series argument
//
void Get_If_Word_Or_Path_Arg(
    REBVAL *out,
    REBSYM *opt_sym, // may return SYM_0
    const REBVAL *value
) {
    REBVAL adjusted = *value;

    if (ANY_WORD(value)) {
        *opt_sym = VAL_WORD_SYM(value);
        VAL_SET_TYPE_BITS(&adjusted, REB_GET_WORD);
    }
    else if (ANY_PATH(value)) {
        //
        // In theory we could get a symbol here, assuming we only do non
        // evaluated GETs.  Not implemented at the moment.
        //
        *opt_sym = SYM_0;
        VAL_SET_TYPE_BITS(&adjusted, REB_GET_PATH);
    }
    else {
        *opt_sym = SYM_0;
        *out = *value;
        return;
    }

    if (DO_VALUE_THROWS(out, &adjusted)) {
        //
        // !!! GET_PATH should not evaluate GROUP!, and hence shouldn't be
        // able to throw.  TBD.
        //
        fail (Error_No_Catch_For_Throw(out));
    }
}


//
//  specialize: native [
//
//  {Create a new function through partial or full specialization of another}
//
//      value [function! any-word! any-path!]
//          {Function or specifying word (preserves word name for debug info)}
//      def [block!]
//          {Definition for FRAME! fields for args and refinements}
//  ]
//
REBNATIVE(specialize)
{
    PARAM(1, value);
    PARAM(2, def);

    REBSYM opt_sym; // may be SYM_0

    // We don't limit to taking a FUNCTION! value directly, because that loses
    // the symbol (for debugging, errors, etc.)  If caller passes a WORD!
    // then we lookup the variable to get the function, but save the symbol.
    //
    REBVAL specializee;
    Get_If_Word_Or_Path_Arg(&specializee, &opt_sym, ARG(value));

    if (!IS_FUNCTION(&specializee))
        fail (Error(RE_APPLY_NON_FUNCTION, ARG(value))); // for APPLY too

    if (Specialize_Function_Throws(D_OUT, &specializee, opt_sym, ARG(def)))
        return R_OUT_IS_THROWN;

    return R_OUT;
}


//
//  chain: native [
//
//  {Create a processing pipeline of functions that consume the last's result}
//
//      chainees [block!]
//      /only
//  ]
//
REBNATIVE(chain)
{
    PARAM(1, chainees);
    REFINE(2, only);

    REBVAL *out = D_OUT; // plan ahead for factoring into Chain_Function(out..

    assert(!REF(only)); // not written yet

    REBARR *chainees;

    if (REF(only)) {
        chainees = Copy_Array_At_Deep_Managed(
            VAL_ARRAY(ARG(chainees)),
            VAL_INDEX(ARG(chainees))
        );
    }
    else {
        if (Reduce_Array_Throws(
            out, VAL_ARRAY(ARG(chainees)), VAL_INDEX(ARG(chainees)), FALSE
        )) {
            return R_OUT_IS_THROWN;
        }

        chainees = VAL_ARRAY(out); // should be all specific values
        ASSERT_ARRAY_MANAGED(chainees);
    }

    // !!! Should validate pipeline here.  What would be legal besides
    // just functions...is it a dialect?

    assert(IS_FUNCTION(ARR_HEAD(chainees)));
    REBVAL *starter = KNOWN(ARR_HEAD(chainees));

    // Begin initializing the returned function value
    //
    VAL_RESET_HEADER(out, REB_FUNCTION);

    // The "body" is just the pipeline.
    //
    out->payload.function.body = chainees;
    VAL_FUNC_DISPATCH(out) = &Chainer_Dispatcher;

    // The paramlist needs to be unique to designate this function, but
    // will be identical typesets to the first function in the chain.  It's
    // [0] element must identify the function we're creating vs the original,
    // however.
    //
    REBARR *paramlist = Copy_Array_Shallow(VAL_FUNC_PARAMLIST(starter));
    MANAGE_ARRAY(paramlist);
    out->payload.function.func = AS_FUNC(paramlist);

    // See %sysobj.r for `specialized-meta:` object template

    REBVAL *std_meta = Get_System(SYS_STANDARD, STD_CHAINED_META);
    REBCTX *meta = Copy_Context_Shallow(VAL_CONTEXT(std_meta));

    assert(IS_VOID(CTX_VAR(meta, SELFISH(1)))); // no description by default
    Val_Init_Block(CTX_VAR(meta, SELFISH(2)), chainees);
    //
    // !!! There could be a system for preserving names in the chain, by
    // accepting lit-words instead of functions--or even by reading the
    // GET-WORD!s in the block.  Consider for the future.
    //
    assert(IS_VOID(CTX_VAR(meta, SELFISH(3))));

    MANAGE_ARRAY(CTX_VARLIST(meta));
    VAL_FUNC_META(out) = meta;
    VAL_FUNC_EXIT_FROM(out) = NULL;

    // Update canon value's bits to match what we're giving back in out.
    //
    *ARR_HEAD(paramlist) = *out;

    return R_OUT;
}


//
//  adapt: native [
//
//  {Create a variant of a function that preprocesses its arguments}
//
//      adaptee [function! any-word! any-path!]
//          {Function or specifying word (preserves word name for debug info)}
//      prelude [block!]
//          {Code to run in constructed frame before adapted function runs}
//  ]
//
REBNATIVE(adapt)
{
    PARAM(1, adaptee);
    PARAM(2, prelude);

    REBVAL *adaptee = ARG(adaptee);

    REBSYM opt_adaptee_sym;
    Get_If_Word_Or_Path_Arg(D_OUT, &opt_adaptee_sym, adaptee);
    if (!IS_FUNCTION(D_OUT))
        fail (Error(RE_APPLY_NON_FUNCTION, adaptee));

    *adaptee = *D_OUT;

    REBVAL *out = D_OUT; // plan ahead for factoring into Adapt_Function(out..

    // !!! In a future branch it may be possible that specific binding allows
    // a read-only input to be "viewed" with a relative binding, and no copy
    // would need be made if input was R/O.  For now, we copy to relativize.
    //
    // Note that the code needs to be bound to the same function keylist as
    // the adaptee's code is, because only one frame is being created--and
    // the adaptee's code already requires its keylist to be the one used.
    //
    REBARR *prelude = Copy_Array_At_Deep_Managed(
        VAL_ARRAY(ARG(prelude)),
        VAL_INDEX(ARG(prelude))
    );
    Bind_Relative_Deep(VAL_FUNC(adaptee), ARR_HEAD(prelude), TS_ANY_WORD);

    // We need to store the 2 values describing the adaptation so that Do_Core
    // knows what to do when it sees FUNC_CLASS_ADAPTED.  [0] is the prelude
    // BLOCK!, [1] is the FUNCTION! we've adapted.
    //
    // !!! This could be optimized to make a copy of the array leaving the
    // 0 cell blank to leave room for the adaptee, then DO the block from
    // the 1 index.
    //
    REBARR *adaptation = Make_Array(2);
    Val_Init_Block(ARR_AT(adaptation, 0), prelude);
    *ARR_AT(adaptation, 1) = *adaptee;
    SET_ARRAY_LEN(adaptation, 2);
    TERM_ARRAY(adaptation);
    MANAGE_ARRAY(adaptation);

    // Begin initializing the returned function value
    //
    VAL_RESET_HEADER(out, REB_FUNCTION);

    // The "body" is just the adaptation.
    //
    out->payload.function.body = adaptation;
    VAL_FUNC_DISPATCH(out) = &Adapter_Dispatcher;

    // The paramlist needs to be unique to designate this function, but
    // will be identical typesets to the original.  It's [0] element must
    // identify the function we're creating vs the original, however.
    //
    REBARR *paramlist = Copy_Array_Shallow(VAL_FUNC_PARAMLIST(adaptee));
    MANAGE_ARRAY(paramlist);
    out->payload.function.func = AS_FUNC(paramlist);

    // See %sysobj.r for `specialized-meta:` object template

    REBVAL *example = Get_System(SYS_STANDARD, STD_ADAPTED_META);

    REBCTX *meta = Copy_Context_Shallow(VAL_CONTEXT(example));
    assert(IS_VOID(CTX_VAR(meta, SELFISH(1)))); // no description by default
    *CTX_VAR(meta, SELFISH(2)) = *adaptee;
    if (opt_adaptee_sym != SYM_0)
        Val_Init_Word(CTX_VAR(meta, SELFISH(3)), REB_WORD, opt_adaptee_sym);

    MANAGE_ARRAY(CTX_VARLIST(meta));
    VAL_FUNC_META(out) = meta;
    VAL_FUNC_EXIT_FROM(out) = NULL;

    // Update canon value's bits to match what we're giving back in out.
    //
    *ARR_HEAD(paramlist) = *out;

    return R_OUT;
}


//
//  Apply_Frame_Core: C
//
// Work in progress to factor out common code used by DO and APPLY.  Needs
// to be streamlined.
//
// Expects the following Reb_Frame fields to be preloaded:
//
//    f->out
//    f->func
//    f->exit_from
//
// If opt_def is NULL, then f->data.context must be set
//
// !!! Because APPLY is being written as a regular native (and not a
// special exception case inside of Do_Core) it has to "re-enter" Do_Core
// and jump to the argument processing.  This is the first example of
// such a re-entry, and is not particularly streamlined yet.
//
// This could also be accomplished if function dispatch were a subroutine
// that would be called both here and from the evaluator loop.  But if
// the subroutine were parameterized with the frame state, it would be
// basically equivalent to a re-entry.  And re-entry is interesting to
// experiment with for other reasons (e.g. continuations), so that is what
// is used here.
//
REB_R Apply_Frame_Core(struct Reb_Frame *f, REBSYM sym, REBVAL *opt_def)
{
    assert(IS_FUNCTION(f->gotten));

    f->eval_type = ET_FUNCTION;
    SET_FRAME_SYM(f, sym);

    // We pretend our "input source" has ended.
    //
    f->value = END_CELL;
    f->indexor = 0;
    f->source.array = EMPTY_ARRAY;
    f->eval_fetched = NULL;
    f->lookback = FALSE;

    f->dsp_orig = DSP;

    f->flags =
        DO_FLAG_NEXT
        | DO_FLAG_NO_LOOKAHEAD
        | DO_FLAG_NO_ARGS_EVALUATE
        | DO_FLAG_APPLYING;

    // !!! We have to push a call here currently because prior to specific
    // binding, the stack gets walked to resolve variables.   Hence in the
    // apply case, Do_Core doesn't do its own push to the frame stack.
    //
    PUSH_CALL(f);

#if !defined(NDEBUG)
    //
    // We may push a data chunk, which is one of the things the snapshot state
    // checks.  It also checks the top of stack, so that has to be set as well.
    // So this has to come before Push_Or_Alloc_Vars
    //
    SNAP_STATE(&f->state);
#endif

    // If applying an existing FRAME! there should be no need to push vars
    // for it...it should have its own space.
    //
    if (opt_def) {
        Push_Or_Alloc_Args_For_Underlying_Func(f); // sets f->func
    }
    else {
        // f->func and f->exit_from should already be set

        // !!! This form of execution raises a ton of open questions about
        // what to do if a frame is used more than once.  Function calls
        // are allowed to destroy their arguments and will contaminate the
        // pure locals.  We need to treat this as a "non-specializing
        // specialization", and push a frame.  The narrow case of frame
        // reuse needs to be contained to something that a function can only
        // do to itself--e.g. to facilitate tail recursion, because no caller
        // but the function itself understands the state of its locals in situ.
        //
        ASSERT_CONTEXT(AS_CONTEXT(f->varlist));
        f->stackvars = NULL;
    }

    f->refine = TRUE_VALUE;
    f->cell.subfeed = NULL;

    f->arg = FRM_ARGS_HEAD(f);

    if (opt_def) {
        //
        // !!! Prior to specific binding, it's necessary to signal to the
        // Is_Function_Frame_Fulfilling() that this frame is *not* fulfilling
        // by setting f->param to END_CELL.  That way it will be considered
        // a valid target for the stack walk to do the binding.
        //
        f->param = END_CELL;

        if (f->varlist != NULL) {
            //
            // Here we are binding with a maybe-not-valid context.  Should
            // probably just use the keylist...
            //
            REBCTX *frame_ctx = Context_For_Frame_May_Reify_Managed(f);

            // There's a pool-allocated context, specific binding available
            //
            Bind_Values_Core(
                VAL_ARRAY_AT(opt_def),
                frame_ctx,
                FLAGIT_KIND(REB_SET_WORD), // types to bind (just set-word!)
                0, // types to "add midstream" to binding as we go (nothing)
                BIND_DEEP
            );
        }
        else {
            // Relative binding (long term this would be specific also)
            //
            Bind_Relative_Deep(
                f->func, VAL_ARRAY_AT(opt_def), FLAGIT_KIND(REB_SET_WORD)
            );
        }

        f->param = END_CELL; // for Is_Function_Frame_Fulfilling() during GC
        f->refine = NULL; // necessary since GC looks at it

        // Do the block into scratch space--we ignore the result (unless it is
        // thrown, in which case it must be returned.)
        //
        if (DO_VAL_ARRAY_AT_THROWS(f->out, opt_def)) {
            DROP_CALL(f);
            return R_OUT_IS_THROWN;
        }
    }

    f->param = FUNC_PARAMS_HEAD(f->func);
    SET_END(f->out);

    Do_Core(f);

    if (THROWN(f->out))
        return R_OUT_IS_THROWN; // prohibits recovery from exits

    assert(IS_END(f->value)); // we started at END_FLAG, can only throw

    return R_OUT;
}


//
//  apply: native [
//
//  {Invoke a function with all required arguments specified.}
//
//      value [function! any-word! any-path!]
//          {Function or specifying word (preserves word name for debug info)}
//      def [block!]
//          {Frame definition block (will be bound and evaluated)}
//  ]
//
REBNATIVE(apply)
{
    PARAM(1, value);
    PARAM(2, def);

    REBVAL *def = ARG(def);
    REBSYM sym;

    struct Reb_Frame frame;
    struct Reb_Frame *f = &frame;

#if !defined(NDEBUG)
    REBVAL *first_def = VAL_ARRAY_AT(def);

    // !!! Because APPLY has changed, help warn legacy usages by alerting
    // if the first element of the block is not a SET-WORD!.  A BAR! can
    // subvert the warning: `apply :foo [| comment {This is a new APPLY} ...]`
    //
    if (NOT_END(first_def)) {
        if (!IS_SET_WORD(first_def) && !IS_BAR(first_def)) {
            fail (Error(RE_APPLY_HAS_CHANGED));
        }
    }
#endif

    // We don't limit to taking a FUNCTION! value directly, because that loses
    // the symbol (for debugging, errors, etc.)  If caller passes a WORD!
    // then we lookup the variable to get the function, but save the symbol.
    //
    Get_If_Word_Or_Path_Arg(D_OUT, &sym, ARG(value));
    if (sym == SYM_0)
        sym = SYM___ANONYMOUS__; // Do_Core requires *some* non SYM_0 symbol

    if (!IS_FUNCTION(D_OUT))
        fail (Error(RE_APPLY_NON_FUNCTION, ARG(value))); // for SPECIALIZE too

    f->gotten = D_OUT;
    f->out = D_OUT;

    return Apply_Frame_Core(f, sym, def);
}


#if !defined(NDEBUG)

//
//  FUNC_PARAM_Debug: C
//
REBVAL *FUNC_PARAM_Debug(REBFUN *f, REBCNT n) {
    assert(n != 0 && n < ARR_LEN(FUNC_PARAMLIST(f)));
    return ARR_AT(FUNC_PARAMLIST(f), (n));
}


//
//  VAL_FUNC_Debug: C
//
REBFUN *VAL_FUNC_Debug(const REBVAL *v) {
    REBFUN *func = v->payload.function.func;
    struct Reb_Value_Header v_header = v->header;
    struct Reb_Value_Header func_header = FUNC_VALUE(func)->header;

    assert(IS_FUNCTION(v));
    assert(func == FUNC_VALUE(func)->payload.function.func);
    assert(GET_ARR_FLAG(FUNC_PARAMLIST(func), SERIES_FLAG_ARRAY));

    assert(VAL_FUNC_BODY(v) == FUNC_BODY(func));
    assert(VAL_FUNC_DISPATCH(v) == FUNC_DISPATCH(func));

    // set VALUE_FLAG_LINE on both headers for sake of comparison, we allow
    // it to be different from the value stored in frame.
    //
    // !!! Should formatting flags be moved into their own section, perhaps
    // the section currently known as "resv: reserved for future use"?
    //
    // We also set VALUE_FLAG_THROWN as that is not required to be sync'd
    // with the persistent value in the function.  This bit is deprecated
    // however, for many of the same reasons it's a nuisance here.
    //
    v_header.bits |= (
        VALUE_FLAG_LINE
        | VALUE_FLAG_THROWN
        | VALUE_FLAG_EVALUATED
    );
    func_header.bits |= (
        VALUE_FLAG_LINE
        | VALUE_FLAG_THROWN
        | VALUE_FLAG_EVALUATED
    );

    if (v_header.bits != func_header.bits) {
        //
        // If this happens, these help with debugging if stopped at breakpoint.
        //
        REBVAL *func_value = FUNC_VALUE(func);
        REBOOL has_return_value
            = GET_VAL_FLAG(v, FUNC_FLAG_LEAVE_OR_RETURN);
        REBOOL has_return_func
            = GET_VAL_FLAG(func_value, FUNC_FLAG_LEAVE_OR_RETURN);

        Debug_Fmt("Mismatch header bits found in FUNC_VALUE from payload");
        Panic_Array(FUNC_PARAMLIST(func));
    }

    return func;
}

#endif
