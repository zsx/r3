REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Datatype definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        These words define the REBOL datatypes and their related attributes.
        This table generates a variety of C defines and intialization tables.
        During build, when this file is processed, this section is changed to
        hold just the datatype words - the initial entries the word table.

        name        - name of datatype (generates words)
        class       - how type actions are dispatched (T_type)
        mold        - mold format: - self, + type, * typeclass
        form        - form format: above, and f* for special form functions
        path        - it supports various path forms (* for same as typeclass)
        make        - It can be made with #[datatype] method
        typesets    - what typesets the type belongs to

        Note that if there is `somename` in the class column, that means you
        will find the ACTION! dispatch for that type in `REBTYPE(Somename)`.

        If the (CLASS) is in a GROUP! that means it has evaluator behavior,
        vs. being passed through as-is.  (e.g. a lit-word is "evaluative")
        This is used to build the table used for fast lookup of whether the
        evaluator needs to be called on a given type.
    }
]


[name       class       mold    form    path    make    typesets]

; 0 is not a real data type.  It is reserved for a kind of "garbage", as well
; as used internally for REB_0_LOOKBACK...since the evaluator switch statement
; wants to treat functions that look back differently from function.
;
0           0           -       -       -       -       -

function    function    *       -       -       *       -

bar         unit        +       +       -       *       -
lit-bar     unit        +       +       -       *       -

; ANY-WORD!, order matters (tests like ANY_WORD use >= REB_WORD, <= REB_ISSUE)
;
word        word        +       *       -       *       word
set-word    word        +       *       -       *       word
get-word    word        +       *       -       *       word
lit-word    word        +       *       -       *       word
refinement  word        +       *       -       *       word
issue       word        +       *       -       *       word

; ANY-ARRAY!, order matters (and contiguous with ANY-SERIES below matters!)
;
path        array       *       *       *       *       [series path array]
set-path    array       *       *       *       *       [series path array]
get-path    array       *       *       *       *       [series path array]
lit-path    array       *       *       *       *       [series path array]
group       array       *       f*      *       *       [series array]
;
; ^--above this line MAY have "evaluator behavior", below types are "inert"--v
;    (basically types are compared as >= REB_BLOCK and not dispatched)
;
block       array       *       f*      *       *       [series array]

; ANY-SERIES!, order matters (and contiguous with ANY-ARRAY above matters!)
;
binary      string      +       +       *       *       [series]
string      string      +       f*      *       *       [series string]
file        string      +       f*      file    *       [series string]
email       string      +       f*      *       *       [series string]
url         string      +       f*      url     *       [series string]
tag         string      +       +       *       *       [series string]

bitset      bitset      *       *       *       *       -
image       image       +       +       *       *       [series]
vector      vector      -       -       *       *       [series]

; Note: BLANK! is a "unit type" https://en.wikipedia.org/wiki/Unit_type
;
blank       unit        +       +       -       *       -

;-- Scalars

logic       logic       *       *       -       *       -
integer     integer     *       *       -       *       [number scalar]
decimal     decimal     *       *       -       *       [number scalar]
percent     decimal     *       *       -       *       [number scalar]
money       money       *       *       -       *       scalar
char        char        *       f*      -       *       scalar
pair        pair        *       *       *       *       scalar
tuple       tuple       *       *       *       *       scalar
time        time        *       *       *       *       scalar
date        date        *       *       *       *       -

map         map         +       f*      *       *       -

datatype    datatype    +       f*      -       *       -
typeset     typeset     +       f*      -       *       -

varargs     varargs     -       -       -       *       -

object      context     *       f*      *       *       context
frame       context     *       f*      *       *       context
module      context     *       f*      *       *       context
error       context     +       f+      *       *       context
port        port        context context context *       context

gob         gob         *       *       *       *       -
event       event       *       *       *       *       -
handle      handle      -       -       -       -       -
struct      struct      *       *       *       *       -
library     library     -       -       -       *       -

; Note that the "void?" state has no associated VOID! datatype.  Internally
; it uses REB_MAX, but like the REB_0 it stays off the type map.  (REB_0
; is used for lookback as opposed to void in order to implement an
; optimization in Get_Var_Core())
