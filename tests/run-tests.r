Rebol [
    Title: "Run-tests"
    File: %run-tests.r
    Copyright: [2014 "Saphirion AG"]
    Author: "Ladislav Mecir"
    License: {
        Licensed under the Apache License, Version 2.0 (the "License");
        you may not use this file except in compliance with the License.
        You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "Ladislav Mecir"
    Purpose: {Click and run tests in a file or directory.}
]

; define the INCLUDE function
do %include.r

do %test-framework.r

run-tests: func [
    tests
    /local
    result log-file summary log-file-prefix suffix
] [
    if dir? tests [
        tests: dirize tests
        change-dir tests
        foreach file read tests [
            ; check if it is a test file
            if %.tst = find/last file %. [run-tests file]
        ]
        return ()
    ]

    ; having an individual file
    suffix: find/last tests %.
    log-file-prefix: copy/part tests suffix

    print "Testing ..."
    change-dir first split-path tests
    set [log-file summary] do-recover tests [] blank log-file-prefix
]

run-tests to-rebol-file system/script/args
