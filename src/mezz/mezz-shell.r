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
