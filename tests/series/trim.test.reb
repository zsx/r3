; functions/series/trim.r
; bug#83
; refinement order
[strict-equal? trim/all/with "a" "a" trim/with/all "a" "a"]
; bug#1948
["foo^/" = trim "  foo ^/"]
[[a b] = trim [a b]]
[[a b] = trim [a b _]]
[[a b] = trim [_ a b _]]
[[a b] = trim [_ a _ b _]]
