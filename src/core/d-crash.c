//
//  File: %d-crash.c
//  Summary: "low level crash output"
//  Section: debug
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


enum Reb_Pointer_Guess {
    GUESSED_AS_UTF8,
    GUESSED_AS_SERIES,
    GUESSED_AS_FREED_SERIES,
    GUESSED_AS_VALUE,
    GUESSED_AS_END
};

// See the elaborate explanation in %m-gc.c for how this works!
//
static enum Reb_Pointer_Guess Guess_Rebol_Pointer(const void *p) {
    const REBYTE *bp = cast(const REBYTE*, p);
    REBYTE left_4_bits = *bp >> 4;

    switch (left_4_bits) {
    case 0: {
        if (*bp != 0) {
            //
            // only top 4 bits 0, could be ASCII control character (including
            // line feed...
            //
            return GUESSED_AS_UTF8;
        }

        // All 0 bits in first byte would either be an empty UTF-8 string or
        // a *freed* series.  We will guess freed series in the case that the
        // Reb_Header interpretation comes up with all 0 bits.
        //
        // Since this read of the header can crash the system on a valid
        // empty string (if not legal to read bytes after terminator), this
        // guess is only done in the debug build.
        //
    #if !defined(NDEBUG)
        const struct Reb_Header *h = cast(const struct Reb_Header*, p);
        if (h->bits == 0)
            return GUESSED_AS_FREED_SERIES;
    #endif

        return GUESSED_AS_UTF8;
    }

    case 1:
    case 2:
    case 3:
        return GUESSED_AS_UTF8; // slightly higher ASCII codepoints

    case 4:
    case 5:
        // NODE_FLAG_END (0x4) is set, but other bits aren't set...including
        // NODE_FLAG_CELL (0x2).  This *could* be an internal END marker, as
        // opposed to a UTF-8 string.  A debug check might try doing a UTF-8
        // decode, and if it fails, it's probably an internal END.
        //
        return GUESSED_AS_UTF8;

    case 6:
    case 7:
        return GUESSED_AS_UTF8; // topmost ASCII codepoints

    // v-- bit sequences starting with `10` (continuation bytes, so not
    // valid starting points for a UTF-8 string)

    case 8:
        return GUESSED_AS_SERIES; // not free, not cell, not managed

    case 9:
        return GUESSED_AS_SERIES; // not free, not cell, managed

    case 10:
        return GUESSED_AS_VALUE; // not free, cell, not managed

    case 11:
        return GUESSED_AS_VALUE; // not free, cell, managed (pairing key)

    // v-- bit sequences starting with `11` are usually legal multi-byte
    // valid starting points, with a few exceptions.

    case 12:
    case 13:
    case 14:
        return GUESSED_AS_UTF8;

    case 15:
        if (*bp == 255)
            return GUESSED_AS_END;

        return GUESSED_AS_UTF8;
    }

    DEAD_END;
}



// Size of crash buffers
#define PANIC_TITLE_BUF_SIZE 80
#define PANIC_BUF_SIZE 512


//
//  Panic_Core: C
//
// See comments on `panic (...)` macro, which calls this routine.
//
ATTRIBUTE_NO_RETURN void Panic_Core(
    const void *p, // REBSER* (array, context, etc), REBVAL*, or UTF-8 char*
    const char* file,
    int line
) {
    if (p == NULL)
        p = "panic (...) was passed NULL"; // avoid later NULL tests

    // We are crashing, so a legitimate time to be disabling the garbage
    // collector.  (It won't be turned back on.)
    //
    GC_Disabled = TRUE;

#if !defined(NDEBUG)
    //
    // Generally Rebol does not #include <stdio.h>, but the debug build does.
    // It's often used for debug spew--as opposed to Debug_Fmt()--when there
    // is a danger of causing recursive errors if the problem is being caused
    // by I/O in the first place.  So flush anything lingering in the
    // standard output or error buffers
    //
    fflush(stdout);
    fflush(stderr);
#endif

    // Because the release build of Rebol does not link to printf or its
    // support functions, the crash buf is assembled into a buffer for
    // raw output through the host.
    //
    char title[PANIC_TITLE_BUF_SIZE + 1]; // account for null terminator
    char buf[PANIC_BUF_SIZE + 1]; // "

    title[0] = '\0';
    buf[0] = '\0';

#if !defined(NDEBUG)
    if (Reb_Opts && Reb_Opts->crash_dump) {
        Dump_Info();
        Dump_Stack(NULL, 0);
    }
#endif

    strncat(title, "PANIC()", PANIC_TITLE_BUF_SIZE - 0);

    strncat(buf, Str_Panic_Directions, PANIC_BUF_SIZE - 0);

    strncat(buf, "C Source File ", PANIC_BUF_SIZE - strlen(buf));
    strncat(buf, file, PANIC_BUF_SIZE - strlen(buf));
    strncat(buf, ", Line ", PANIC_BUF_SIZE - strlen(buf));
    Form_Int(b_cast(buf + strlen(buf)), line); // !!! no bounding...
    strncat(buf, "\n", PANIC_BUF_SIZE - strlen(buf));

    switch (Guess_Rebol_Pointer(p)) {
    case GUESSED_AS_UTF8:
        strncat(
            buf,
            cast(const char*, p),
            PANIC_BUF_SIZE - strlen(buf)
        );
        break;

    case GUESSED_AS_SERIES: {
        REBSER *s = m_cast(REBSER*, cast(const REBSER*, p)); // don't mutate
    #if !defined(NDEBUG)
        Panic_Series_Debug(cast(REBSER*, s));
    #else
        strncat(buf, "valid series", PANIC_BUF_SIZE - strlen(buf));
    #endif
        break; }

    case GUESSED_AS_FREED_SERIES: {
        REBSER *s = m_cast(REBSER*, cast(const REBSER*, p)); // don't mutate
    #if !defined(NDEBUG)
        Panic_Series_Debug(s);
    #else
        strncat(buf, "freed series", PANIC_BUF_SIZE - strlen(buf));
    #endif
        break; }

    case GUESSED_AS_VALUE:
    #if !defined(NDEBUG)
        Panic_Value_Debug(cast(const REBVAL*, p));
    #else
        strncat(buf, "value", PANIC_BUF_SIZE - strlen(buf));
    #endif
        break;

    case GUESSED_AS_END:
    #if !defined(NDEBUG)
        Panic_Value_Debug(cast(const REBVAL*, p));
    #else
        strncat(buf, "end marker", PANIC_BUF_SIZE - strlen(buf));
    #endif
        break;

    default:
        strncat(buf,
            "Guess_Rebol_Pointer() failure",
            PANIC_BUF_SIZE - strlen(buf)
        );
        break;
    }


#if !defined(NDEBUG)
    //
    // In a debug build, we'd like to try and cause a break so as not to lose
    // the state of the panic, which would happen if we called out to the
    // host kit's exit routine...
    //
    printf("%s\n", Str_Panic_Title);
    printf("%s\n", buf);
    fflush(stdout);
    debug_break(); // see %debug_break.h
#endif

    OS_CRASH(cb_cast(Str_Panic_Title), cb_cast(buf));

    // Note that since we crash, we never return so that the caller can run
    // a va_end on the passed-in args.  This is illegal in the general case:
    //
    //    http://stackoverflow.com/a/587139/211160

    DEAD_END;
}
