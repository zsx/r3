; datatypes/map.r
; map! =? hash! in R2/Forward, R2 2.7.7+
[empty? make map! []]
[empty? make map! 4]
; The length of a map is the number of key/value pairs it holds.
[2 == length? make map! [a 1 b 2]]  ; 4 in R2, R2/Forward
[m: make map! [a 1 b 2] 1 == m/a]
[m: make map! [a 1 b 2] 2 == m/b]
[
    m: make map! [a 1 b 2]
    error? trap [m/c]
]
[m: make map! [a 1 b 2] m/c: 3 3 == m/c]
; Maps contain key/value pairs and must be created from blocks of even length.
[error? try [make map! [1]]]
[empty? clear make map! [a 1 b 2]]
[
    #1930
    m: make map! 8
    clear m
    not find m 'a
]
