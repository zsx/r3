//
//  File: %t-event.c
//  Summary: "event datatype"
//  Section: datatypes
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
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
// Events are kept compact in order to fit into normal 128 bit
// values cells. This provides high performance for high frequency
// events and also good memory efficiency using standard series.
//

#include "sys-core.h"
#include "reb-evtypes.h"


//
//  CT_Event: C
//
REBINT CT_Event(const RELVAL *a, const RELVAL *b, REBINT mode)
{
    REBINT diff = Cmp_Event(a, b);
    if (mode >=0) return diff == 0;
    return -1;
}


//
//  Cmp_Event: C
//
// Given two events, compare them.
//
REBINT Cmp_Event(const RELVAL *t1, const RELVAL *t2)
{
    REBINT  diff;

    if (
           (diff = VAL_EVENT_MODEL(t1) - VAL_EVENT_MODEL(t2))
        || (diff = VAL_EVENT_TYPE(t1) - VAL_EVENT_TYPE(t2))
        || (diff = VAL_EVENT_XY(t1) - VAL_EVENT_XY(t2))
    ) return diff;

    return 0;
}


//
//  Set_Event_Var: C
//
static REBOOL Set_Event_Var(REBVAL *event, const REBVAL *word, const REBVAL *val)
{
    RELVAL *arg;
    REBINT n;

    switch (VAL_WORD_SYM(word)) {
    case SYM_TYPE:
        if (!IS_WORD(val) && !IS_LIT_WORD(val)) return FALSE;
        arg = Get_System(SYS_VIEW, VIEW_EVENT_TYPES);
        if (IS_BLOCK(arg)) {
            REBSTR *w = VAL_WORD_CANON(val);
            for (n = 0, arg = VAL_ARRAY_HEAD(arg); NOT_END(arg); arg++, n++) {
                if (IS_WORD(arg) && VAL_WORD_CANON(arg) == w) {
                    VAL_EVENT_TYPE(event) = n;
                    return TRUE;
                }
            }
            fail (val);
        }
        return FALSE;

    case SYM_PORT:
        if (IS_PORT(val)) {
            VAL_EVENT_MODEL(event) = EVM_PORT;
            VAL_EVENT_SER(event) = SER(CTX_VARLIST(VAL_CONTEXT(val)));
        }
        else if (IS_OBJECT(val)) {
            VAL_EVENT_MODEL(event) = EVM_OBJECT;
            VAL_EVENT_SER(event) = SER(CTX_VARLIST(VAL_CONTEXT(val)));
        }
        else if (IS_BLANK(val)) {
            VAL_EVENT_MODEL(event) = EVM_GUI;
        } else return FALSE;
        break;

    case SYM_WINDOW:
    case SYM_GOB:
        if (IS_GOB(val)) {
            VAL_EVENT_MODEL(event) = EVM_GUI;
            VAL_EVENT_SER(event) = cast(REBSER*, VAL_GOB(val));
            break;
        }
        return FALSE;

    case SYM_OFFSET:
        if (IS_PAIR(val)) {
            SET_EVENT_XY(
                event,
                Float_Int16(VAL_PAIR_X(val)),
                Float_Int16(VAL_PAIR_Y(val))
            );
        }
        else return FALSE;
        break;

    case SYM_KEY:
        //VAL_EVENT_TYPE(event) != EVT_KEY && VAL_EVENT_TYPE(value) != EVT_KEY_UP)
        VAL_EVENT_MODEL(event) = EVM_GUI;
        if (IS_CHAR(val)) {
            VAL_EVENT_DATA(event) = VAL_CHAR(val);
        }
        else if (IS_LIT_WORD(val) || IS_WORD(val)) {
            arg = Get_System(SYS_VIEW, VIEW_EVENT_KEYS);
            if (IS_BLOCK(arg)) {
                arg = VAL_ARRAY_AT(arg);
                for (n = VAL_INDEX(arg); NOT_END(arg); n++, arg++) {
                    if (IS_WORD(arg) && VAL_WORD_CANON(arg) == VAL_WORD_CANON(val)) {
                        VAL_EVENT_DATA(event) = (n+1) << 16;
                        break;
                    }
                }
                if (IS_END(arg)) return FALSE;
                break;
            }
            return FALSE;
        }
        else return FALSE;
        break;

    case SYM_CODE:
        if (IS_INTEGER(val)) {
            VAL_EVENT_DATA(event) = VAL_INT32(val);
        }
        else return FALSE;
        break;

    case SYM_FLAGS: {
        if (NOT(IS_BLOCK(val)))
            return FALSE;

        VAL_EVENT_FLAGS(event)
            &= ~((1 << EVF_DOUBLE) | (1 << EVF_CONTROL) | (1 << EVF_SHIFT));

        RELVAL *item;
        for (item = VAL_ARRAY_HEAD(val); NOT_END(item); ++item) {
            if (NOT(IS_WORD(item)))
                continue;

            switch (VAL_WORD_SYM(item)) {
            case SYM_CONTROL:
                SET_FLAG(VAL_EVENT_FLAGS(event), EVF_CONTROL);
                break;

            case SYM_SHIFT:
                SET_FLAG(VAL_EVENT_FLAGS(event), EVF_SHIFT);
                break;

            case SYM_DOUBLE:
                SET_FLAG(VAL_EVENT_FLAGS(event), EVF_DOUBLE);
                break;

            default:
                fail (Error_Invalid_Arg_Core(item, VAL_SPECIFIER(val)));
            }
        }
        break; }

    default:
        return FALSE;
    }

    return TRUE;
}


