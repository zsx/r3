/***********************************************************************
**
**  REBOL Language Interpreter and Run-time Environment
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
**  Module:  m-series.c
**  Summary: implements REBOL's series concept
**  Section: memory
**  Author:  Carl Sassenrath
**
***********************************************************************/

#include "sys-core.h"
#include "mem-series.h" // low-level series memory access
#include "sys-int-funcs.h"



//
//  Extend_Series: C
// 
// Extend a series at its end without affecting its tail index.
//
void Extend_Series(REBSER *series, REBCNT delta)
{
    REBCNT len_old = series->content.dynamic.len; // maintain tail position
    EXPAND_SERIES_TAIL(series, delta);
    series->content.dynamic.len = len_old;
}


//
//  Insert_Series: C
// 
// Insert a series of values (bytes, longs, reb-vals) into the
// series at the given index.  Expand it if necessary.  Does
// not add a terminator to tail.
//
REBCNT Insert_Series(
    REBSER *series,
    REBCNT index,
    const REBYTE *data,
    REBCNT len
) {
    if (index > series->content.dynamic.len)
        index = series->content.dynamic.len;

    Expand_Series(series, index, len); // tail += len

    memcpy(
        series->content.dynamic.data + (SER_WIDE(series) * index),
        data,
        SER_WIDE(series) * len
    );

    return index + len;
}


//
//  Append_Series: C
// 
// Append value(s) onto the tail of a series.  The len is
// the number of units (bytes, REBVALS, etc.) of the data,
// and does not include the terminator (which will be added).
// The new tail position will be returned as the result.
// A terminator will be added to the end of the appended data.
//
void Append_Series(REBSER *series, const REBYTE *data, REBCNT len)
{
    REBCNT len_old = series->content.dynamic.len;
    REBYTE wide = SER_WIDE(series);

    assert(!Is_Array_Series(series));

    EXPAND_SERIES_TAIL(series, len);
    memcpy(series->content.dynamic.data + (wide * len_old), data, wide * len);

    CLEAR(
        series->content.dynamic.data + (wide * series->content.dynamic.len),
        wide
    );
}


//
//  Append_Values_Len: C
// 
// Append value(s) onto the tail of an array.  The len is
// the number of units and does not include the terminator
// (which will be added).
//
void Append_Values_Len(REBARR *array, const REBVAL *head, REBCNT len)
{
    REBYTE *dest = cast(REBYTE*, ARR_TAIL(array));

    // updates tail (hence we calculated dest before)
    //
    EXPAND_SERIES_TAIL(ARR_SERIES(array), len);

    memcpy(dest, head, sizeof(REBVAL) * len);

    TERM_ARRAY(array);
}


//
//  Append_Mem_Extra: C
// 
// An optimized function for appending raw memory bytes to
// a byte-sized series. The series will be expanded if room
// is needed. A zero terminator will be added at the tail.
// The extra size will be assured in the series, but is not
// part of the appended length. (Allows adding additional bytes.)
//
void Append_Mem_Extra(
    REBSER *series,
    const REBYTE *data,
    REBCNT len,
    REBCNT extra
) {
    REBCNT len_old = series->content.dynamic.len;

    if ((len_old + len + extra + 1) >= SER_REST(series)) {
        Expand_Series(series, len_old, len + extra); // SER_LEN changed
        series->content.dynamic.len -= extra;
    }
    else {
        series->content.dynamic.len += len;
    }

    memcpy(series->content.dynamic.data + len_old, data, len);
    TERM_SEQUENCE(series);
}


//
//  Copy_Sequence: C
// 
// Copy any series that *isn't* an "array" (such as STRING!,
// BINARY!, BITSET!, VECTOR!...).  Includes the terminator.
// 
// Use Copy_Array routines (which specify Shallow, Deep, etc.) for
// greater detail needed when expressing intent for Rebol Arrays.
// 
// Note: No suitable name for "non-array-series" has been picked.
// "Sequence" is used for now because Copy_Non_Array() doesn't
// look good and lots of things aren't "Rebol Arrays" that aren't
// series.  The main idea was just to get rid of the generic
// Copy_Series() routine, which doesn't call any attention
// to the importance of stating one's intentions specifically
// about semantics when copying an array.
//
REBSER *Copy_Sequence(REBSER *original)
{
    REBCNT len_orig_plus = original->content.dynamic.len + 1;
    REBSER *copy = Make_Series(len_orig_plus, SER_WIDE(original), MKS_NONE);

    assert(!Is_Array_Series(original));

    memcpy(
        copy->content.dynamic.data,
        original->content.dynamic.data,
        len_orig_plus * SER_WIDE(original)
    );
    copy->content.dynamic.len = original->content.dynamic.len;
    return copy;
}


