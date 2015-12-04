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
**  Module:  t-object.c
**  Summary: object datatype
**  Section: datatypes
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"


static REBOOL Same_Object(REBVAL *val, REBVAL *arg)
{
    if (
        VAL_TYPE(arg) == VAL_TYPE(val) &&
        //VAL_OBJ_SPEC(val) == VAL_OBJ_SPEC(arg) &&
        VAL_FRAME(val) == VAL_FRAME(arg)
    ) return TRUE;
    return FALSE;
}


static REBOOL Equal_Object(REBVAL *val, REBVAL *arg)
{
    REBSER *f1;
    REBSER *f2;
    REBSER *k1;
    REBSER *k2;
    REBINT n;

    if (VAL_TYPE(arg) != VAL_TYPE(val)) return FALSE;

    f1 = VAL_FRAME(val);
    f2 = VAL_FRAME(arg);
    if (f1 == f2) return TRUE;
    if (f1->tail != f2->tail) return FALSE;

    k1 = FRM_KEYLIST(f1);
    k2 = FRM_KEYLIST(f2);
    if (k1->tail != k2->tail) return FALSE;

    // Compare each entry:
    for (n = 1; n < (REBINT)(f1->tail); n++) {
        // Do ordinary comparison of the typesets
        if (Cmp_Value(BLK_SKIP(k1, n), BLK_SKIP(k2, n), FALSE) != 0)
            return FALSE;

        // The typesets contain a symbol as well which must match for
        // objects to consider themselves to be equal (but which do not
        // count in comparison of the typesets)
        if (
            VAL_TYPESET_CANON(BLK_SKIP(k1, n))
            != VAL_TYPESET_CANON(BLK_SKIP(k2, n))
        ) {
            return FALSE;
        }

        // !!! A comment here said "Use Compare_Modify_Values();"...but it
        // doesn't... it calls Cmp_Value (?)
        if (Cmp_Value(BLK_SKIP(f1, n), BLK_SKIP(f2, n), FALSE) != 0)
            return FALSE;
    }

    return TRUE;
}

static void Append_Obj(REBSER *obj, REBVAL *arg)
{
    REBCNT i, len;
    REBVAL *word, *val;
    REBINT *binds; // for binding table

    // Can be a word:
    if (ANY_WORD(arg)) {
        if (!Find_Word_Index(obj, VAL_WORD_SYM(arg), TRUE)) {
            // bug fix, 'self is protected only in selfish frames
            if ((VAL_WORD_CANON(arg) == SYM_SELF) && !IS_SELFLESS(obj))
                fail (Error(RE_SELF_PROTECTED));
            Expand_Frame(obj, 1, 1); // copy word table also
            Append_Frame(obj, 0, VAL_WORD_SYM(arg));
            // val is UNSET
        }
        return;
    }

    if (!IS_BLOCK(arg)) fail (Error_Invalid_Arg(arg));

    // Process word/value argument block:
    arg = VAL_BLK_DATA(arg);

    // Use binding table
    binds = WORDS_HEAD(Bind_Table);
    // Handle selfless
    Collect_Keys_Start(IS_SELFLESS(obj) ? BIND_NO_SELF | BIND_ALL : BIND_ALL);
    // Setup binding table with obj words:
    Collect_Object(obj);

    // Examine word/value argument block
    for (word = arg; NOT_END(word); word += 2) {

        if (!IS_WORD(word) && !IS_SET_WORD(word)) {
            // release binding table
            TERM_ARRAY(BUF_COLLECT);
            Collect_Keys_End(obj);
            fail (Error_Invalid_Arg(word));
        }

        if ((i = binds[VAL_WORD_CANON(word)])) {
            // bug fix, 'self is protected only in selfish frames:
            if ((VAL_WORD_CANON(word) == SYM_SELF) && !IS_SELFLESS(obj)) {
                // release binding table
                TERM_ARRAY(BUF_COLLECT);
                Collect_Keys_End(obj);
                fail (Error(RE_SELF_PROTECTED));
            }
        } else {
            // collect the word
            binds[VAL_WORD_CANON(word)] = SERIES_TAIL(BUF_COLLECT);
            EXPAND_SERIES_TAIL(BUF_COLLECT, 1);
            val = BLK_LAST(BUF_COLLECT);
            Val_Init_Typeset(val, ALL_64, VAL_WORD_SYM(word));
        }
        if (IS_END(word + 1)) break; // fix bug#708
    }

    TERM_ARRAY(BUF_COLLECT);

    // Append new words to obj
    len = SERIES_TAIL(obj);
    Expand_Frame(obj, SERIES_TAIL(BUF_COLLECT) - len, 1);
    for (val = BLK_SKIP(BUF_COLLECT, len); NOT_END(val); val++)
        Append_Frame(obj, 0, VAL_TYPESET_SYM(val));

    // Set new values to obj words
    for (word = arg; NOT_END(word); word += 2) {
        REBVAL *key;

        i = binds[VAL_WORD_CANON(word)];
        val = FRM_VALUE(obj, i);
        key = FRM_KEY(obj, i);

        if (VAL_GET_EXT(key, EXT_WORD_LOCK)) {
            Collect_Keys_End(obj);
            fail (Error_Protected_Key(key));
        }

        if (VAL_GET_EXT(key, EXT_WORD_HIDE)) {
            Collect_Keys_End(obj);
            fail (Error(RE_HIDDEN));
        }

        if (IS_END(word + 1)) SET_NONE(val);
        else *val = word[1];

        if (IS_END(word + 1)) break; // fix bug#708
    }

    // release binding table
    Collect_Keys_End(obj);
}

