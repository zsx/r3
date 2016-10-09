REBOL [
    Title: {Initial test for user natives by @ShixinZeng}
]

c-fib: first make-native/opt [
    "N_c_fib" [
        "nth Fibonacci Number"
        n [integer!]
    ]
] {
    REBNATIVE(c_fib) {
        PARAM(1, n);

        int i = VAL_INT64(ARG(n));
        int i0 = 0, i1 = 1;
        //MARK_CELL_WRITABLE_IF_CPP_DEBUG(D_OUT);
        if (i < 0) SET_INTEGER(D_OUT, -1);
        if (i <= 1) SET_INTEGER(D_OUT, i);
        while (-- i > 0) {
            int t = i1;
            i1 = i1 + i0;
            i0 = t;
        }
        SET_INTEGER(D_OUT, i1);
        D_OUT->header.bits |= 1 << (GENERAL_VALUE_BIT + 5);
        return R_OUT;
    }
} compose [
    runtime-path (join first split-path system/options/boot %tcc)
]

fib: func [
    n [integer!]
][
    if n < 0 [return -1]
    if n <= 1 [return n]
    i0: 0
    i1: 1
    while [n > 1] [
        t: i1
        i1: i0 + i1
        i0: t
        -- n
    ]
    i1
]

print ["fib 30:" c-fib 30]
print ["fib 30:" fib 30]

n-loop: 10000

c-t: dt [
    loop n-loop [c-fib 30]
]
r-t: dt [
    loop n-loop [fib 30]
]
print ["c-t:" c-t "r-t:" r-t "improvement:" r-t / c-t]
