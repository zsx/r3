Rebol [
    Title: "Log diff"
    File: %log-diff.r
    Copyright: [2012 "Saphirion AG"]
    License: {
        Licensed under the Apache License, Version 2.0 (the "License");
        you may not use this file except in compliance with the License.
        You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "Ladislav Mecir"
    Purpose: "Test framework"
]

do %test-parsing.r

make-diff: func [
    old-log [file!]
    new-log [file!]
    diff-file [file!]
    /local old-log-contents new-log-contents
    old-test old-result new-test new-result
    new-successes new-failures new-crashes
    progressions regressions removed summary
    next-old-log next-new-log
] [
    if exists? diff-file [delete diff-file]

    collect-logs old-log-contents: copy [] old-log
    collect-logs new-log-contents: copy [] new-log

    sort/case/skip old-log-contents 2
    sort/case/skip new-log-contents 2

    ; counter initialization
    new-successes:
    new-failures:
    new-crashes:
    progressions:
    regressions:
    removed:
    unchanged:
    0

    ; cycle initialization
    set [old-test old-result] old-log-contents
    old-log-contents: skip old-log-contents 2

    set [new-test new-result] new-log-contents
    new-log-contents: skip new-log-contents 2

    while [any [old-test new-test]] [
        case [
            all [
                new-test
                new-result <> 'skipped
                any [
                    blank? old-test
                    all [
                        strict-not-equal? old-test new-test
                        old-test == second sort/case reduce [new-test old-test]
                    ]
                    all [
                        old-test == new-test
                        old-result = 'skipped
                    ]
                ]
            ] [
                ; fresh test
                write/append diff-file rejoin [
                    new-test
                    " "
                    switch new-result [
                        succeeded [
                            new-successes: new-successes + 1
                            "succeeded"
                        ]
                        failed [
                            new-failures: new-failures + 1
                            "failed"
                        ]
                        crashed [
                            new-crashes: new-crashes + 1
                            "crashed"
                        ]
                    ]
                    "^/"
                ]
            ]
            all [
                old-test
                old-result <> 'skipped
                any [
                    blank? new-test
                    all [
                        strict-not-equal? new-test old-test
                        new-test == second sort/case reduce [old-test new-test]
                    ]
                    all [
                        new-test == old-test
                        new-result = 'skipped
                    ]
                ]
            ] [
                ; removed test
                removed: removed + 1
                write/append diff-file rejoin [
                    old-test
                    " removed^/"
                ]
            ]
            any [
                old-result = new-result
                strict-not-equal? old-test new-test
            ] [unchanged: unchanged + 1]
            ; having one test with different results
            (
                write/append diff-file new-test
                any [
                    old-result = 'succeeded
                    all [
                        old-result = 'failed
                        new-result = 'crashed
                    ]
                ]
            ) [
                ; regression
                regressions: regressions + 1
                write/append diff-file rejoin [" regression, " new-result "^/"]
            ]
            'else [
                ; progression
                progressions: progressions + 1
                write/append diff-file rejoin [" progression, " new-result "^/"]
            ]
        ]
        next-old-log: all [
            old-test
            any [
                blank? new-test
                old-test == first sort/case reduce [old-test new-test]
            ]
        ]
        next-new-log: all [
            new-test
            any [
                blank? old-test
                new-test == first sort/case reduce [new-test old-test]
            ]
        ]
        if next-old-log [
            if old-test == pick old-log-contents 1 [
                print old-test
                do make error! {duplicate test in old-log}
            ]
            set [old-test old-result] old-log-contents
            old-log-contents: skip old-log-contents 2
        ]
        if next-new-log [
            if new-test == pick new-log-contents 1 [
                print new-test
                do make error! {duplicate test in new-log}
            ]
            set [new-test new-result] new-log-contents
            new-log-contents: skip new-log-contents 2
        ]
    ]

    print "Done."

    summary: rejoin [
        "^/new-successes: " new-successes
        "^/new-failures: " new-failures
        "^/new-crashes: " new-crashes
        "^/progressions: " progressions
        "^/regressions: " regressions
        "^/removed: " removed
        "^/unchanged: " unchanged
        "^/total: " new-successes + new-failures + new-crashes + progressions
        + regressions + removed + unchanged
    ]
    print summary

    write/append diff-file rejoin ["^/Summary:^/" summary "^/"]
]

make-diff to-file first load system/script/args to-file second load system/script/args %diff.r