static REBSER *Trim_Frame(REBSER *obj)
{
    REBVAL *val;
    REBINT cnt = 0;
    REBSER *nobj;
    REBVAL *nval;
    REBVAL *key;
    REBVAL *nkey;

    key = FRM_KEYS(obj) + 1;
    for (val = FRM_VALUES(obj) + 1; NOT_END(val); val++, key++) {
        if (VAL_TYPE(val) > REB_NONE && !VAL_GET_EXT(key, EXT_WORD_HIDE))
            cnt++;
    }

    nobj = Alloc_Frame(cnt, TRUE);
    nval = FRM_VALUES(nobj)+1;
    key = FRM_KEYS(obj) + 1;
    nkey = FRM_KEYS(nobj) + 1;
    for (val = FRM_VALUES(obj) + 1; NOT_END(val); val++, key++) {
        if (VAL_TYPE(val) > REB_NONE && !VAL_GET_EXT(key, EXT_WORD_HIDE)) {
            *nval++ = *val;
            *nkey++ = *key;
        }
    }
    SET_END(nval);
    SET_END(nkey);
    SERIES_TAIL(nobj) = cnt+1;
    SERIES_TAIL(FRM_KEYLIST(nobj)) = cnt+1;

    return nobj;
}


//
//  CT_Object: C
//
REBINT CT_Object(REBVAL *a, REBVAL *b, REBINT mode)
{
    if (mode < 0) return -1;
    if (mode == 3) return Same_Object(a, b);
    return Equal_Object(a, b);
}


//
//  CT_Frame: C
//
REBINT CT_Frame(REBVAL *a, REBVAL *b, REBINT mode)
{
    if (mode < 0) return -1;
    return VAL_SERIES(a) == VAL_SERIES(b);
}



