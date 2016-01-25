REBOL [
    Title: "Rebol2 and R3-Alpha Future Bridge to Ren-C"
    Rights: {
        Rebol 3 Language Interpreter and Run-time Environment
        "Ren-C" branch @ https://github.com/metaeducation/ren-c

        Copyright 2012 REBOL Technologies
        Copyright 2012-2015 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        These routines can be run from R3-Alpha or Rebol2 to make them act
        more like the vision of Rebol3-Beta and beyond (as conceived by the
        "Ren-C" initiative).

        It also must remain possible to run it from Ren-C without disrupting
        the environment.  This is because the primary motivation for its
        existence is to shim older R3-MAKE utilities to be compatible with
        Ren-C...and the script is run without knowing whether the R3-MAKE
        you are using is old or new.  No canonized versioning strategy has
        been yet chosen, so words are "sniffed" for existing definitions in
        this somewhat simplistic method.

        !!! Because the primary purpose is for Ren-C's bootstrap, the file
        is focused squarely on those needs.  However, it is a beginning for
        a more formalized compatibility effort.  Hence it is awaiting someone
        who has a vested interest in Rebol2 or R3-Alpha code to become a
        "maintenance czar" to extend the concept.  In the meantime it will
        remain fairly bare-bones, but enhanced if-and-when needed.
    }
    Description: {
        Currently uses a simple dialect for remapping constructs:

        word! <as> [get-word! | (expression)]
            If the word is unset, set it to new value (otherwise leave it)

        word! <to> word!
            If left word defined, give right word its value, unset the left
            (Note: was >>, which was clearer, but not a WORD! in Rebol2)
    }
]

; Older versions of Rebol had a different concept of what FUNCTION meant
; (an arity-3 variation of FUNC).  Eventually the arity-2 construct that
; did locals-gathering by default named FUNCT overtook it, with the name
; FUNCT deprecated.
;
unless (copy/part words-of :function 2) = [spec body] [
    function: :funct
]

; A lone vertical bar is an "expression barrier" in Ren-C, but a word character
; in other situations.  Having a word behave as a function that returns an
; UNSET! in older Rebols is not quite the same, but can have a similar effect
; in terms of creating errors if picked up by normal function args.
;
if word? '| [
    set '| does []
]


unless find words-of :set /opt [
    ;
    ; SET/OPT is the Ren-C replacement for SET/ANY, with /ANY supported
    ; via <r3-legacy>.  But Rebol2 and R3-Alpha do not know /OPT.
    ;
    lib-set: :set ; overwriting lib/set for now
    set: func [
        {Sets a word, path, block of words, or context to specified value(s).}

        ;-- Note: any-context! not defined until after migrations
        target [any-word! any-path! block! any-object!]
            {Word, block of words, path, or object to be set (modified)}

        ;-- Note: opt-any-value! not defined until after migrations
        value [any-type!]
            "Value or block of values"
        /opt
            "Value is optional, and if no value is provided then unset the word"
        /pad
            {For objects, set remaining words to NONE if block is too short}
        /any
            "Deprecated legacy synonym for /opt"
    ][
        set_ANY: any
        any: :lib/any ;-- in case it needs to be used
        opt_ANY: opt
        opt: none ;-- no OPT defined yet, but just in case, keep clear

        apply :lib-set [target :value (any [opt_ANY set_ANY]) pad]
    ]
]

unless find words-of :get /opt [
    ;
    ; GET/OPT is the Ren-C replacement for GET/ANY, with /ANY supported
    ; via <r3-legacy>.  But Rebol2 and R3-Alpha do not know /OPT.
    ;
    lib-get: :get
    get: function [
        {Gets the value of a word or path, or values of a context.}
        source
            "Word, path, context to get"
        /opt
            "The source may optionally have no value (allows returning UNSET!)"
        /any
            "Deprecated legacy synonym for /OPT"
    ][
        set_ANY: any
        any: :lib/any ;-- in case it needs to be used
        opt_ANY: opt
        opt: none ;-- no OPT defined yet, but just in case, keep clear

        apply :lib-get [source (any [opt_ANY set_ANY])]
    ]
]

if paren? reduce quote () [
    ;
    ; R3-Alpha would only REDUCE a block and pass through other outputs.
    ; REDUCE in Ren-C (and also in Red) is willing to reduce anything that
    ; does not require EVAL-like argument consumption (so GROUP!, GET-WORD!,
    ; GET-PATH!).
    ;
    lib-reduce: :reduce
    reduce: func [
        {Evaluates expressions and returns multiple results.}
        value
        /no-set
            "Keep set-words as-is. Do not set them."
        /only
            "Only evaluate words and paths, not functions"
        words [block! none!]
            "Optional words that are not evaluated (keywords)"
        /into
            {Output results into a series with no intermediate storage}
        target [any-block!]
    ][
        either block? :value [
            apply :lib-reduce [value no-set only words into target]
        ][
            ; For non-blocks, put the item in a block, reduce the block,
            ; then pick the first element out.  This may error (e.g. if you
            ; try to reduce a word looking up to a function taking arguments)
            ;
            ; !!! Simple with no refinements for now--enhancement welcome.
            ;
            assert [not no-set not only not into]
            first (lib-reduce lib-reduce [:value])
        ]
    ]
]

