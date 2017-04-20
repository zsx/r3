REBOL []

to-c-string: function [
    s [any-string! binary!]
][
    if binary? s [
        ret: copy {"}
        for-next s [
            i: to integer! first s
            append ret rejoin [
                "\x"
                pick "0123456789ABCDEF" (1 + shift i -4)
                pick "0123456789ABCDEF" (1 + and~ i 15)
            ]
        ]
        append ret {"}
        return ret
    ]

    s: to string! s
    parse s [
        while [
            change #"^(07)" "\a"
            | change #"^(08)" "\b"
            | change #"^(0A)" "\n"
            | change #"^(0C)" "\f"
            | change #"^(0D)" "\r"
            | change #"^-" "\t"
            | change #"^(0B)" "\v"
            | change #"\" "\\"
            | change #"'" "\'"
            | change #"^"" "\^""
            | change #"?" "\?"
            | skip
        ]
    ]
    insert s "^""
    append s "^""

    head s
]

make-block: procedure [
    reb-block [block!]
    title [string!] "File title"
    func-name [string!] "Function name"
    dest [file!] "Destination file"
] [

    emit-header title dest
    err: _
    arr: _
    any-array-rule: [
        (
            either blank? arr [
                emit-line [ "^{^{" ]
            ][
                emit-line [
{^{
REBVAL *v = Alloc_Tail_Array(arr);
^{
    REBARR *arr = Make_Array(} length arr {);
    Init_Any_Array(v, } case [
            block? arr ["REB_BLOCK"]
            group? arr ["REB_GROUP"]
            path? arr ["REB_PATH"]
            set-path? arr ["REB_SET_PATH"]
            get-path? arr ["REB_GET_PATH"]
            lit-path? arr ["REB_LIT_PATH"]
            'else [
                fail spaced ["unknown array type:" type-of arr]
            ]
        ] {, arr);^/}
    ]
    ])
        any [
            trivial-rule
            | any-word-rule
            | int-rule
            | decimal-rule
            | char-rule
            | any-string-rule
            | tuple-rule
            | time-rule
            | date-rule
            | and set arr any-array! into any-array-rule
            | end
            | pos: if (void? first pos) void-rule accept ;there is not void! type
            | pos: (fail spaced ["Unhandled type:" type-of first pos "," either void? first pos ["void"][mold first pos]])
        ]
        ( emit-line ["}^/}"])
    ]

    any-word-rule: [
        set w any-word! (emit-line [
{^{
REBVAL *w = Alloc_Tail_Array(arr);
REBSTR *spelling=Intern_UTF8_Managed(cb_cast("} s: to string! to word! w {"),} length s {);
Init_Any_Word(w, } case [
        word? w ["REB_WORD"]
        set-word? w ["REB_SET_WORD"]
        get-word? w ["REB_GET_WORD"]
        lit-word? w ["REB_LIT_WORD"]
        refinement? w ["REB_REFINEMENT"]
        issue? w ["REB_ISSUE"]
        'else [
            fail spaced ["Unknown word type:" type-of w]
        ]
    ]
{, spelling);
^}
}
        ])
    ]

    int-rule: [
        set i integer! (emit-line [
{^{
REBVAL *i = Alloc_Tail_Array(arr);
SET_INTEGER(i, } mold i {);
^}}
        ])
    ]

    decimal-rule: [
        set d decimal! (emit-line [
{^{
REBVAL *d = Alloc_Tail_Array(arr);
SET_DECIMAL(d, } mold d {);
^}
}
        ])
    ]

    any-string-rule: [
        set s [any-string! | binary!] (emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    Init_Any_Series(v, } case [
            string? s [ "REB_STRING"]
            tag? s ["REB_TAG"]
            file? s ["REB_FILE"]
            email? s ["REB_EMAIL"]
            url? s ["REB_URL"]
            binary? s ["REB_BINARY"]
            'else [
                fail spaced ["Unknown string type:" type-of s]
            ]
        ] {, Copy_Bytes(cb_cast(} to-c-string s {), } length s {));
^}
}
        ])
    ]

    trivial-rule: [
        set v [bar! | lit-bar! | blank!] (emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    SET_} case [
            bar? v ["BAR"]
            lit-bar? v ["LIT_BAR"]
            blank? v ["BLANK"]
        ] {(v);
^}
}
        ])
    ]

    void-rule: [
        (
            print ["voids found"]
            emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    SET_VOID(v);
^}
}
        ]) skip
    ]

    char-rule: [
        set v char! (emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    SET_CHAR(v, } to integer! v {);
^}
}
        ])
    ]

    tuple-rule: [
        set v tuple! (
            bin: make binary! length v
            for i 1 length v 1 [
                append bin v/(i)
            ]
            emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    SET_TUPLE(v, } to-c-string bin {,} length v {);
^}
}
        ])
    ]

    time-rule: [
        set v time! (
            emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    SET_TIME(v, } to integer! 1E9 * to decimal! v {);
^}
}
        ])
    ]

    date-rule: [
        set v date! (
            emit-line [
{^{
    REBVAL *v = Alloc_Tail_Array(arr);
    Set_Date_UTC(v, } v/year {,} v/month {,} v/day {,}
        either error? try [nanosec: to integer! 1E9 * to decimal! v/time][0][nanosec] {,}
        either error? try [tz: to integer! v/zone / 00:15:00][0][tz] ; +/- 15 min
        {);
^}
}
        ])
    ]


    emit-line [
    {
    #include "sys-core.h"

    REBARR *} func-name {(void)
    ^{
        REBARR *arr = Make_Array(} length reb-block {);
    }
    ]

    parse reb-block any-array-rule

    emit-line [
    {   MANAGE_ARRAY(arr);
        return arr;
    ^}
    }
    ]

    write-emitted dest
]

