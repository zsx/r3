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


map: func [
    {Make a map value (hashed associative block).}
    val
][
    make map! :val
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