; === ABOVE ROUTINES NEEDED TO RUN BELOW ===


migrations: [
    ;
    ; Note: EVERY cannot be written in R3-Alpha because there is no way
    ; to write loop wrappers, given lack of definitionally scoped return
    ;
    for-each <as> :foreach

    ; Ren-C replaces the awkward term PAREN! with GROUP!  (Retaining PAREN!
    ; for compatibility as pointing to the same datatype).  Older Rebols
    ; haven't heard of GROUP!, so establish the reverse compatibility.
    ;
    group! <as> :paren!
    group? <as> :paren?

    ; Not having category members have the same name as the category
    ; themselves helps both cognition and clarity inside the source of the
    ; implementation.
    ;
    any-array? <as> :any-block?
    any-array! <as> :any-block!
    any-context? <as> :any-object?
    any-context! <as> :any-object!

    ; *all* typesets now ANY-XXX to help distinguish them from concrete types
    ; https://trello.com/c/d0Nw87kp
    ;
    any-scalar? <as> :scalar?
    any-scalar! <as> :scalar!
    any-series? <as> :series?
    any-series! <as> :series!
    any-number? <as> :number?
    any-number! <as> :number!

    ; ANY-VALUE! is anything that isn't UNSET!.  OPT-ANY-VALUE! is a
    ; placeholder for [<opt> ANY-VALUE!] or [#opt ANY-VALUE] in function specs,
    ; a final format has not been picked for the generator to use.
    ;
    any-value? <as> (func [item [any-type!]] [not unset? :item])
    any-value! <as> (difference any-type! (make typeset! [unset!]))
    opt-any-value! <as> :any-type!

    ; Renamings to conform to ?-means-returns-true-false rule
    ; https://trello.com/c/BxLP8Nch
    ;
    length? <to> length
    index? <to> index-of
    offset? <to> offset-of
    type? <to> type-of

    ; "optional" (a.k.a. UNSET!) handling
    opt <as> (func [
        {NONE! to unset, all other value types pass through.}
        value [any-type!]
    ][
        either none? get/opt 'value [()][
            get/opt 'value
        ]
    ])

    to-value <as> (func [
        {Turns unset to NONE, with ANY-VALUE! passing through. (See: OPT)}
        value [any-type!]
    ][
        either unset? get/opt 'value [none][:value]
    ])

    something? <as> (func [value [any-type!]] [
        not any [
            unset? :value
            none? :value
        ]
    ])

    ; It is not possible to make a version of eval that does something other
    ; than everything DO does in an older Rebol.  Which points to why exactly
    ; it's important to have only one function like eval in existence.
    ;
    eval <as> :do

    ; Ren-C's FAIL dialect is still being designed, but the basic is to be
    ; able to ramp up from simple strings to block-composed messages to
    ; fully specifying ERROR! object fields.  Most commonly it is a synonym
    ; for `do make error! form [...]`.
    ;
    fail <as> (func [
        {Interrupts execution by reporting an error (TRAP can intercept it).}
        reason [error! string! block!]
            "ERROR! value, message string, or failure spec"
    ][
        case [
            error? reason [do error]
            string? reason [do make error! reason]
            block? reason [
                for-each item reason [
                    unless any [
                        any-scalar? :item
                        string? :item
                        group? :item
                        all [
                            word? :item
                            not any-function? get :item
                        ]
                    ][
                        do make error! (
                            "FAIL requires complex expressions in a GROUP!"
                        )
                    ]
                ]
                do make error! form reduce reason
            ]
        ]
    ])

    ; R3-Alpha and Rebol2 did not allow you to make custom infix operators.
    ; There is no way to get a conditional infix AND using those binaries.
    ; In some cases, the bitwise and will be good enough for logic purposes...
    ;
    and* <as> :and
    and? <as> (func [a b] [true? all [:a :b]])
    and <as> :and ; see above

    or+ <as> :or
    or? <as> (func [a b] [true? any [:a :b]])
    or <as> :or ; see above

    xor- <as> :xor
    xor? <as> (func [a b] [true? any [all [:a (not :b)] all [(not :a) :b]]])
]


; Main remapping... use parse to process the dialect
;
unless parse migrations [
    some [
        ;-- Note: GROUP! defined during migration, so use PAREN! here
        [set left word! <as> set right [get-word! | paren!]] (
            unless value? left [
                set left either paren? right [reduce right] [get right]
            ]
        )
    |
        [set left word! <to> set right word!] (
            unless value? right [
                set right get left
                unset left
            ]
        )
    ]
][
    print ["last left:" mold left]
    print ["last right:" mold right]

    do make error! "%r2r3-future.r did not parse migrations completely"
]
