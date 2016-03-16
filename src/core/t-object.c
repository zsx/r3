//
//  File: %t-object.c
//  Summary: "object datatype"
//  Section: datatypes
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

#include "sys-core.h"



static REBOOL Equal_Context(const REBVAL *val, const REBVAL *arg)
{
    REBCTX *f1;
    REBCTX *f2;
    REBVAL *key1;
    REBVAL *key2;
    REBVAL *var1;
    REBVAL *var2;

    // ERROR! and OBJECT! may both be contexts, for instance, but they will
    // not compare equal just because their keys and fields are equal
    //
    if (VAL_TYPE(arg) != VAL_TYPE(val)) return FALSE;

    f1 = VAL_CONTEXT(val);
    f2 = VAL_CONTEXT(arg);

    // Short circuit equality: `same?` objects always equal
    //
    if (f1 == f2) return TRUE;

    // We can't short circuit on unequal frame lengths alone, because hidden
    // fields of objects (notably `self`) do not figure into the `equal?`
    // of their public portions.

    key1 = CTX_KEYS_HEAD(f1);
    key2 = CTX_KEYS_HEAD(f2);
    var1 = CTX_VARS_HEAD(f1);
    var2 = CTX_VARS_HEAD(f2);

    // Compare each entry, in order.  This order dependence suggests that
    // an object made with `make object! [[a b][a: 1 b: 2]]` will not be equal
    // to `make object! [[b a][b: 1 a: 2]]`.  Although Rebol does not allow
    // positional picking out of objects, it does allow positional setting
    // currently (which it likely should not), hence they are functionally
    // distinct for now.  Yet those two should probably be `equal?`.
    //
    for (; NOT_END(key1) && NOT_END(key2); key1++, key2++, var1++, var2++) {
    no_advance:
        //
        // Hidden vars shouldn't affect the comparison.
        //
        if (GET_VAL_FLAG(key1, TYPESET_FLAG_HIDDEN)) {
            key1++; var1++;
            if (IS_END(key1)) break;
            goto no_advance;
        }
        if (GET_VAL_FLAG(key2, TYPESET_FLAG_HIDDEN)) {
            key2++; var2++;
            if (IS_END(key2)) break;
            goto no_advance;
        }

        // Do ordinary comparison of the typesets
        //
        if (Cmp_Value(key1, key2, FALSE) != 0)
            return FALSE;

        // The typesets contain a symbol as well which must match for
        // objects to consider themselves to be equal (but which do not
        // count in comparison of the typesets)
        //
        if (VAL_TYPESET_CANON(key1) != VAL_TYPESET_CANON(key2))
            return FALSE;

        // !!! A comment here said "Use Compare_Modify_Values();"...but it
        // doesn't... it calls Cmp_Value (?)
        //
        if (Cmp_Value(var1, var2, FALSE) != 0)
            return FALSE;
    }

    // Either key1 or key2 is at the end here, but the other might contain
    // all hidden values.  Which is okay.  But if a value isn't hidden,
    // they don't line up.
    //
    for (; NOT_END(key1); key1++, var1++) {
        if (!GET_VAL_FLAG(key1, TYPESET_FLAG_HIDDEN))
            return FALSE;
    }
    for (; NOT_END(key2); key2++, var2++) {
        if (!GET_VAL_FLAG(key2, TYPESET_FLAG_HIDDEN))
            return FALSE;
    }

    return TRUE;
}