//
//  Copy_Sequence_At_Len: C
// 
// Copy a subseries out of a series that is not an array.
// Includes the terminator for it.
// 
// Use Copy_Array routines (which specify Shallow, Deep, etc.) for
// greater detail needed when expressing intent for Rebol Arrays.
//
REBSER *Copy_Sequence_At_Len(REBSER *original, REBCNT index, REBCNT len)
{
    REBSER *copy = Make_Series(len + 1, SER_WIDE(original), MKS_NONE);

    assert(!Is_Array_Series(original));

    memcpy(
        copy->content.dynamic.data,
        original->content.dynamic.data + index * SER_WIDE(original),
        (len + 1) * SER_WIDE(original)
    );
    copy->content.dynamic.len = len;
    TERM_SEQUENCE(copy);
    return copy;
}


//
//  Copy_Sequence_At_Position: C
// 
// Copy a non-array series from its value structure, using the
// value's index as the location to start copying the data.
//
REBSER *Copy_Sequence_At_Position(const REBVAL *position)
{
    return Copy_Sequence_At_Len(
        VAL_SERIES(position), VAL_INDEX(position), VAL_LEN_AT(position)
    );
}


//
//  Remove_Series: C
// 
// Remove a series of values (bytes, longs, reb-vals) from the
// series at the given index.
//
void Remove_Series(REBSER *series, REBCNT index, REBINT len)
{
    REBCNT  start;
    REBCNT  length;
    REBYTE  *data;

    if (len <= 0) return;

    // Optimized case of head removal:
    if (index == 0) {
        if (cast(REBCNT, len) > series->content.dynamic.len)
            len = series->content.dynamic.len;

        series->content.dynamic.len -= len;
        if (series->content.dynamic.len == 0) {
            // Reset bias to zero:
            len = SER_BIAS(series);
            SER_SET_BIAS(series, 0);
            series->content.dynamic.rest += len;
            series->content.dynamic.data -= SER_WIDE(series) * len;
            TERM_SERIES(series);
        }
        else {
            // Add bias to head:
            REBCNT bias = SER_BIAS(series);
            if (REB_U32_ADD_OF(bias, len, &bias))
                fail (Error(RE_OVERFLOW));

            if (bias > 0xffff) { //bias is 16-bit, so a simple SER_ADD_BIAS could overflow it
                REBYTE *data = series->content.dynamic.data;

                data += SER_WIDE(series) * len;
                series->content.dynamic.data -=
                    SER_WIDE(series) * SER_BIAS(series);

                series->content.dynamic.rest += SER_BIAS(series);
                SER_SET_BIAS(series, 0);

                memmove(
                    series->content.dynamic.data,
                    data,
                    SER_USED(series)
                );
            }
            else {
                SER_SET_BIAS(series, bias);
                series->content.dynamic.rest -= len;
                series->content.dynamic.data += SER_WIDE(series) * len;
                if ((start = SER_BIAS(series))) {
                    // If more than half biased:
                    if (start >= MAX_SERIES_BIAS || start > SER_REST(series))
                        Unbias_Series(series, TRUE);
                }
            }
        }
        return;
    }

    if (index >= series->content.dynamic.len) return;

    start = index * SER_WIDE(series);

    // Clip if past end and optimize the remove operation:
    if (len + index >= series->content.dynamic.len) {
        series->content.dynamic.len = index;
        TERM_SERIES(series);
        return;
    }

    length = (SER_LEN(series) + 1) * SER_WIDE(series); // include term.
    series->content.dynamic.len -= cast(REBCNT, len);
    len *= SER_WIDE(series);
    data = series->content.dynamic.data + start;
    memmove(data, data + len, length - (start + len));
}


//
//  Remove_Sequence_Last: C
// 
// Remove last value from a sequence.
//
void Remove_Sequence_Last(REBSER *series)
{
    assert(!Is_Array_Series(series));
    assert(series->content.dynamic.len != 0);
    series->content.dynamic.len--;
    TERM_SEQUENCE(series);
}


//
//  Remove_Array_Last: C
// 
// Remove last value from an array.
//
void Remove_Array_Last(REBARR *array)
{
    assert(ARR_LEN(array) != 0);
    SET_ARRAY_LEN(array, ARR_LEN(array) - 1);
    TERM_ARRAY(array);
}


//
//  Unbias_Series: C
// 
// Reset series bias.
//
void Unbias_Series(REBSER *series, REBOOL keep)
{
    REBCNT len;
    REBYTE *data = series->content.dynamic.data;

    len = SER_BIAS(series);
    if (len == 0) return;

    SER_SET_BIAS(series, 0);
    series->content.dynamic.rest += len;
    series->content.dynamic.data -= SER_WIDE(series) * len;

    if (keep)
        memmove(series->content.dynamic.data, data, SER_USED(series));
}


//
//  Reset_Series: C
// 
// Reset series to empty. Reset bias, tail, and termination.
// The tail is reset to zero.
//
void Reset_Series(REBSER *series)
{
    assert(!Is_Array_Series(series));
    Unbias_Series(series, FALSE);
    series->content.dynamic.len = 0;
    TERM_SERIES(series);
}


//
//  Reset_Array: C
// 
// Reset series to empty. Reset bias, tail, and termination.
// The tail is reset to zero.
//
void Reset_Array(REBARR *array)
{
    Unbias_Series(ARR_SERIES(array), FALSE);
    SET_ARRAY_LEN(array, 0);
    TERM_ARRAY(array);
}


