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
REBARR *List_Func_Words(const REBVAL *func)
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
            // treat as invisible and do not expose via WORDS-OF
            continue;

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
// ["description" arg "notes" [type! type2! ...] /ref ...]
// 
// Throw an error for invalid values.
//
REBARR *Make_Paramlist_Managed(
    REBARR *spec,
    REBOOL *punctuates,
    REBCNT opt_sym_last
) {
    REBVAL *item;
    REBARR *paramlist;
    REBVAL *typeset;

    *punctuates = FALSE;

    // Use a temporary to hold a value being "bubbled" toward the end if there
    // was a request for a canon symbol to be moved to the end.  (Feature used
    // by definitional return.)
    //
    // !!! This could be done more efficiently as a feature of Collect_Keylist
    // when it was forming the array, but that efficiency would be at the cost
    // of burdening Collect_Keylist's interface and adding overhead for more
    // common binding operations than function spec analysis.
    //
    REBVAL bubble;
    SET_END(&bubble); // not holding a value being bubbled to end...

    // Start by reusing the code that makes keylists out of Rebol-structured
    // data.  Scan for all words and error on duplicates
    //
    paramlist = Collect_Keylist_Managed(
        NULL, ARR_HEAD(spec), NULL, COLLECT_ANY_WORD | COLLECT_NO_DUP
    );

    // Whatever function is being made, it must fill in the paramlist slot 0
    // with an ANY-FUNCTION! value corresponding to the function that it is
    // the paramlist of.  Use SET_TRASH so that the debug build will leave
    // an alarm if that value isn't thrown in (the GC would complain...)

    typeset = ARR_HEAD(paramlist);
    SET_TRASH_IF_DEBUG(typeset);

    // !!! needs more checks
    for (item = ARR_HEAD(spec); NOT_END(item); item++) {

        if (ANY_BINSTR(item)) {
            // A goal of the Ren-C design is that core generators like
            // MAKE FUNCTION! an MAKE OBJECT! do not know any keywords or
            // key strings.  As a consequence, the most flexible offering
            // to function generators is to allow them to let as many
            // strings or tags or otherwise be stored in the spec as
            // they might wish to.  It's up to them to take them out.
            //
            // So it's not Check_Func_Spec's job to filter out "bad" string
            // patterns.  Anything is fair game:
            //
            //      [foo [type!] {doc string :-)}]
            //      [foo {doc string :-/} [type!]]
            //      [foo {doc string1 :-/} {doc string2 :-(} [type!]]
            //
            // HELP and other clients of SPEC-OF are left with the burden of
            // sorting out the variants.  The current policy of HELP is only
            // to show strings.
            //
            // !!! Though the system isn't supposed to have a reaction to
            // strings, is there a meaning for BINARY! besides ignoring it?

            continue;
        }

        if (IS_BLOCK(item)) {
            REBVAL *attribute;

            if (typeset != ARR_HEAD(paramlist)) {
                //
                // Turn block into typeset for parameter at current index
                // Note: Make_Typeset leaves VAL_TYPESET_SYM as-is
                //
                Update_Typeset_Bits_Core(
                    typeset,
                    VAL_ARRAY_HEAD(item),
                    FALSE // `trap`: false means fail vs. return FALSE if error
                );

                // A hard quote can only get a void if it is an <end>.
                //
                if (VAL_PARAM_CLASS(typeset) == PARAM_CLASS_HARD_QUOTE)
                    if (TYPE_CHECK(typeset, REB_0)) {
                        REBVAL param_name;
                        Val_Init_Word(
                            &param_name, REB_WORD, VAL_TYPESET_SYM(typeset)
                        );
                        fail (Error(RE_HARD_QUOTE_VOID, &param_name));
                    }

                continue;
            }

            // !!! Rebol2 had the ability to put a block in the first
            // slot before any parameters, in which you could put words.
            // This is deprecated in favor of the use of tags.  We permit
            // [catch] and [throw] during Rebol2 => Rebol3 migration.
            //
            // !!! Longer-term this will likely be where a typeset goes that
            // indicates the return type of the function.  The tricky part
            // of that is there's nowhere to put that typeset.  Adding it
            // as a key to the frame would add an extra VAR to the frame
            // also...which would be a possibility, perhaps with a special
            // symbol ID.  The storage space for the VAR might not need
            // to be wasted; there may be another use for a value-sized
            // spot per-invocation.
            //
            attribute = VAL_ARRAY_AT(item);
            for (; NOT_END(attribute); attribute++) {
                if (IS_WORD(attribute)) {
                    if (VAL_WORD_SYM(attribute) == SYM_CATCH)
                        continue; // ignore it;
                    if (VAL_WORD_SYM(attribute) == SYM_THROW) {
                        // !!! Basically a synonym for <no-return>, but
                        // transparent is now a manipulation done by the
                        // function generators *before* the internal spec
                        // is checked...and the flag is removed.  So
                        // simulating it here is no longer easy...hence
                        // ignore it;
                        //
                        continue;
                    }
                    // no other words supported, fall through to error
                }
                fail (Error(RE_BAD_FUNC_DEF, item));
            }
            continue;
        }

        if (IS_BAR(item)) {
            *punctuates = TRUE;
            continue;
        }

        if (!ANY_WORD(item))
            fail (Error(RE_BAD_FUNC_DEF, item));

        typeset++;

        assert(
            IS_TYPESET(typeset)
            && VAL_TYPESET_SYM(typeset) == VAL_WORD_SYM(item)
        );

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

            // !!! The typeset bits of a refinement are not currently used.
            // They are checked for TRUE or FALSE but this is done literally
            // by the code.  This means that every refinement has some spare
            // bits available in it for another purpose.
            //
            VAL_TYPESET_BITS(typeset) = 0;
            break;

        case REB_SET_WORD:
            // "Pure locals"... these will not be visible via WORDS-OF and
            // will be skipped during argument fulfillment.  We re-use the
            // same option flag that is used to hide words other places.
            //
            INIT_VAL_PARAM_CLASS(typeset, PARAM_CLASS_PURE_LOCAL);
            break;

        default:
            fail (Error(RE_BAD_FUNC_DEF, item));
        }

        if (VAL_TYPESET_CANON(typeset) == opt_sym_last) {
            //
            // If we find the canon symbol we were looking for then grab it
            // into the bubble.
            //
            assert(opt_sym_last != SYM_0 && IS_END(&bubble));
            bubble = *typeset;
        }
        else if (NOT_END(&bubble)) {
            //
            // If we already found our bubble, keep moving the typeset bits
            // back one slot to cover up each hole left.
            //
            *(typeset - 1) = *typeset;
        }

        assert(VAL_PARAM_CLASS(typeset) != PARAM_CLASS_0);
    }

    // Note the above code leaves us in the final typeset position... the loop
    // is incrementing the *spec* and bumps the typeset on demand.
    //
    assert(IS_END(typeset + 1));

    // If we were looking for something to bubble to the end, assert we've
    // found it...and place it in that final slot.  (It may have come from
    // the last slot so it's a No-Op, but no reason to check that.)
    //
    if (opt_sym_last != SYM_0) {
        assert(NOT_END(&bubble));
        *typeset = bubble;

        // !!! For now we set the typeset of the element to ALL_64, because
        // this is where the definitional return will hide its type info.
        // Until a notation is picked for the spec this capability isn't
        // enabled, but will be.
        //
        VAL_TYPESET_BITS(typeset) = ALL_64;
    }

    // Make sure the parameter list does not expand.
    //
    // !!! Should more precautions be taken, at some point locking and
    // protecting the whole array?  (It will be changed more by the caller,
    // but after that.)
    //
    SET_ARR_FLAG(paramlist, SERIES_FLAG_FIXED_SIZE);

    return paramlist;
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
    REBNAT code,
    enum Reb_Func_Class fclass
) {
    //Print("Make_Native: %s spec %d", Get_Sym_Name(type+1), SER_LEN(spec));

    ENSURE_ARRAY_MANAGED(spec);

    VAL_RESET_HEADER(out, REB_FUNCTION);
    INIT_VAL_FUNC_CLASS(out, fclass);

    VAL_FUNC_CODE(out) = code;
    VAL_FUNC_SPEC(out) = spec;

    REBOOL punctuates;
    out->payload.function.func
        = AS_FUNC(Make_Paramlist_Managed(spec, &punctuates, SYM_0));

    if (punctuates)
        SET_VAL_FLAG(out, FUNC_FLAG_PUNCTUATES);

    // Save the function value in slot 0 of the paramlist so that having
    // just the paramlist REBARR can get you the full REBVAL of the function
    // that it is the paramlist for.

    *FUNC_VALUE(out->payload.function.func) = *out;

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

    assert(IS_FUNCTION(func) && VAL_FUNC_CLASS(func) == FUNC_CLASS_USER);

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
//  Make_Function: C
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
void Make_Function(
    REBVAL *out,
    REBOOL is_procedure,
    const REBVAL *spec,
    const REBVAL *body,
    REBOOL has_return
) {
    REBOOL durable = FALSE;

    VAL_RESET_HEADER(out, REB_FUNCTION); // clears value flags in header...
    INIT_VAL_FUNC_CLASS(out, FUNC_CLASS_USER);

    if (is_procedure)
        SET_VAL_FLAG(out, FUNC_FLAG_PUNCTUATES);

    if (!IS_BLOCK(spec) || !IS_BLOCK(body))
        fail (Error_Bad_Func_Def(spec, body));

    if (VAL_LEN_AT(spec) == 0) {
        //
        // Empty specs are semi-common (e.g. DOES [...] is FUNC [] [...]).
        // Since the spec is read-only once put into the function value,
        // re-use an appropriate instance of [], [return:], or [leave:] based
        // on whether the "effective spec" needs a definitional exit or not.
        //
        if (has_return) {
            VAL_FUNC_SPEC(out) = is_procedure
                ? VAL_ARRAY(ROOT_LEAVE_BLOCK)
                : VAL_ARRAY(ROOT_RETURN_BLOCK);
        } else
            VAL_FUNC_SPEC(out) = EMPTY_ARRAY;
    }
    else if (!has_return) {
        //
        // If has_return is FALSE upon entry, then nothing in the spec disabled
        // a definitional exit...and this was called by `make function!`.  So
        // there are no bells and whistles (including <opt> or <...> tag
        // conversion).  It is "effectively <no-return>", though the
        // non-definitional EXIT and EXIT/WITH will still be available.
        //
        VAL_FUNC_SPEC(out) = Copy_Array_At_Deep_Managed(
            VAL_ARRAY(spec), VAL_INDEX(spec)
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
        VAL_FUNC_SPEC(out) = Copy_Array_At_Extra_Deep_Managed(
            VAL_ARRAY(spec),
            VAL_INDEX(spec),
            1 // +1 capacity hint
        );

        item = ARR_HEAD(VAL_FUNC_SPEC(out));
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
                    // This tolerance exists because the system is not to have
                    // any features based on recognizing specific keywords,
                    // so there's no need for tags to be "for future expansion"
                    // ... hence the mechanical cost burden of being forced
                    // to copy and remove them is a cost generators may not
                    // want to pay.

                    /*Remove_Series(VAL_FUNC_SPEC(out), index, 1);*/
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
                VAL_FUNC_SPEC(out),
                is_procedure
                    ? ROOT_LEAVE_SET_WORD
                    : ROOT_RETURN_SET_WORD
            );
        }
    }

    // Spec checking will longjmp out with an error if the spec is bad.
    // For efficiency, we tell the paramlist what symbol we would like to
    // have located in the final slot if its symbol is found (so SYM_RETURN
    // if the function has a optimized definitional return).
    //
    REBOOL punctuates;
    out->payload.function.func = AS_FUNC(
        Make_Paramlist_Managed(
            VAL_FUNC_SPEC(out),
            &punctuates,
            has_return ? (is_procedure ? SYM_LEAVE : SYM_RETURN) : SYM_0
        )
    );

    if (punctuates)
        SET_VAL_FLAG(out, FUNC_FLAG_PUNCTUATES);

    // We copy the body or do the empty body optimization to not copy and
    // use the EMPTY_ARRAY (which probably doesn't happen often...)
    //
    if (VAL_LEN_AT(body) == 0)
        VAL_FUNC_BODY(out) = EMPTY_ARRAY;
    else
        VAL_FUNC_BODY(out) = Copy_Array_At_Deep_Managed(
            VAL_ARRAY(body), VAL_INDEX(body)
        );

    // Even if `has_return` was passed in true, the FUNC or CLOS generator
    // may have seen something to turn it off and turned it false.  But if
    // it's still on, then signal we want the fancy fake return!
    //
    if (has_return) {
        //
        // Make_Paramlist above should have ensured it's in the last slot.
        //
    #if !defined(NDEBUG)
        REBVAL *param = ARR_LAST(AS_ARRAY(out->payload.function.func));

        assert(is_procedure
            ? VAL_TYPESET_CANON(param) == SYM_LEAVE
            : VAL_TYPESET_CANON(param) == SYM_RETURN);

        assert(VAL_PARAM_CLASS(param) == PARAM_CLASS_PURE_LOCAL);
    #endif

        // Flag that this function has a definitional return, so Dispatch_Call
        // knows to write the "hacked" function in that final local.  (Arg
        // fulfillment should leave the hidden parameter unset)
        //
        SET_VAL_FLAG(out, FUNC_FLAG_LEAVE_OR_RETURN);
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
        || GET_ARR_FLAG(VAL_ARRAY(spec), SERIES_FLAG_LEGACY)
        || GET_ARR_FLAG(VAL_ARRAY(body), SERIES_FLAG_LEGACY)
    ) {
        SET_VAL_FLAG(out, FUNC_FLAG_LEGACY);
    }
#endif

    // Now that we've created the function's fields, we pull a trick.  It
    // would be useful to be able to navigate to a full function value
    // given just its identifying series, but where to put it?  We use
    // slot 0 (a trick learned from R3-Alpha's object strategy)
    //
    *FUNC_VALUE(out->payload.function.func) = *out;

    // !!! This is a lame way of setting the durability, because it means
    // that there's no way a user with just `make function!` could do it.
    // However, it's a step closer to the solution and eliminating the
    // FUNCTION!/CLOSURE! distinction.
    //
    if (durable) {
        REBVAL *param;
        param = VAL_FUNC_PARAMS_HEAD(out);
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
    Bind_Relative_Deep(
        VAL_FUNC(out), ARR_HEAD(VAL_FUNC_BODY(out)), TS_ANY_WORD
    );

#if !defined(NDEBUG)
    if (LEGACY(OPTIONS_MUTABLE_FUNCTION_BODIES))
        return; // don't run protection code below
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
    Val_Init_Block(&new_body, VAL_FUNC_BODY(out));

    Protect_Series(&new_body, FLAGIT(PROT_DEEP) | FLAGIT(PROT_SET));
    assert(GET_ARR_FLAG(VAL_ARRAY(&new_body), SERIES_FLAG_LOCKED));
    Unmark(&new_body);
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

    // Usually we can reuse the keylist we're given, but the exception is in
    // the case of definitional return and leave.  The problem there is that
    // each return and leave is not actually a unique function, but we would
    // be only storing the unused archetypal RETURN function in the rootkey.
    // We have to make a new keylist if that's the case.
    //
    REBARR *paramlist = FUNC_PARAMLIST(func);
    if (func == NAT_FUNC(return) || func == NAT_FUNC(leave)) {
        paramlist = Copy_Array_Deep_Managed(paramlist);
        assert(VAL_FUNC_EXIT_FROM(value) != NULL);
        *ARR_AT(paramlist, 0) = *value;
    }
    INIT_CTX_KEYLIST_SHARED(AS_CONTEXT(varlist), paramlist);
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
    CTX_STACKVARS(AS_CONTEXT(varlist)) = NULL;
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
    REBVAL *func_value,
    REBSYM opt_original_sym,
    REBVAL *block // !!! REVIEW: gets binding modified directly (not copied)
) {
    REBCTX *frame_ctx;

    if (VAL_FUNC_CLASS(func_value) == FUNC_CLASS_SPECIALIZED) {
        //
        // Specializing a specialization is ultimately just a specialization
        // of the innermost function being specialized.  (Imagine specializing
        // a specialization of APPEND, to the point where it no longer takes
        // any parameters.  Nevertheless, the frame being stored and invoked
        // needs to have as many parameters as APPEND has.  The frame must be
        // be built for the code ultimately being called--and specializations
        // have no code of their own.)
        //
        frame_ctx = AS_CONTEXT(Copy_Array_Deep_Managed(
            CTX_VARLIST(func_value->payload.function.impl.special)
        ));
        INIT_CTX_KEYLIST_SHARED(
            frame_ctx,
            CTX_KEYLIST(func_value->payload.function.impl.special)
        );
        SET_ARR_FLAG(CTX_VARLIST(frame_ctx), ARRAY_FLAG_CONTEXT_VARLIST);
        INIT_VAL_CONTEXT(CTX_VALUE(frame_ctx), frame_ctx);
    }
    else {
        // An initial specialization is responsible for making a frame out
        // of the function's paramlist.  Frame vars default void.
        //
        frame_ctx = Make_Frame_For_Function(func_value);
        MANAGE_ARRAY(CTX_VARLIST(frame_ctx)); // because above case manages
    }

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
        frame_ctx,
        FLAGIT_KIND(REB_SET_WORD), // types to bind (just set-word!)
        0, // types to "add midstream" to binding as we go (nothing)
        BIND_DEEP
    );

    // Do the block into scratch space--we ignore the result (unless it is
    // thrown, in which case it must be returned.)
    {
        PUSH_GUARD_ARRAY(CTX_VARLIST(frame_ctx));

        if (DO_VAL_ARRAY_AT_THROWS(out, block)) {
            DROP_GUARD_ARRAY(CTX_VARLIST(frame_ctx));
            return TRUE;
        }

        DROP_GUARD_ARRAY(CTX_VARLIST(frame_ctx));
    }

    // The spec is specially generated to be an optimized single-element
    // series with a WORD! of the symbol of the function being specialized
    // (if any).  The non-trivial generation process for a "fake" spec derived
    // from the original function's spec is left to SPEC-OF, which will only
    // be run when necessary.
    //
    Val_Init_Word(out, REB_WORD, opt_original_sym);
    REBARR *spec = Make_Singular_Array(out);
    MANAGE_ARRAY(spec);

    // Begin initializing the returned function value
    //
    VAL_RESET_HEADER(out, REB_FUNCTION);
    INIT_VAL_FUNC_CLASS(out, FUNC_CLASS_SPECIALIZED);
    out->payload.function.spec = spec;

    // The "body" is just the frame of specialization information.
    //
    out->payload.function.impl.special = frame_ctx;

    // Generate paramlist by way of the data stack.  Push empty value (to
    // become the function value afterward), then all the args that remain
    // unspecialized (indicated by being void...<opt> is not supported)
    //
    REBDSP dsp_orig = DSP;
    DS_PUSH_TRASH_SAFE; // later initialized as [0] canon value

    REBVAL *param = CTX_KEYS_HEAD(frame_ctx);
    REBVAL *arg = CTX_VARS_HEAD(frame_ctx);
    for (; NOT_END(param); ++param, ++arg) {
        if (!IS_VOID(arg))
            DS_PUSH(param);
    }

    REBARR *paramlist = Pop_Stack_Values(dsp_orig);
    MANAGE_ARRAY(paramlist);
    out->payload.function.func = AS_FUNC(paramlist);

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

    if (!IS_FUNCTION_AND(value, FUNC_CLASS_USER))
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

    VAL_FUNC_BODY(value) = Copy_Array_Deep_Managed(VAL_FUNC_BODY(value));

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
//  Do_Native_Core_Throws: C
//
REBOOL Do_Native_Core_Throws(struct Reb_Frame *f)
{
    REB_R ret;

    Eval_Natives++;

    // For all other native function pointers (for now)...ordinary dispatch.

    ret = FUNC_CODE(f->func)(f);

    switch (ret) {
    case R_OUT: // put sequentially in switch() for jump-table optimization
        break;
    case R_OUT_IS_THROWN:
        return TRUE;
    case R_BLANK:
        SET_BLANK(f->out);
        break;
    case R_VOID:
        SET_VOID(f->out);
        break;
    case R_TRUE:
        SET_TRUE(f->out);
        break;
    case R_FALSE:
        SET_FALSE(f->out);
        break;
    default:
        assert(FALSE);
    }

    return FALSE;
}


//
//  Do_Action_Core_Throws: C
//
REBOOL Do_Action_Core_Throws(struct Reb_Frame *f)
{
    enum Reb_Kind type = VAL_TYPE(FRM_ARG(f, 1));
    REBACT action;
    REB_R ret;

    Eval_Natives++;

    assert(type < REB_MAX);

    // Handle special datatype test cases (eg. integer?).
    //
    if (FUNC_ACT(f->func) < REB_MAX_0) {
        if (TO_0_FROM_KIND(type) == FUNC_ACT(f->func))
            SET_TRUE(f->out);
        else
            SET_FALSE(f->out);

        return FALSE;
    }

    action = Value_Dispatch[TO_0_FROM_KIND(type)];
    if (!action) fail (Error_Illegal_Action(type, FUNC_ACT(f->func)));
    ret = action(f, FUNC_ACT(f->func));

    switch (ret) {
    case R_OUT: // put sequentially in switch() for jump-table optimization
        break;
    case R_OUT_IS_THROWN:
        return TRUE;
    case R_BLANK:
        SET_BLANK(f->out);
        break;
    case R_VOID:
        SET_VOID(f->out);
        break;
    case R_TRUE:
        SET_TRUE(f->out);
        break;
    case R_FALSE:
        SET_FALSE(f->out);
        break;
    default:
        assert(FALSE);
    }
    return FALSE;
}


//
//  Do_Function_Core_Throws: C
//
REBOOL Do_Function_Core_Throws(struct Reb_Frame *f)
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
        return Do_At_Throws(f->out, FUNC_BODY(f->func), 0);
    }
    else {
        assert(f->flags & DO_FLAG_HAS_VARLIST);

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

        return DO_VAL_ARRAY_AT_THROWS(f->out, &body);

        // References to parts of this function's copied body may still be
        // extant, but we no longer need to hold it from GC.  Fortunately the
        // PROTECT_FRM_X will be implicitly dropped when the call ends.
    }
}


