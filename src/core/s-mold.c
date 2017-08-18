//
//  File: %s-mold.c
//  Summary: "value to string conversion"
//  Section: strings
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
// "Molding" is the term in Rebol for getting a string representation of a
// value that is intended to be LOADed back into the system.  So if you
// mold a STRING!, you would get back another STRING! that would include
// the delimiters for that string.
//
// "Forming" is the term for creating a string representation of a value that
// is intended for print output.  So if you were to form a STRING!, it would
// *not* add delimiters--just giving the string back as-is.
//
// There are several technical problems in molding regarding the handling of
// values that do not have natural expressions in Rebol source.  For instance,
// it might be legal to `make word! "123"` but that cannot just be molded as
// 123 because that would LOAD as an integer.  There are additional problems
// with `mold next [a b c]`, because there is no natural representation for a
// series that is not at its head.  These problems were addressed with
// "construction syntax", e.g. #[word! "123"] or #[block! [a b c] 1].  But
// to get this behavior MOLD/ALL had to be used, and it was implemented in
// something of an ad-hoc way.
//
// These concepts are a bit fuzzy in general, and though MOLD might have made
// sense when Rebol was supposedly called "Clay", it now looks off-putting.
// (Who wants to deal with old, moldy code?)  Most of Ren-C's focus has been
// on the evaluator, so there are not that many advances in molding--other
// than the code being tidied up and methodized a little.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Notes:
//
// * Because molding and forming of a type share a lot of code, they are
//   implemented in "(M)old or (F)orm" hooks (MF_Xxx).  Also, since classes
//   of types can share behavior, several types are sometimes handled in the
//   same hook.  See %types.r for these categorizations in the "mold" column.
//
// * Molding is done into a REB_MOLD structure, which in addition to the
//   series to mold into contains options for the mold--including length
//   limits, whether commas or periods should be used for decimal points,
//   indentation rules, etc.
//
// * If you create the REB_MOLD using the Push_Mold() function, then it will
//   append in a stacklike way to the thread-local "mold buffer".  This
//   allows new molds to start running and use that buffer while another is in
//   progress, so long as it pops or drops the buffer before returning to the
//   code doing the higher level mold.
//
// * It's hard to know in advance how long molded output will be or whether
//   it will use any wide characters, using the mold buffer allows one to use
//   a "hot" preallocated wide-char buffer for the mold...and copy out a
//   series of the precise width and length needed.  (That is, if copying out
//   the result is needed at all.)
//

#include "sys-core.h"


