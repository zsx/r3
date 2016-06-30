Rebol [
    Title: "Catch-any"
    File: %catch-any.r
    Copyright: [2012 "Saphirion AG"]
    License: {
        Licensed under the Apache License, Version 2.0 (the "License");
        you may not use this file except in compliance with the License.
        You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "Ladislav Mecir"
    Purpose: "Catch any REBOL exception"
]

make object! [
    do-block: func [
        ; helper for catching BREAK, CONTINUE, THROW or QUIT
        block [block!]
        exception [word!]
        /local result
    ] [
        ; TRY wraps CATCH/QUIT to circumvent bug#851
        try [
            catch/quit [
                catch [
                    loop 1 [
                        try [
                            set exception 'return
                            print mold block ;-- !!! make this an option
                            set/opt 'result do block
                            set exception blank
                            return :result
                        ]
                        ; an error was triggered
                        set exception 'error
                        return ()
                    ]
                    ; BREAK or CONTINUE
                    set exception 'break
                    return ()
                ]
                ; THROW
                set exception 'throw
                return ()
            ]
            ; QUIT
            set exception 'quit
            return ()
        ]
    ]

    set 'catch-any func [
        {catches any REBOL exception}
        block [block!] {block to evaluate}
        exception [word!] {used to return the exception type}
        /local result
    ] either rebol/version >= 2.100.0 [[
        ; catch RETURN, EXIT and RETURN/REDO
        ; using the DO-BLOCK helper call
        ; the helper call is enclosed in a block
        ; not containing any additional values
        ; to not give REDO any "excess arguments"
        ; also, it is necessary to catch all above exceptions again
        ; in case they are triggered by REDO
        ; TRY wraps CATCH/QUIT to circumvent bug#851
        try [
            catch/quit [
                try [
                    catch [
                        loop 1 [set/opt 'result do-block block exception]
                    ]
                ]
            ]
        ]
        either get exception [()] [:result]
    ]] [[
        error? set/opt 'result catch [
            error? set/opt 'result loop 1 [
                error? result: try [
                    ; RETURN or EXIT
                    set exception 'return
                    set/opt 'result do block

                    ; no exception
                    set exception blank
                    return get/opt 'result
                ]
                ; an error was triggered
                set exception 'error
                return result
            ]
            ; BREAK
            set exception 'break
            return get/opt 'result
        ]
        ; THROW
        set exception 'throw
        return get/opt 'result
    ]]
]