//
//  MT_Object: C
//
REBFLG MT_Object(REBVAL *out, REBVAL *data, enum Reb_Kind type)
{
    if (!IS_BLOCK(data)) return FALSE;
    VAL_FRAME(out) = Construct_Object(VAL_BLK_DATA(data), FALSE, NULL);
    VAL_SET(out, type);
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
//  PD_Object: C
//
REBINT PD_Object(REBPVS *pvs)
{
    REBINT n = 0;
    REBSER *frame = VAL_FRAME(pvs->value);

    assert(frame);

    if (IS_WORD(pvs->select)) {
        n = Find_Word_Index(frame, VAL_WORD_SYM(pvs->select), FALSE);
    }
    else
        return PE_BAD_SELECT;

    if (n <= 0 || cast(REBCNT, n) >= SERIES_TAIL(frame))
        return PE_BAD_SELECT;

    if (
        pvs->setval
        && IS_END(pvs->path + 1)
        && VAL_GET_EXT(FRM_KEY(frame, n), EXT_WORD_LOCK)
    ) {
        fail (Error(RE_LOCKED_WORD, pvs->select));
    }

    pvs->value = FRM_VALUE(frame, n);
    return PE_SET;
}


//
//  REBTYPE: C
// 
// Handles object!, module!, and error! datatypes.
//
REBTYPE(Object)
{
    REBVAL *value = D_ARG(1);
    REBVAL *arg = D_ARGC > 1 ? D_ARG(2) : NULL;
    REBSER *frame;
    REBSER *src_frame;
    enum Reb_Kind target;

    switch (action) {

    case A_MAKE:
        // make object! | error! | module! | task!
        if (IS_DATATYPE(value)) {

            target = VAL_TYPE_KIND(value);

            if (IS_BLOCK(arg)) {

                // make object! [init]
                //
                if (target == REB_OBJECT) {
                    //
                    // First we scan the object for top-level set words in
                    // order to make an appropriately sized frame.  Then
                    // we put it into an object in D_OUT to GC protect it.
                    //
                    frame = Make_Frame_Detect(
                        REB_OBJECT, // type
                        EMPTY_ARRAY, // spec
                        NULL, // body
                        VAL_BLK_DATA(arg), // scan for toplevel set-words
                        NULL // parent
                    );
                    Val_Init_Object(D_OUT, frame);

                    // !!! This binds the actual arg data, not a copy of it
                    // (functions make a copy of the body they are passed to
                    // be rebound).  This seems wrong.
                    //
                    Bind_Values_Deep(VAL_BLK_DATA(arg), frame);

                    // Do the block into scratch space (we ignore the result,
                    // unless it is thrown in which case it must be returned.
                    //
                    if (DO_ARRAY_THROWS(D_CELL, arg)) {
                        *D_OUT = *D_CELL;
                        return R_OUT_IS_THROWN;
                    }

                    return R_OUT;
                }

                if (target == REB_MODULE) {
                    Make_Module(D_OUT, arg);
                    return R_OUT;
                }

                // make task! [init]
                if (target == REB_TASK) {
                    // Does it include a spec?
                    if (IS_BLOCK(VAL_BLK_HEAD(arg))) {
                        arg = VAL_BLK_HEAD(arg);
                        if (!IS_BLOCK(arg + 1))
                            fail (Error_Bad_Make(REB_TASK, value));
                        frame = Make_Module_Spec(arg);
                        VAL_MOD_BODY(value) = VAL_SERIES(arg+1);
                    } else {
                        frame = Make_Module_Spec(0);
                        VAL_MOD_BODY(value) = VAL_SERIES(arg);
                    }

                    // !!! Tasks were never very well specified, though what
                    // was intended should be studied.  Why were they objects,
                    // and was that important?
                    //
                    fail (Error(RE_MISC));
                }
            }

            // make error! [....]
            if (target == REB_ERROR) {
                // arg is block/string
                if (Make_Error_Object_Throws(D_OUT, arg))
                    return R_OUT_IS_THROWN;
                return R_OUT;
            }

            // make object! 10
            if (IS_NUMBER(arg)) {
                REBINT n = Int32s(arg, 0);
                frame = Alloc_Frame(n, TRUE);
                VAL_SET(FRM_CONTEXT(frame), target);
                FRM_SPEC(frame) = EMPTY_ARRAY;
                FRM_BODY(frame) = NULL;
                Val_Init_Context(D_OUT, target, frame, EMPTY_ARRAY, NULL);
                return R_OUT;
            }

            // make object! map!
            if (IS_MAP(arg)) {
                frame = Map_To_Object(VAL_SERIES(arg));
                Val_Init_Context(D_OUT, target, frame, EMPTY_ARRAY, NULL);
                return R_OUT;
            }

            fail (Error_Bad_Make(target, arg));
        }

        // make parent-object ....
        if (IS_OBJECT(value)) {
            src_frame = VAL_FRAME(value);

            // make parent none | []
            if (IS_NONE(arg) || (IS_BLOCK(arg) && IS_EMPTY(arg))) {
                frame = Copy_Array_Core_Managed(
                    src_frame,
                    0, // at
                    SERIES_TAIL(src_frame), // tail
                    0, // extra
                    TRUE, // deep
                    TS_CLONE // types
                );
                SERIES_SET_FLAG(frame, SER_FRAME);
                FRM_KEYLIST(frame) = FRM_KEYLIST(src_frame);
                VAL_FRAME(FRM_CONTEXT(frame)) = frame;
                Rebind_Frame(src_frame, frame);
                Val_Init_Object(D_OUT, frame);
                return R_OUT;
            }

            // make parent [...]
            if (IS_BLOCK(arg)) {
                frame = Make_Frame_Detect(
                    REB_OBJECT, // type
                    EMPTY_ARRAY, // spec
                    NULL, // body
                    VAL_BLK_DATA(arg), // values to scan for toplevel set-words
                    src_frame // parent
                );
                Rebind_Frame(src_frame, frame);
                Val_Init_Object(D_OUT, frame);
                Bind_Values_Deep(VAL_BLK_DATA(arg), frame);

                // frame is GC safe, run the bound block body and put the
                // output into a scratch cell.  We ignore the result unless
                // it is thrown (in which case we return it)
                //
                if (DO_ARRAY_THROWS(D_CELL, arg)) {
                    *D_OUT = *D_CELL;
                    return R_OUT_IS_THROWN;
                }

                return R_OUT;
            }

            // make parent-object object
            if (IS_OBJECT(arg)) {
                frame = Merge_Frames(src_frame, VAL_FRAME(arg));
                MANAGE_FRAME(frame);
                Val_Init_Object(D_OUT, frame);
                return R_OUT;
            }
        }
        fail (Error_Bad_Make(VAL_TYPE(value), value));

    case A_TO:
        target = IS_DATATYPE(value)
            ? VAL_TYPE_KIND(value)
            : VAL_TYPE(value);

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
                frame = VAL_FRAME(arg);
                break; // returns frame
            }
            Val_Init_Object(D_OUT, VAL_FRAME(arg));
            return R_OUT;
        }
        else if (target == REB_MODULE) {
            REBVAL *item;
            if (!IS_BLOCK(arg))
                fail (Error_Bad_Make(REB_MODULE, arg));

            item = VAL_BLK_DATA(arg);

            // Called from `make-module*`, as `to module! reduce [spec obj]`
            //
            // item[0] should be module spec
            // item[1] should be module object
            //
            if (IS_END(item) || IS_END(item + 1))
                fail (Error_Bad_Make(REB_MODULE, arg));
            if (!IS_OBJECT(item))
                fail (Error_Invalid_Arg(item));
            if (!IS_OBJECT(item + 1))
                fail (Error_Invalid_Arg(item + 1));

            // !!! We must make a shallow copy of the frame, otherwise there
            // is no way to change the frame type to module without wrecking
            // the object passed in.

            frame = Copy_Array_Shallow(VAL_FRAME(item + 1));
            SERIES_SET_FLAG(frame, SER_FRAME);
            FRM_KEYLIST(frame) = FRM_KEYLIST(VAL_FRAME(item + 1));
            MANAGE_SERIES(frame);
            VAL_SET(FRM_CONTEXT(frame), REB_MODULE);
            VAL_FRAME(FRM_CONTEXT(frame)) = frame;
            FRM_SPEC(frame) = VAL_FRAME(item);
            FRM_BODY(frame) = NULL;

            Val_Init_Module(
                D_OUT,
                frame,
                VAL_FRAME(item),
                NULL
            );
            return R_OUT;
        }
        fail (Error_Bad_Make(target, arg));

    case A_APPEND:
        FAIL_IF_PROTECTED(VAL_FRAME(value));
        if (!IS_OBJECT(value))
            fail (Error_Illegal_Action(VAL_TYPE(value), action));
        Append_Obj(VAL_FRAME(value), arg);
        return R_ARG1;

    case A_LENGTH:
        if (!IS_OBJECT(value))
            fail (Error_Illegal_Action(VAL_TYPE(value), action));
        SET_INTEGER(D_OUT, SERIES_TAIL(VAL_FRAME(value)) - 1);
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
            if (IS_DATATYPE(arg)) types |= FLAGIT_64(VAL_TYPE_KIND(arg));
            else types |= VAL_TYPESET_BITS(arg);
        }
        frame = Copy_Array_Shallow(VAL_FRAME(value));
        FRM_KEYLIST(frame) = FRM_KEYLIST(VAL_FRAME(value));
        MANAGE_SERIES(frame);
        SERIES_SET_FLAG(frame, SER_FRAME);
        VAL_FRAME(FRM_CONTEXT(frame)) = frame;
        if (types != 0) {
            Clonify_Values_Len_Managed(
                BLK_SKIP(frame, 1),
                SERIES_TAIL(frame) - 1,
                D_REF(ARG_COPY_DEEP),
                types
            );
        }
        Val_Init_Context(
            D_OUT,
            VAL_TYPE(value),
            frame,
            VAL_OBJ_SPEC(value),
            VAL_OBJ_BODY(value)
        );
        return R_OUT;
    }

    case A_SELECT:
    case A_FIND: {
        REBINT n;

        if (!IS_WORD(arg))
            return R_NONE;

        n = Find_Word_Index(VAL_FRAME(value), VAL_WORD_SYM(arg), FALSE);

        if (n <= 0)
            return R_NONE;

        if (cast(REBCNT, n) >= SERIES_TAIL(VAL_FRAME(value)))
            return R_NONE;

        if (action == A_FIND) return R_TRUE;

        *D_OUT = *(VAL_OBJ_VALUES(value) + n);
        return R_OUT;
    }

    case A_REFLECT:
        action = What_Reflector(arg); // zero on error
        if (action == OF_SPEC) {
            //
            // !!! Rename to META-OF
            //
            // We do not return this by copy because it belongs to the user
            // constructs to manage.  If they wish to PROTECT it they may,
            // but what we give back here can be modified.
            //
            Val_Init_Object(D_OUT, VAL_OBJ_SPEC(value));
            return R_OUT;
        }

        // Adjust for compatibility with PICK:
        if (action == OF_VALUES) action = 2;
        else if (action == OF_BODY) action = 3;

        if (action < 1 || action > 3)
            fail (Error_Cannot_Reflect(VAL_TYPE(value), arg));

        Val_Init_Block(D_OUT, Make_Object_Block(VAL_FRAME(value), action));
        return R_OUT;

    case A_TRIM:
        if (Find_Refines(call_, ALL_TRIM_REFS)) {
            // no refinements are allowed
            fail (Error(RE_BAD_REFINES));
        }
        Val_Init_Context(
            D_OUT,
            VAL_TYPE(value),
            Trim_Frame(VAL_FRAME(value)),
            VAL_OBJ_SPEC(value),
            VAL_OBJ_BODY(value)
        );
        return R_OUT;

    case A_TAIL_Q:
        if (IS_OBJECT(value)) {
            SET_LOGIC(D_OUT, SERIES_TAIL(VAL_FRAME(value)) <= 1);
            return R_OUT;
        }
        fail (Error_Illegal_Action(VAL_TYPE(value), action));
    }

    fail (Error_Illegal_Action(VAL_TYPE(value), action));
}
