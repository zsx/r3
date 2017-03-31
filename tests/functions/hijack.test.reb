; better than-nothing HIJACK tests

[
    foo: func [x] [x + 1]
    another-foo: :foo

    old-foo: copy :foo

    all? [
        (old-foo 10) = 11
        hijack 'foo func [x] [(old-foo x) + 20]
        (old-foo 10) = 11
        (foo 10) = 31
        (another-foo 10) = 31
    ]
]