//
//  Emit: C
//
// This is a general "printf-style" utility function, which R3-Alpha used to
// make some formatting tasks easier.  It was not applied consistently, and
// some callsites avoided using it because it would be ostensibly slower
// than calling the functions directly.
//
void Emit(REB_MOLD *mo, const char *fmt, ...)
{
    REBSER *s = mo->series;
    assert(SER_WIDE(s) == 2);

    va_list va;
    va_start(va, fmt);

    REBYTE ender = '\0';

    for (; *fmt; fmt++) {
        switch (*fmt) {
        case 'W': { // Word symbol
            const REBVAL *any_word = va_arg(va, const REBVAL*);
            REBSTR *spelling = VAL_WORD_SPELLING(any_word);
            Append_UTF8_May_Fail(
                s, STR_HEAD(spelling), STR_NUM_BYTES(spelling)
            );
            break;
        }

        case 'V': // Value
            Mold_Value(mo, va_arg(va, const REBVAL*));
            break;

        case 'S': // String of bytes
            Append_Unencoded(s, va_arg(va, const char *));
            break;

        case 'C': // Char
            Append_Codepoint_Raw(s, va_arg(va, REBCNT));
            break;

        case 'E': { // Series (byte or uni)
            REBSER *src = va_arg(va, REBSER*);
            Insert_String(s, SER_LEN(s), src, 0, SER_LEN(src), FALSE);
            break; }

        case 'I': // Integer
            Append_Int(s, va_arg(va, REBINT));
            break;

        case 'i':
            Append_Int_Pad(s, va_arg(va, REBINT), -9);
            Trim_Tail(s, '0');
            break;

        case '2': // 2 digit int (for time)
            Append_Int_Pad(s, va_arg(va, REBINT), 2);
            break;

        case 'T': {  // Type name
            const REBYTE *bytes = Get_Type_Name(va_arg(va, REBVAL*));
            Append_UTF8_May_Fail(s, bytes, LEN_BYTES(bytes));
            break; }

        case 'N': {  // Symbol name
            REBSTR *spelling = va_arg(va, REBSTR*);
            Append_UTF8_May_Fail(
                s, STR_HEAD(spelling), STR_NUM_BYTES(spelling)
            );
            break; }

        case '+': // Add #[ if mold/all
            if (GET_MOLD_FLAG(mo, MOLD_FLAG_ALL)) {
                Append_Unencoded(s, "#[");
                ender = ']';
            }
            break;

        case 'D': // Datatype symbol: #[type
            if (ender != '\0') {
                REBSTR *canon = Canon(cast(REBSYM, va_arg(va, int)));
                Append_UTF8_May_Fail(
                    s, STR_HEAD(canon), STR_NUM_BYTES(canon)
                );
                Append_Codepoint_Raw(s, ' ');
            }
            else
                va_arg(va, REBCNT); // ignore it
            break;

        default:
            Append_Codepoint_Raw(s, *fmt);
        }
    }

    va_end(va);

    if (ender != '\0')
        Append_Codepoint_Raw(s, ender);
}


//
//  Prep_Uni_Series: C
//
REBUNI *Prep_Uni_Series(REB_MOLD *mo, REBCNT len)
{
    REBCNT tail = SER_LEN(mo->series);

    EXPAND_SERIES_TAIL(mo->series, len);

    return UNI_AT(mo->series, tail);
}


//
//  Pre_Mold: C
//
// Emit the initial datatype function, depending on /ALL option
//
void Pre_Mold(REB_MOLD *mo, const RELVAL *v)
{
    Emit(mo, GET_MOLD_FLAG(mo, MOLD_FLAG_ALL) ? "#[T " : "make T ", v);
}


//
//  End_Mold: C
//
// Finish the mold, depending on /ALL with close block.
//
void End_Mold(REB_MOLD *mo)
{
    if (GET_MOLD_FLAG(mo, MOLD_FLAG_ALL))
        Append_Codepoint_Raw(mo->series, ']');
}


//
//  Post_Mold: C
//
// For series that has an index, add the index for mold/all.
// Add closing block.
//
void Post_Mold(REB_MOLD *mo, const RELVAL *v)
{
    if (VAL_INDEX(v)) {
        Append_Codepoint_Raw(mo->series, ' ');
        Append_Int(mo->series, VAL_INDEX(v) + 1);
    }
    if (GET_MOLD_FLAG(mo, MOLD_FLAG_ALL))
        Append_Codepoint_Raw(mo->series, ']');
}


//
//  New_Indented_Line: C
//
// Create a newline with auto-indent on next line if needed.
//
void New_Indented_Line(REB_MOLD *mo)
{
    // Check output string has content already but no terminator:
    //
    REBUNI *cp;
    if (SER_LEN(mo->series) == 0)
        cp = NULL;
    else {
        cp = UNI_LAST(mo->series);
        if (*cp == ' ' || *cp == '\t')
            *cp = '\n';
        else
            cp = NULL;
    }

    // Add terminator:
    if (cp == NULL)
        Append_Codepoint_Raw(mo->series, '\n');

    // Add proper indentation:
    if (NOT_MOLD_FLAG(mo, MOLD_FLAG_INDENT)) {
        REBINT n;
        for (n = 0; n < mo->indent; n++)
            Append_Unencoded(mo->series, "    ");
    }
}


