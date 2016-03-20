%src/core/a-constants.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "a-constants.c"
        Summary "special global constants and strings"
        Section "environment"
        Author "Carl Sassenrath"
        Notes {Very few strings should be located here. Most strings are
put in the compressed embedded boot image. That saves space,
reduces tampering, and allows UTF8 encoding. See ../boot dir.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/a-globals.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "a-globals.c"
        Summary "global variables"
        Section "environment"
        Author "Carl Sassenrath"
        Notes {There are two types of global variables:
  process vars - single instance for main process
  thread vars - duplicated within each R3 task}
    ]
    msg none
    rest none
    analysis none
]
%src/core/a-lib.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "a-lib.c"
        Summary "exported REBOL library functions"
        Section "environment"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/a-stubs.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "a-stubs.c"
        Summary "function stubs"
        Section "environment"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/b-boot.c {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Natives and Bootstrap
// Build: A0
// Date:  20-Mar-2016
// File:  b-boot.c
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/core/b-init.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "b-init.c"
        Summary "initialization functions"
        Section "bootstrap"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/c-bind.c {// Rebol 3 Language Interpreter and Run-time Environment
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
}
%src/core/c-do.c {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: DO Evaluator Wrappers
//  File: %c-do.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These are the "slightly more user-friendly" interfaces to the evaluator
// from %c-eval.c.  These routines will do the setup of the Reb_Frame state
// for you.
//
// Even "friendlier" interfaces are available as macros on top of these.
// See %sys-do.h for DO_VAL_ARRAY_AT_THROWS() and similar macros.
//
}
%src/core/c-error.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "c-error.c"
        Summary "error handling"
        Section "core"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/c-eval.c {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: Central Interpreter Evaluator
//  File: %c-do.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This file contains `Do_Core()`, which is the central evaluator which
// is behind DO.  It can execute single evaluation steps (e.g. a DO/NEXT)
// or it can run the array to the end of its content.  A flag controls that
// behavior, and there are other flags for controlling its other behaviors.
//
// For comprehensive notes on the input parameters, output parameters, and
// internal state variables...see %sys-do.h and `struct Reb_Frame`.
//
// NOTES:
//
// * This is a very long routine.  That is largely on purpose, because it
//   doesn't contain repeated portions.  If it were broken into functions that
//   would add overhead for little benefit, and prevent interesting tricks
//   and optimizations.  Note that it is broken down into sections, and
//   the invariants in each section are made clear with comments and asserts.
//
// * The evaluator only moves forward, and it consumes exactly one element
//   from the input at a time.  This input may be a source where the index
//   needs to be tracked and care taken to contain the index within its
//   boundaries in the face of change (e.g. a mutable ARRAY).  Or it may be
//   an entity which tracks its own position on each fetch, where "indexor"
//   is serving as a flag and should be left static.
//
// !!! There is currently no "locking" or other protection on the arrays that
// are in the call stack and executing.  Each iteration must be prepared for
// the case that the array has been modified out from under it.  The code
// evaluator will not crash, but re-fetches...ending the evaluation if the
// array has been shortened to before the index, and using possibly new
// values.  The benefits of this self-modifying lenience should be reviewed
// to inform a decision regarding the locking of arrays during evaluation.
//
}
%src/core/c-frame.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "c-frame.c"
        Summary "frame management"
        Section "core"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/c-function.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "c-function.c"
        Summary "support for functions, actions, and routines"
        Section "core"
        Author "Carl Sassenrath, Shixin Zeng"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/c-path.c {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: Core Path Dispatching and Chaining
//  File: %c-path.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// When a path like `a/(b + c)/d` is evaluated, it moves in steps.  The
// evaluative result of chaining the prior steps is offered as input to
// the next step.  The path evaluator `Do_Path_Throws` delegates steps to
// type-specific "(P)ath (D)ispatchers" with names like PD_Context,
// PD_Array, etc.
//
// R3-Alpha left several open questions about the handling of paths.  One
// of the trickiest regards the mechanics of how to use a SET-PATH! to
// write data into native structures when more than one path step is
// required.  For instance:
//
//     >> gob/size
//     == 10x20
//
//     >> gob/size/x: 304
//     >> gob/size
//     == 10x304
//
// Because GOB! stores its size as packed bits that are not a full PAIR!,
// the `gob/size` path dispatch can't give back a pointer to a REBVAL* to
// which later writes will update the GOB!.  It can only give back a
// temporary value built from its internal bits.  So workarounds are needed,
// as they are for a similar situation in trying to set values inside of
// C arrays in STRUCT!.
//
// The way the workaround works involves allowing a SET-PATH! to run forward
// and write into a temporary value.  Then in these cases the temporary
// REBVAL is observed and used to write back into the native bits before the
// SET-PATH! evaluation finishes.  This means that it's not currently
// prohibited for the effect of a SET-PATH! to be writing into a temporary.
//
// Further, the `value` slot is writable...even when it is inside of the path
// that is being dispatched:
//
//     >> code: compose [(make set-path! [12-Dec-2012 day]) 1]
//     == [12-Dec-2012/day: 1]
//
//     >> do code
//
//     >> probe code
//     [1-Dec-2012/day: 1]
//
// Ren-C has largely punted on resolving these particular questions in order
// to look at "more interesting" ones.  However, names and functions have
// been updated during investigation of what was being done.
//
}
%src/core/c-port.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "c-port.c"
        Summary "support for I/O ports"
        Section "core"
        Author "Carl Sassenrath"
        Notes {See comments in Init_Ports for startup.
See www.rebol.net/wiki/Event_System for full details.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/c-signal.c {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: Evaluator Interrupt Signal Handling
//  File: %c-signal.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// "Signal" refers to special events to process periodically during
// evaluation. Search for SET_SIGNAL to find them.
//
// (Note: Not to be confused with SIGINT and unix "signals", although on
// unix an evaluator signal can be triggered by a unix signal.)
//
}
%src/core/c-task.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "c-task.c"
        Summary "sub-task support"
        Section "core"
        Author "Carl Sassenrath"
        Notes "INCOMPLETE IMPLEMENTATION (partially operational)"
    ]
    msg none
    rest none
    analysis none
]
%src/core/c-value.c {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2016 Rebol Open Source Contributors
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
//  Summary: Generic REBVAL Support Services and Debug Routines
//  File: %c-value.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These are routines to support the macros and definitions in %sys-value.h
// which are not specific to any given type.  For the type-specific code,
// see files with names like %t-word.c, %t-logic.c, %t-integer.c...
//
}
%src/core/c-word.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "c-word.c"
        Summary "symbol table and word related functions"
        Section "core"
        Author "Carl Sassenrath"
        Notes {Word table is a block composed of symbols, each of which contain
a canon word number, alias word number (if it exists), and an
index that refers to the string for the text itself.

The canon number for a word is unique and is used to compare
words. The word table is independent of context frames and
words are never garbage collected.

The alias is used mainly for upper and lower case equality,
but can also be used to create ALIASes.

The word strings are stored as a single large string series.
NEVER CACHE A WORD NAME POINTER if new words may be added (e.g.
LOAD), because the series may get moved in memory.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/d-crash.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "d-crash.c"
        Summary "low level crash output"
        Section "debug"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/d-dump.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "d-dump.c"
        Summary "various debug output functions"
        Section "debug"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/d-legacy.c {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2016 Rebol Open Source Contributors
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
//  Summary: Legacy Support Routines for Debug Builds
//  File: %d-legacy.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// In order to make porting code from R3-Alpha or Rebol2 easier, Ren-C set
// up several LEGACY() switches and a <r3-legacy> mode.  The switches are
// intended to only be available in debug builds, so that compatibility for
// legacy code will not be a runtime cost in the release build.  However,
// they could be enabled by any sufficiently motivated individual who
// wished to build a version of the interpreter with the old choices in an
// optimized build as well.
//
// Support routines for legacy mode are quarantined here when possible.
//
}
%src/core/d-print.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "d-print.c"
        Summary "low-level console print interface"
        Section "debug"
        Author "Carl Sassenrath"
        Notes {R3 is intended to run on fairly minimal devices, so this code may
duplicate functions found in a typical C lib. That's why output
never uses standard clib printf functions.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/d-trace.c {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: Tracing Debug Routines
//  File: %d-trace.c
//
//=////////////////////////////////////////////////////////////////////////=//
//
// TRACE is functionality that was in R3-Alpha for doing low-level tracing.
// It could be turned on with `trace on` and off with `trace off`.  While
// it was on, it would print out information about the current execution step.
//
// Ren-C's goal is to have a fully-featured debugger that should allow a
// TRACE-like facility to be written and customized by the user.  They would
// be able to get access on each step to the call frame, and control the
// evaluator from within.
//
// A lower-level trace facility may still be interesting even then, for
// "debugging the debugger".  Either way, the routines have been extracted
// from %c-do.c in order to reduce the total length of that very long file.
//
}
%src/core/f-blocks.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-blocks.c"
        Summary "primary block series support functions"
        Section "functional"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-deci.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-deci.c"
        Summary "extended precision arithmetic functions"
        Section "functional"
        Author "Ladislav Mecir for REBOL Technologies"
        Notes {Deci significands are 87-bit long, unsigned, unnormalized, stored in
little endian order. (Maximal deci significand is 1e26 - 1, i.e. 26
nines)

Sign is one-bit, 1 means nonpositive, 0 means nonnegative.

Exponent is 8-bit, unbiased.

64-bit and/or double arithmetic used where they bring advantage.

!!! Inlining was once hinted here, and it may be possible to use
the hint to speed up this code.  But for the moment, inlining
decisions are being left up to the compiler due to it not being
a standard feature in C89 and numerous quirks in both C and C++
regarding how inline works.  A broader review of inline for
the whole codebase is required at some later date. --@HF}
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-dtoa.c none
%src/core/f-enbase.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-enbase.c"
        Summary "base representation conversions"
        Section "functional"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-extension.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-extension.c"
        Summary "support for extensions"
        Section "functional"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-int.c [
    title r3
    rights [
        "Copyright 2014 Atronix Engineering, Inc"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-int.c"
        Summary "integer arithmetic functions"
        Section "functional"
        Author "Shixin Zeng"
        Notes "Based on original code in t-integer.c"
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/core/f-math.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-math.c"
        Summary "basic math conversions"
        Section "functional"
        Author "Carl Sassenrath, Ladislav Mecir"
        Notes {Do not underestimate what it takes to make some parts of this
portable over all systems. Modifications to this code should be
tested on multiple operating system runtime libraries, including
older/obsolete systems.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-modify.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-modify.c"
        Summary "block series modification (insert, append, change)"
        Section "functional"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-qsort.c none
%src/core/f-random.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-random.c"
        Summary "random number generation"
        Section "functional"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-round.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-round.c"
        Summary "special rounding math functions"
        Section "functional"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-series.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-series.c"
        Summary "common series handling functions"
        Section "functional"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/f-stubs.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "f-stubs.c"
        Summary "miscellaneous little functions"
        Section "functional"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/l-scan.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "l-scan.c"
        Summary "lexical analyzer for source to binary translation"
        Section "lexical"
        Author "Carl Sassenrath"
        Notes {WARNING WARNING WARNING
This is highly tuned code that should only be modified by experts
who fully understand its design. It is very easy to create odd
side effects so please be careful and extensively test all changes!}
    ]
    msg none
    rest none
    analysis none
]
%src/core/l-types.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "l-types.c"
        Summary "special lexical type converters"
        Section "lexical"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/m-gc.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "m-gc.c"
        Summary "main memory garbage collection"
        Section "memory"
        Author "Carl Sassenrath, Ladislav Mecir, HostileFork"
        Notes {The garbage collector is based on a conventional "mark and sweep":

    https://en.wikipedia.org/wiki/Tracing_garbage_collection

From an optimization perspective, there is an attempt to not incur
function call overhead just to check if a GC-aware item has its
SERIES_FLAG_MARK flag set.  So the flag is checked by a macro before making
any calls to process the references inside of an item.

"Shallow" marking only requires setting the flag, and is suitable for
series like strings (which are not containers for other REBVALs).  In
debug builds shallow marking is done with a function anyway, to give
a place to put assertion code or set breakpoints to catch when a
shallow mark is set (when that is needed).

"Deep" marking was originally done with recursion, and the recursion
would stop whenever a mark was hit.  But this meant deeply nested
structures could quickly wind up overflowing the C stack.  Consider:

    a: copy []
    loop 200'000 [a: append/only copy [] a]
    recycle

The simple solution is that when an unmarked item is hit that it is
marked and put into a queue for processing (instead of recursed on the
spot.  This queue is then handled as soon as the marking stack is
exited, and the process repeated until no more items are queued.

Regarding the two stages:

MARK -  Mark all series and gobs ("collectible values")
        that can be found in:

        Root Block: special structures and buffers
        Task Block: special structures and buffers per task
        Data Stack: current state of evaluation
        Safe Series: saves the last N allocations

SWEEP - Free all collectible values that were not marked.

GC protection methods:

KEEP flag - protects an individual series from GC, but
    does not protect its contents (if it holds values).
    Reserved for non-block system series.

Root_Vars - protects all series listed. This list is
    used by Sweep as the root of the in-use memory tree.
    Reserved for important system series only.

Task_Vars - protects all series listed. This list is
    the same as Root, but per the current task context.

Save_Series - protects temporary series. Used with the
    SAVE_SERIES and UNSAVE_SERIES macros. Throws and errors
    must roll back this series to avoid "stuck" memory.

Safe_Series - protects last MAX_SAFE_SERIES series from GC.
    Can only be used if no deeply allocating functions are
    called within the scope of its protection. Not affected
    by throws and errors.

Data_Stack - all values in the data stack that are below
    the TOP (DSP) are automatically protected. This is a
    common protection method used by native functions.

DONE flag - do not scan the series; it has no links.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/m-pools.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "m-pools.c"
        Summary "memory allocation pool management"
        Section "memory"
        Author "Carl Sassenrath"
        Notes {A point of Rebol's design was to remain small and solve its
problems without relying on a lot of abstraction.  Its
memory-management was thus focused on staying low-level...and
being able to do efficient and lightweight allocations of
two major elements: series and graphic objects (GOBs).

Both series and GOBs have a fixed-size component that can
be easily allocated from a memory pool.  This portion is
called the "Node" (or NOD) in both Rebol and Red terminology;
it is an item whose pointer is valid for the lifetime of
the object, regardless of resizing.  This is where header
information is stored, and pointers to these objects may
be saved in REBVAL values; such that they are kept alive
by the garbage collector.

The more complicated thing to do memory pooling of is the
variable-sized portion of a series (currently called the
"series data")...as series sizes can vary widely.  But a
trick Rebol has is that a series might be able to take
advantage of being given back an allocation larger than
requested.  They can use it as reserved space for growth.

(Typical models for implementation of things like C++'s
std::vector do not reach below new[] or delete[]...which
are generally implemented with malloc and free under
the hood.  Their buffered additional capacity is done
assuming the allocation they get is as big as they asked
for...no more and no less.)

While Rebol's memory pooling is a likely-useful tool even
with modern alternatives, there are also useful tools
like Valgrind and Address Sanitizer which can more easily
root out bugs if each allocation and free is done
separately through malloc and free.  Therefore there is
an option for always using malloc, which you can enable
by setting the environment variable R3_ALWAYS_MALLOC to 1.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/m-series.c [
    title {REBOL Language Interpreter and Run-time Environment}
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "m-series.c"
        Summary "implements REBOL's series concept"
        Section "memory"
        Author "Carl Sassenrath"
    ]
    msg none
    rest none
    analysis [
        non-standard-title
    ]
]
%src/core/m-stacks.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "m-stack.c"
        Summary "data and function call stack implementation"
        Section "memory"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-control.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-control.c"
        Summary "native functions for control flow"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-data.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-data.c"
        Summary "native functions for data and context"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-io.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-io.c"
        Summary "native functions for input and output"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-loop.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-loop.c"
        Summary "native functions for loops"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-math.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-math.c"
        Summary "native functions for math"
        Section "natives"
        Author "Carl Sassenrath"
        Notes "See also: the numeric datatypes"
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-reduce.c {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: REDUCE and COMPOSE natives and associated service routines
//  File: %n-reduce.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! The R3-Alpha REDUCE routine contained several auxiliariy refinements
// used by fringe dialects.  These need review for whether they are still in
// working order--or if they need to just be replaced or removed.
//
}
%src/core/n-sets.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-sets.c"
        Summary "native functions for data sets"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-strings.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-strings.c"
        Summary "native functions for strings"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/n-system.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "n-system.c"
        Summary "native functions for system operations"
        Section "natives"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-clipboard.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-clipboard.c"
        Summary "clipboard port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-console.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-console.c"
        Summary "console port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-dir.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-dir.c"
        Summary "file directory port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-dns.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-dns.c"
        Summary "DNS port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-event.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-event.c"
        Summary "event port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-file.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-file.c"
        Summary "file port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-net.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-net.c"
        Summary "network port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-serial.c [
    title r3
    rights [
        "Copyright 2013 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-serial.c"
        Summary "serial port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-signal.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
        "Copyright 2014 Atronix Engineering, Inc."
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-signal.c"
        Summary "signal port interface"
        Section "ports"
        Author "Shixin Zeng"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/p-timer.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "p-timer.c"
        Summary "timer port interface"
        Section "ports"
        Author "Carl Sassenrath"
        Notes "NOT IMPLEMENTED"
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-cases.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-cases.c"
        Summary "unicode string case handling"
        Section "strings"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-crc.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-crc.c"
        Summary "CRC computation"
        Section "strings"
        Author "Carl Sassenrath (REBOL interface sections)"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-file.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-file.c"
        Summary "file and path string handling"
        Section "strings"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-find.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-find.c"
        Summary "string search and comparison"
        Section "strings"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-make.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-make.c"
        Summary "binary and unicode string support"
        Section "strings"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-mold.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-mold.c"
        Summary "value to string conversion"
        Section "strings"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-ops.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-ops.c"
        Summary "string handling utilities"
        Section "strings"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-trim.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-trim.c"
        Summary "string trimming"
        Section "strings"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/s-unicode.c [
    title r3
    rights none
    trademark rebol
    notice apache-2.0
    meta [
        Module "s-unicode.c"
        Summary "unicode support functions"
        Section "strings"
        Author "Carl Sassenrath"
        Notes {The top part of this code is from Unicode Inc. The second
part was added by REBOL Technologies.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-bitset.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-bitset.c"
        Summary "bitset datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-block.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-block.c"
        Summary "block related datatypes"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-char.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-char.c"
        Summary "character datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-datatype.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-datatype.c"
        Summary "datatype datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-date.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-date.c"
        Summary "date datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes {Date and time are stored in UTC format with an optional timezone.
The zone must be added when a date is exported or imported, but not
when date computations are performed.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-decimal.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-decimal.c"
        Summary "decimal datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-event.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-event.c"
        Summary "event datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes {Events are kept compact in order to fit into normal 128 bit
values cells. This provides high performance for high frequency
events and also good memory efficiency using standard series.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-function.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-function.c"
        Summary "function related datatypes"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-gob.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-gob.c"
        Summary "graphical object datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-image.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-image.c"
        Summary "image datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-integer.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-integer.c"
        Summary "integer datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-library.c [
    title r3
    rights [
        "Copyright 2014 Atronix Engineering, Inc."
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-library.c"
        Summary "External Library Support"
        Section "datatypes"
        Author "Shixin Zeng"
        Notes ""
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/core/t-logic.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-logic.c"
        Summary "logic datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-map.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-map.c"
        Summary "map datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-money.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-money.c"
        Summary "extended precision datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-none.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-none.c"
        Summary "none datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-object.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-object.c"
        Summary "object datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-pair.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-pair.c"
        Summary "pair datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-port.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-port.c"
        Summary "port datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-routine.c [
    title r3
    rights [
        "Copyright 2014 Atronix Engineering, Inc."
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-routine.c"
        Summary "External Routine Support"
        Section "datatypes"
        Author "Shixin Zeng"
        Notes {When Rebol3 was open-sourced in 12-Dec-2012, that version had lost
support for the ROUTINE! type from Rebol2.  It was later
reimplemented by Atronix in their fork via the cross-platform (and
popularly used) Foreign Function Interface library "libffi":

    https://en.wikipedia.org/wiki/Libffi

Yet Rebol is very conservative about library dependencies that
introduce their "own build step", due to the complexity introduced.
If one is to build libffi for a particular platform, that requires
having the rather messy GNU autotools installed.  Notice the
`Makefile.am`, `acinclude.m4`, `autogen.sh`, `configure.ac`,
`configure.host`, etc:

    https://github.com/atgreen/libffi

Suddenly, you need more than just a C compiler (and a rebol.exe) to
build Rebol.  You now need to have everything to configure and
build libffi.  -OR- it would mean a dependency on a built library
you had to find or get somewhere that was not part of the OS
naturally, which can be a wild goose chase with version
incompatibility.  If you `sudo apt-get libffi`, now you need apt-get
*and* you pull down any dependencies as well!

(Note: Rebol's "just say no" attitude is the heart of the Rebellion:

    http://www.rebol.com/cgi-bin/blog.r?view=0497

...so keeping the core true to this principle is critical.  If this
principle is compromised, the whole point of the project is lost.)

Yet Rebol2 had ROUTINE!.  Red also has ROUTINE!, and is hinging its
story for rapid interoperability on it (you should not have to
wrap and recompile a DLL of C functions just to call them).  Users
want the feature and always ask...and Atronix needs it enough to have
had @ShixinZeng write it!

Regarding the choice of libffi in particular, it's a strong sign to
notice how many other language projects are using it.  Short list
taken from 2015 Wikipedia:

    Python, Haskell, Dalvik, F-Script, PyPy, PyObjC, RubyCocoa,
    JRuby, Rubinius, MacRuby, gcj, GNU Smalltalk, IcedTea, Cycript,
    Pawn, Squeak, Java Native Access, Common Lisp, Racket,
    Embeddable Common Lisp and Mozilla.

Rebol could roll its own implementation.  But that takes time and
maintenance, and it's hard to imagine how much better a job could
be done for a C-based foreign function interface on these platforms;
it's light and quite small once built.  So it makes sense to
"extract" libffi's code out of its repo to form one .h and .c file.
They'd live in the Rebol sources and build with the existing process,
with no need for GNU Autotools (which are *particularly* crufty!!!)

Doing such extractions by hand is how Rebol was originally done;
that made it hard to merge updates.  As a more future-proof method,
@HostileFork wrote a make-zlib.r extractor that can take a copy of
the zlib repository and do the work (mostly) automatically.  Going
forward it seems prudent to do the same with libffi and any other
libraries that Rebol co-opts into its turnkey build process.

Until that happens for libffi, not definining HAVE_LIBFFI_AVAILABLE,
will give you a short list of non-functional "stubs".  These can
allow t-routine.c to compile anyway.  That assists with maintenance
of the code and keeping it on the radar, even among those doing core
maintenance who are not building against the FFI.

(Note: Longer term there may be a story by which a feature like
ROUTINE! could be implemented as a third party extension.  There is
short-term thinking trying to facilitate this for GOB! in Ren/C, to
try and open the doors to more type extensions.  That's a hard
problem in itself...and the needs of ROUTINE! are hooked a bit more
tightly into the evaluation loop.  So possibly not happening.)}
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/core/t-string.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-string.c"
        Summary "string related datatypes"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-struct.c [
    title r3
    rights [
        "Copyright 2014 Atronix Engineering, Inc."
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-strut.c"
        Summary "C struct object datatype"
        Section "datatypes"
        Author "Shixin Zeng"
        Notes ""
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/core/t-time.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-time.c"
        Summary "time datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-tuple.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-tuple.c"
        Summary "tuple datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-typeset.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-typeset.c"
        Summary "typeset datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-utype.c {
This file should be GC'd during the next make-make disruption
(changing the files makes it more troublesome to switch around
branches, and is best done in a batch when there's an important
reason to do so.)

Also in that pending list: move linux's dev-serial.c to posix,
as it appears to not have anything linux-specific about it.
}
%src/core/t-varargs.c {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2016 Rebol Open Source Contributors
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
//  Summary: Variadic Argument Type and Services
//  File: %t-varargs.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Experimental design being incorporated for testing.  For working notes,
// see the Ren-C Trello:
//
// https://trello.com/c/Y17CEywN
//
// The VARARGS! data type implements an abstraction layer over a call frame
// or arbitrary array of values.  All copied instances of a REB_VARARG value
// remain in sync as values are TAKE-d out of them, and once they report
// reaching a TAIL? they will always report TAIL?...until the call that
// spawned them is off the stack, at which point they will report an error.
//
}
%src/core/t-vector.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-vector.c"
        Summary "vector datatype"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/t-word.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "t-word.c"
        Summary "word related datatypes"
        Section "datatypes"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-bmp.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "u-bmp.c"
        Summary "conversion to and from BMP graphics format"
        Section "utility"
        Notes {This is an optional part of R3. This file can be replaced by
library function calls into an updated implementation.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-compress.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "u-compress.c"
        Summary "interface to zlib compression"
        Section "utility"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-dialect.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "u-dialect.c"
        Summary "support for dialecting"
        Section "utility"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-gif.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "u-gif.c"
        Summary "GIF image format conversion"
        Section "utility"
        Notes {This is an optional part of R3. This file can be replaced by
library function calls into an updated implementation.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-jpg.c none
%src/core/u-md5.c none
%src/core/u-parse.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "u-parse.c"
        Summary "parse dialect interpreter"
        Section "utility"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-png.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Module "u-png.c"
        Summary "PNG image format conversion"
        Section "utility"
        Notes {This is an optional part of R3. This file can be replaced by
library function calls into an updated implementation.}
    ]
    msg none
    rest none
    analysis none
]
%src/core/u-sha1.c none
%src/core/u-zlib.c {
Extraction of ZLIB compression and decompression routines
for REBOL [R3] Language Interpreter and Run-time Environment
This is a code-generated file.

ZLIB Copyright notice:

  (C) 1995-2013 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

      Jean-loup Gailly        Mark Adler
      jloup@gzip.org          madler@alumni.caltech.edu

REBOL is a trademark of REBOL Technologies
Licensed under the Apache License, Version 2.0

**********************************************************************

Title: ZLIB aggregated source file
Build: A0
Date:  29-Sep-2013
File:  u-zlib.c

AUTO-GENERATED FILE - Do not modify. (From: make-zlib.r)
}
%src/include/debugbreak.h {// This allows for the programmatic triggering of debugger breaks from C
// code, using `debug_break()`.
//
// The file was obtained from:
//
//     https://github.com/scottt/debugbreak
//
// Supported platforms are listed as:
//
//     "gcc and Clang, works well on ARM, AArch64, i686, x86-64 and has
//      a fallback code path for other architectures."
//
// Ren-C modifications:
//
//     + integrates iOS ARM64 patch (an un-processed PR as of 21-Dec-2015)
//     + __inline__ moved to beginning of declarations (suppresses warning)
//     + tabs converted to spaces
//
}
%src/include/ext-types.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Extension Types (Isolators)
// Build: A0
// Date:  20-Mar-2016
// File:  ext-types.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/host-ext-core.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: REBOL Core extension
// Build: A0
// Date:  20-Mar-2016
// File:  host-ext-core
//
// AUTO-GENERATED FILE - Do not modify. (From: make-host-ext.r)
//
}
%src/include/host-lib.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Host Access Library
// Build: A0
// Date:  20-Mar-2016
// File:  host-lib.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-os-ext.r)
//
}
%src/include/mem-pools.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Memory allocation"
        Module "sys-mem.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/mem-series.h {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
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
//  Summary: Low level memory-oriented access routines for series
//  File: %mem-series.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These are implementation details of series that most code should not need
// to use.
//
}
%src/include/reb-args.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Program startup arguments"
        Module "reb-args.h"
        Author "Carl Sassenrath"
        Notes {Arg struct is used by R3 lib, so must not be modified.}
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-c.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "General C definitions and constants"
        Module "reb-c.h"
        Author "Carl Sassenrath, Ladislav Mecir"
        Notes {Various configuration defines (from reb-config.h):

HAS_LL_CONSTS - compiler allows 1234LL constants
WEIRD_INT_64 - old MSVC typedef for 64 bit int
OS_WIDE_CHAR - the OS uses wide chars (not UTF-8)}
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-codec.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "REBOL Codec Definitions"
        Module "reb-codec.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-config.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "General build configuration"
        Module "reb-config.h"
        Author "Carl Sassenrath"
        Notes {This is the first file included.  It is included by both
reb-host.h and sys-core.h, and all Rebol code can include
one (and only one) of those...based on whether the file is
part of the core or in the "host".

Many of the flags controlling the build (such as
the TO_<target> definitions) come from -DTO_<target> in the
compiler command-line.  These command lines are generally
produced automatically, based on the build that is picked
from %systems.r.

However, some flags require the preprocessor's help to
decide if they are relevant, for instance if they involve
detecting features of the compiler while it's running.
Or they may adjust a feature so narrowly that putting it
into the system configuration would seem unnecessary.

Over time, this file should be balanced and adjusted with
%systems.r in order to make the most convenient and clear
build process.  If there is difficulty in making a build
work on a system, use that as an opportunity to reflect
how to make this better.}
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-defs.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Miscellaneous structures and definitions"
        Module "reb-defs.h"
        Author "Carl Sassenrath"
        Notes {This file is used by internal and external C code. It
should not depend on many other header files prior to it.}
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-device.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "External REBOL Devices (OS Independent)"
        Module "reb-device.h"
        Author "Carl Sassenrath"
        Notes {Critical: all struct alignment must be 4 bytes (see compile options)}
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-dialect.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Dialects
// Build: A0
// Date:  20-Mar-2016
// File:  reb-dialect.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/reb-dtoa.h [
    title r3
    rights [
        "Copyright 2012 Saphirion AG"
    ]
    trademark none
    notice apache-2.0
    meta [
        Title "Settings for the f-dtoa.c file"
        Author "Ladislav Mecir"
    ]
    msg standard-programmer-note
    rest none
    analysis [
        no-rebol-technologies-copyright
        no-trademark
    ]
]
%src/include/reb-event.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "REBOL event definitions"
        Module "reb-event.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-evtypes.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Event Types
// Build: A0
// Date:  20-Mar-2016
// File:  reb-evtypes.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/reb-ext.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Extensions Include File"
        Module "reb-ext.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-file.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Special file device definitions"
        Module "reb-file.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-filereq.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "File requestor definitions"
        Module "reb-filereq.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-gob.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Graphical compositing objects"
        Module "reb-gob.h"
        Author "Carl Sassenrath"
        Description {GOBs are lower-level graphics object used by the compositing
and rendering system. Because a GUI can contain thousands of
GOBs, they are designed and structured to be simple and small.
Note that GOBs are also used for windowing.}
        Warning {GOBs are allocated from a special pool and
are accounted for by the standard garbage collector.}
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-host.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Include files for hosting"
        Module "reb-host.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-lib-lib.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: REBOL Host/Extension API
// Build: A0
// Date:  20-Mar-2016
// File:  reb-lib-lib.r
//
// AUTO-GENERATED FILE - Do not modify. (From: make-reb-lib.r)
//
}
%src/include/reb-lib.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: REBOL Host and Extension API
// Build: A0
// Date:  20-Mar-2016
// File:  reb-lib.r
//
// AUTO-GENERATED FILE - Do not modify. (From: make-reb-lib.r)
//
}
%src/include/reb-math.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Math related definitions"
        Module "reb-math.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-net.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Network device definitions"
        Module "reb-net.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/reb-struct.h [
    title r3
    rights [
        "Copyright 2014 Atronix Engineering, Inc."
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Struct to C function"
        Module "reb-struct.h"
        Author "Shixin Zeng"
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/include/reb-types.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Datatype Definitions
// Build: A0
// Date:  20-Mar-2016
// File:  reb-types.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/sys-core.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "System Core Include"
        Module "sys-core.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/sys-dec-to-char.h [
    title r3
    rights [
        "Copyright 2012 Saphirion AG"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Decimal conversion wrapper"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/include/sys-deci-funcs.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Deci Datatype Functions"
        Module "sys-deci-funcs.h"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/sys-deci.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Deci Datatype"
        Module "sys-deci.h"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/sys-do-cpp.h {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2016 Rebol Open Source Contributors
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
//  Summary: Optional C++ Checking Classes for %sys-do.h
//  File: %sys-do-cpp.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// These checking classes are only for debug builds that wish to use them.
// They are not used in release builds.
//
// See "Static-and-Dynamic-Analysis-in-the-Cpp-Build" on:
//
// https://github.com/metaeducation/ren-c/wiki/
//
}
%src/include/sys-do.h {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
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
//  Summary: Evaluator "Do State" and Helpers
//  File: %sys-do.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The primary routine that performs DO and DO/NEXT is called Do_Core().  It
// takes a single parameter which holds the running state of the evaluator.
// This state may be allocated on the C variable stack:  Do_Core() is
// written such that a longjmp up to a failure handler above it can run
// safely and clean up even though intermediate stacks have vanished.
//
// Ren-C can not only run the evaluator across a REBSER-style series of
// input based on index, it can also fetch those values from a standard C
// array of REBVAL[].  Alternately, it can enumerate through C's `va_list`,
// providing the ability to pass pointers as REBVAL* to comma-separated input
// at the source level.
//
// To provide even greater flexibility, it allows the very first element's
// pointer in an evaluation to come from an arbitrary source.  It doesn't
// have to be resident in the same sequence from which ensuing values are
// pulled, allowing a free head value (such as a FUNCTION! REBVAL in a local
// C variable) to be evaluated in combination from another source (like a
// va_list or series representing the arguments.)  This avoids the cost and
// complexity of allocating a series to combine the values together.
//
// These features alone would not cover the case when REBVAL pointers that
// are originating with C source were intended to be supplied to a function
// with no evaluation.  In R3-Alpha, the only way in an evaluative context
// to suppress such evaluations would be by adding elements (such as QUOTE).
// Besides the cost and labor of inserting these, the risk is that the
// intended functions to be called without evaluation, if they quoted
// arguments would then receive the QUOTE instead of the arguments.
//
// The problem was solved by adding a feature to the evaluator which was
// also opened up as a new privileged native called EVAL.  EVAL's refinements
// completely encompass evaluation possibilities in R3-Alpha, but it was also
// necessary to consider cases where a value was intended to be provided
// *without* evaluation.  This introduced EVAL/ONLY.
//
}
%src/include/sys-globals.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Program and Thread Globals"
        Module "sys-globals.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/sys-int-funcs.h [
    title r3
    rights [
        "Copyright 2014 Atronix Engineering, Inc."
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Integer Datatype Functions"
        Module "sys-int-funcs.h"
        Notes {To grok these routine names, consider unsigned multiplication:

umull is 'U MUL L' for unsigned multiplication of long
umulll is 'U MUL LL' for unsigned multiplication of long long

REBU64 may be an unsigned long long of equivalent size to
unsigned long, and similarly for REBI64 and long long.  But
the types may be incidentally the same size, if you turn up
warnings it will require a cast instead of silently passing
pointers of one to routines expecting a pointer to the other.
So we cast to the singularly-long variant before calling any
of the __builtin 'l' variants with a 64-bit REBU64 or REBI64.}
    ]
    msg none
    rest none
    analysis [
        no-rebol-technologies-copyright
    ]
]
%src/include/sys-jpg.h none
%src/include/sys-net.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "System network definitions"
        Module "sys-net.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/sys-scan.h [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Summary "Lexical Scanner Definitions"
        Module "sys-scan.h"
        Author "Carl Sassenrath"
        Notes ""
    ]
    msg none
    rest none
    analysis none
]
%src/include/sys-series.h {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
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
//  Summary: Definitions for Series (REBSER) plus Array, Frame, and Map
//  File: %sys-series.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Note: the word "Series" is overloaded in Rebol to refer to two related but
// distinct concepts:
//
// * The internal system datatype, also known as a REBSER.  It's a low-level
//   implementation of something similar to a vector or an array in other
//   languages.  It is an abstraction which represens a contiguous region
//   of memory containing equally-sized elements.
//
// * The user-level value type ANY-SERIES!.  This might be more accurately
//   called ITERATOR!, because it includes both a pointer to a REBSER of
//   data and an index offset into that data.  Attempts to reconcile all
//   the naming issues from historical Rebol have not yielded a satisfying
//   alternative, so the ambiguity has stuck.
//
// This file regards the first meaning of the word "series" and covers the
// low-level implementation details of a REBSER and its variants.  For info
// about the higher-level ANY-SERIES! value type and its embedded index,
// see %sys-value.h in the definition of `struct Reb_Any_Series`.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A REBSER is a contiguous-memory structure with an optimization of behaving
// like a kind of "double-ended queue".  It is able to reserve capacity at
// both the tail and the head, and when data is taken from the head it will
// retain that capacity...reusing it on later insertions at the head.
//
// The space at the head is called the "bias", and to save on pointer math
// per-access, the stored data pointer is actually adjusted to include the
// bias.  This biasing is backed out upon insertions at the head, and also
// must be subtracted completely to free the pointer using the address
// originally given by the allocator.
//
// The element size in a REBSER is known as the "width".  It is designed
// to support widths of elements up to 255 bytes.  (See note on SER_FREED
// about accomodating 256-byte elements.)
//
// REBSERs may be either manually memory managed or delegated to the garbage
// collector.  Free_Series() may only be called on manual series.  See
// MANAGE_SERIES() and PUSH_GUARD_SERIES() for remarks on how to work safely
// with pointers to garbage-collected series, to avoid having them be GC'd
// out from under the code while working with them.
//
// This file defines series subclasses which are type-incompatible with
// REBSER for safety.  (In C++ they would be derived classes, so common
// operations would not require casting...but this is C.)  The subclasses
// are explained where they are defined.
//
}
%src/include/sys-stack.h {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
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
//  Summary: Definitions for "Data Stack", "Chunk Stack" and the C stack
//  File: %sys-stack.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The data stack and chunk stack are two different data structures which
// are optimized for temporarily storing REBVALs and protecting them from
// garbage collection.  With the data stack, values are pushed one at a
// time...while with the chunk stack, an array of value cells of a given
// length is returned.
//
// A key difference between the two stacks is pointer stability.  Though the
// data stack can accept any number of pushes and then pop the last N pushes
// into a series, each push could potentially change the memory address of
// every value in the stack.  This is because the data stack uses a REBARR
// series as its implementation.  The chunk stack guarantees that the address
// of the values in a chunk will stay stable over the course of its lifetime.
//
// Because of their differences, they are applied to different problems:
//
// A notable usage of the data stack is by REDUCE and COMPOSE.  They use it
// as a buffer for values that are being gathered to be inserted into the
// final array.  It's better to use the data stack as a buffer because it
// means the precise size of the result can be known before either creating
// a new series or inserting /INTO a target.  This prevents wasting space on
// expansions or resizes and shuffling due to a guessed size.
//
// The chunk stack has an important use as the storage for arguments to
// functions being invoked.  The pointers to these arguments are passed by
// natives through the stack to other routines, which may take arbitrarily
// long to return...and may call code involving many pushes and pops.  These
// pointers must be stable, so using the data stack would not work.
//
}
%src/include/sys-state.h {// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
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
//  Summary: CPU and Interpreter State Snapshot/Restore
//  File: %sys-state.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Rebol is settled upon a stable and pervasive implementation baseline of
// ANSI-C (C89).  That commitment provides certain advantages.
//
// One of the *disadvantages* is that there is no safe way to do non-local
// jumps with stack unwinding (as in C++).  If you've written some code that
// performs a raw malloc and then wants to "throw" via a `longjmp()`, that
// will leak the malloc.
//
// In order to mitigate the inherent failure of trying to emulate stack
// unwinding via longjmp, the macros in this file provide an abstraction
// layer.  These allow Rebol to clean up after itself for some kinds of
// "dangling" state--such as manually memory managed series that have been
// made with Make_Series() but never passed to either Free_Series() or
// MANAGE_SERIES().  This covers several potential leaks known-to-Rebol,
// but custom interception code is needed for any generalized resource
// that might be leaked in the case of a longjmp().
//
// The triggering of the longjmp() is done via "fail", and it's important
// to know the distinction between a "fail" and a "throw".  In Rebol
// terminology, a `throw` is a cooperative concept, which does *not* use
// longjmp(), and instead must cleanly pipe the thrown value up through
// the OUT pointer that each function call writes into.  The `throw` will
// climb the stack until somewhere in the backtrace, one of the calls
// chooses to intercept the thrown value instead of pass it on.
//
// By contrast, a `fail` is non-local control that interrupts the stack,
// and can only be intercepted by points up the stack that have explicitly
// registered themselves interested.  So comparing these two bits of code:
//
//     catch [if 1 < 2 [trap [print ["Foo" (throw "Throwing")]]]]
//
//     trap [if 1 < 2 [catch [print ["Foo" (fail "Failing")]]]]
//
// In the first case, the THROW is offered to each point up the chain as
// a special sort of "return value" that only natives can examine.  The
// `print` will get a chance, the `trap` will get a chance, the `if` will
// get a chance...but only CATCH will take the opportunity.
//
// In the second case, the FAIL is implemented with longjmp().  So it
// doesn't make a return value...it never reaches the return.  It offers an
// ERROR! up the stack to native functions that have called PUSH_TRAP() in
// advance--as a way of registering interest in intercepting failures.  For
// IF or CATCH or PRINT to have an opportunity, they would need to be chang
// d to include a PUSH_TRAP() call.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// NOTE: If you are integrating with C++ and a longjmp crosses a constructed
// object, abandon all hope...UNLESS you use Ren-cpp.  It is careful to
// avoid this trap, and you don't want to redo that work.
//
//     http://stackoverflow.com/questions/1376085/
//
}
%src/include/sys-value.h {// Rebol 3 Language Interpreter and Run-time Environment
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
//  Summary: Definitions for the Rebol Value Struct (REBVAL) and Helpers
//  File: %sys-value.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// REBVAL is the structure/union for all Rebol values. It's designed to be
// four C pointers in size (so 16 bytes on 32-bit platforms and 32 bytes
// on 64-bit platforms).  Operation will be most efficient with those sizes,
// and there are checks on boot to ensure that `sizeof(REBVAL)` is the
// correct value for the platform.  But from a mechanical standpoint, the
// system should be *able* to work even if the size is different.
//
// Of the four 32-or-64-bit slots that each value has, the first is used for
// the value's "Header".  This includes the data type, such as REB_INTEGER,
// REB_BLOCK, REB_STRING, etc.  Then there are 8 flags which are for general
// purposes that could apply equally well to any type of value (including
// whether the value should have a new-line after it when molded out inside
// of a block).  There are 8 bits which are custom to each type--for
// instance whether a key in an object is hidden or not.  Then there are
// 8 bits currently reserved for future use.
//
// The remaining content of the REBVAL struct is the "Payload".  It is the
// size of three (void*) pointers, and is used to hold whatever bits that
// are needed for the value type to represent itself.  Perhaps obviously,
// an arbitrarily long string will not fit into 3*32 bits, or even 3*64 bits!
// You can fit the data for an INTEGER or DECIMAL in that (at least until
// they become arbitrary precision) but it's not enough for a generic BLOCK!
// or a FUNCTION! (for instance).  So those pointers are used to point to
// things, and often they will point to one or more Rebol Series (see
// %sys-series.h for an explanation of REBSER, REBARR, REBCTX, and REBMAP.)
//
// While some REBVALs are in C stack variables, most reside in the allocated
// memory block for a Rebol series.  The memory block for a series can be
// resized and require a reallocation, or it may become invalid if the
// containing series is garbage-collected.  This means that many pointers to
// REBVAL are unstable, and could become invalid if arbitrary user code
// is run...this includes values on the data stack, which is implemented as
// a series under the hood.  (See %sys-stack.h)
//
// A REBVAL in a C stack variable does not have to worry about its memory
// address becoming invalid--but by default the garbage collector does not
// know that value exists.  So while the address may be stable, any series
// it has in the payload might go bad.  Use PUSH_GUARD_VALUE() to protect a
// stack variable's payload, and then DROP_GUARD_VALUE() when the protection
// is not needed.  (You must always drop the last guard pushed.)
//
// For a means of creating a temporary array of GC-protected REBVALs, see
// the "chunk stack" in %sys-stack.h.  This is used when building function
// argument frames, which means that the REBVAL* arguments to a function
// accessed via ARG() will be stable as long as the function is running.
//
}
%src/include/sys-zlib.h {
Extraction of ZLIB compression and decompression routines
for REBOL [R3] Language Interpreter and Run-time Environment
This is a code-generated file.

ZLIB Copyright notice:

  (C) 1995-2013 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

      Jean-loup Gailly        Mark Adler
      jloup@gzip.org          madler@alumni.caltech.edu

REBOL is a trademark of REBOL Technologies
Licensed under the Apache License, Version 2.0

**********************************************************************

Title: ZLIB aggregated header file
Build: A0
Date:  29-Sep-2013
File:  sys-zlib.h

AUTO-GENERATED FILE - Do not modify. (From: make-zlib.r)
}
%src/include/tmp-boot.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Bootstrap Structure and Root Module
// Build: A0
// Date:  20-Mar-2016
// File:  boot.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-bootdefs.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Boot Definitions
// Build: A0
// Date:  20-Mar-2016
// File:  bootdefs.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-comptypes.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Datatype Comparison Functions
// Build: A0
// Date:  20-Mar-2016
// File:  comptypes.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-errnums.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Error Structure and Constants
// Build: A0
// Date:  20-Mar-2016
// File:  errnums.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-evaltypes.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Evaluation Maps
// Build: A0
// Date:  2-Feb-2016
// File:  evaltypes.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-exttypes.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Extension Type Equates
// Build: A0
// Date:  20-Mar-2016
// File:  tmp-exttypes.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-funcargs.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Function Argument Enums
// Build: A0
// Date:  20-Mar-2016
// File:  func-args.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-headers)
//
}
%src/include/tmp-funcs.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Function Prototypes
// Build: A0
// Date:  20-Mar-2016
// File:  funcs.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-headers)
//
}
%src/include/tmp-maketypes.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Datatype Makers
// Build: A0
// Date:  20-Mar-2016
// File:  maketypes.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-portmodes.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Port Modes
// Build: A0
// Date:  20-Mar-2016
// File:  port-modes.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-strings.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: REBOL Constants Strings
// Build: A0
// Date:  20-Mar-2016
// File:  str-consts.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-headers)
//
}
%src/include/tmp-sysctx.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: Sys Context
// Build: A0
// Date:  20-Mar-2016
// File:  sysctx.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/include/tmp-sysobj.h {// REBOL [R3] Language Interpreter and Run-time Environment
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
// Licensed under the Apache License, Version 2.0
// This is a code-generated file.
//
// **********************************************************************
//
// Title: System Object
// Build: A0
// Date:  20-Mar-2016
// File:  sysobj.h
//
// AUTO-GENERATED FILE - Do not modify. (From: make-boot.r)
//
}
%src/os/dev-dns.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: DNS access"
        Author "Carl Sassenrath"
        Purpose "Calls local DNS services for domain name lookup."
        Notes {See MS WSAAsyncGetHost* details regarding multiple requests.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/dev-net.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: TCP/IP network access"
        Author "Carl Sassenrath"
        Purpose "Supports TCP and UDP (but not raw socket modes.)"
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/generic/host-gob.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "GOB Hostkit Facilities"
        Purpose {@HostileFork doesn't particularly like the way GOB! is done,
and feels it's an instance of a more general need for external
types that participate in Rebol's type system and garbage
collector.  For now these routines are kept together here.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/generic/host-locale.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Locale Support"
        Purpose {Support for language and language groups (ISO 639)...as well as
country, state, and province codes (ISO 3166)

https://en.wikipedia.org/wiki/ISO_639
https://en.wikipedia.org/wiki/ISO_3166}
    ]
    msg none
    rest none
    analysis none
]
%src/os/generic/host-memory.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Host Memory Allocator"
        Purpose {See notes about OS_ALLOC and OS_FREE in make-os-ext.r}
    ]
    msg none
    rest none
    analysis none
]
%src/os/generic/iso-3166.c none
%src/os/generic/iso-3166.h none
%src/os/generic/iso-639.c none
%src/os/generic/iso-639.h none
%src/os/host-args.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Command line argument processing"
        Author "Carl Sassenrath"
        Caution "OS independent"
        Purpose {Parses command line arguments and options, storing them
in a structure to be used by the REBOL library.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/host-core.c [
    title r3
    rights [
        "Copyright 2012 Saphirion AG"
    ]
    trademark none
    notice apache-2.0
    meta [
        Title {Core extension. Contains commands not yet migrated to core codebase.}
        Author "Richard Smolak"
    ]
    msg standard-programmer-note
    rest none
    analysis [
        no-rebol-technologies-copyright
        no-trademark
    ]
]
%src/os/host-device.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device management and command dispatch"
        Author "Carl Sassenrath"
        Caution "OS independent"
        Purpose {This module implements a device management system for
REBOL devices and tracking their I/O requests.
It is intentionally kept very simple (makes debugging easy!)}
        Special-note {This module is parsed for function declarations used to
build prototypes, tables, and other definitions. To change
function arguments requires a rebuild of the REBOL library.}
        Design-comments {1. Not a lot of devices are needed (dozens, not hundreds).
2. Devices are referenced by integer (index into device table).
3. A single device can support multiple requests.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/host-ext-test.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Test for Embedded Extension Modules"
        Author "Carl Sassenrath"
        Purpose {Provides test code for extensions that can be easily
built and run in the host-kit. Not part of release,
but can be used as an example.}
        See {http://www.rebol.com/r3/docs/concepts/extensions-embedded.html}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/host-main.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
        {Additional code modifications and improvements Copyright 2012 Saphirion AG}
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Host environment main entry point"
        Note "OS independent"
        Author "Carl Sassenrath"
        Purpose {Provides the outer environment that calls the REBOL lib.
This module is more or less just an example and includes
a very simple console prompt.}
    ]
    msg {WARNING to PROGRAMMERS:

    This open source code is strictly managed to maintain
    source consistency according to our standards, not yours.

    1. Keep code clear and simple.
    2. Document odd code, your reasoning, or gotchas.
    3. Use our source style for code, indentation, comments, etc.
    4. It must work on Win32, Linux, OS X, BSD, big/little endian.
    5. Test your code really well before submitting it.}
    rest none
    analysis [
        non-standard-message
    ]
]
%src/os/host-stdio.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Simple helper functions for host-side standard I/O"
        Author "Carl Sassenrath"
        Caution "OS independent"
        Purpose {Interfaces to the stdio device for standard I/O on the host.
All stdio within REBOL uses UTF-8 encoding so the functions
shown here operate on UTF-8 bytes, regardless of the OS.
The conversion to wide-chars for OSes like Win32 is done in
the StdIO Device code.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/host-table.c {// Isolated C source file for making the host table an isolated link entity.
//
// Libraries may not wish to include the resulting %host-table.o (or %.obj)
// in order to make it possible to relink against different hosts.
//
// See %host-table.inc for more information.
//
}
%src/os/linux/dev-serial.c [
    title r3
    rights [
        "Copyright 2013 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Serial port access for Posix"
        Author "Carl Sassenrath"
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/linux/dev-signal.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
        "Copyright 2014 Atronix Engineering, Inc."
        {Additional code modifications and improvements Copyright 2012 Saphirion AG}
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Signal access on Linux"
        Author "Shixin Zeng"
        Purpose {Provides a very simple interface to the signals on Linux}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/linux/host-browse.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Browser Launch Host"
        Purpose {This provides the ability to launch a web browser or file
browser on the host.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/linux/host-encap.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Linux Encap Facility"
        Purpose {This host routine is used to read out a binary blob stored in
an ELF executable, used for "encapping" a script and its
resources.  Unlike a large constant blob that is compiled into
the data segment requiring a C compiler, encapped data can be
written into an already compiled ELF executable.

Because this method is closely tied to the ELF format, it
cannot be used with systems besides Linux (unless they happen
to also use ELF):

https://en.wikipedia.org/wiki/Executable_and_Linkable_Format}
    ]
    msg none
    rest none
    analysis none
]
%src/os/linux/host-event.c none
%src/os/posix/dev-event.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Event handler for Posix"
        Author "Carl Sassenrath"
        Purpose {Processes events to pass to REBOL. Note that events are
used for more than just windowing.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/posix/dev-file.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: File access for Posix"
        Author "Carl Sassenrath"
        Purpose "File open, close, read, write, and other actions."
        Compile-note "-D_FILE_OFFSET_BITS=64 to support large files"
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/posix/dev-stdio.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Standard I/O for Posix"
        Author "Carl Sassenrath"
        Purpose {Provides basic I/O streams support for redirection and
opening a console window if necessary.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/posix/host-browse.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Browser Launch Host"
        Purpose {This provides the ability to launch a web browser or file
browser on the host.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-config.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "POSIX Host Configuration Routines"
        Purpose {This file is for situations where there is some kind of
configuration information (e.g. environment variables, boot
paths) that Rebol wants to get at from the host.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-error.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "POSIX Exit and Error Functions"
        Purpose "..."
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-library.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "POSIX Library-related functions"
        Purpose {This is for support of the LIBRARY! type from the host on
systems that support 'dlopen'.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-process.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "POSIX Process API"
        Author "Carl Sassenrath, Richard Smolak, Shixin Zeng"
        Purpose {This was originally the file host-lib.c, providing the entire
host API.  When the host routines were broken into smaller
pieces, it made sense that host-lib.c be kept as the largest
set of related routines.  That turned out to be the process
related routines and support for CALL.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-readline.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Simple readline() line input handler"
        Author "Carl Sassenrath"
        Purpose {Processes special keys for input line editing and recall.
Avoides use of complex OS libraries and GNU readline().
but hardcodes some parts only for the common standard.}
        Usage {This file is meant to be used in more than just REBOL, so
it does not include the normal REBOL header files, but rather
defines its own types and constants.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/posix/host-thread.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Host Thread Services"
        Purpose {Support for the TASK! type (not currently implemented).}
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-time.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "POSIX Host Time Functions"
        Purpose {Provide platform support for times and timing information.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/posix/host-window.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Windowing stubs"
        File "host-window.c"
        Purpose "Provides stub functions for windowing."
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/stub/host-encap.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Stub File for Encap"
        Purpose {Under the current design of the hostkit, certain functions that
are called must be provided.  The encap hook is called by the
main, so we need a null function.}
    ]
    msg none
    rest none
    analysis none
]
%src/os/windows/dev-clipboard.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
        {Additional code modifications and improvements Copyright 2012 Saphirion AG}
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Clipboard access for Win32"
        Author "Carl Sassenrath"
        Purpose {Provides a very simple interface to the clipboard for text.
May be expanded in the future for images, etc.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/windows/dev-event.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Event handler for Win32"
        Author "Carl Sassenrath"
        Purpose {Processes events to pass to REBOL. Note that events are
used for more than just windowing.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/windows/dev-file.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: File access for Win32"
        Author "Carl Sassenrath"
        Purpose "File open, close, read, write, and other actions."
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/windows/dev-serial.c [
    title r3
    rights [
        "Copyright 2013 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Serial port access for Windows"
        Author "Carl Sassenrath, Joshua Shireman"
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/windows/dev-stdio.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title "Device: Standard I/O for Win32"
        Author "Carl Sassenrath"
        Purpose {Provides basic I/O streams support for redirection and
opening a console window if necessary.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/windows/host-lib.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
        {Additional code modifications and improvements Copyright 2012 Saphirion AG}
    ]
    trademark rebol
    notice apache-2.0
    meta [
        Title {OS API function library called by REBOL interpreter}
        Author "Carl Sassenrath, Richard Smolak"
        Purpose {This module provides the functions that REBOL calls
to interface to the native (host) operating system.
REBOL accesses these functions through the structure
defined in host-lib.h (auto-generated, do not modify).}
        Flags "compile with -DUNICODE for Win32 wide char API"
        Special-note {This module is parsed for function declarations used to
build prototypes, tables, and other definitions. To change
function arguments requires a rebuild of the REBOL library.}
    ]
    msg standard-programmer-note
    rest none
    analysis none
]
%src/os/windows/rpic-test.c [
    title r3
    rights [
        "Copyright 2012 REBOL Technologies"
    ]
    trademark rebol
    notice apache-2.0
    meta none
    msg none
    rest {**********************************************************************

rpic-test.c - Test for REBOL Plug-In Component
}
    analysis [
        missing-meta
        additional-information
    ]
]
