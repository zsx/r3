//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
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
//  Summary: Word Binding Routines
//  File: %c-bind.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Binding relates a word to a context.  Every word can be either bound,
// specifically bound to a particular context, or bound relatively to a
// function (where additional information is needed in order to find the
// specific instance of the variable for that word as a key).
//

#include "sys-core.h"


//
//  Get_Var_Core: C
//
// Get the word--variable--value. (Generally, use the macros like
// GET_VAR or GET_MUTABLE_VAR instead of this).  This routine is
// called quite a lot and so attention to performance is important.
//
// If `trap` is TRUE, return NULL instead of raising errors on unbounds.
//
// Coded assuming most common case is trap=FALSE and writable=FALSE
//
REBVAL *Get_Var_Core(const REBVAL *any_word, REBOOL trap, REBOOL writable)
{
    if (GET_VAL_FLAG(any_word, VALUE_FLAG_RELATIVE)) {
        //
        // RELATIVE CONTEXT: Word is stack-relative bound to a function with
        // no persistent varlist held by the GC.  The value *might* be found
        // on the stack (or not, if all instances of the function on the
        // call stack have finished executing).  We walk backward in the call
        // stack to see if we can find the function's "identifying series"
        // in a call frame...and take the first instance we see (even if
        // multiple invocations are on the stack, most recent wins)
        //
        // !!! This is the temporary answer to relative binding.  NewFunction
        // aims to resolve relative bindings with the help of an extra
        // parameter to Get_Var, that will be "tunneled" through ANY-SERIES!
        // REBVALs that are "viewing" an array that contains relatively
        // bound elements.  That extra parameter will fill in the *actual*
        // frame so this code will not have to guess that "the last stack
        // level is close enough"

        REBCNT index = VAL_WORD_INDEX(any_word);
        REBVAL *value;

        struct Reb_Frame *frame
            = Frame_For_Relative_Word(any_word, trap);

        assert(GET_VAL_FLAG(any_word, WORD_FLAG_BOUND)); // should be set too

        if (!frame) {
            assert(trap);
            return NULL;
        }

        if (
            writable &&
            GET_VAL_FLAG(
                FUNC_PARAM(FRM_FUNC(frame), index),
                TYPESET_FLAG_LOCKED
            )
        ) {
            if (trap) return NULL;

            fail (Error(RE_LOCKED_WORD, any_word));
        }

        value = FRM_ARG(frame, index);
        assert(!THROWN(value));
        return value;
    }
    else if (GET_VAL_FLAG(any_word, WORD_FLAG_BOUND)) {
        //
        // The word is bound directly to a value inside a varlist, and
        // represents the zero-based offset into that series.  This is how
        // values would be picked out of object-like things...
        //
        // (Including e.g. looking up 'append' in the user context.)

        REBCTX *context = VAL_WORD_CONTEXT(any_word);
        REBCNT index = VAL_WORD_INDEX(any_word);
        REBVAL *value;

        assert(
            SAME_SYM(
                VAL_WORD_SYM(any_word), CTX_KEY_SYM(context, index)
            )
        );

        if (
            GET_CTX_FLAG(context, CONTEXT_FLAG_STACK)
            && !GET_CTX_FLAG(context, SERIES_FLAG_ACCESSIBLE)
        ) {
            // In R3-Alpha, the closure construct created a persistent object
            // which would keep all of its args, refinements, and locals
            // alive after the closure ended.  In trying to eliminate the
            // distinction between FUNCTION! and CLOSURE! in Ren-C, the
            // default is for them not to survive...though a mechanism for
            // allowing some to be marked ("<durable>") is under development.
            //
            // In the meantime, report the same error as a function which
            // is no longer on the stack.

            if (trap) return NULL;

            fail (Error(RE_NO_RELATIVE, any_word));
        }

        if (
            writable &&
            GET_VAL_FLAG(CTX_KEY(context, index), TYPESET_FLAG_LOCKED)
        ) {
            if (trap) return NULL;

            fail (Error(RE_LOCKED_WORD, any_word));
        }

        value = CTX_VAR(context, index);
        assert(!THROWN(value));
        return value;
    }

    // If none of the above cases matched, then it's not bound at all.

    if (trap) return NULL;

    fail (Error(RE_NOT_BOUND, any_word));
}