//
//  Set_Event_Vars: C
//
void Set_Event_Vars(REBVAL *evt, RELVAL *blk, REBSPC *specifier)
{
    DECLARE_LOCAL (var);
    DECLARE_LOCAL (val);

    while (NOT_END(blk)) {
        Derelativize(var, blk, specifier);
        ++blk;

        if (IS_END(blk))
            Init_Blank(val);
        else
            Get_Simple_Value_Into(val, blk, specifier);

        ++blk;

        if (!Set_Event_Var(evt, var, val))
            fail (Error_Bad_Field_Set_Raw(var, Type_Of(val)));
    }
}


//
//  Get_Event_Var: C
//
static REBOOL Get_Event_Var(REBVAL *out, const RELVAL *v, REBSTR *name)
{
    switch (STR_SYMBOL(name)) {
    case SYM_TYPE: {
        if (VAL_EVENT_TYPE(v) == 0)
            goto is_blank;

        REBVAL *arg = Get_System(SYS_VIEW, VIEW_EVENT_TYPES);
        if (IS_BLOCK(arg) && VAL_LEN_HEAD(arg) >= EVT_MAX) {
            Derelativize(
                out,
                VAL_ARRAY_AT_HEAD(arg, VAL_EVENT_TYPE(v)),
                VAL_SPECIFIER(arg)
            );
            break;
        }
        return FALSE; }

    case SYM_PORT: {
        // Most events are for the GUI:
        if (IS_EVENT_MODEL(v, EVM_GUI)) {
            Move_Value(out, Get_System(SYS_VIEW, VIEW_EVENT_PORT));
        }
        // Event holds a port:
        else if (IS_EVENT_MODEL(v, EVM_PORT)) {
            Init_Port(out, CTX(VAL_EVENT_SER(v)));
        }
        // Event holds an object:
        else if (IS_EVENT_MODEL(v, EVM_OBJECT)) {
            Init_Object(out, CTX(VAL_EVENT_SER(v)));
        }
        else if (IS_EVENT_MODEL(v, EVM_CALLBACK)) {
            Move_Value(out, Get_System(SYS_PORTS, PORTS_CALLBACK));
        }
        else { // Event holds the IO-Request, which has the PORT:
            assert(IS_EVENT_MODEL(v, EVM_DEVICE));
            REBREQ *req = VAL_EVENT_REQ(v);
            if (req == NULL || req->port == NULL)
                goto is_blank;
            Init_Port(out, CTX(req->port));
        }
        break; }

    case SYM_WINDOW:
    case SYM_GOB: {
        if (IS_EVENT_MODEL(v, EVM_GUI)) {
            if (VAL_EVENT_SER(v)) {
                SET_GOB(out, cast(REBGOB*, VAL_EVENT_SER(v)));
                break;
            }
        }
        return FALSE; }

    case SYM_OFFSET: {
        if (VAL_EVENT_TYPE(v) == EVT_KEY || VAL_EVENT_TYPE(v) == EVT_KEY_UP)
            goto is_blank;
        SET_PAIR(out, VAL_EVENT_X(v), VAL_EVENT_Y(v));
        break; }

    case SYM_KEY: {
        if (VAL_EVENT_TYPE(v) != EVT_KEY && VAL_EVENT_TYPE(v) != EVT_KEY_UP)
            goto is_blank;

        REBINT n = VAL_EVENT_DATA(v); // key-words in top 16, char in lower 16
        if (n & 0xffff0000) {
            REBVAL *arg = Get_System(SYS_VIEW, VIEW_EVENT_KEYS);
            n = (n >> 16) - 1;
            if (IS_BLOCK(arg) && n < cast(REBINT, VAL_LEN_HEAD(arg))) {
                Derelativize(
                    out,
                    VAL_ARRAY_AT_HEAD(arg, n),
                    VAL_SPECIFIER(arg)
                );
                break;
            }
            return FALSE;
        }
        Init_Char(out, n);
        break; }

    case SYM_FLAGS:
        if (
            VAL_EVENT_FLAGS(v)
            & (1 << EVF_DOUBLE | 1 << EVF_CONTROL | 1 << EVF_SHIFT)
        ){
            REBARR *array = Make_Array(3);

            if (GET_FLAG(VAL_EVENT_FLAGS(v), EVF_DOUBLE))
                Init_Word(Alloc_Tail_Array(array), Canon(SYM_DOUBLE));

            if (GET_FLAG(VAL_EVENT_FLAGS(v), EVF_CONTROL))
                Init_Word(Alloc_Tail_Array(array), Canon(SYM_CONTROL));

            if (GET_FLAG(VAL_EVENT_FLAGS(v), EVF_SHIFT))
                Init_Word(Alloc_Tail_Array(array), Canon(SYM_SHIFT));

            Init_Block(out, array);
        }
        else
            Init_Blank(out);
        break;

    case SYM_CODE: {
        if (VAL_EVENT_TYPE(v) != EVT_KEY && VAL_EVENT_TYPE(v) != EVT_KEY_UP)
            goto is_blank;
        REBINT n = VAL_EVENT_DATA(v); // key-words in top 16, char in lower 16
        Init_Integer(out, n);
        break; }

    case SYM_DATA: {
        // Event holds a file string:
        if (VAL_EVENT_TYPE(v) != EVT_DROP_FILE)
            goto is_blank;

        if (!GET_FLAG(VAL_EVENT_FLAGS(v), EVF_COPIED)) {
            void *str = VAL_EVENT_SER(v);

            // !!! This modifies a const-marked values's bits, which
            // is generally a bad thing.  The reason it appears to be doing
            // this is to let clients can put ordinary OS_ALLOC'd arrays of
            // bytes into a field which are then on-demand turned into
            // string series when seen here.  This flips a bit to say the
            // conversion has been done.  Review this implementation.
            //
            REBVAL *writable = m_cast(REBVAL*, const_KNOWN(v));

            VAL_EVENT_SER(writable) = Copy_Bytes(cast(REBYTE*, str), -1);
            SET_FLAG(VAL_EVENT_FLAGS(writable), EVF_COPIED);

            OS_FREE(str);
        }
        Init_File(out, VAL_EVENT_SER(v));
        break; }

    default:
        return FALSE;
    }

    return TRUE;

is_blank:
    Init_Blank(out);
    return TRUE;
}