//=//// DEALING WITH CYCLICAL MOLDS ///////////////////////////////////////=//
//
// While Rebol has never had a particularly coherent story about how cyclical
// data structures will be handled in evaluation, they do occur--and the GC
// is robust to their existence.  These helper functions can be used to
// maintain a stack of series.
//
// !!! TBD: Unify this with the PUSH_GUARD and DROP_GUARD implementation so
// that improvements in one will improve the other?
//
//=////////////////////////////////////////////////////////////////////////=//

//
//  Find_Pointer_In_Series: C
//
REBCNT Find_Pointer_In_Series(REBSER *s, void *p)
{
    REBCNT index = 0;
    for (; index < SER_LEN(s); ++index) {
        if (*SER_AT(void*, s, index) == p)
            return index;
    }
    return NOT_FOUND;
}

//
//  Push_Pointer_To_Series: C
//
void Push_Pointer_To_Series(REBSER *s, void *p)
{
    if (SER_FULL(s))
        Extend_Series(s, 8);
    *SER_AT(void*, s, SER_LEN(s)) = p;
    SET_SERIES_LEN(s, SER_LEN(s) + 1);
}

//
//  Drop_Pointer_From_Series: C
//
void Drop_Pointer_From_Series(REBSER *s, void *p)
{
    assert(p == *SER_AT(void*, s, SER_LEN(s) - 1));
    UNUSED(p);
    SET_SERIES_LEN(s, SER_LEN(s) - 1);

    // !!! Could optimize so mold stack is always dynamic, and just use
    // s->content.dynamic.len--
}


/***********************************************************************
************************************************************************
**
**  SECTION: Block Series Datatypes
**
************************************************************************
***********************************************************************/

//
//  Mold_Array_At: C
//
void Mold_Array_At(
    REB_MOLD *mo,
    REBARR *a,
    REBCNT index,
    const char *sep
) {
    if (sep == NULL)
        sep = "[]";

    // Recursion check:
    if (Find_Pointer_In_Series(TG_Mold_Stack, a) != NOT_FOUND) {
        Emit(mo, "C...C", sep[0], sep[1]);
        return;
    }

    Push_Pointer_To_Series(TG_Mold_Stack, a);

    if (sep[1]) {
        Append_Codepoint_Raw(mo->series, sep[0]);
        mo->indent++;
    }

    REBOOL line_flag = FALSE; // newline was part of block
    REBOOL had_lines = FALSE;
    RELVAL *item = ARR_AT(a, index);
    while (NOT_END(item)) {
        if (GET_VAL_FLAG(item, VALUE_FLAG_LINE)) {
            if (sep[1] || line_flag)
                New_Indented_Line(mo);
            had_lines = TRUE;
        }
        line_flag = TRUE;
        Mold_Value(mo, item);
        item++;
        if (NOT_END(item))
            Append_Codepoint_Raw(mo->series, (sep[0] == '/') ? '/' : ' ');
    }

    if (sep[1]) {
        mo->indent--;
        if (had_lines)
            New_Indented_Line(mo);
        Append_Codepoint_Raw(mo->series, sep[1]);
    }

    Drop_Pointer_From_Series(TG_Mold_Stack, a);
}


//
//  Form_Array_At: C
//
void Form_Array_At(
    REB_MOLD *mo,
    REBARR *array,
    REBCNT index,
    REBCTX *opt_context
) {
    // Form a series (part_mold means mold non-string values):
    REBINT len = ARR_LEN(array) - index;
    if (len < 0)
        len = 0;

    REBINT n;
    for (n = 0; n < len;) {
        RELVAL *item = ARR_AT(array, index + n);
        REBVAL *wval = NULL;
        if (opt_context && (IS_WORD(item) || IS_GET_WORD(item))) {
            wval = Select_Canon_In_Context(opt_context, VAL_WORD_CANON(item));
            if (wval)
                item = wval;
        }
        Mold_Or_Form_Value(mo, item, LOGICAL(wval == NULL));
        n++;
        if (GET_MOLD_FLAG(mo, MOLD_FLAG_LINES)) {
            Append_Codepoint_Raw(mo->series, LF);
        } else {
            // Add a space if needed:
            if (n < len && SER_LEN(mo->series)
                && *UNI_LAST(mo->series) != LF
                && NOT_MOLD_FLAG(mo, MOLD_FLAG_TIGHT)
            ){
                Append_Codepoint_Raw(mo->series, ' ');
            }
        }
    }
}


