; functions/context/set.r
; bug#1763
[a: 1 all [error? try [set [a] reduce [()]] a = 1]]
[a: 1 attempt [set [a b] reduce [2 ()]] a = 1]
[x: has [a: 1] all [error? try [set x reduce [()]] x/a = 1]]
[x: has [a: 1 b: 2] all [error? try [set x reduce [3 ()]] x/a = 1]]
; set [:get-word] [word]
[a: 1 b: _ set [b] [a] b = 'a]

; Behavior in R3-Alpha allowed `set [a b] 10` to set both, but created a
; bad variance with `set [a b] [10]`.  Rather than complicate SET with /ONLY,
; the behavior going forward is just to allow blanking with pad, so that
; `set/pad [a b] blank` gives the same effect as `set/pad [a b] []`, except
; returning blank instead of the empty block.
[
    a: 10
    b: 20
    all? [blank = set/pad [a b] blank | blank? a | blank? b]
]
