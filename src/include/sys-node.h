//
//  File: %sys-node.h
//  Summary: {Convenience routines for the Node "superclass" structure}
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
// This provides some convenience routines that require more definitions than
// are available when %sys-rebnod.h is being processed.  (e.g. REBVAL, 
// REBSER, REBFRM...)
//
// See %sys-rebnod.h for what a "node" means in this context.
//

// NOD(p) gives REBNOD* from a pointer to another type, with optional checking
//
#ifdef DEBUG_CHECK_CASTS
    inline static REBNOD *NOD(void *p) {
        assert(p != NULL);

        REBNOD *node = cast(REBNOD*, p);
        assert(
            (node->header.bits & NODE_FLAG_NODE)
            && NOT(node->header.bits & NODE_FLAG_FREE)
        );
        return node;
    }
#else
    #define NOD(p) \
        cast(REBNOD*, (p))
#endif


#ifdef NDEBUG
    inline static REBOOL IS_CELL(REBNOD *node) {
        return LOGICAL(node->header.bits & NODE_FLAG_CELL);
    }

    inline static REBOOL NOT_CELL(REBNOD *node) {
        return NOT(node->header.bits & NODE_FLAG_CELL);
    }
#else
    // We want to get a compile-time check on whether the argument is a
    // REBNOD (and not, say, a REBSER or REBVAL).  But we don't want to pay
    // for the function call in debug builds, so only check in release builds.
    //
    #define IS_CELL(node) \
        LOGICAL((node)->header.bits & NODE_FLAG_CELL)

    #define NOT_CELL(node) \
        NOT((node)->header.bits & NODE_FLAG_CELL)
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
// POINTER DETECTION (UTF-8, SERIES, FREED SERIES, END...)
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Rebol's "nodes" all have a platform-pointer-sized header of bits, which
// is constructed using byte-order-sensitive bit flags (see FLAGIT_LEFT and
// related definitions).
//
// The values for the bits were chosen carefully, so that the leading byte of
// Rebol structures could be distinguished from the leading byte of a UTF-8
// string.  This is taken advantage of in the API.
//
// During startup, Assert_Pointer_Detection_Working() checks that:
//
//     LEFT_8_BITS(NODE_FLAG_CELL) == 0x1
//     LEFT_8_BITS(NODE_FLAG_END) == 0x8
//

enum Reb_Pointer_Detect {
    DETECTED_AS_UTF8 = 0,
    
    DETECTED_AS_SERIES = 1,
    DETECTED_AS_FREED_SERIES = 2,

    DETECTED_AS_VALUE = 3,
    DETECTED_AS_END = 4, // may be a cell, or made with Init_Endlike_Header()
    DETECTED_AS_TRASH_CELL = 5
};

inline static enum Reb_Pointer_Detect Detect_Rebol_Pointer(const void *p) {
    REBYTE bp = *cast(const REBYTE*, p);

    switch (bp >> 4) { // switch on the left 4 bits of the byte
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        return DETECTED_AS_UTF8; // ASCII codepoints 0 - 127

    // v-- bit sequences starting with `10` (continuation bytes, so not
    // valid starting points for a UTF-8 string)

    case 8: // 0xb1000
        if (bp & 0x8)
            return DETECTED_AS_END; // may be end cell or "endlike" header
        if (bp & 0x1)
            return DETECTED_AS_VALUE; // unmanaged
        return DETECTED_AS_SERIES; // unmanaged

    case 9: // 0xb1001
        if (bp & 0x8)
            return DETECTED_AS_END; // has to be an "endlike" header
        panic (p); // would be "marked and unmanaged", not legal

    case 10: // 0b1010
    case 11: // 0b1011
        if (bp & 0x8)
            return DETECTED_AS_END;
        if (bp & 0x1)
            return DETECTED_AS_VALUE; // managed, marked if `case 11`
        return DETECTED_AS_SERIES; // managed, marked if `case 11`

    // v-- bit sequences starting with `11` are *usually* legal multi-byte
    // valid starting points for UTF-8, with only the exceptions made for
    // the illegal 192 and 193 bytes which represent freed series and trash.

    case 12: // 0b1100
        if (bp == FREED_SERIES_BYTE)
            return DETECTED_AS_FREED_SERIES;

        if (bp == TRASH_CELL_BYTE)
            return DETECTED_AS_TRASH_CELL;

        return DETECTED_AS_UTF8;

    case 13: // 0b1101
    case 14: // 0b1110
    case 15: // 0b1111
        return DETECTED_AS_UTF8;
    }

    DEAD_END;
}