//
//  MF_Fail: C
//
void MF_Fail(REB_MOLD *mo, const RELVAL *v, REBOOL form)
{
    UNUSED(form);

    if (VAL_TYPE(v) == REB_0) {
        //
        // REB_0 is reserved for special purposes, and should only be molded
        // in debug scenarios.
        //
    #if defined(NDEBUG)
        UNUSED(mo);
        panic (v);
    #else
        printf("!!! Request to MOLD or FORM a REB_0 value !!!\n");
        Append_Unencoded(mo->series, "!!!REB_0!!!");
        debug_break(); // don't crash if under a debugger, just "pause"
    #endif
    }

    fail ("Cannot MOLD or FORM datatype.");
}


//
//  MF_Unhooked: C
//
void MF_Unhooked(REB_MOLD *mo, const RELVAL *v, REBOOL form)
{
    UNUSED(mo);
    UNUSED(form);

    REBVAL *type = Get_Type(VAL_TYPE(v));
    UNUSED(type); // use in error message?

    fail ("Datatype does not have extension with a MOLD handler registered");
}


//
//  Mold_Or_Form_Value: C
//
// Mold or form any value to string series tail.
//
void Mold_Or_Form_Value(REB_MOLD *mo, const RELVAL *v, REBOOL form)
{
    REBSER *s = mo->series;
    assert(SER_WIDE(s) == sizeof(REBUNI));
    ASSERT_SERIES_TERM(s);

    if (C_STACK_OVERFLOWING(&s))
        Trap_Stack_Overflow();

    if (GET_MOLD_FLAG(mo, MOLD_FLAG_LIMIT)) {
        //
        // It's hard to detect the exact moment of tripping over the length
        // limit unless all code paths that add to the mold buffer (e.g.
        // tacking on delimiters etc.) check the limit.  The easier thing
        // to do is check at the end and truncate.  This adds a lot of data
        // wastefully, so short circuit here in the release build.  (Have
        // the debug build keep going to exercise mold on the data.)
        //
    #ifdef NDEBUG
        if (SER_LEN(s) >= mo->limit)
            return;
    #endif
    }

    if (THROWN(v)) {
        //
        // !!! You do not want to see THROWN values leak into user awareness,
        // as they are an implementation detail.  In the C code, a developer
        // might explicitly PROBE() a thrown value, however.
        //
    #if defined(NDEBUG)
        panic (v);
    #else
        printf("!!! Request to MOLD or FORM a THROWN() value !!!\n");
        Append_Unencoded(s, "!!!THROWN(");
        debug_break(); // don't crash if under a debugger, just "pause"
    #endif
    }

    if (IS_VOID(v)) {
        //
        // Voids should only be molded out in debug scenarios, but this still
        // happens a lot, e.g. PROBE() of context arrays when they have unset
        // variables.  This happens so often in debug builds, in fact, that a
        // debug_break() here would be very annoying (the method used for
        // REB_0 and THROWN() items)
        //
    #ifdef NDEBUG
        panic (v);
    #else
        printf("!!! Request to MOLD or FORM a void value !!!\n");
        Append_Unencoded(s, "!!!void!!!");
        return;
    #endif
    }

    MOLD_FUNC dispatcher = Mold_Or_Form_Dispatch[VAL_TYPE(v)];
    dispatcher(mo, v, form); // all types have a hook, even if it just fails

#if !defined(NDEBUG)
    if (THROWN(v))
        Append_Unencoded(s, ")!!!"); // close the "!!!THROWN(" we started
#endif

    ASSERT_SERIES_TERM(s);
}


