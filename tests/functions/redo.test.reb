; Better-than-nothing REDO tests

; REDO via a direct FRAME! value
[
    foo: func [n] [
        frame: context-of 'n
        if n = 0 [
            return "success!" 
        ]
        n: n - 1 
        redo frame
    ]

    "success!" = foo 100
]

; REDO via extraction of FRAME! from an ANY-WORD!
; (has binding to a FRAME! to lookup variable value)
[
    foo: func [n] [
        if n = 0 [
           return "success!"
        ]
        n: n - 1
        redo 'n
    ]

    "success!" = foo 100
]

; REDO via information in definitional RETURN
; (has binding to a FRAME! to know where to return from)
[
    foo: func [n] [
        if n = 0 [
            return "success!"
        ]
        n: n - 1
        redo :return
    ]

    "success!" = foo 100
]

; REDO locals clearing test
; (locals should be cleared on each redo)
[
    foo: func [n <local> unset-me] [
        if set? 'unset-me [
            return "failure"
        ]
        if n = 0 [
            return "success!"
        ]
        n: n - 1
        unset-me: #some-junk
        redo :return
    ]

    "success!" = foo 100
]

; REDO type checking test
; (args and refinements must pass function's type checking)
[
    foo: func [n tag [tag!]] [
        if n = 0 [
            return "success!"
        ]
        n: n - 1
        tag: #some-junk ;-- type check should fail on redo 
        redo :return
    ]

    error? trap [foo 100]
] 

; REDO phase test
; (shared frame compositions should redo the appropriate "phase")
[
    inner: func [n] [
        if n = 0 [
            return "success!"
        ]
        n: 0
        redo 'n ;-- should redo INNER, not outer
    ]

    outer: adapt 'inner [
        if n = 0 [
            return "failure"
        ]
        ;-- fall through to inner, using same frame
    ]

    "success!" = outer 1
][
    inner: func [n /captured-frame f] [
        if n = 0 [
           return "failure"
        ]
        n: 0
        redo f ;-- should redo OUTER, not INNER
    ]

    outer: adapt 'inner [
        if n = 0 [
            return "success!"
        ]

        f: context-of 'n
        captured-frame: true

        ;-- fall through to inner
        ;-- it is running in the same frame's memory, but...
        ;-- F is a FRAME! value that stowed outer's "phase"
    ]

    "success!" = outer 1
]