//
//  Bind_Values_Inner_Loop: C
//
// Bind_Values_Core() sets up the binding table and then calls
// this recursive routine to do the actual binding.
//
static void Bind_Values_Inner_Loop(
    REBINT *binds,
    REBVAL *head,
    REBCTX *context,
    REBU64 bind_types, // !!! REVIEW: force word types low enough for 32-bit?
    REBU64 add_midstream_types,
    REBFLGS flags
) {
    REBVAL *value = head;
    for (; NOT_END(value); value++) {
        REBU64 type_bit = FLAGIT_KIND(VAL_TYPE(value));

        if (type_bit & bind_types) {
            REBCNT n = binds[VAL_WORD_CANON(value)];
            if (n != 0) {
                //
                // Word is in context, bind it.  Note that VAL_RESET_HEADER
                // is a macro and VAL_TYPE is a macro, so we cannot directly
                // initialize the header while also needing the type.
                //
                assert(ANY_WORD(value));
                assert(n <= CTX_LEN(context));

                // We're overwriting any previous binding, which may have
                // been relative.
                //
                CLEAR_VAL_FLAG(value, VALUE_FLAG_RELATIVE);

                SET_VAL_FLAG(value, WORD_FLAG_BOUND);
                INIT_WORD_CONTEXT(value, context);
                INIT_WORD_INDEX(value, n);
            }
            else if (type_bit & add_midstream_types) {
                //
                // Word is not in context, so add it if option is specified
                //
                Expand_Context(context, 1);
                Append_Context(context, value, 0);
                binds[VAL_WORD_CANON(value)] = VAL_WORD_INDEX(value);
            }
        }
        else if (ANY_ARRAY(value) && (flags & BIND_DEEP)) {
            Bind_Values_Inner_Loop(
                binds,
                VAL_ARRAY_AT(value),
                context,
                bind_types,
                add_midstream_types,
                flags
            );
        }
        else if (
            IS_FUNCTION_AND(value, FUNC_CLASS_USER)
            && (flags & BIND_FUNC)
        ) {
            // !!! Likely-to-be deprecated functionality--rebinding inside the
            // content of an already formed function.  :-/
            //
            Bind_Values_Inner_Loop(
                binds,
                ARR_HEAD(VAL_FUNC_BODY(value)),
                context,
                bind_types,
                add_midstream_types,
                flags
            );
        }
    }
}


//
//  Bind_Values_Core: C
//
// Bind words in an array of values terminated with END
// to a specified context.  See warnings on the functions like
// Bind_Values_Deep() about not passing just a singular REBVAL.
//
// NOTE: If types are added, then they will be added in "midstream".  Only
// bindings that come after the added value is seen will be bound.
//
void Bind_Values_Core(
    REBVAL *head,
    REBCTX *context,
    REBU64 bind_types,
    REBU64 add_midstream_types,
    REBFLGS flags // see %sys-core.h for BIND_DEEP, etc.
) {
    REBVAL *key;
    REBCNT index;
    REBINT *binds = WORDS_HEAD(Bind_Table); // GC safe to do here

    ASSERT_BIND_TABLE_EMPTY;

    // Note about optimization: it's not a big win to avoid the
    // binding table for short blocks (size < 4), because testing
    // every block for the rare case adds up.

    // Setup binding table
    index = 1;
    key = CTX_KEYS_HEAD(context);
    for (; index <= CTX_LEN(context); key++, index++) {
        if (!GET_VAL_FLAG(key, TYPESET_FLAG_UNBINDABLE))
            binds[VAL_TYPESET_CANON(key)] = index;
    }

    Bind_Values_Inner_Loop(
        binds, head, context, bind_types, add_midstream_types, flags
    );

    // Reset binding table:
    key = CTX_KEYS_HEAD(context);
    for (; NOT_END(key); key++)
        binds[VAL_TYPESET_CANON(key)] = 0;

    ASSERT_BIND_TABLE_EMPTY;
}


//
//  Unbind_Values_Core: C
//
// Unbind words in a block, optionally unbinding those which are
// bound to a particular target (if target is NULL, then all
// words will be unbound regardless of their VAL_WORD_CONTEXT).
//
void Unbind_Values_Core(REBVAL *head, REBCTX *context, REBOOL deep)
{
    REBVAL *value = head;
    for (; NOT_END(value); value++) {
        if (
            ANY_WORD(value)
            && (
                !context
                || (
                    IS_WORD_BOUND(value)
                    && VAL_WORD_CONTEXT(value) == context
                )
            )
        ) {
            UNBIND_WORD(value);
        }
        else if (ANY_ARRAY(value) && deep)
            Unbind_Values_Core(VAL_ARRAY_AT(value), context, TRUE);
    }
}


