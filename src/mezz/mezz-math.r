REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Mezzanine: Math"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
]

rsa-make-key: func [
    "Creates a key object for RSA algorithm."
][
    has [
        n:          ;modulus
        e:          ;public exponent
        d:          ;private exponent
        p:          ;prime num 1
        q:          ;prime num 2
        dp:         ;CRT exponent 1
        dq:         ;CRT exponent 2
        qinv:       ;CRT coefficient
        _
    ]
]

dh-make-key: func [
    "Creates a key object for Diffie-Hellman algorithm."
;NOT YET IMPLEMENTED
;   /generate
;       size [integer!] "Key length"
;       generator [integer!] "Generator number"
][
    has [
        priv-key:   ;private key
        pub-key:    ;public key
        g:          ;generator
        p:          ;prime modulus
        _
    ]
]

; ++ and -- were previously used to take a quoted word and increment
; it.  They were ordinary prefix operations

++: func [
    {Increment an integer or series index. Return its prior value.}
    'word [word!] "Integer or series variable"
    /local prior
][
    also (prior: get word) (
        set word either series? prior [next prior] [prior + 1]
    )
]

--: func [
    {Decrement an integer or series index. Return its prior value.}
    'word [word!] "Integer or series variable"
    /local prior
][
    also (prior: get word) (
        set word either series? prior [back prior] [prior - 1]
    )
]

mod: function [
    "Compute a nonnegative remainder of A divided by B."
    a [any-number! money! time!]
    b [any-number! money! time!]
        "Must be nonzero."
][
    ; This function tries to find the remainder that is "almost non-negative"
    ; Example: 0.15 - 0.05 - 0.1 // 0.1 is negative,
    ; but it is "almost" zero, i.e. "almost non-negative"

    ; Compute the smallest non-negative remainder
    all [negative? r: a // b | r: r + b]
    ; Use abs a for comparisons
    a: abs a
    ; If r is "almost" b (i.e. negligible compared to b), the
    ; result will be r - b. Otherwise the result will be r
    either all [(a + r) = (a + b) | positive? (r + r) - b] [r - b] [r]
]

modulo: function [
    {Wrapper for MOD that handles errors like REMAINDER.}
    return:
        {Negligible values (compared to A and B) are rounded to zero}
    a [any-number! money! time!]
    b [any-number! money! time!]
        "Absolute value will be used"
] [
    ; Coerce B to a type compatible with A
    any [any-number? a  b: make a b]
    ; Get the "accurate" MOD value
    r: mod a abs b
    ; If the MOD result is "near zero", w.r.t. A and B,
    ; return 0--the "expected" result, in human terms.
    ; Otherwise, return the result we got from MOD.
    either any [(a - r) = a | (r + b) = b] [make r 0] [r]
]

sign-of: func [
    "Returns sign of number as 1, 0, or -1 (to use as multiplier)."
    number [any-number! money! time!]
][
    case [
        positive? number [1]
        negative? number [-1]
        true [0]
    ]
]

minimum-of: func [
    {Finds the smallest value in a series}
    series [any-series!] {Series to search}
    /skip {Treat the series as records of fixed size}
    size [integer!]
    /local spot
][
    size: any [size 1]
    if 1 > size [cause-error 'script 'out-of-range size]
    spot: series
    for-skip series size [
        if lesser? first series first spot [spot: series]
    ]
    spot
]

maximum-of: func [
    {Finds the largest value in a series}
    series [any-series!] {Series to search}
    /skip {Treat the series as records of fixed size}
    size [integer!]
    /local spot
][
    size: any [:size 1]
    if 1 > size [cause-error 'script 'out-of-range size]
    spot: series
    for-skip series size [
        if greater? first series first spot [spot: series]
    ]
    spot
]

; A simple iterative implementation; returns 1 for negative
; numbers. FEEL FREE TO IMPROVE THIS!
;
factorial: func [n [integer!] /local res] [
    if n < 2 [return 1]
    res: 1
    ; should avoid doing the loop for i = 1...
    repeat i n [res: res * i]
]

; This MATH implementation is from Gabrielle Santilli circa 2001, found
; via http://www.amyresource.it/AGI/.  It implements the much-requested
; (by new users) idea of * and / running before + and - in math expressions.
;
math: function [
    {Process expression taking "usual" operator precedence into account.}
    expr [block!]
        {Block to evaluate}
    /only
        {Translate operators to their prefix calls, but don't execute}

    <static>

    slash (to-lit-word first [ / ])

    expr-val (_)

    expr-op (_)

    expression  ([
        term (expr-val: term-val)
        any [
            ['+ (expr-op: 'add) | '- (expr-op: 'subtract)]
            term (expr-val: compose [(expr-op) (expr-val) (term-val)])
        ]
    ])

    term-val (_)

    term-op (_)

    term ([
        pow (term-val: power-val)
        any [
            ['* (term-op: 'multiply) | slash (term-op: 'divide)]
            pow (term-val: compose [(term-op) (term-val) (power-val)])
        ]
    ])

    power-val (_)

    pow ([
        unary (power-val: unary-val)
        opt ['** unary (power-val: compose [power (power-val) (unary-val)])]
    ])

    unary-val (_)

    pre-uop (_)

    post-uop (_)

    unary ([
        (post-uop: pre-uop: [])
        opt ['- (pre-uop: 'negate)]
        primary
        opt ['! (post-uop: 'factorial)]
        (unary-val: compose [(post-uop) (pre-uop) (prim-val)])
    ])

    prim-val (_)

    ; WARNING: uses recursion for parens.
    primary ([
        set prim-val [any-number! | word!]
        | set prim-val group! (prim-val: translate to-block :prim-val)
    ])
][
    ; to allow recursive calling, we need to preserve our state
    recursion: reduce [
        :expr-val :expr-op :term-val :term-op :power-val :unary-val
        :pre-uop :post-uop :prim-val
    ]

    res: either parse expr expression [expr-val] [blank]

    set [
        expr-val expr-op term-val term-op power-val unary-val
        pre-uop post-uop prim-val
    ] recursion

    either only [res] [do res]
]
