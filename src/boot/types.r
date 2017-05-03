REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Datatype definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2017 Rebol Open Source Developers
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
        class       - how type actions are dispatched (T_type), + is extension
        path        - it supports various path forms (+ for same as typeclass)
        make        - It can be made with #[datatype] method
        typesets    - what typesets the type belongs to

        Note that if there is `somename` in the class column, that means you
        will find the ACTION! dispatch for that type in `REBTYPE(Somename)`.

        Also included in this file are macros which are not automatically
        generated (though perhaps some of them could be?).  They are in the
        header of this file because all the type concerns tie together...if
        they are reordered then tests using > or < might start failing.  It's
        easier not to forget the importance of the order by keeping the
        macros here.
    }
    Macros: {    
        #define Is_Bindable(v) \
            (VAL_TYPE(v) < REB_BAR)

        #define Not_Bindable(v) \
            (VAL_TYPE(v) >= REB_BAR)

        #define IS_ANY_VALUE(v) \
            LOGICAL(VAL_TYPE(v) != REB_MAX_VOID)

        #define ANY_SCALAR(v) \
            LOGICAL(VAL_TYPE(v) >= REB_LOGIC && VAL_TYPE(v) <= REB_DATE)

        #define ANY_SERIES(v) \
            LOGICAL(VAL_TYPE(v) >= REB_PATH && VAL_TYPE(v) <= REB_VECTOR)

        #define ANY_STRING(v) \
            LOGICAL(VAL_TYPE(v) >= REB_STRING && VAL_TYPE(v) <= REB_TAG)

        #define ANY_BINSTR(v) \
            LOGICAL(VAL_TYPE(v) >= REB_BINARY && VAL_TYPE(v) <= REB_TAG)

        inline static REBOOL ANY_ARRAY_KIND(enum Reb_Kind k) {
            return LOGICAL(k >= REB_PATH && k <= REB_BLOCK);
        }

        #define ANY_ARRAY(v) \
            ANY_ARRAY_KIND(VAL_TYPE(v))

        inline static REBOOL ANY_WORD_KIND(enum Reb_Kind k) {
            return LOGICAL(k >= REB_WORD && k <= REB_ISSUE);
        }

        #define ANY_WORD(v) \
            ANY_WORD_KIND(VAL_TYPE(v))

        #define ANY_PATH(v) \
            LOGICAL(VAL_TYPE(v) >= REB_PATH && VAL_TYPE(v) <= REB_LIT_PATH)

        #define ANY_EVAL_BLOCK(v) \
            LOGICAL(VAL_TYPE(v) == REB_BLOCK || VAL_TYPE(v) == REB_GROUP)

        inline static REBOOL ANY_CONTEXT_KIND(enum Reb_Kind k) {
            return LOGICAL(k >= REB_OBJECT && k <= REB_PORT);
        }

        #define ANY_CONTEXT(v) \
            ANY_CONTEXT_KIND(VAL_TYPE(v))
    }
]


[name       class       path    make    mold     typesets]

; 0 is not a real data type.  It is reserved for internal purposes.

0           0           -       -       -       -

; There is only one FUNCTION! type in Ren-C

function    function    +       +       +       -

; ANY-WORD!, order matters (tests like ANY_WORD use >= REB_WORD, <= REB_ISSUE)
;
word        word        -       +       +       word
set-word    word        -       +       +       word
get-word    word        -       +       +       word
lit-word    word        -       +       +       word
refinement  word        -       +       +       word
issue       word        -       +       +       word

; ANY-ARRAY!, order matters (and contiguous with ANY-SERIES below matters!)
;
path        array       +       +       +       [series path array]
set-path    array       +       +       +       [series path array]
get-path    array       +       +       +       [series path array]
lit-path    array       +       +       +       [series path array]
group       array       +       +       +       [series array]
block       array       +       +       +       [series array]

; ANY-SERIES!, order matters (and contiguous with ANY-ARRAY above matters!)
;
binary      string      +       +       binary  [series]
string      string      +       +       +       [series string]
file        string      +       +       +       [series string]
email       string      +       +       +       [series string]
url         string      +       +       +       [series string]
tag         string      +       +       +       [series string]

bitset      bitset      +       +       +       -
image       image       +       +       +       [series]
vector      vector      +       +       +       [series]

map         map         +       +       +       -

varargs     varargs     +       +       +       -

object      context     +       +       +       context
frame       context     +       +       +       context
module      context     +       +       +       context
error       context     +       +       error   context
port        port        context +       context context

; ^-------- Everything above is a "bindable" type, see Is_Bindable() --------^

; v------- Everything below is an "unbindable" type, see Is_Bindable() ------v

; "unit types" https://en.wikipedia.org/wiki/Unit_type

bar         unit        -       +       +       -
lit-bar     unit        -       +       +       -
blank       unit        -       +       +       -

; scalars

logic       logic       -       +       +       -
integer     integer     -       +       +       [number scalar]
decimal     decimal     -       +       +       [number scalar]
percent     decimal     -       +       +       [number scalar]
money       money       -       +       +       scalar
char        char        -       +       +       scalar
pair        pair        +       +       +       scalar
tuple       tuple       +       +       +       scalar
time        time        +       +       +       scalar
date        date        +       +       +       -

; type system

datatype    datatype    -       +       +       -
typeset     typeset     -       +       +       -

; things likely to become user-defined types or extensions

gob         gob         +       +       +       -
event       event       +       +       +       -
handle      handle      -       -       +       -
struct      *           *       *       *       -
library     library     -       +       +       -

; Note that the "void?" state has no associated VOID! datatype.  Internally
; it uses REB_MAX, but like the REB_0 it stays off the type map.