//
//  Try_Bind_Word: C
//
// Binds a word to a context. If word is not part of the context.
//
REBCNT Try_Bind_Word(REBCTX *context, REBVAL *word)
{
    REBCNT n;

    n = Find_Word_In_Context(context, VAL_WORD_SYM(word), FALSE);
    if (n != 0) {
        //
        // Previously may have been bound relative, remove flag.
        //
        CLEAR_VAL_FLAG(word, VALUE_FLAG_RELATIVE);

        SET_VAL_FLAG(word, WORD_FLAG_BOUND);
        INIT_WORD_CONTEXT(word, context);
        INIT_WORD_INDEX(word, n);
    }
    return n;
}


//
//  Bind_Relative_Inner_Loop: C
//
// Recursive function for relative function word binding.
//
static void Bind_Relative_Inner_Loop(
    REBINT *binds,
    REBFUN *func,
    REBVAL *head,
    REBU64 bind_types
) {
    REBVAL *value = head;
    for (; NOT_END(value); value++) {
        REBU64 type_bit = FLAGIT_KIND(VAL_TYPE(value));

        if (type_bit & bind_types) {
            REBINT n;
            assert(ANY_WORD(value));
            if ((n = binds[VAL_WORD_CANON(value)]) != 0) {
                //
                // Word's canon symbol is in frame.  Relatively bind it.
                // (clear out existing header flags first).  Note that
                // VAL_RESET_HEADER is a macro and it's not safe to pass
                // it VAL_TYPE(value) directly while initializing value...
                //
                enum Reb_Kind kind = VAL_TYPE(value);
                VAL_RESET_HEADER(value, kind);
                SET_VAL_FLAGS(value, WORD_FLAG_BOUND | VALUE_FLAG_RELATIVE);
                INIT_WORD_FUNC(value, func);
                INIT_WORD_INDEX(value, n);
            }
        }
        else if (ANY_ARRAY(value))
            Bind_Relative_Inner_Loop(
                binds, func, VAL_ARRAY_AT(value), bind_types
            );
    }
}


//
//  Bind_Relative_Deep: C
//
// Bind the words of a function block to a stack frame.
// To indicate the relative nature of the index, it is set to
// a negative offset.
//
void Bind_Relative_Deep(REBFUN *func, REBVAL *head, REBU64 bind_types)
{
    REBVAL *param;
    REBCNT index;
    REBINT *binds = WORDS_HEAD(Bind_Table); // GC safe to do here

    // !!! Historically, relative binding was not allowed for NATIVE! or
    // other function types.  It was not desirable for user code to be
    // capable of binding to the parameters of a native.  However, for
    // purposes of debug inspection, read-only access presents an
    // interesting case.  While this avenue is explored, relative bindings
    // for all function types are being permitted.
    //
    /*assert(
        IS_FUNCTION(FUNC_VALUE(func))
        && VAL_FUNC_CLASS(FUNC_VALUE(func)) == FUNC_CLASS_USER
    );*/

    ASSERT_BIND_TABLE_EMPTY;

    //Dump_Block(words);

    // Setup binding table from the argument word list
    //
    index = 1;
    param = FUNC_PARAMS_HEAD(func);
    for (; NOT_END(param); param++, index++)
        binds[VAL_TYPESET_CANON(param)] = index;

    Bind_Relative_Inner_Loop(binds, func, head, bind_types);

    // Reset binding table
    //
    param = FUNC_PARAMS_HEAD(func);
    for (; NOT_END(param); param++)
        binds[VAL_TYPESET_CANON(param)] = 0;

    ASSERT_BIND_TABLE_EMPTY;
}


//
//  Bind_Stack_Word: C
//
void Bind_Stack_Word(REBFUN *func, REBVAL *word)
{
    REBINT index;
    enum Reb_Kind kind;

    index = Find_Param_Index(FUNC_PARAMLIST(func), VAL_WORD_SYM(word));
    if (index == 0)
        fail (Error(RE_NOT_IN_CONTEXT, word));

    kind = VAL_TYPE(word); // safe--can't pass VAL_TYPE(value) while resetting
    VAL_RESET_HEADER(word, kind);
    SET_VAL_FLAGS(word, WORD_FLAG_BOUND | VALUE_FLAG_RELATIVE);
    INIT_WORD_FUNC(word, func);
    INIT_WORD_INDEX(word, index);
}


