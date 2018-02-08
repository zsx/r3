; functions/series/trim.r

; bug#83
; refinement order
[strict-equal? trim/all/with "a" "a" trim/with/all "a" "a"]

; bug#1948
["foo^/" = trim "  foo ^/"]

[#{BFD3} = trim #{0000BFD30000}]
[#{10200304} = trim/with #{AEAEAE10200304BDBDBD} #{AEBD}]

["a  ^/  b  " = trim/head "  a  ^/  b  "]
["  a  ^/  b" = trim/tail "  a  ^/  b  "]
["foo^/^/bar^/" = trim "  foo  ^/ ^/  bar  ^/  ^/  "]
["foobar" = trim/all "  foo  ^/ ^/  bar  ^/  ^/  "]
["foo bar" = trim/lines "  foo  ^/ ^/  bar  ^/  ^/  "]

[[a b] = trim [a b]]
[[a b] = trim [a b _]]
[[a b] = trim [_ a b _]]
[[a _ b] = trim [_ a _ b _]]
[[a b] = trim/all [_ a _ b _]]
[[_ _ a _ b] = trim/tail [_ _ a _ b _ _]]
[[a _ b _ _] = trim/head [_ _ a _ b _ _]]
[[a _ b] = trim/head/tail [_ _ a _ b _ _]]
