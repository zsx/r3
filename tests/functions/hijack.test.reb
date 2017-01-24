; better than-nothing HIJACK tests

[
    foo: func [x] [x + 1]
    another-foo: :foo

    old-foo: hijack 'foo _

    all? [
        (old-foo 10) = 11
        error? try [foo 10] ;-- hijacked function captured but no body
        blank? hijack 'foo func [x] [(old-foo x) + 20]
        (foo 10) = 31
        (another-foo 10) = 31
    ]
]