//
//  Rebind_Values_Deep: C
//
// Rebind all words that reference src target to dst target.
// Rebind is always deep.
//
void Rebind_Values_Deep(
    REBCTX *src,
    REBCTX *dst,
    REBVAL *head,
    REBINT *opt_binds
) {
    REBVAL *value = head;
    for (; NOT_END(value); value++) {
        if (ANY_ARRAY(value)) {
            Rebind_Values_Deep(src, dst, VAL_ARRAY_AT(value), opt_binds);
        }
        else if (
            ANY_WORD(value)
            && GET_VAL_FLAG(value, WORD_FLAG_BOUND)
            && !GET_VAL_FLAG(value, VALUE_FLAG_RELATIVE)
            && VAL_WORD_CONTEXT(value) == src
        ) {
            INIT_WORD_CONTEXT(value, dst);

            if (opt_binds) {
                REBCNT canon = VAL_WORD_CANON(value);
                INIT_WORD_INDEX(value, opt_binds[canon]);
            }
        }
        else if (
            IS_FUNCTION(value)
            && VAL_FUNC_CLASS(value) == FUNC_CLASS_USER
        ) {
            //
            // !!! Extremely questionable feature--walking into function
            // bodies and changing them.  This R3-Alpha concept was largely
            // broken (didn't work for closures) and created a lot of extra
            // garbage (inheriting an object's methods meant making deep
            // copies of all that object's method bodies...each time).
            // Ren-C has a different idea in the works.
            //
            Rebind_Values_Deep(
                src, dst, ARR_HEAD(VAL_FUNC_BODY(value)), opt_binds
            );
        }
    }
}


//
//  Rebind_Values_Relative_Deep: C
//
// Rebind all words that reference src target to dst target.
// Rebind is always deep.
//
// !!! This function is temporary and should not be necessary after the FRAME!
// is implemented.
//
void Rebind_Values_Relative_Deep(
    REBFUN *src,
    REBFUN *dst,
    REBVAL *head
) {
    REBVAL *value = head;
    for (; NOT_END(value); value++) {
        if (ANY_ARRAY(value)) {
            Rebind_Values_Relative_Deep(src, dst, VAL_ARRAY_AT(value));
        }
        else if (
            ANY_WORD(value)
            && GET_VAL_FLAG(value, VALUE_FLAG_RELATIVE)
            && VAL_WORD_FUNC(value) == src
        ) {
            INIT_WORD_FUNC(value, dst);
        }
    }
}


//
//  Rebind_Values_Specifically_Deep: C
//
// Rebind all words that reference src target to dst target.
// Rebind is always deep.
//
// !!! This function is temporary and should not be necessary after the FRAME!
// is implemented.
//
void Rebind_Values_Specifically_Deep(REBFUN *src, REBCTX *dst, REBVAL *head) {
    REBVAL *value = head;
    for (; NOT_END(value); value++) {
        if (ANY_ARRAY(value)) {
            Rebind_Values_Specifically_Deep(src, dst, VAL_ARRAY_AT(value));
        }
        else if (
            ANY_WORD(value)
            && GET_VAL_FLAG(value, VALUE_FLAG_RELATIVE)
            && VAL_WORD_FUNC(value) == src
        ) {
            // Note that VAL_RESET_HEADER(value...) is a macro for setting
            // value, so passing VAL_TYPE(value) which is also a macro can be
            // dangerous...
            //
            assert(GET_VAL_FLAG(value, WORD_FLAG_BOUND)); // should be set
            CLEAR_VAL_FLAG(value, VALUE_FLAG_RELATIVE);
            INIT_WORD_CONTEXT(value, dst);
        }
    }
}


#if !defined(NDEBUG)

//
//  Assert_Bind_Table_Empty: C
//
void Assert_Bind_Table_Empty(void)
{
    REBCNT n;
    REBINT *binds = WORDS_HEAD(Bind_Table);

    //Debug_Fmt("Bind Table (Size: %d)", SER_LEN(Bind_Table));
    for (n = 0; n < SER_LEN(Bind_Table); n++) {
        if (binds[n]) {
            Debug_Fmt(
                "Bind table fault: %3d to %3d (%s)",
                n,
                binds[n],
                Get_Sym_Name(n)
            );
        }
    }
}

#endif