//
//  MAKE_Event: C
//
void MAKE_Event(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg) {
    assert(kind == REB_EVENT);
    UNUSED(kind);

    if (IS_BLOCK(arg)) {
        CLEARS(out);
        INIT_CELL(out);
        VAL_RESET_HEADER(out, REB_EVENT);
        Set_Event_Vars(
            out,
            VAL_ARRAY_AT(arg),
            VAL_SPECIFIER(arg)
        );
    }
    else
        fail (Error_Unexpected_Type(REB_EVENT, VAL_TYPE(arg)));
}


//
//  TO_Event: C
//
void TO_Event(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    assert(kind == REB_EVENT);
    UNUSED(kind);

    UNUSED(out);
    fail (arg);
}


//
//  PD_Event: C
//
REBINT PD_Event(REBPVS *pvs)
{
    if (IS_WORD(pvs->picker)) {
        if (!pvs->opt_setval || NOT_END(pvs->item + 1)) {
            if (!Get_Event_Var(
                pvs->store, pvs->value, VAL_WORD_CANON(pvs->picker)
            )) {
                fail (Error_Bad_Path_Set(pvs));
            }

            return PE_USE_STORE;
        }
        else {
            if (!Set_Event_Var(
                KNOWN(pvs->value), pvs->picker, pvs->opt_setval
            )) {
                fail (Error_Bad_Path_Set(pvs));
            }

            return PE_OK;
        }
    }

    fail (Error_Bad_Path_Select(pvs));
}


//
//  REBTYPE: C
//
REBTYPE(Event)
{
    UNUSED(frame_);

    fail (Error_Illegal_Action(REB_EVENT, action));
}


//
//  MF_Event: C
//
void MF_Event(REB_MOLD *mo, const RELVAL *v, REBOOL form)
{
    UNUSED(form);

    REBCNT field;
    REBSYM fields[] = {
        SYM_TYPE, SYM_PORT, SYM_GOB, SYM_OFFSET, SYM_KEY,
        SYM_FLAGS, SYM_CODE, SYM_DATA, SYM_0
    };

    Pre_Mold(mo, v);
    Append_Codepoint_Raw(mo->series, '[');
    mo->indent++;

    DECLARE_LOCAL (var); // declare outside loop (has init code)

    for (field = 0; fields[field] != SYM_0; field++) {
        Get_Event_Var(var, v, Canon(fields[field]));
        if (IS_BLANK(var))
            continue;

        New_Indented_Line(mo);

        REBSTR *canon = Canon(fields[field]);
        Append_UTF8_May_Fail(
            mo->series, STR_HEAD(canon), STR_NUM_BYTES(canon)
        );
        Append_Unencoded(mo->series, ": ");
        if (IS_WORD(var))
            Append_Codepoint_Raw(mo->series, '\'');
        Mold_Value(mo, var);
    }

    mo->indent--;
    New_Indented_Line(mo);
    Append_Codepoint_Raw(mo->series, ']');

    End_Mold(mo);
}

