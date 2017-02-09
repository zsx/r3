REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Common Code for Emitting Text Files"
    Rights: {
        Copyright 2016 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        While emitting text files isn't exactly rocket science, it can help
        to have a few sanity checks on the process.
    }
]

; !!! %make-headers.r has a dependency on BUF-EMIT, because in addition to
; just outputting data it also merges #ifdef/#endif, and does so on two files
; at once.  That suggests that the emitter really needs to be an object so
; you can run multiple emit instances at once without overwriting each other,
; but the primitive bootstrap scripts weren't prepared for this case.  For
; now, the buffer is exposed.
;
buf-emit: make string! 100000


emit: proc [data] [adjoin buf-emit data]


unemit: proc [
    data [char!]
][
    if data != last buf-emit [
        probe skip (tail buf-emit) -100
        fail ["UNEMIT did not match" data "as the last piece of input"]
    ]
    assert [data = last buf-emit]
    take/last buf-emit
]


emit-line: proc [data /indent] [
    unless any [tail? buf-emit | newline = last buf-emit] [
        probe skip (tail buf-emit) -100
        fail "EMIT-LINE should always start a new line"
    ]
    data: reduce data
    if find data newline [
        probe data
        fail "data passed to EMIT-LINE should not contain embedded newlines"
    ]
    if indent [emit spaced-tab]
    emit data
    emit newline
]


emit-lines: proc [block [block!]] [
    for-each data block [emit-line data]
]


emit-header: proc [title [string!] file [file!]] [
    unless tail? head buf-emit [
        probe file
        probe title
        fail "EMIT-HEADER should only be called when the emit buffer is empty"
    ]

    emit form-header/gen title file (system/script/header/file)
]


emit-item: proc [
    {Emits an indented identifier and comma for enums and initializer lists}
    name
        {Will be converted using TO-C-NAME which joins BLOCK! and forms WORD!}
    /upper
        {Make the name uppercase -after- the conversion using TO-C-NAME (!)}
    /assign
        {Give the item an assigned value}
    num [integer!]
][
    name: to-c-name name
    if upper [uppercase name]
    either assign [
        emit-line/indent [name space "=" space num ","]
    ][
        emit-line/indent [name ","]
    ]

    ; NOTE: standard C++ and C do not like commas on the last item in lists, 
    ; so they are removed with EMIT-END, by taking the last comma out of the
    ; emit buffer.
]


emit-annotation: procedure [
    {Adds a C++ "//"-style comment to the end of the last line emitted.}
    note [word! string! integer!]
][
    unemit newline
    emit [space "//" space note newline]
]


emit-end: proc [] [
    remove find/last buf-emit #","
    emit-line ["};"]
    emit newline
]


write-emitted: proc [file] [
    if newline != last buf-emit [
        probe skip (tail buf-emit) -100
        fail "WRITE-EMITTED must have a NEWLINE as last character in buffer"
    ]

    ; Would be nice to write something here, but preferable if the begin
    ; of an emit told you what was coming and then had a "...DONE" finisher.
    ;
    comment [print ["WRITING" file]]

    write file buf-emit
    clear buf-emit
]
