REBOL [
    Title: {Demo tunneling of REBVAL* through routine to callback in FFI}
    Description: {
        There are two versions of quicksort in the C library.  Plain `qsort`
        is written in such a way that if your comparator needs any information
        besides the two items to compare, it has to get that from global
        variables.  `qsort_r` takes an additional void pointer parameter
        which it passes through to the comparator, which could hold some
        state information and thus eliminate the requirement to use global
        variables for any parameterization of the comparator.

        This demonstrates the use of the FFI argument type of REBVAL.
        While the purpose of the FFI is to talk to libraries that likely
        are not linked to any Rebol APIs (and hence would not be able to
        make use of a REBVAL), this shows the use of it to "tunnel" a
        Rebol value through to a written-in-Rebol comparator callback.
    }
]

recycle/torture


f: func [
    a [integer!] "pointer to an integer"
    b [integer!] "pointer to an integer"
    arg [any-value!] "some tunneled argument"
][
    print mold arg

    i: make struct! compose/deep [
        [raw-memory: (a)]
        i [int32]
    ]
    j: make struct! compose/deep [
        [raw-memory: (b)]
        i [int32]
    ]
    case [
        i/i = j/i [0]
        i/i < j/i [-1]
        i/i > j/i [1]
     ]
]

cb: make callback! [
    [
        a [pointer]
        b [pointer]
        arg [rebval]
        return: [int64]
    ]
    :f
]

libc: make library! %libc.so.6

x64?: 40 = fifth system/version
size_t: either x64? ['int64]['int32]

; This tests the compatibility shim for MAKE that lets MAKE ROUTINE! work,
; though that is deprecated (use MAKE-ROUTINE or MAKE-ROUTINE-RAW)
;
qsort_r: make routine! compose/deep [
    [
        base [pointer]
        nmemb [(size_t)]
        size [(size_t)]
        comp [pointer]
        arg [rebval]
    ]
    (libc) "qsort_r"
]

array: make vector! [integer! 32 5 [10 8 2 9 5]]
print ["array:" mold array]
probe (reflect :cb 'addr)
qsort_r array 5 4 :cb <A Tunneled Tag>
print ["array:" mold array] ; [2 5 8 9 10]

close libc
