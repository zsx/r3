REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Generate native specifications"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "@codebybrett"
    Needs: 2.100.100
]

do %common.r
do %common-parsers.r

print "------ Generate tmp-natives.r"

r3: system/version > 2.100.0

verbose: false

unsorted-buffer: make string! 20000

emit-proto: func [proto] [

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
        block? proto-parser/data/3
    ] [

        line: line-of source.text proto-parser/parse.position

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

process: func [file] [
    if verbose [?? file]
    source.text: read join core-folder the-file: file
    if r3 [source.text: deline to-string source.text]
    proto-parser/emit-proto: :emit-proto
    proto-parser/process source.text
]

;-------------------------------------------------------------------------

output-buffer: make string! 20000

append output-buffer {REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Native function specs"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0.
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Note: {This is a generated file.}
]
}

proto-count: 0

files: sort read core-folder: %../core/

remove-each file files [

    not all [
        %.c = suffix? file
        not find/match file "host-"
        not find/match file "os-"
    ]
]

for-each file files [process file]

append output-buffer unsorted-buffer

write %../boot/tmp-natives.r output-buffer

print [proto-count "natives"]
print " "


print "------ Generate tmp-actions.r"

clear output-buffer

append output-buffer {REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Action function specs"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0.
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Note: {This is a generated file.}
]

}

boot-types: load %../boot/types.r
n: 0
for-each-record-NO-RETURN type boot-types [
    if n == 0 [
        ;-- We skip TRASH!
        n: n + 1
        continue
    ]

    caps-name: rejoin [(uppercase form type/name) {!}]

    append output-buffer rejoin [
        type/name "?: action/typecheck" space {[} newline
        spaced-tab
            {"} {Returns TRUE if value is of type} space caps-name {"} newline
        spaced-tab
            {value [<opt> any-value!]} newline
        {]} space n
        newline
        newline
    ]

    n: n + 1
]

append output-buffer mold/only load %../boot/actions.r

append output-buffer rejoin [newline newline]

write %../boot/tmp-actions.r output-buffer
