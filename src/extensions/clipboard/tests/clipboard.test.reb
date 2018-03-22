; !!! The R3-Alpha clipboard returned bytes, because that's what READ
; returns.  Hence TO-STRING is needed in these cases.  It may be that LOAD
; could be more abstract, and get some cue from the metadata of what type
; to return.

; empty clipboard
(
    write clipboard:// ""
    c: to-string read clipboard://
    string? c and (empty? c)
)

; ASCII string
(
    write clipboard:// c: "This is a test."
    d: to-string read clipboard://
    strict-equal? c d
)

; Separate open step
(
    p: open clipboard://
    write p c: "Clipboard port test"
    strict-equal? c to-string read p
)

; Unicode string
(
    write clipboard:// c: "Příliš žluťoučký kůň úpěl ďábelské ódy."
    strict-equal? to-string read clipboard:// c
)

; WRITE returns a PORT! in R3
;
(equal? to-string read write clipboard:// c: "test" c)
