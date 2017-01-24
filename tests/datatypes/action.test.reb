; datatypes/action.r

[action? :abs]
[not action? 1]
[function! = type-of :abs]
; bug#1659
; actions are active
[1 == do reduce [:abs -1]]
