REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Sys: Startup"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Context: sys
    Note: {
        The Init_Core() function in %b-init.c is supposed to be a fairly
        minimal startup, to get the system running.  For instance, it does
        not do any command-line processing...as the host program might not
        even *have* a command line.  It just gets basic things set up like
        the garbage collector and other interpreter services.

        Not much of that work can be delegated to Rebol routines, because
        the evaluator can't run for a lot of that setup time.  But at the
        end of Init_Core() when the evaluator is ready, it runs this
        routine for any core initialization code which can reasonably be
        delegated to Rebol.

        After this point, it is expected that further initialization be done
        by the host.  That includes the mentioned command-line processing,
        which due to this layering can be done with PARSE.
    }
]

finish-init-core: procedure [
    "Completes the boot sequence for Ren-C core."
    boot-mezz [block!]
        {Mezzanine code loaded as part of the boot block in Init_Core()}
][
    ; Remove the reference through which this function we are running is
    ; found, so it's invisible to the user and can't run again (but leave
    ; a hint that it's in the process of running vs. just unsetting it)
    ;
    finish-init-core: 'running

    ; Make the user's global context.  Remove functions whose names are being
    ; retaken for new functionality--to be kept this way during a deprecation
    ; period.  Ther lib definitions are left as-is, however, since the new
    ; definitions are required by SYS and LIB code itself.
    ;
    tmp: make object! 320
    append tmp reduce [
        'system :system
        
        'adjoin (get 'join)
        'join (func [dummy1 dummy2] [
            fail/where [
                {JOIN is reserved in Ren-C for future use}
                {(It will act like R3's REPEND, which has a slight difference}
                {from APPEND of a REDUCE'd value: it only reduces blocks).}
                {Use ADJOIN for the future JOIN, JOIN-OF for non-mutating.}
                {If in <r3-legacy> mode, old JOIN meaning is available.}
            ] 'dummy1
        ])

        'while-not (get 'until)
        'until (func [dummy] [
            fail/where [
                {UNTIL is reserved in Ren-C for future use}
                {(It will be arity-2 and act like WHILE [NOT ...] [...])}
                {Use LOOP-UNTIL for the single arity form, and see also}
                {LOOP-WHILE for the arity-1 form of WHILE.}
                {If in <r3-legacy> mode, old UNTIL meaning is available.}
            ] 'dummy
        ])
    ]
    system/contexts/user: tmp

    ; It was a stated goal at one point that it should be possible to protect
    ; the entire system object and still run the interpreter.  This was
    ; commented out, so the state of that feature is unknown.
    ;
    comment [if :lib/secure [protect-system-object]]

    ; The mezzanine is currently considered part of what Init_Core() will
    ; initialize for all clients.
    ;
    do bind-lib boot-mezz

    finish-init-core: 'done
]