//
//  Copy_Mold_Or_Form_Value: C
//
// Form a value based on the mold opts provided.
//
REBSER *Copy_Mold_Or_Form_Value(const RELVAL *v, REBFLGS opts, REBOOL form)
{
    DECLARE_MOLD (mo);
    mo->opts = opts;

    Push_Mold(mo);
    Mold_Or_Form_Value(mo, v, form);
    return Pop_Molded_String(mo);
}


//
//  Form_Reduce_Throws: C
//
// Evaluates each item in a block and forms it, with an optional delimiter.
//
// The special treatment of BLANK! in the source block is to act as an
// opt-out, and the special treatment of BAR! is to act as a line break.
// There's no such thing as a void literal in the incoming block, but if
// an element evaluated to void it is also considered an opt-out, equivalent
// to a BLANK!.
//
// BAR!, BLANK!/void, and CHAR! suppress the delimiter logic.  Hence if you
// are to form `["a" space "b" | () (blank) "c" newline "d" "e"]` with a
// delimiter of ":", you will get back `"a b^/c^/d:e"... where only the
// last interstitial is considered a valid candidate for delimiting.
//
REBOOL Form_Reduce_Throws(
    REBVAL *out,
    REBARR *array,
    REBCNT index,
    REBSPC *specifier,
    const REBVAL *delimiter
) {
    assert(!IS_VOID(delimiter)); // use BLANK! to indicate no delimiting
    if (IS_BAR(delimiter))
        delimiter = ROOT_NEWLINE_CHAR; // BAR! is synonymous to newline here

    DECLARE_MOLD (mo);

    Push_Mold(mo);

    DECLARE_FRAME (f);
    Push_Frame_At(f, array, index, specifier, DO_FLAG_NORMAL);

    REBOOL pending = FALSE;

    while (NOT_END(f->value)) {
        if (IS_BLANK(f->value)) { // opt-out
            Fetch_Next_In_Frame(f);
            continue;
        }

        if (IS_BAR(f->value)) { // newline
            Append_Codepoint_Raw(mo->series, '\n');
            pending = FALSE;
            Fetch_Next_In_Frame(f);
            continue;
        }

        if (Do_Next_In_Frame_Throws(out, f)) {
            Drop_Frame(f);
            return TRUE;
        }

        if (IS_VOID(out) || IS_BLANK(out)) // opt-out
            continue;

        if (IS_BAR(out)) { // newline
            Append_Codepoint_Raw(mo->series, '\n');
            pending = FALSE;
            continue;
        }

        if (IS_CHAR(out)) {
            Append_Codepoint_Raw(mo->series, VAL_CHAR(out));
            pending = FALSE;
        }
        else if (IS_BLANK(delimiter)) // no delimiter
            Form_Value(mo, out);
        else {
            if (pending)
                Form_Value(mo, delimiter);

            Form_Value(mo, out);
            pending = TRUE;
        }
    }

    Drop_Frame(f);

    Init_String(out, Pop_Molded_String(mo));

    return FALSE;
}


//
//  Form_Tight_Block: C
//
REBSER *Form_Tight_Block(const REBVAL *blk)
{
    DECLARE_MOLD (mo);

    Push_Mold(mo);

    RELVAL *item;
    for (item = VAL_ARRAY_AT(blk); NOT_END(item); ++item)
        Form_Value(mo, item);

    return Pop_Molded_String(mo);
}