static void Append_To_Context(REBCTX *context, REBVAL *arg)
{
    REBCNT i, len;
    REBVAL *word;
    REBVAL *key;
    REBINT *binds; // for binding table

    // Can be a word:
    if (ANY_WORD(arg)) {
        if (!Find_Word_In_Context(context, VAL_WORD_SYM(arg), TRUE)) {
            Expand_Context(context, 1); // copy word table also
            Append_Context(context, 0, VAL_WORD_SYM(arg));
            // default of Append_Context is that arg's value is void
        }
        return;
    }

    if (!IS_BLOCK(arg)) fail (Error_Invalid_Arg(arg));

    // Process word/value argument block:
    arg = VAL_ARRAY_AT(arg);

    // Use binding table
    binds = WORDS_HEAD(Bind_Table);

    Collect_Keys_Start(COLLECT_ANY_WORD);

    // Setup binding table with obj words.  Binding table is empty so don't
    // bother checking for duplicates.
    //
    Collect_Context_Keys(context, FALSE);

    // Examine word/value argument block
    for (word = arg; NOT_END(word); word += 2) {
        REBCNT canon;

        if (!IS_WORD(word) && !IS_SET_WORD(word))
            fail (Error_Invalid_Arg(word));

        canon = VAL_WORD_CANON(word);

        if (binds[canon] == 0) {
            //
            // Not already collected, so add it...
            //
            binds[canon] = ARR_LEN(BUF_COLLECT);
            EXPAND_SERIES_TAIL(ARR_SERIES(BUF_COLLECT), 1);
            Val_Init_Typeset(
                ARR_LAST(BUF_COLLECT), ALL_64, VAL_WORD_SYM(word)
            );
        }
        if (IS_END(word + 1)) break; // fix bug#708
    }

    TERM_ARRAY(BUF_COLLECT);

    // Append new words to obj
    //
    len = CTX_LEN(context) + 1;
    Expand_Context(context, ARR_LEN(BUF_COLLECT) - len);
    for (key = ARR_AT(BUF_COLLECT, len); NOT_END(key); key++) {
        assert(IS_TYPESET(key));
        Append_Context_Core(
            context,
            NULL,
            VAL_TYPESET_SYM(key),
            GET_VAL_FLAG(key, TYPESET_FLAG_LOOKBACK) // !!! needed?  others?
        );
    }

    // Set new values to obj words
    for (word = arg; NOT_END(word); word += 2) {
        REBVAL *key;
        REBVAL *var;

        i = binds[VAL_WORD_CANON(word)];
        var = CTX_VAR(context, i);
        key = CTX_KEY(context, i);

        if (GET_VAL_FLAG(key, TYPESET_FLAG_LOCKED))
            fail (Error_Protected_Key(key));

        if (GET_VAL_FLAG(key, TYPESET_FLAG_HIDDEN))
            fail (Error(RE_HIDDEN));

        if (IS_END(word + 1)) SET_BLANK(var);
        else *var = word[1];

        if (IS_END(word + 1)) break; // fix bug#708
    }

    // release binding table
    Collect_Keys_End();
}


static REBCTX *Trim_Context(REBCTX *context)
{
    REBVAL *var;
    REBCNT copy_count = 0;
    REBCTX *context_new;
    REBVAL *var_new;
    REBVAL *key;
    REBVAL *key_new;

    // First pass: determine size of new context to create by subtracting out
    // any void (unset fields), NONE!, or hidden fields
    //
    key = CTX_KEYS_HEAD(context);
    var = CTX_VARS_HEAD(context);
    for (; NOT_END(var); var++, key++) {
        if (VAL_TYPE(var) > REB_BLANK && !GET_VAL_FLAG(key, TYPESET_FLAG_HIDDEN))
            copy_count++;
    }

    // Create new context based on the size found
    //
    context_new = Alloc_Context(copy_count);

    // Second pass: copy the values that were not skipped in the first pass
    //
    key = CTX_KEYS_HEAD(context);
    var = CTX_VARS_HEAD(context);
    var_new = CTX_VARS_HEAD(context_new);
    key_new = CTX_KEYS_HEAD(context_new);
    for (; NOT_END(var); var++, key++) {
        if (VAL_TYPE(var) > REB_BLANK && !GET_VAL_FLAG(key, TYPESET_FLAG_HIDDEN)) {
            *var_new++ = *var;
            *key_new++ = *key;
        }
    }

    // Terminate the new context
    //
    SET_END(var_new);
    SET_END(key_new);
    SET_ARRAY_LEN(CTX_VARLIST(context_new), copy_count + 1);
    SET_ARRAY_LEN(CTX_KEYLIST(context_new), copy_count + 1);

    return context_new;
}


//
//  CT_Context: C
//
REBINT CT_Context(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    if (mode < 0) return -1;
    return Equal_Context(a, b) ? 1 : 0;
}


