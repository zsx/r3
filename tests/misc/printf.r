REBOL []

recycle/torture


libc: make library! %libc.so.6

x64?: 40 = fifth system/version
size_t: either x64? ['int64]['int32]
printf: make-routine compose/deep [
    [
        f [pointer]
        ...
    ]
    (libc) "printf"
]

printf ["hello"]
print "hi"
