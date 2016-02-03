REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Mezzanine: Function Helpers"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
]


procedure: func [
    ; !!! Should have a unified constructor with FUNCTION
    {Defines a closure function with all set-words as locals.}
    spec [block!] {Help string (opt) followed by arg words (and opt type and string)}
    body [block!] {The body block of the function}
    /with {Define or use a persistent object (self)}
    object [object! block! map!] {The object or spec}
    /extern words [block!] {These words are not local}
][
    ; Copy the spec and add /local to the end if not found (no deep copy needed)
    unless find spec: copy spec /local [append spec [
        /local ; In a block so the generated source gets the newlines
    ]]

    ; Collect all set-words in the body as words to be used as locals, and add
    ; them to the spec. Don't include the words already in the spec or object.
    insert find/tail spec /local collect-words/deep/set/ignore body either with [
        ; Make our own local object if a premade one is not provided
        unless object? object [object: make object! object]

        ; Make a full copy of the body, to allow reuse of the original
        body: copy/deep body

        bind body object  ; Bind any object words found in the body

        ; Ignore the words in the spec and those in the object. The spec needs
        ; to be copied since the object words shouldn't be added to the locals.
        append append append copy spec 'self words-of object words ; ignore 'self too
    ][
        ; Don't include the words in the spec, or any extern words.
        either extern [append copy spec words] [spec]
    ]

    proc spec body
]

map: func [
    {Make a map value (hashed associative block).}
    val
][
    make map! :val
]


task: func [
    {Creates a task.}
    spec [block!] {Name or spec block}
    body [block!] {The body block of the task}
][
    make task! copy/deep reduce [spec body]
]


spec-of: function [
    value [any-value!]
][
    spec: reflect :value 'spec
    unless function? :value [return spec]

    ; For performance and memory usage reasons, SPECIALIZE does not produce a
    ; spec at the time of specialization.  It only makes one on-demand when
    ; SPEC-OF is called.  The information the spec actually retains is just
    ; an optimized 1-element series containing the WORD! name that the
    ; specialization is for.
    ;
    switch func-class-of :value [
        1 2 3 4 5 6 [spec]

        7 [
            ; It was specialized, and what we really want to do is look at
            ; the spec of the function it specialized, and pare it down
            ;
            assert [word? first spec]
            frame: reflect :value 'body
            assert [frame? :frame]
            spec: reflect function-of frame 'spec
            insert spec [
                <specialized> ;-- is block for newline
            ]

            ; If we look at the frame, any values that are not BAR! are
            ; considered to be specialized.  The soft-quoted value from the
            ; frame will be used vs. information at the callsite.
            ;
            specials: copy []
            for-each [key value] frame [
                unless bar? value [
                    append specials key
                ]
            ]

            ; Pare down the original function spec by removing all the words
            ; that were specialized, as well as any type blocks or commentary
            ; string notes that came afterwards.
            ;
            while [not tail? spec] [
                either all [any-word? spec/1 | find specials spec/1] [
                    take spec
                    while [
                        all [not tail? spec | not any-word? spec/1]
                    ][
                        take spec
                    ]
                ][
                    spec: next spec
                ]
            ]

            head spec
        ]

        (fail "Unknown function class")
    ]
]


body-of: function [
    value [any-value!]
][
    body: reflect :value 'body
    unless function? :value [return body]

    ; FUNCTION! has a number of special tricks for its implementation, where
    ; the body information is not what you could just pass to MAKE FUNCTION!
    ; and get equivalent behavior.  The goal of this usermode code is to
    ; build simulated equivalants that *could* be passed to MAKE FUNCTION!.
    ;
    switch func-class-of :value [
        1 [
            ; Native.  The actual "body of" is a function pointer, which
            ; is currently rendered as a HANDLE!.
            ;
            ; !!! Near-term-future feature: native bodies able to provide
            ; equivalent user-mode code, if provided via native/body

            remark: [
                comment {Native code, implemented in C (this body is fake)}
            ]

            either block? body [
                body: compose [
                    (remark)
                    (body)
                ]
            ][
                body: compose [
                    (remark)
                    do-native (body) <...>
                ]
            ]

            body
        ]

        2 [
            ; Usermode-written function (like this one is!) via MAKE FUNCTION!
            ; so just give back the body as-is.
            ;
            ; Note: The body given back here may be fake if it's a PROC or
            ; FUNC...though that level of fakeness is more tightly integrated
            ; into the dispatch than the other fakes here.  It's needed for
            ; efficient definitional returns, but pains were taken to ensure
            ; that this could indeed be done by equivalent user mode code.

            body
        ]

        3 [
            ; Action.  Currently action bodies are numbers, because the
            ; `switch` statement in C that implements type-specific actions
            ; isn't able to switch on the function's identity (via paramlist)

            compose [
                comment {Type-Specific action method (internal, ATM)}
                do-action (body) <...>
            ]
        ]

        4 [
            ; Command.  These are a historical extension mechanism, used to
            ; make native routines that are built with the extension API
            ; (as opposed to Ren-C).

            compose [
                comment {Rebol Lib (RL_Api) Extension (made by make-command)}
                do-command (body) <...>
            ]
        ]

        5 [
            ; FFI Routine...likely to become user function.

            compose [
                comment {FFI Bridge to C Function (via make-routine)}
                do-routine (body) <...>
            ]
        ]

        6 [
            ; FFI Callback...likely to be folded in as an internal mechanism
            ; in the FFI for calling ordinary user functions.

            append copy [
                comment {FFI C thunk for Rebol Function (via make-callback)}
            ] body
        ]

        7 [
            ; Function Specialization.  These are partially (or fully) filled
            ; frames that EVAL automatically, by being stuffed in FUNCTION!
            ;
            ; Currently the low-level spec of these functions is just a single
            ; element series of the name of what is specialized.
            ;
            ; !!! It would be possible to inject commentary information on
            ; the specialized fields for what they meant, by taking it that
            ; from the original function's spec and putting it inline with
            ; the specialization assignment.

            spec-with-word: reflect :value 'spec
            assert [word? first spec-with-word]

            compose [
                comment (
                    rejoin [
                        {Specialization of}
                        space first spec-with-word space
                        {(this body is fake)}
                    ]
                )
                eval (body) <...>
            ]
        ]

        (fail "Unknown function class")
    ]
]