//
//  Clear_Series: C
// 
// Clear an entire series to zero. Resets bias and tail.
// The tail is reset to zero.
//
void Clear_Series(REBSER *series)
{
    Unbias_Series(series, FALSE);
    series->content.dynamic.len = 0;
    CLEAR(series->content.dynamic.data, SER_SPACE(series));
    TERM_SERIES(series);
}


//
//  Resize_Series: C
// 
// Reset series and expand it to required size.
// The tail is reset to zero.
//
void Resize_Series(REBSER *series, REBCNT size)
{
    series->content.dynamic.len = 0;
    Unbias_Series(series, TRUE);
    EXPAND_SERIES_TAIL(series, size);
    series->content.dynamic.len = 0;
    TERM_SERIES(series);
}


//
//  Reset_Buffer: C
// 
// Setup to reuse a shared buffer. Expand it if needed.
// 
// NOTE: The length will be set to the supplied value, but the series will
// not be terminated.
//
REBYTE *Reset_Buffer(REBSER *buf, REBCNT len)
{
    if (!buf) panic (Error(RE_NO_BUFFER));

    RESET_TAIL(buf);
    Unbias_Series(buf, TRUE);
    Expand_Series(buf, 0, len); // sets new tail

    return SER_DATA_RAW(buf);
}


//
//  Copy_Buffer: C
// 
// Copy a shared buffer, starting at index. Set tail and termination.
//
REBSER *Copy_Buffer(REBSER *buf, REBCNT index, void *end)
{
    REBSER *ser;
    REBCNT len;

    assert(!Is_Array_Series(buf));

    len = BYTE_SIZE(buf)
        ? cast(REBYTE*, end) - BIN_HEAD(buf)
        : cast(REBUNI*, end) - UNI_HEAD(buf);

    if (index) len -= index;

    ser = Make_Series(len + 1, SER_WIDE(buf), MKS_NONE);

    memcpy(
        ser->content.dynamic.data,
        buf->content.dynamic.data + index * SER_WIDE(buf),
        SER_WIDE(buf) * len
    );
    ser->content.dynamic.len = len;
    TERM_SEQUENCE(ser);

    return ser;
}


#if !defined(NDEBUG)

//
//  SER_AT_RAW_Debug: C
//
// This functionality is extremely common, and having too much checking on
// each invocation as part of the macro expansion was creating explosions
// of unreadable macro code in asserts.  Putting it into a function in
// the debug build reigns in the expansions...and since performance is
// not a concern in the debug build, it's ok.
//
REBYTE *SER_AT_RAW_Debug(size_t w, REBSER *s, REBCNT i) {
    //
    // Callsites know the type of pointer they are asking for, so they often
    // know the width without having to look in the series to get it.  Assert
    // the width they think is consistent with what SER_WIDE thinks.
    //
    assert(w == SER_WIDE(s));
    return SER_AT_RAW_MACRO(w, s, i);
}


//
//  Assert_Series_Term_Core: C
//
void Assert_Series_Term_Core(REBSER *series)
{
    if (Is_Array_Series(series)) {
        //
        // END values aren't canonized to zero bytes, check IS_END explicitly
        //
        if (NOT_END(ARR_AT(AS_ARRAY(series), SER_LEN(series)))) {
            Debug_String(
               "Unterminated blocklike series detected",
               38, // ^--- leng
               FALSE, 1
            );
            Panic_Series(series);
        }
    }
    else {
        //
        // If they are terminated, then non-REBVAL-bearing series must have
        // their terminal element as all 0 bytes (to use this check)
        //
        REBCNT n;
        for (n = 0; n < SER_WIDE(series); n++) {
            if (0 != series->content.dynamic.data[
                series->content.dynamic.len * SER_WIDE(series) + n
            ]) {
                Debug_String(
                   "Non-zero byte in terminator of non-block series",
                   47, // ^--- length of this
                   FALSE, 1
                );
                Panic_Series(series);
            }
        }
    }
}


//
//  Assert_Series_Core: C
//
void Assert_Series_Core(REBSER *series)
{
    if (SER_FREED(series))
        Panic_Series(series);

    assert(SER_LEN(series) < SER_REST(series));

    Assert_Series_Term_Core(series);
}


//
//  Panic_Series_Debug: C
// 
// This could be done in the PANIC_SERIES macro, but having it
// as an actual function gives you a place to set breakpoints.
//
ATTRIBUTE_NO_RETURN void Panic_Series_Debug(
    const REBSER *series,
    const char *file,
    int line
) {
    if (TG_Pushing_Mold) { // cannot call Debug_Fmt !
        Debug_String(
            "Panic_Series() while pushing_mold",
            33, // ^--- length of this!
            FALSE, 1
        );
    }
    else {
        Debug_Fmt("Panic_Series() in %s at line %d", file, line);
    }
    if (*series->guard == 1020) // should make valgrind or asan alert
        panic (Error(RE_MISC));
    panic (Error(RE_MISC)); // just in case it didn't crash
}

#endif
