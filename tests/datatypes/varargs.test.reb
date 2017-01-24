[
    foo: func [x [integer! <...>]] [
        sum: 0
        while [not tail? x] [
            sum: sum + take x
        ]
    ]
    y: (z: foo 1 2 3 | 4 5)
    all [y = 5 | z = 6]
]
[
    foo: func [x [integer! <...>]] [make block! x]
    [1 2 3 4] = foo 1 2 3 4
]

;-- !!! Chaining was removed, this test should be rethought or redesigned.
;[
;    alpha: func [x [integer! string! tag! <...>]] [
;        beta 1 2 (x) 3 4
;    ]
;    beta: func ['x [integer! string! word! <...>]] [
;        reverse (make block! x)
;    ]
;    all [
;        [4 3 "back" "wards" 2 1] = alpha "wards" "back"
;            |
;        error? trap [alpha <some> <thing>] ;-- both checks are applied in chain
;            |
;        [4 3 other thing 2 1] = alpha thing other
;    ]
;]

[
    ;-- leaked VARARGS! cannot be accessed after call is over
    error? trap [take eval (foo: func [x [integer! <...>]] [x])]
]

