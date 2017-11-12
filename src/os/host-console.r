REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Host Console (Rebol's Read-Eval-Print-Loop, ie. REPL)"
    Rights: {
        Copyright 2016-2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Description: {
        This implements a simple interactive command line.  It gathers strings
        to be executed but does not actually run them with DO--instead it
        returns a block to C code which does the actual execution.  The
        reason it is done this way is to avoid having Rebol CONSOLE stack frames
        hanging around when debug commands are executed (e.g. one does not
        want BACKTRACE to see a FOREVER [] loop of gathering input, or the DO)

        Though not implemented in C as the R3-Alpha CONSOLE was, it still relies
        upon INPUT to receive lines.  INPUT reads lines from the "console
        port", which is C code that is linked to STDTERM on POSIX and the
        Win32 Console API on Windows.  Thus, the ability to control the cursor
        and use it to page through line history is still a "black box" at
        that layer.
     }
]


; The ECHO routine has to collaborate specifically with the console, because
; it is often desirable to capture the input only, the output only, or both.
;
; !!! The features that tie the echo specifically to the console would be
; things like ECHO INPUT, e.g.:
;
; https://github.com/red/red/issues/2487
;
; They are not implemented yet, but ECHO is moved here to signify the known
; issue that the CONSOLE must collaborate specifically with ECHO to achieve
; this.
;
echo: procedure [
    {Copies console I/O to a file.}

    'instruction [file! string! block! word!]
        {File or template with * substitution, or command: [ON OFF RESET].}

    <static>
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


host-console: function [
    {Implements one Print-and-Read step of a Read-Eval-Print-Loop (REPL).}

    return: [block! group!]
        {Code to ask the top-level to run, BLOCK! makes last-failed blank}

    last-result [<opt> any-value!]
        {The result from the last time HOST-CONSOLE ran to display (if any)}

    last-failed [<opt> blank! logic! bar! error!]
        {blank after BLOCK!, TRUE on a FAIL, BAR! if HALT, ERROR! if BLOCK!}

    focus-level [blank! integer!]
        {If at a breakpoint, the integer index of how deep the stack was}

    focus-frame [blank! frame!]
        {If at a breakpoint, the function frame where the breakpoint was hit}

    <static>

    RE_SCAN_INVALID (2000)
    RE_SCAN_MISSING (2001)
    RE_SCAN_EXTRA (2002)
    RE_SCAN_MISMATCH (2003)
][
    ; Note: SYSTEM/CONSOLE is an external object for skinning the behaviour
    ; and appearance.  Since users can write arbitrary code in that skin, it
    ; may contain bugs, infinite loops, etc.
    ;
    ; Yet HOST-CONSOLE is a function called from C at the top level, with no
    ; recourse should it error...and Ctrl-C is disabled while it runs.  So
    ; for safety, calls into SYSTEM/CONSOLE are returned as BLOCK! to the
    ; C code to run.  If the code runs successfully, then LAST-FAILED will
    ; be BLANK! and the LAST-RESULT can be used to trigger a re-entry with
    ; the right properties for where the code should pick back up.
    ;
    ; This is very similar to continuation-style programming, and these
    ; variables are what help pick up the continuation at the proper point.
    ;
    ; Notice that when the SYSTEM/CONSOLE functions are called, BAR!s are
    ; used to make sure they don't accidentally consume data they should
    ; not (e.g. by accidentally having too big an arity)
    ;
    needs-prompt: true
    needs-gap: true
    needs-input: true
    source: copy {} ;-- source code potentially built of multiple lines

    ; Output the last evaluation result if there was one.  MOLD it unless it
    ; was an actual error that FAILed.
    ;
    case [
        not set? 'last-failed [
            ;
            ; First time running, hasn't had a chance to fail yet.  Each
            ; recursion in the debugger also starts a new REPL without a
            ; prior last result available.

            if not focus-frame [
                return [
                    system/console/print-greeting
                        |
                    'needs-prompt
                ]
            ] else [
                ;
                ; Internally there is a known difference between whether an
                ; interruption came from a Ctrl-C or a BREAKPOINT or PAUSE
                ; instruction...but this is not currently passed through as
                ; information to the console.  Should it be?  Also, this
                ; should be skinnable.

                print [
                    "** Execution Interrupted"
                        "(see BACKTRACE, DEBUG, and RESUME)"
                ]
            ]
        ]

        false = last-failed [
            ;
            ; Successful evaluation of a returned GROUP!
            ;
            if set? 'last-result [
                return compose/deep [
                    system/console/last-result: (mold :last-result)
                        |
                    system/console/print-result
                        |
                    'needs-prompt
                ]
            ]
        ]

        blank? last-failed [
            ;
            ; This means the last thing that we asked to run was a BLOCK! and
            ; not a GROUP!, and that execution did not itself fail.  (See
            ; notes on the start of this function for why these "continuation"
            ; results are needed.)
            ;
            ; WORD!s are used to make the needs of the continuations more
            ; clear at the `return [...]` points in this function.  But the
            ; special case of returning a BLOCK! is the self-trigger that
            ; indicates a need for the actual execution on the user's behalf.
            ; And the case of a STRING! is used to feed back the source after
            ; allowing a processing hook to run on it.
            ;
            case [
                block? last-result [
                    return as group! last-result
                ]
                string? last-result [
                    source: last-result
                    needs-prompt: needs-gap: needs-input: false
                ]
            ] else [
                switch last-result [
                    no-op []
                    needs-prompt [needs-prompt: true]
                    needs-gap: [needs-gap: true | needs-prompt: false]
                    no-prompt: [needs-prompt: needs-gap: false]
                    no-gap: [needs-gap: false]
                ] else [
                    return compose/deep [
                        fail ["Bad REPL continuation:" quote (last-result)]
                    ]
                ]
            ]
        ]

        bar? last-failed [
            ;
            ; !!! This used to say "[escape]".  Should be skinnable, but what
            ; should the default be?
            ;
            print "[interrupted by Ctrl-C or HALT instruction]"
        ]

        true = last-failed [
            if not error? :last-result [
                return compose/only/deep [
                    fail ["REPL broken contract, non-error:" (:last-result)]
                ]
            ]

            return compose/deep [
                system/console/print-error (last-result)
                    |
                'needs-prompt
            ]
        ]

        error? last-failed [
            ;
            ; This is reserved for the serious case when a BLOCK! was asked
            ; to be executed, and a failure happened.  That means something
            ; internal to the skin itself has a problem...which may mean
            ; that the console becomes unusable.  Fall back to the default.
            ;
            system/console: make console! []

            return compose/deep [
                print [
                    "*** ERROR WHILE RUNNING CONSOLE SKIN CODE ***"
                        |
                    "...Reverting to default skin for safety, report error..."
                ]
                    |
                system/console/print-error (last-failed)
                    |
                'needs-prompt
            ]
        ]
    ] else [
        ;
        ; This would be bad...some kind of contract violation of the calling
        ; C code of what HOST-CONSOLE expects to be possible.
        ;
        return compose/only/deep [
            fail ["REPL broken contract, LAST-FAILED:" (:last-failed)]
        ]
    ]

    if needs-gap [
        return [system/console/print-gap | 'no-gap]
    ]

    ; If a debug frame is in focus then show it in the prompt, e.g.
    ; as `if:|4|>>` to indicate stack frame 4 is being examined, and
    ; it was an `if` statement...so it will be used for binding (you
    ; can examine the condition and branch for instance)
    ;
    if needs-prompt [
        return compose/deep [
            if (focus-frame) [
                if label-of (focus-frame) [
                    print/only [label-of (focus-frame) ":"]
                ]
                print/only ["|" (focus-level) "|"]
            ]
                |
            system/console/print-prompt
                |
            'no-prompt
        ]
    ]

    ; The LOADed and bound code.  It's initialized to empty block so that if
    ; there is no input text (just newline at a prompt) , it will be treated
    ; as DO [].
    ;
    code: copy []

    forever [ ;-- gather potentially multi-line input

        if needs-input [
            ;
            ; !!! Unfortunately Windows ReadConsole() has no way of being set
            ; to ignore Ctrl-C.  In usermode code, this is okay as Ctrl-C
            ; stops the Rebol code from running...but HOST-CONSOLE disables
            ; the halting behavior assigned to Ctrl-C.  To avoid glossing over
            ; that problem, INPUT doesn't just return blank or void...it
            ; FAILs.  We make a special effort to TRAP it here, but it would
            ; be a bug if seen by any other function.
            ;
            ; Upshot is that on Windows, Ctrl-C during HOST-CONSOLE is made
            ; to act as escape.  (It does nothing on POSIX as we can actually
            ; ask Ctrl-C to be ignored by the read() loop.)
            ;
            user-input: trap/with [input] [blank]

            if blank? user-input [
                ;
                ; It was aborted.  This comes from ESC on POSIX (which is the
                ; ideal behavior), Ctrl-D on Windows (because ReadConsole()
                ; can't trap ESC), Ctrl-D on POSIX (just to be compatible with
                ; Windows), and the case of Ctrl-C on Windows just on calls
                ; to INPUT here in HOST-CONSOLE (usually it HALTs).
                ;
                ; Do a no-op execution that just cycles the prompt.
                ;
                return ['needs-gap]
            ]

            return compose/deep [
                use [line] [
                    line: system/console/input-hook (user-input)
                        |
                    append (source) line
                ]
                (source) ;-- STRING! signals feedback to BLANK? LAST-RESULT
            ]
        ]

        needs-input: true

        trap/with [
            ;
            ; Note that LOAD/ALL makes BLOCK! even for a single item,
            ; e.g. `load/all "word"` => `[word]`
            ;
            code: load/all source
            assert [block? code]

        ] func [error <with> return] [
            ;
            ; If loading the string gave back an error, check to see if it
            ; was the kind of error that comes from having partial input
            ; (RE_SCAN_MISSING).  If so, CONTINUE and read more data until
            ; it's complete (or until an empty line signals to just report
            ; the error as-is)
            ;
            ; Save the error even if it's a "needs continuation" error, in
            ; case the next input is an empty line.  That makes the error get
            ; reported, stopping people from getting trapped in a loop.
            ;
            ; !!! Note that this is a bit unnatural, but it's similar to what
            ; Ren Garden does (though it takes two lines).  It should not be
            ; applied to input that is pasted as text from another source,
            ; and arguably this could be disruptive to multi-line strings even
            ; if being entered in the CONSOLE
            ;
            code: error

            if error/code = RE_SCAN_MISSING [
                ;
                ; !!! Error message tells you what's missing, not what's open
                ; and needs to be closed.  Invert the symbol.
                ;
                unclosed: switch error/arg1 [
                    "}" ["{"]
                    ")" ["("]
                    "]" ["["]
                ]

                if set? 'unclosed [
                    print/only [unclosed space space space]
                    append source newline
                    continue
                ] else [
                    ;
                    ; Could be an unclosed double quote (unclosed tag?) which
                    ; more input on a new line cannot legally close ATM
                    ;
                ]
            ]

            ; Potentially large print operations should be handed back to the
            ; top level, so that they can be halted.
            ;
            return compose/deep [
                system/console/print-error (error)
                    |
                'needs-prompt
            ]
        ]

        break ;-- Exit FOREVER if no additional input to be gathered
    ]

    assert [block? code]

    ; If we're focused on a debug frame, try binding into it
    ;
    if focus-frame [
        bind code focus-frame
    ]

    instruction: copy []

    if shortcut: select system/console/shortcuts first code [
        ;
        ; Shortcuts.  Built-ins are:
        ;
        ;     d => [dump]
        ;     h => [help]
        ;     q => [quit]
        ;
        if all [bound? code/1 | set? code/1] [
            ;
            ; Help confused user who might not know about the shortcut not
            ; panic by giving them a message.  Reduce noise for the casual
            ; shortcut by only doing so a bound variable exists.
            ;
            instruction: compose/deep [
                system/console/print-warning [
                    (uppercase to-string code/1)
                        "interpreted by console as:" form :shortcut
                ]
                    |
                system/console/print-warning [
                    "use" form to-get-word (code/1) "to get variable."
                ]
            ]
        ]
        take code
        insert code shortcut
    ]

    ; There is a question of how it should be decided whether the code
    ; in the CONSOLE should be locked as read-only or not.  It may be a
    ; configuration switch, as it also may be an option for a module or
    ; a special type of function which does not lock its source.
    ;
    lock code

    ; We make it a bit safer in case there's an error in the dialect-hook
    ; itself to transmit the code to execute back to ourselves.  If there's
    ; no error and the instruction evaluates to a BLOCK!, then that combined
    ; with receiving BLANK! as the last result signals us to execute the
    ; code on the user's behalf.
    ;
    append instruction compose/only [
            |
        system/console/dialect-hook (code)
    ]
    return instruction
]


why: procedure [
    "Explain the last error in more detail."
    'err [<end> word! path! error! blank!] "Optional error value"
][
    case [
        not set? 'err [err: _]
        word? err [err: get err]
        path? err [err: get err]
    ]

    either all [
        error? err: any [:err system/state/last-error]
        err/type ; avoids lower level error types (like halt)
    ][
        say-browser
        err: lowercase unspaced [err/type #"-" err/id]
        browse join-of http://www.rebol.com/r3/docs/errors/ [err ".html"]
    ][
        print "No information is available."
    ]
]


upgrade: procedure [
    "Check for newer versions."
][
    ; Should this be a console-detected command, like Q, or is it meaningful
    ; to define this as a function you could call from code?
    ;
    do <upgrade>
]
