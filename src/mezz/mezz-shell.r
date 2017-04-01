REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Mezzanine: Shell-like Command Functions"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
]

ls:     :list-dir
pwd:    :what-dir

rm: does [
    fail "Use DELETE, not RM (Rebol REMOVE is different, shell dialect coming)"
]

mkdir:  :make-dir

cd: func [
    "Change directory (shell shortcut function)."
    'path [<end> file! word! path! string!]
        "Accepts %file, :variables and just words (as dirs)"
][
    switch type-of :path [
        _ [print what-dir]
        :file! [change-dir path]
        :string! [change-dir to-rebol-file path]
        :word! :path! [change-dir to-file path]
    ]
]

more: func [
    "Print file (shell shortcut function)."
    'file [file! word! path! string!]
        "Accepts %file and also just words (as file names)"
][
    ; !!! to-word necessary as long as OPTIONS_DATATYPE_WORD_STRICT exists
    print deline to-string read switch to-word type-of :file [
        file! [file]
        string! [to-rebol-file file]
        word! path! [to-file file]
    ]
]


echo: procedure [
    "Copies console I/O to a file."
    
    'instruction [file! string! block! word!]
        {File or template with * substitution, or command: [ON OFF RESET].}

    <has>
    target ([%echo * %.txt])
    form-target
    sub ("")
    old-input (copy :input)
    old-write-stdout (copy :write-stdout)
    hook-in
    hook-out
    logger
    ensure-echo-on
    ensure-echo-off
][
    ; Sample "interesting" feature, be willing to form the filename by filling
    ; in the blank with a substitute string you can change.
    ;
    form-target: default [func [return: [file!]] [
        either block? target [
            as file! unspaced replace (copy target) '* (
                either empty? sub [[]] [unspaced ["-" sub]]
            )
        ][
            target
        ]
    ]]

    logger: default [func [value][
        write/append form-target either char? value [to-string value][value]
        value
    ]]

    ; Installed hook; in an ideal world, WRITE-STDOUT would not exist and
    ; would just be WRITE, so this would be hooking WRITE and checking for
    ; STDOUT or falling through.  Note WRITE doesn't take CHAR! right now.
    ;
    hook-out: default [proc [
        value [string! char! binary!]
            {Text to write, if a STRING! or CHAR! is converted to OS format}
    ][
        old-write-stdout value
        logger value
    ]]

    ; It looks a bit strange to look at a console log without the input
    ; being included too.  Note that hooking the input function doesn't get
    ; the newlines, has to be added.
    ;
    hook-in: default [
        chain [
            :old-input
                |
            func [value] [
                logger value
                logger newline
                value ;-- hook still needs to return the original value
            ]
        ]
    ]

    ensure-echo-on: default [does [
        ;
        ; Hijacking is a NO-OP if the functions are the same.
        ; (this is indicated by a BLANK! return vs a FUNCTION!)
        ;
        hijack 'write-stdout 'hook-out
        hijack 'input 'hook-in
    ]]

    ensure-echo-off: default [does [
        ;
        ; Restoring a hijacked function with its original will
        ; remove any overhead and be as fast as it was originally.
        ;
        hijack 'write-stdout 'old-write-stdout
        hijack 'input 'old-input
    ]]

    case [
        word? instruction [
            switch instruction [
                on [ensure-echo-on]
                off [ensure-echo-off]
                reset [
                    delete form-target
                    write/append form-target "" ;-- or just have it not exist?
                ]
            ] else [
                word: to-uppercase word
                fail [
                    "Unknown ECHO command, not [ON OFF RESET]"
                        |
                    unspaced ["Use ECHO (" word ") to force evaluation"]
                ]
            ]
        ]

        string? instruction [
            sub: instruction
            ensure-echo-on
        ]

        any [block? instruction | file? instruction] [
            target: instruction
            ensure-echo-on
        ]
    ]
]