//
//  Do_Routine_Core_Throws: C
//
REBOOL Do_Routine_Core_Throws(struct Reb_Frame *f)
{
    REBARR *args = Copy_Values_Len_Shallow(
        FRM_NUM_ARGS(f) > 0 ? FRM_ARG(f, 1) : NULL,
        FRM_NUM_ARGS(f)
    );

    Call_Routine(f->func, args, f->out);

    Free_Array(args);

    // Note: cannot "throw" a Rebol value across an FFI boundary.

    return FALSE;
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
// generator.  See comments on Make_Function for full notes.
{
    PARAM(1, spec);
    PARAM(2, body);

    const REBOOL has_return = TRUE;
    const REBOOL is_procedure = FALSE;

    Make_Function(D_OUT, is_procedure, ARG(spec), ARG(body), has_return);
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

    Make_Function(D_OUT, is_procedure, ARG(spec), ARG(body), has_return);

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
    REBSYM *sym, // will not return SYM_0, but might be SYM___ANONYMOUS__
    const REBVAL *value
) {
    REBVAL adjusted = *value;

    if (ANY_WORD(value)) {
        *sym = VAL_WORD_SYM(value);
        VAL_SET_TYPE_BITS(&adjusted, REB_GET_WORD);
    }
    else if (ANY_PATH(value)) {
        //
        // In theory we could get a symbol here, assuming we only do non
        // evaluated GETs.  Not implemented at the moment.
        //
        *sym = SYM___ANONYMOUS__;
        VAL_SET_TYPE_BITS(&adjusted, REB_GET_PATH);
    }
    else {
        *sym = SYM___ANONYMOUS__;
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

    REBSYM sym; // may be anonymous

    // We don't limit to taking a FUNCTION! value directly, because that loses
    // the symbol (for debugging, errors, etc.)  If caller passes a WORD!
    // then we lookup the variable to get the function, but save the symbol.
    //
    Get_If_Word_Or_Path_Arg(D_OUT, &sym, ARG(value));

    if (!IS_FUNCTION(D_OUT))
        fail (Error(RE_APPLY_NON_FUNCTION, ARG(value))); // for APPLY too

    if (Specialize_Function_Throws(D_OUT, D_OUT, sym, ARG(def)))
        return R_OUT_IS_THROWN;

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
#if !defined(NDEBUG)
    f->label_sym = SYM_0; // debug build checks label was SYM_0 before SET
#endif

    f->eval_type = ET_FUNCTION;
    SET_FRAME_SYM(f, sym);

    // We pretend our "input source" has ended.
    //
    f->value = END_CELL;
    f->indexor = END_FLAG;
    f->source.array = EMPTY_ARRAY;
    f->eval_fetched = NULL;

    f->dsp_orig = DSP;

    f->flags =
        DO_FLAG_NEXT
        | DO_FLAG_NO_LOOKAHEAD
        | DO_FLAG_NO_ARGS_EVALUATE
        | DO_FLAG_APPLYING;

    assert(NOT(f->flags & DO_FLAG_HAS_VARLIST));

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
        // f->func should already be set
        f->flags |= DO_FLAG_HAS_VARLIST;

        // !!! This form of execution raises a ton of open questions about
        // what to do if a frame is used more than once.  Function calls
        // are allowed to destroy their arguments and will contaminate the
        // pure locals.  We need to treat this as a "non-specializing
        // specialization", and push a frame.  The narrow case of frame
        // reuse needs to be contained to something that a function can only
        // do to itself--e.g. to facilitate tail recursion, because no caller
        // but the function itself understands the state of its locals in situ.
        //
        ASSERT_CONTEXT(AS_CONTEXT(f->data.varlist));
    }

    f->arg = FRM_ARGS_HEAD(f);
    f->refine = TRUE_VALUE;
    f->cell.subfeed = NULL;

    if (opt_def) {
        //
        // !!! Prior to specific binding, it's necessary to signal to the
        // Is_Function_Frame_Fulfilling() that this frame is *not* fulfilling
        // by setting f->param to END_CELL.  That way it will be considered
        // a valid target for the stack walk to do the binding.
        //
        f->param = END_CELL;

        if (f->flags & DO_FLAG_HAS_VARLIST) {
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

            f->arg = &f->data.stackvars[0];
        }

        // Do the block into scratch space--we ignore the result (unless it is
        // thrown, in which case it must be returned.)
        //
        if (DO_VAL_ARRAY_AT_THROWS(f->out, opt_def)) {
            DROP_CALL(f);
            return R_OUT_IS_THROWN;
        }
    }

    f->param = FUNC_PARAMS_HEAD(f->func);

    Do_Core(f);

    if (f->indexor == THROWN_FLAG)
        return R_OUT_IS_THROWN;

    assert(f->indexor == END_FLAG); // we started at END_FLAG, can only throw

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

    if (!IS_FUNCTION(D_OUT))
        fail (Error(RE_APPLY_NON_FUNCTION, ARG(value))); // for SPECIALIZE too

    f->param = D_OUT;
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
    assert(GET_ARR_FLAG(v->payload.function.spec, SERIES_FLAG_ARRAY));

    switch (VAL_FUNC_CLASS(v)) {
    case FUNC_CLASS_NATIVE:
        //
        // Only the definitional returns are allowed to lie on a per-value
        // basis and put a differing field in besides the canon FUNC_CODE
        // which lives in the [0] cell of the paramlist.
        //
        if (func != NAT_FUNC(return) && func != NAT_FUNC(leave)) {
            assert(
                v->payload.function.impl.code == FUNC_CODE(func)
            );
        }
        else {
            // !!! There's ROOT_RETURN_NATIVE and also the native in the
            // system context which have the real code in them.  If those
            // are accounted for then it might be possible to assert that
            // any returns we see are definitional...but until then we
            // don't know if it has a valid code field or not.
            //
            /*assert(
                GET_ARR_FLAG(v->payload.function.impl.body, SERIES_FLAG_ARRAY)
            );*/
        }
        break;

    case FUNC_CLASS_ACTION:
        assert(
            v->payload.function.impl.act == FUNC_ACT(func)
        );
        break;

    case FUNC_CLASS_COMMAND:
    case FUNC_CLASS_USER:
        assert(
            v->payload.function.impl.body == FUNC_BODY(func)
        );
        break;

    case FUNC_CLASS_CALLBACK:
    case FUNC_CLASS_ROUTINE:
        assert(
            v->payload.function.impl.info == FUNC_INFO(func)
        );
        break;

    case FUNC_CLASS_SPECIALIZED:
        assert(
            v->payload.function.impl.special
            == FUNC_VALUE(func)->payload.function.impl.special
        );
        break;

    default:
        assert(FALSE);
        break;
    }

    // set VALUE_FLAG_LINE on both headers for sake of comparison, we allow
    // it to be different from the value stored in frame.
    //
    // !!! Should formatting flags be moved into their own section, perhaps
    // the section currently known as "resv: reserved for future use"?
    //
    // We also set VALUE_FLAG_THROWN as that is not required to be sync'd
    // with the persistent value in the function.  This bit is deprecated
    // however, for many of the same reasons it's a nuisance here.  The
    // VALUE_FLAG_EXIT_FROM needs to be handled in the same way.
    //
    v_header.bits |= (
        VALUE_FLAG_EXIT_FROM
        | VALUE_FLAG_LINE
        | VALUE_FLAG_THROWN
        | VALUE_FLAG_EVALUATED
    );
    func_header.bits |= (
        VALUE_FLAG_EXIT_FROM
        | VALUE_FLAG_LINE
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
        Debug_Array(v->payload.function.spec);
        Panic_Array(FUNC_PARAMLIST(func));
    }

    return func;
}

#endif