//
//  Push_Mold: C
//
void Push_Mold(REB_MOLD *mo)
{
#if !defined(NDEBUG)
    //
    // If some kind of Debug_Fmt() happens while this Push_Mold is happening,
    // it will lead to a recursion.  It's necessary to look at the stack in
    // the debugger and figure it out manually.  (e.g. any failures in this
    // function will break the very mechanism by which failure messages
    // are reported.)
    //
    // !!! This isn't ideal.  So if all the routines below guaranteed to
    // use some kind of assert reporting mechanism "lower than mold"
    // (hence "lower than Debug_Fmt") that would be an improvement.
    //
    assert(!TG_Pushing_Mold);
    TG_Pushing_Mold = TRUE;
#endif

    // Series is nulled out on Pop in debug builds to make sure you don't
    // Push the same mold tracker twice (without a Pop)
    //
    assert(mo->series == NULL);

#if !defined(NDEBUG)
    // Sanity check that if they set a limit it wasn't 0.  (Perhaps over the
    // long term it would be okay, but for now we'll consider it a mistake.)
    //
    if (GET_MOLD_FLAG(mo, MOLD_FLAG_LIMIT))
        assert(mo->limit != 0);
#endif

    REBSER *s = mo->series = UNI_BUF;
    mo->start = SER_LEN(s);

    ASSERT_SERIES_TERM(s);

    if (GET_MOLD_FLAG(mo, MOLD_FLAG_RESERVE) && SER_REST(s) < mo->reserve) {
        //
        // Expand will add to the series length, so we set it back.
        //
        // !!! Should reserve actually leave the length expanded?  Some cases
        // definitely don't want this, others do.  The protocol most
        // compatible with the appending mold is to come back with an
        // empty buffer after a push.
        //
        Expand_Series(s, mo->start, mo->reserve);
        SET_SERIES_LEN(s, mo->start);
    }
    else if (SER_REST(s) - SER_LEN(s) > MAX_COMMON) {
        //
        // If the "extra" space in the series has gotten to be excessive (due
        // to some particularly large mold), back off the space.  But preserve
        // the contents, as there may be important mold data behind the
        // ->start index in the stack!
        //
        Remake_Series(
            s,
            SER_LEN(s) + MIN_COMMON,
            SER_WIDE(s),
            NODE_FLAG_NODE // NODE_FLAG_NODE means preserve the data
        );
    }

    if (GET_MOLD_FLAG(mo, MOLD_FLAG_ALL))
        mo->digits = MAX_DIGITS;
    else {
        // If there is no notification when the option is changed, this
        // must be retrieved each time.
        //
        // !!! It may be necessary to mold out values before the options
        // block is loaded, and this 'Get_System_Int' is a bottleneck which
        // crashes that in early debugging.  BOOT_ERRORS is sufficient.
        //
        if (PG_Boot_Phase >= BOOT_ERRORS) {
            REBINT idigits = Get_System_Int(
                SYS_OPTIONS, OPTIONS_DECIMAL_DIGITS, MAX_DIGITS
            );
            if (idigits < 0)
                mo->digits = 0;
            else if (idigits > MAX_DIGITS)
                mo->digits = cast(REBCNT, idigits);
            else
                mo->digits = MAX_DIGITS;
        }
        else
            mo->digits = MAX_DIGITS;
    }

#if !defined(NDEBUG)
    TG_Pushing_Mold = FALSE;
#endif
}


//
//  Throttle_Mold: C
//
// Contain a mold's series to its limit (if it has one).
//
void Throttle_Mold(REB_MOLD *mo) {
    if (NOT_MOLD_FLAG(mo, MOLD_FLAG_LIMIT))
        return;

    if (SER_LEN(mo->series) > mo->limit) {
        SET_SERIES_LEN(mo->series, mo->limit - 3); // account for ellipsis
        Append_Unencoded(mo->series, "..."); // adds a null at the tail
    }
}


