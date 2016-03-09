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
    Note: {
        This code is evaluated just after actions, natives, sysobj, and other lower
        levels definitions. This file intializes a minimal working environment
        that is used for the rest of the boot.
    }
]

;-- Create the reflector functions (e.g. spec-of, body-of, ...)

; Must be defined in A108 (no forward refs)
spec-of:
body-of: ;-- real BODY-OF defined in %mezz-func.r
words-of:
values-of:
types-of:
    none

use [word title] [
    for-each name system/catalog/reflectors [
        word: make word! ajoin [name "-of"]
        word: bind/new word 'reflect
        title: ajoin ["Returns a copy of the " name " of a " switch/default name [
            spec        ["function or module"]
            values      ["object or module"]
            types title ["function"] ; title should include module Title too...
            addr ["struct or callback"]
        ] ["function, object, or module"]] ; body, words
        set word func
            reduce [title 'value]
            compose [reflect :value (to lit-word! name)]
    ]
]

decode-url: none ; set in sys init

r3-legacy*: none ; set in %mezz-legacy.r

; used only by Ren-C++ as a test of how to patch the lib context prior to
; boot at the higher levels.
test-rencpp-low-level-hook: none

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
    unset! handle!
]

immediate!: make typeset! [
    ; Does not include internal datatypes
    none! logic! any-scalar! date! any-word! datatype! typeset! event!
]

system/options/result-types: make typeset! [
    immediate! any-series! bitset! image! object! map! gob!
]


;-- Create "To-Datatype" conversion functions early in bootstrap:

any-string?: func [
    "Return TRUE if value is any type of string."
    value [opt-any-value!]
][find any-string! type-of :value]

any-word?: func [
    "Return TRUE if value is any type of word."
    value [opt-any-value!]
][find any-word! type-of :value]

any-path?: func [
    "Return TRUE if value is any type of path."
    value [opt-any-value!]
][find any-path! type-of :value]

any-context?: func [
    "Return TRUE if value is an OBJECT!, ERROR!, PORT!, or MODULE!"
    value [opt-any-value!]
][find any-context! type-of :value]

any-number?: func [
    "Return TRUE if value is a number (integer or decimal)."
    value [opt-any-value!]
][find any-number! type-of :value]

any-series?: func [
    "Return TRUE if value is any type of series."
    value [opt-any-value!]
][find any-series! type-of :value]

any-scalar?: func [
    "Return TRUE if value is any type of scalar."
    value [opt-any-value!]
][find any-scalar! type-of :value]

any-array?: func [
    "Return TRUE if value is a series containing all the same type."
    value [opt-any-value!]
][find any-array! type-of :value]

ok?: func [
    "Returns TRUE on all values that are not ERROR!"
    value [opt-any-value!]
][
    not error? :value
]
