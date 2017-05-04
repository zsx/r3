REBOL []

recycle/torture


libc: make library! %libc.so.6

x64?: 40 = fifth system/version
size_t: either x64? ['int64]['int32]
printf: make-routine libc "printf" compose/deep [
    return: [int32]
    f [pointer]
    ...
]

;(printf "hello^/" 0 [pointer])
(printf "hello %s^/" "world" [pointer])
(printf "hello^/")
(print "hi")