//
//  MT_Context: C
//
REBOOL MT_Context(REBVAL *out, REBVAL *data, enum Reb_Kind type)
{
    REBCTX *context;
    if (!IS_BLOCK(data)) return FALSE;

    context = Construct_Context(
        type, VAL_ARRAY_AT(data), VAL_SPECIFIER(data), NULL
    );

    Val_Init_Context(out, type, context);

    if (type == REB_ERROR) {
        REBVAL result;
        if (Make_Error_Object_Throws(&result, out)) {
            *out = result;
            return FALSE;
        }
        assert(IS_ERROR(&result));
        *out = result;
    }
    return TRUE;
}


//
//  PD_Context: C
//
REBINT PD_Context(REBPVS *pvs)
{
    REBCNT n;
    REBCTX *context = VAL_CONTEXT(pvs->value);

    if (IS_WORD(pvs->selector)) {
        n = Find_Word_In_Context(context, VAL_WORD_SYM(pvs->selector), FALSE);
    }
    else fail (Error_Bad_Path_Select(pvs));

    if (n == 0) {
        //
        // !!! The logic for allowing a GET-PATH! to be void if it's the last
        // lookup that fails here is hacked in, but desirable for parity
        // with the behavior of GET-WORD!
        //
        if (IS_GET_PATH(pvs->orig) && IS_END(pvs->item + 1)) {
            SET_VOID(pvs->store);
            return PE_USE_STORE;
        }
        fail (Error_Bad_Path_Select(pvs));
    }

    if (
        pvs->opt_setval
        && IS_END(pvs->item + 1)
        && GET_VAL_FLAG(CTX_KEY(context, n), TYPESET_FLAG_LOCKED)
    ) {
        fail (Error(RE_LOCKED_WORD, pvs->selector));
    }

    pvs->value = CTX_VAR(context, n);
    return PE_SET_IF_END;
}


//
//  meta-of: native [
//
//  {Get a reference to the "meta" object associated with a value.}
//
//      value [function! object! module!]
//  ]
//
REBNATIVE(meta_of)
//
// The first implementation of linking a "meta object" to another object
// originates from the original module system--where it was called the
// "module spec".  By moving it out of object REBVALs to the misc field of
// a keylist, it becomes possible to change the meta object and have that
// change seen by all references.
//
// As modules are still the first client of this meta information, it works
// a similar way.  It is mutable by all references by default, unless
// it is protected.
//
// !!! This feature is under development and expected to extend to functions
// and possibly other types of values--both as the meta information, and
// as being able to have the meta information.
{
    PARAM(1, value);

    REBVAL *value = ARG(value);

    REBCTX *meta;
    if (IS_FUNCTION(value))
        meta = VAL_FUNC_META(value);
    else {
        assert(ANY_CONTEXT(value));
        meta = VAL_CONTEXT_META(value);
    }

    if (!meta) return R_BLANK;

    Val_Init_Object(D_OUT, meta);
    return R_OUT;
}


//
//  set-meta: native [
//
//  {Set "meta" object associated with all references to a value.}
//
//      value [function! object! module!]
//      meta [object! blank!]
//  ]
//
REBNATIVE(set_meta)
//
// !!! You cannot currently put meta information onto a FRAME!, because the
// slot where the meta information would go is where the meta information
// would live for the function--since frames use a functions "paramlist"
// as their keylist.  Types taken are deliberately narrow for the moment.
{
    PARAM(1, value);
    PARAM(2, meta);

    REBCTX *meta;
    if (ANY_CONTEXT(ARG(meta))) {
        //
        // !!! Putting a context value that has an `exit_from` into a single
        // REBCTX* will only have the canon context value, which has no
        // per-instance REBVAL bits.  Consider systemic checks for this.
        //
        assert(VAL_CONTEXT_EXIT_FROM(ARG(meta)) == NULL);
        meta = VAL_CONTEXT(ARG(meta));
    }
    else {
        assert(IS_BLANK(ARG(meta)));
        meta = NULL;
    }

    REBVAL *value = ARG(value);

    if (IS_FUNCTION(value))
        VAL_FUNC_META(value) = meta;
    else {
        assert(ANY_CONTEXT(value));
        INIT_CONTEXT_META(VAL_CONTEXT(value), meta);
    }

    return R_VOID;
}


