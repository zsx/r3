; functions/series/back.r
[
    a: [1]
    same? a back a
]
[
    a: tail of [1]
    same? head of a back a
]
; path
[
    a: 'b/c
    same? a back a
]
[
    a: tail of 'b/c
    same? head of a back back a
]
; string
[
    a: tail of "1"
    same? head of a back a
]
[
    a: "1"
    same? a back a
]
