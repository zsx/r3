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

; Words for BLANK! and BAR!, for those who don't like symbols

blank: _
bar: '|


; There is no Rebol value representing void, so it cannot be assigned as
; a word to a literal.  This VOID function is an alternative to `()`

void: does []


eval func [
    {Make reflector functions (variadic to quote "top-level" words)}
    :set-word... [[ set-word!]]
    :divider... [[word!]]
    :categories... [[string!]]
    /local set-word categories name
][
    while [any-value? set-word: take set-word...] [
        assert ['-- = take divider...]
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
    spec-of: -- {function, object, or module}
    body-of: -- {function or module} ; %mezz-func.r overwrites
    words-of: -- {function, object, or module}
    values-of: -- {object or module}
    types-of: -- {function}
    addr-of: -- {struct or callback}
    title-of: -- {function} ; should work for module
|


eval func [
    {Make the ANY-XXX? typeset testers (variadic to quote top-level words)}
    :set-word... [[set-word!]]
    :divider... [[word!]]
    :summary... [[string!]]
    /local set-word summary typeset-word typeset
][
    while [any-value? set-word: take set-word...] [
        assert ['-- = take divider...]
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
    any-string?: -- "any type of string"
    any-word?: -- "any type of word"
    any-path?: -- "any type of path"
    any-context?: -- "any type of context"
    any-number?: -- "a number (integer or decimal)"
    any-series?: -- "any type of series"
    any-scalar?: -- "any type of scalar"
    any-array?: -- "a series of Rebol values"
|


decode-url: _ ; set in sys init

r3-legacy*: _ ; set in %mezz-legacy.r

; used only by Ren-C++ as a test of how to patch the lib context prior to
; boot at the higher levels.
test-rencpp-low-level-hook: _

;-- Setup Codecs -------------------------------------------------------------

for-each [codec handler] system/codecs [
    if handle? handler [
        ; Change boot handle into object:
        codec: set codec make object! [
            entry: handler
            title: form reduce ["Internal codec for" codec "media type"]
            name: codec
            type: 'image!
            suffixes: select [
                text [%.txt]
                utf-16le [%.txt]
                utf-16be [%.txt]
                markup [%.html %.htm %.xml %.xsl %.wml %.sgml %.asp %.php %.cgi]
                bmp  [%.bmp]
                gif  [%.gif]
                jpeg [%.jpg %.jpeg]
                png  [%.png]
            ] codec
        ]
        ; Media-types block format: [.abc .def type ...]
        append append system/options/file-types codec/suffixes codec/name
    ]
]

; Special import case for extensions:
append system/options/file-types switch/default fourth system/version [
    3 [[%.rx %.dll extension]]  ; Windows
    2 [[%.rx %.dylib %.so extension]]  ; OS X
    4 7 [[%.rx %.so extension]]  ; Other Posix
] [[%.rx extension]]

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


; Experimental shorthand for ANY-VALUE? test (will also be VALUE?)
;
?: :any-value?