//
//  REBTYPE: C
// 
// Handles object!, module!, and error! datatypes.
//
REBTYPE(Context)
{
    REBVAL *value = D_ARG(1);
    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;
    REBCTX *context;

    enum Reb_Kind target;

    switch (action) {

    case A_MAKE:
        //
        // `make object! | error! | module!`; first parameter must be either
        // a datatype or a type exemplar.
        //
        assert(IS_DATATYPE(value));
        target = VAL_TYPE_KIND(value);

        if (target == REB_FRAME) {
            REBARR *varlist;
            REBCNT n;
            REBVAL *var;

            // !!! Current experiment for making frames lets you give it
            // a FUNCTION! only.
            //
            if (!IS_FUNCTION(arg))
                fail (Error_Bad_Make(target, arg));

            // In order to have the frame survive the call to MAKE and be
            // returned to the user it can't be stack allocated, because it
            // would immediately become useless.  Allocate dynamically.
            //
            Val_Init_Context(
                D_OUT,
                REB_FRAME,
                Make_Frame_For_Function(arg) // all void vars default
            );

            return R_OUT;
        }

        if ((target == REB_OBJECT || target == REB_MODULE) && IS_BLANK(arg)) {
            //
            // Special case (necessary?) to return an empty object.
            //
            Val_Init_Object(
                D_OUT,
                Construct_Context(REB_OBJECT, END_CELL, SPECIFIED, NULL)
            );
            return R_OUT;
        }

        if ((target == REB_OBJECT || target == REB_MODULE) && IS_BLOCK(arg)) {
            //
            // Simple object creation with no evaluation, so all values are
            // handled "as-is".  Should have a spec block and a body block.
            //
            // Note: In %r3-legacy.r, the old evaluative MAKE OBJECT! is
            // done by redefining MAKE itself, and calling the CONSTRUCT
            // generator if the make def is not the [[spec][body]] format.

            if (
                VAL_LEN_AT(arg) != 2
                || !IS_BLOCK(VAL_ARRAY_AT(arg)) // spec
                || !IS_BLOCK(VAL_ARRAY_AT(arg) + 1) // body
            ) {
                fail (Error_Bad_Make(target, arg));
            }

            // !!! Spec block is currently ignored, but required.

            Val_Init_Object(
                D_OUT,
                Construct_Context(
                    REB_OBJECT,
                    VAL_ARRAY_AT(VAL_ARRAY_AT(arg) + 1),
                    VAL_SPECIFIER(VAL_ARRAY_AT(arg) + 1),
                    NULL // no parent
                )
            );

            return R_OUT;
        }

        // make error! [....]
        //
        // arg is block/string, but let Make_Error_Object_Throws do the
        // type checking.
        //
        if (target == REB_ERROR) {
            if (Make_Error_Object_Throws(D_OUT, arg))
                return R_OUT_IS_THROWN;
            return R_OUT;
        }

        // `make object! 10` - currently not prohibited for any context type
        //
        if (IS_NUMBER(arg)) {
            REBINT n = Int32s(arg, 0);

            // !!! Temporary!  Ultimately SELF will be a user protocol.
            // We use Make_Selfish_Context while MAKE is filling in for
            // what will be responsibility of the generators, just to
            // get "completely fake SELF" out of index slot [0]
            //
            context = Make_Selfish_Context_Detect(
                target, // type
                NULL, // spec
                NULL, // body
                END_CELL, // scan for toplevel set-words, empty
                NULL // parent
            );

            // !!! Allocation when SELF is not the responsibility of MAKE
            // will be more basic and look like this.
            //
            /* context = Alloc_Context(n);
            VAL_RESET_HEADER(CTX_VALUE(context), target);
            CTX_META(context) = NULL;
            CTX_BODY(context) = NULL; */
            Val_Init_Context(D_OUT, target, context);
            return R_OUT;
        }

        // make object! map!
        if (IS_MAP(arg)) {
            context = Alloc_Context_From_Map(VAL_MAP(arg));
            Val_Init_Context(D_OUT, target, context);
            return R_OUT;
        }
        fail (Error_Bad_Make(target, arg));

    case A_TO:
        assert(IS_DATATYPE(value));
        target = VAL_TYPE_KIND(value);

        // special conversions to object! | error! | module!
        if (target == REB_ERROR) {
            // arg is block/string, returns value
            if (Make_Error_Object_Throws(D_OUT, arg))
                return R_OUT_IS_THROWN;
            return R_OUT;
        }
        else if (target == REB_OBJECT) {
            if (IS_ERROR(arg)) {
                if (VAL_ERR_NUM(arg) < 100) fail (Error_Invalid_Arg(arg));
                context = VAL_CONTEXT(arg);
                break; // returns context
            }
            Val_Init_Object(D_OUT, VAL_CONTEXT(arg));
            return R_OUT;
        }
        fail (Error_Bad_Make(target, arg));

    case A_APPEND:
        FAIL_IF_LOCKED_CONTEXT(VAL_CONTEXT(value));
        if (!IS_OBJECT(value) && !IS_MODULE(value))
            fail (Error_Illegal_Action(VAL_TYPE(value), action));
        Append_To_Context(VAL_CONTEXT(value), arg);
        *D_OUT = *D_ARG(1);
        return R_OUT;

    case A_LENGTH:
        if (!IS_OBJECT(value))
            fail (Error_Illegal_Action(VAL_TYPE(value), action));
        SET_INTEGER(D_OUT, CTX_LEN(VAL_CONTEXT(value)));
        return R_OUT;

    case A_COPY:
        // Note: words are not copied and bindings not changed!
    {
        REBU64 types = 0;
        if (D_REF(ARG_COPY_PART)) fail (Error(RE_BAD_REFINES));
        if (D_REF(ARG_COPY_DEEP)) {
            types |= D_REF(ARG_COPY_TYPES) ? 0 : TS_STD_SERIES;
        }
        if (D_REF(ARG_COPY_TYPES)) {
            arg = D_ARG(ARG_COPY_KINDS);
            if (IS_DATATYPE(arg)) types |= FLAGIT_KIND(VAL_TYPE_KIND(arg));
            else types |= VAL_TYPESET_BITS(arg);
        }
        context = AS_CONTEXT(
            Copy_Array_Shallow(CTX_VARLIST(VAL_CONTEXT(value)), SPECIFIED)
        );
        INIT_CTX_KEYLIST_SHARED(context, CTX_KEYLIST(VAL_CONTEXT(value)));
        SET_ARR_FLAG(CTX_VARLIST(context), ARRAY_FLAG_CONTEXT_VARLIST);
        INIT_VAL_CONTEXT(CTX_VALUE(context), context);
        if (types != 0) {
            Clonify_Values_Len_Managed(
                CTX_VARS_HEAD(context),
                SPECIFIED,
                CTX_LEN(context),
                D_REF(ARG_COPY_DEEP),
                types
            );
        }
        Val_Init_Context(D_OUT, VAL_TYPE(value), context);
        return R_OUT;
    }

    case A_SELECT:
    case A_FIND: {
        REBINT n;

        if (!IS_WORD(arg))
            return R_BLANK;

        n = Find_Word_In_Context(VAL_CONTEXT(value), VAL_WORD_SYM(arg), FALSE);

        if (n <= 0)
            return R_BLANK;

        if (cast(REBCNT, n) > CTX_LEN(VAL_CONTEXT(value)))
            return R_BLANK;

        if (action == A_FIND) return R_TRUE;

        *D_OUT = *CTX_VAR(VAL_CONTEXT(value), n);
        return R_OUT;
    }

    case A_REFLECT: {
        REBSYM canon = VAL_WORD_CANON(arg);

        switch (canon) {
        case SYM_WORDS: action = 1; break;
        case SYM_VALUES: action = 2; break;
        case SYM_BODY: action = 3; break;
        default:
            fail (Error_Cannot_Reflect(VAL_TYPE(value), arg));
        }

        Val_Init_Block(D_OUT, Context_To_Array(VAL_CONTEXT(value), action));
        return R_OUT; }

    case A_TRIM:
        if (Find_Refines(frame_, ALL_TRIM_REFS)) {
            // no refinements are allowed
            fail (Error(RE_BAD_REFINES));
        }
        Val_Init_Context(
            D_OUT,
            VAL_TYPE(value),
            Trim_Context(VAL_CONTEXT(value))
        );
        return R_OUT;

    case A_TAIL_Q:
        if (IS_OBJECT(value)) {
            SET_LOGIC(D_OUT, CTX_LEN(VAL_CONTEXT(value)) == 0);
            return R_OUT;
        }
        fail (Error_Illegal_Action(VAL_TYPE(value), action));
    }

    fail (Error_Illegal_Action(VAL_TYPE(value), action));
}


