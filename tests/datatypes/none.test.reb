; datatypes/none.r
[blank? blank]
[not blank? 1]
[blank! = type-of blank]
; literal form
[blank = _]
; bug#845
[blank = _]
[blank = #] ;-- Deprecated!
[blank = make blank! blank]
[blank = to blank! blank] ;-- only thing you can convert a blank to
[blank = to blank! 1]
["_" = mold blank]
; bug#1666
; bug#1650
[
    f: does [#]
    # == f
]
