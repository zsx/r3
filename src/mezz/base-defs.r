REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Base: Other Definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Description: {
        This code is evaluated just after actions, natives, sysobj, and
        other lower level definitions. This file intializes a minimal working
        environment that is used for the rest of the boot.
    }
    Note: {
        Any exported SET-WORD!s must be themselves "top level". This hampers
        procedural code here that would like to use tables to avoid repeating
        itself.  This means variadic approaches have to be used that quote
        SET-WORD!s living at the top level, inline after the function call.
    }
]

; PROBE is a good first function to have handy for debugging all the rest (!)
;
probe: func [
    {Debug print a molded value and returns that same value.}
    value [<opt> any-value!]
        {Value to display.  *Literal* blocks are evaluated and use as labels.}
    /only
        {Output literal blocks as the blocks themselves.}
    out: item:
][
    ; !!! Use "PROBE dialect"...what features might be added?
    ;
    if all [semiquoted? 'value | block? value] [
        out: make string! 50
        for-each item value [
            case [
                word? :item [
                    print [to set-word! item {=>} mold get item]
                ]

                path? :item [
                    print [to set-path! item {=>} mold get item]
                ]

                group? :item [
                    trap/with [
                        print [mold item {=>} mold eval item]
                    ] func [error] [
                        print [mold item {=!!!=>} mold error]
                    ]
                ]

                true [
                    fail [
                        "Item not WORD!, PATH!, or GROUP! in PROBE:" mold :item
                    ]
                ]
            ]
        ]
        return ()
    ]

    print mold :value
    :value
]


; Words for BLANK! and BAR!, for those who don't like symbols

blank: _
bar: '|


; There is no Rebol value representing void, so it cannot be assigned as
; a word to a literal.  This VOID function is an alternative to `()`

void: func [] [] ;-- DOES not defined yet.


eval func [
    {Make reflector functions (variadic to quote "top-level" words)}
    :set-word... [[ set-word!]]
    :divider... [[blank!]]
    :categories... [[string!]]
    /local set-word categories name
][
    while [any-value? set-word: take set-word...] [
        take divider... ;-- so it doesn't look like we're setting to a string
        categories: take categories...

        ; extract XXX string from XXX-OF
        name: head clear find (spelling-of set-word) {-of}

        set set-word make function! compose/deep [
            [
                (ajoin [{Returns a copy of the } name { of a } categories {.}])
                value
            ][
                reflect :value (to lit-word! name)
            ]
        ]
    ]
]
    spec-of: _ {function, object, or module}
    body-of: _ {function or module} ; %mezz-func.r overwrites
    words-of: _ {function, object, or module}
    values-of: _ {object or module}
    types-of: _ {function}
    addr-of: _ {struct or callback}
    title-of: _ {function} ; should work for module
|


eval func [
    {Make the ANY-XXX? typeset testers (variadic to quote top-level words)}
    :set-word... [[set-word!]]
    :divider... [[blank!]]
    :summary... [[string!]]
    /local set-word summary typeset-word typeset
][
    while [any-value? set-word: take set-word...] [
        take divider... ;-- so it doesn't look like we're setting to a string
        summary: take summary...

        ; any-xxx? => any-xxx!, needs to be bound to fetch typeset
        typeset-word: to word! head change (find spelling-of set-word "?") "!"
        typeset-word: bind typeset-word set-word
        assert [typeset? typeset: get typeset-word]

        set set-word make function! compose/deep [
            [
                (ajoin [{Return TRUE if value is } summary {.}])
                value [_ any-value!]
            ][
                find (typeset) type-of :value
            ]
        ]
    ]
]
    any-string?: _ "any type of string"
    any-word?: _ "any type of word"
    any-path?: _ "any type of path"
    any-context?: _ "any type of context"
    any-number?: _ "a number (integer or decimal)"
    any-series?: _ "any type of series"
    any-scalar?: _ "any type of scalar"
    any-array?: _ "a series of Rebol values"
|


is: func [
    {Tests value for being of type or typeset, returns value if so else blank}
    types [datatype! typeset! block!]
    value [<opt> any-value!]
    /relax
        {Do not trigger an error if types contain LOGIC! or BLANK!.}
][
    if not set? 'value [return blank]

    if block? types [types: make typeset! types]

    case [
        datatype? types [
            unless relax [
                if logic! = types [
                    fail "use IS/RELAX to allow a LOGIC! false result"
                ]
                if blank! = types [
                    fail "use IS/RELAX to allow a BLANK! result"
                ]
            ]
            if types = type-of :value [return :value]
        ]

        typeset? types [
            unless relax [
                if find types logic! [
                    fail "use IS/RELAX to allow a LOGIC! false result"
                ]
                if find types blank! [
                    fail "use IS/RELAX to allow a BLANK! result"
                ]
            ]
            if find types type-of :value [return :value]
        ]
    ]

    blank
]


decode-url: _ ; set in sys init

r3-legacy*: _ ; set in %mezz-legacy.r

; used only by Ren-C++ as a test of how to patch the lib context prior to
; boot at the higher levels.
test-rencpp-low-level-hook: _

internal!: make typeset! [
    handle!
]

immediate!: make typeset! [
    ; Does not include internal datatypes
    blank! logic! any-scalar! date! any-word! datatype! typeset! event!
]

system/options/result-types: make typeset! [
    immediate! any-series! bitset! image! object! map! gob!
]


ok?: func [
    "Returns TRUE on all values that are not ERROR!"
    value [<opt> any-value!]
][
    not error? :value
]

; Convenient alternatives for readability
;
neither?: :nand?
both?: :and?


; Experimental shorthand for ANY-VALUE? test (will also be VALUE?)
;
?: :any-value?
