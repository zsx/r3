REBOL [
    Title: "FFI Extension"
    name: 'FFI
    type: 'Extension
    version: 1.0.0
    license: {Apache 2.0}

    Notes: {
        The FFI was not initially implemented with any usermode code.  But
        just as with the routines in the SYS context, there's opportunity for
        replacing some of the non-performance-critical C that does parsing and
        processing into Rebol.  This is especially true since FFI was changed
        to use fewer specialized structures to represent ROUTINE! and
        STRUCT!, instead using arrays...to permit it to be factored into an
        extension.
    }
]

ffi-type-mappings: [
    void [<opt>]
    
    uint8 [integer!]
    int8 [integer!]
    uint16 [integer!]
    int16 [integer!]
    uint32 [integer!]
    int32 [integer!]
    uint64 [integer!]
    
    float [decimal!]
    double [decimal!]

    ; Note: FUNCTION! is only legal to pass to pointer arguments if it is was
    ; created with MAKE-ROUTINE or WRAP-CALLBACK
    ;
    pointer [integer! string! binary! vector! function!]

    rebval [any-value!]

    ; ...struct...
]


make-callback: function [
    {Helper for WRAP-CALLBACK that auto-generates the wrapped function}

    return: [function!]
    args [block!]
    body [block!]
    /fallback
        {If untrapped failure occurs during callback, fallback return value}
    fallback-value [any-value!]
        {Value to return (must be compatible with FFI type of RETURN:)}
][
    r-args: copy []

    ; !!! TBD: Use type mappings to mark up the types of the Rebol arguments,
    ; so that HELP will show useful types.
    ;
    arg-rule: [
        copy a word! (append r-args a)
        block!
        opt string!
    ]

    ; !!! TBD: Should check fallback value for compatibility here, e.g.
    ; make sure [return: [pointer]] has a fallback value that's an INTEGER!.
    ; Because if an improper type is given as the reaction to an error, that
    ; just creates *another* error...so you'll still get a panic() anyway.
    ; Better to just FAIL during the MAKE-CALLBACK call so the interpreter
    ; does not crash.
    ;
    attr-rule: [
        set-word!
        block! | word!
    ]

    if not parse args [
        opt string!
        any [ arg-rule | attr-rule ]
    ][
        fail "Unrecognized pattern in MAKE-CALLBACK function spec"
    ]

    ; print ["args:" mold args]

    wrapped-func: function r-args either fallback [
        compose/deep [
            trap/with [(body)] func [error] [
                print "** TRAPPED CRITICAL ERROR DURING FFI CALLBACK:"
                print mold error
                (fallback-value)
            ]
        ]
    ][
        body
    ]
    ; print ["wrapped-func:" mold :wrapped-func]

    wrap-callback :wrapped-func args
]