//
//  Pop_Molded_String_Core: C
//
// When a Push_Mold is started, then string data for the mold is accumulated
// at the tail of the task-global unicode buffer.  Once the molding is done,
// this allows extraction of the string, and resets the buffer to its length
// at the time when the last push began.
//
// Can limit string output to a specified size to prevent long console
// garbage output if MOLD_FLAG_LIMIT was set in Push_Mold().
//
// If len is END_FLAG then all the string content will be copied, otherwise
// it will be copied up to `len`.  If there are not enough characters then
// the debug build will assert.
//
REBSER *Pop_Molded_String_Core(REB_MOLD *mo, REBCNT len)
{
    assert(mo->series); // if NULL there was no Push_Mold()

    ASSERT_SERIES_TERM(mo->series);
    Throttle_Mold(mo);

    assert(
        (len == UNKNOWN) || (len <= SER_LEN(mo->series) - mo->start)
    );

    // The copy process looks at the characters in range and will make a
    // BYTE_SIZE() target string out of the REBUNIs if possible...
    //
    REBSER *result = Copy_String_Slimming(
        mo->series,
        mo->start,
        (len == UNKNOWN)
            ? SER_LEN(mo->series) - mo->start
            : len
    );

    // Though the protocol of Mold_Value does terminate, it only does so if
    // it adds content to the buffer.  If we did not terminate when we
    // reset the size, then these no-op molds (e.g. mold of "") would leave
    // whatever value in the terminator spot was there.  This could be
    // addressed by making no-op molds terminate.
    //
    TERM_UNI_LEN(mo->series, mo->start);

    mo->series = NULL;

    return result;
}


//
//  Pop_Molded_UTF8: C
//
// Same as Pop_Molded_String() except gives back the data in UTF8 byte-size
// series form.
//
REBSER *Pop_Molded_UTF8(REB_MOLD *mo)
{
    assert(SER_LEN(mo->series) >= mo->start);

    ASSERT_SERIES_TERM(mo->series);
    Throttle_Mold(mo);

    REBSER *bytes = Make_UTF8_Binary(
        UNI_AT(mo->series, mo->start),
        SER_LEN(mo->series) - mo->start,
        0,
        OPT_ENC_UNISRC
    );
    assert(BYTE_SIZE(bytes));

    TERM_UNI_LEN(mo->series, mo->start);

    mo->series = NULL;
    return bytes;
}


//
//  Drop_Mold_Core: C
//
// When generating a molded string, sometimes it's enough to have access to
// the molded data without actually creating a new series out of it.  If the
// information in the mold has done its job and Pop_Molded_String() is not
// required, just call this to drop back to the state of the last push.
//
void Drop_Mold_Core(REB_MOLD *mo, REBOOL not_pushed_ok)
{
    // The tokenizer can often identify tokens to load by their start and end
    // pointers in the UTF8 data it is loading alone.  However, scanning
    // string escapes is a process that requires converting the actual
    // characters to unicode.  To avoid redoing this work later in the scan,
    // it uses the unicode buffer as a storage space from the tokenization
    // that did UTF-8 decoding of string contents to reuse.
    //
    // Despite this usage, it's desirable to be able to do things like output
    // debug strings or do basic molding in that code.  So to reuse the
    // allocated unicode buffer, it has to properly participate in the mold
    // stack protocol.
    //
    // However, only a few token types use the buffer.  Rather than burden
    // the tokenizer with an additional flag, having a modality to be willing
    // to "drop" a mold that hasn't ever been pushed is the easiest way to
    // avoid intervening.  Drop_Mold_If_Pushed(mo) macro makes this clearer.
    //
    if (not_pushed_ok && mo->series == NULL)
        return;

    assert(mo->series != NULL); // if NULL there was no Push_Mold

    // When pushed data are to be discarded, mo->series may be unterminated.
    // (Indeed that happens when Scan_Item_Push_Mold returns NULL/0.)
    //
    NOTE_SERIES_MAYBE_TERM(mo->series);

    TERM_UNI_LEN(mo->series, mo->start); // see Pop_Molded_String() notes

    mo->series = NULL;
}


//
//  Startup_Mold: C
//
void Startup_Mold(REBCNT size)
{
    TG_Mold_Stack = Make_Series(10, sizeof(void*));

    Init_String(TASK_UNI_BUF, Make_Unicode(size));
}


//
//  Shutdown_Mold: C
//
void Shutdown_Mold(void)
{
    Free_Series(TG_Mold_Stack);
}