//
//  construct: native [
//
//  "Creates an ANY-CONTEXT! instance"
//
//      spec [datatype! block! any-context!]
//          "Datatype to create, specification, or parent/prototype context"
//      body [block! any-context! blank!]
//          "keys and values defining instance contents (bindings modified)"
//      /only
//          "Values are kept as-is"
//  ]
//
REBNATIVE(construct)
//
// CONSTRUCT in Ren-C is an effective replacement for what MAKE ANY-OBJECT!
// was able to do in Rebol2 and R3-Alpha.  It takes a spec that can be an
// ANY-CONTEXT! datatype, or it can be a parent ANY-CONTEXT!, or a block that
// represents a "spec".
//
// !!! This assumes you want a SELF defined.  The entire concept of SELF
// needs heavy review, but at minimum this needs a <no-self> override to
// match the <no-return> for functions.
//
// !!! This mutates the bindings of the body block passed in, should it
// be making a copy instead (at least by default, perhaps with performance
// junkies saying `construct/rebind` or something like that?
{
    PARAM(1, spec);
    PARAM(2, body);
    REFINE(3, only);

    REBVAL *spec = ARG(spec);
    REBVAL *body = ARG(body);
    REBCTX *parent = NULL;

    enum Reb_Kind target;
    REBCTX *context;

    if (ANY_CONTEXT(spec)) {
        parent = VAL_CONTEXT(spec);
        target = VAL_TYPE(spec);
    }
    else if (IS_DATATYPE(spec)) {
        //
        // Should this be supported, or just assume OBJECT! ?  There are
        // problems trying to create a FRAME! without a function (for
        // instance), and making an ERROR! from scratch is currently dangerous
        // as well though you can derive them.
        //
        fail (Error(RE_MISC));
    }
    else {
        assert(IS_BLOCK(spec));
        target = REB_OBJECT;
    }

    // This parallels the code originally in CONSTRUCT.  Run it if the /ONLY
    // refinement was passed in.
    //
    if (REF(only)) {
        Val_Init_Object(
            D_OUT,
            Construct_Context(
                REB_OBJECT,
                VAL_ARRAY_AT(body),
                VAL_SPECIFIER(body),
                parent
            )
        );
        return R_OUT;
    }

    // This code came from REBTYPE(Context) for implementing MAKE OBJECT!.
    // Now that MAKE ANY-CONTEXT! has been pulled back, it no longer does
    // any evaluation or creates SELF fields.  It also obeys the rule that
    // the first argument is an exemplar of the type to create only, bringing
    // uniformity to MAKE.
    //
    if (target == REB_OBJECT && (IS_BLOCK(body) || IS_BLANK(body))) {
        //
        // First we scan the object for top-level set words in
        // order to make an appropriately sized context.  Then
        // we put it into an object in D_OUT to GC protect it.
        //
        context = Make_Selfish_Context_Detect(
            REB_OBJECT, // type
            NULL, // spec
            NULL, // body
            // scan for toplevel set-words
            IS_BLANK(body) ? END_CELL : VAL_ARRAY_AT(body),
            parent
        );
        Val_Init_Object(D_OUT, context);

        if (!IS_BLANK(body)) {
            //
            // !!! This binds the actual body data, not a copy of it
            // (functions make a copy of the body they are passed to
            // be rebound).  This seems wrong.
            //
            Bind_Values_Deep(VAL_ARRAY_AT(body), context);

            // Do the block into scratch space (we ignore the result,
            // unless it is thrown in which case it must be returned.
            //
            REBVAL dummy;
            if (DO_VAL_ARRAY_AT_THROWS(&dummy, body)) {
                *D_OUT = dummy;
                return R_OUT_IS_THROWN;
            }
        }

        return R_OUT;
    }

    // "multiple inheritance" case when both spec and body are objects.
    //
    // !!! As with most R3-Alpha concepts, this needs review.
    //
    if ((target == REB_OBJECT) && parent && IS_OBJECT(body)) {
        //
        // !!! Again, the presumption that the result of a merge is to
        // be selfish should not be hardcoded in the C, but part of
        // the generator choice by the person doing the derivation.
        //
        context = Merge_Contexts_Selfish(parent, VAL_CONTEXT(body));
        Val_Init_Object(D_OUT, context);
        return R_OUT;
    }

    fail (Error(RE_MISC));
}
