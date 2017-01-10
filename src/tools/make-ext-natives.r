REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Generate extention native header files"
    File: %make-ext-native.r ;-- used by EMIT-HEADER to indicate emitting script
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "Shixin Zeng <szeng@atronixengineering.com>"
    Needs: 2.100.100
]

do %common.r
do %common-emitter.r
do %form-header.r
do %common-parsers.r

r3: system/version > 2.100.0

args: parse-args system/options/args
output-dir: fix-win32-path to file! any [args/OUTDIR %../]
mkdir/deep output-dir/include
c-src: fix-win32-path to file! args/SRC
m-name: args/MODULE
l-m-name: lowercase copy m-name
u-m-name: uppercase copy m-name

verbose: false

unsorted-buffer: make string! 20000

emit-proto: proc [proto] [

    if all [
        'format2015 = proto-parser/style
        block? proto-parser/data
        any [
            'native = proto-parser/data/2
            all [
                path? proto-parser/data/2
                'native = proto-parser/data/2/1
            ]
        ]
    ] [
        line: line-of source.text proto-parser/parse.position

        if not block? proto-parser/data/3 [
            fail [
                "Native" (uppercase form to word! proto-parser/data/1)
                "needs loadable specification block."
                (mold the-file) (line)
            ]
        ]

        append case [
            ; could do tests here to create special buffer categories to
            ; put certain natives first or last, etc. (not currently needed)
            ;
            true [unsorted-buffer]
        ] rejoin [
            newline newline
            {; !!! DO NOT EDIT HERE! This is generated from }
            mold the-file { line } line newline
            mold/only proto-parser/data
        ]

        proto-count: proto-count + 1
    ]
]

process: func [
    file
    ; <with> the-file ;-- note external variable (can't do this in R3-Alpha)
][
    the-file: file
    if verbose [probe [file]]

    source.text: read the-file: file
    ;print ["source:" to string! source.text]
    if r3 [source.text: deline to-string source.text]
    proto-parser/emit-proto: :emit-proto
    proto-parser/process source.text
]

;------- copied from make-headers.r
emit-include-params-macro: procedure [word [word!] paramlist [block!]] [
    ;
    ; start emitting what will be a multi line macro (backslash as last
    ; character on line is how macros span multiple lines in C).
    ;
    emit-line [
        {#define} space "INCLUDE_PARAMS_OF_" (uppercase to-c-name word)
        space "\"
    ]

    ; Collect the argument and refinements, converted to their "C names"
    ; (so dashes become underscores, * becomes _P, etc.)
    ;
    n: 1
    for-each item paramlist [
        if all [any-word? item | not set-word? item] [
            param-name: switch/default to-word item [
                ? [copy "q"]
            ][
                to-c-name to-word item
            ]

            which: either refinement? item ["REFINE"] ["PARAM"]
            emit-line/indent [
                which "(" n "," space param-name ");" space "\"
            ]
            n: n + 1
        ]
    ]

    ; Get rid of trailing \ for multi-line macro continuation.
    unemit newline
    unemit #"\"
    emit newline
]

;-------------------------------------------------------------------------

c-natives: make block! 128

proto-count: 0
process c-src

native-list: load unsorted-buffer
word-list: copy []
export-list: copy []
num-native: 0
unless parse native-list [
    while [
        set w set-word! [
            'native block!
            | 'native/body 2 block!
            | [
                'native/export block!
                | 'native/export/body 2 block!
                | 'native/body/export 2 block!
            ] (append export-list to word! w)
        ] (++ num-native)
        | remove [quote new-words: set words block! (append word-list words)]
    ]
][
    fail rejoin ["failed to parse" mold native-list ", current word-list:" mold word-list]
]
;print ["specs:" mold native-list]
word-list: unique word-list
spec: compose/deep/only [
    REBOL [
        name: (to word! m-name)
        exports: (export-list)
    ]
]
unless empty? word-list [
    append spec compose/only [words: (word-list)]
]
append spec native-list
comp-data: compress data: to-binary mold spec
;print ["buf:" to string! data]

emit-header m-name to file! rejoin [%tmp- l-m-name %.h]
emit-line ["#define EXT_NUM_NATIVES_" u-m-name space num-native]
emit-line ["#define EXT_NAT_COMPRESSED_SIZE_" u-m-name space length comp-data]
emit-line ["const REBYTE Ext_Native_Specs_" m-name "[EXT_NAT_COMPRESSED_SIZE_" u-m-name "] = {"]

;-- Convert UTF-8 binary to C-encoded string:
emit binary-to-c comp-data
emit-line "};" ;-- EMIT-END would erase the last comma, but there's no extra

emit-line ["REBNAT Ext_Native_C_Funcs_" m-name "[EXT_NUM_NATIVES_" u-m-name "] = {"]
for-each item native-list [
    if set-word? item [
        emit-item ["N_" to word! item]
    ]
]
emit-end

emit-line [ {
int Module_Init_} m-name {_Core(RELVAL *out)
^{
    INIT_} u-m-name {_WORDS;
    REBARR *arr = Make_Array(2);
    TERM_ARRAY_LEN(arr, 2);
    Init_Binary(ARR_AT(arr, 0),
        Copy_Bytes(Ext_Native_Specs_} m-name {, EXT_NAT_COMPRESSED_SIZE_} u-m-name {));
    Init_Handle_Simple(ARR_AT(arr, 1),
        Ext_Native_C_Funcs_} m-name {, EXT_NUM_NATIVES_} u-m-name {);
    Init_Block(out, arr);

    return 0;
^}

int Module_Quit_} m-name {_Core(void)
{
    return 0;
}
}
]

write-emitted to file! rejoin [output-dir/include/tmp-ext- l-m-name %.h]

;--------------------------------------------------------------
; tmp-ext-args.h
emit-header "PARAM() and REFINE() Automatic Macros" to file! rejoin [%tmp-ext- l-m-name %-args.h]

for-next native-list [
    if any [
        'native = native-list/2
        all [path? native-list/2 | 'native = first native-list/2]
    ][
        assert [set-word? native-list/1]
        emit-include-params-macro (to-word native-list/1) (native-list/3)
        emit newline
    ]
]
write-emitted to file! rejoin [output-dir/include/tmp-ext- l-m-name %-args.h]

;--------------------------------------------------------------
; tmp-ext-words.h
emit-header "Local words" to file! rejoin [%tmp-ext- l-m-name %-words.h]
emit-line ["#define NUM_EXT_" u-m-name "_WORDS" space length word-list]

either empty? word-list [
    emit-line ["#define INIT_" u-m-name "_WORDS"]
][
    emit-line ["static const REBYTE* Ext_Words_" m-name "[NUM_EXT_" u-m-name "_WORDS] = {"]
    for-next word-list [
        emit-line/indent [ {"} to-c-name word-list/1 {",} ]
    ]
    emit-end

    emit-line ["static REBSTR* Ext_Canons_" m-name "[NUM_EXT_" u-m-name "_WORDS];"]

    word-seq: 0
    for-next word-list [
        emit-line ["#define" space u-m-name {_WORD_} uppercase to-c-name word-list/1 space
            {Ext_Canons_} m-name {[} word-seq {]}]
        ++ word-seq
    ]
    emit-line ["#define INIT_" u-m-name "_WORDS" space "\"]
    emit-line/indent ["Init_Extension_Words(Ext_Words_" m-name ", Ext_Canons_" m-name ", NUM_EXT_" u-m-name "_WORDS)"]
]

write-emitted to file! rejoin [output-dir/include/tmp-ext- l-m-name %-words.h]
