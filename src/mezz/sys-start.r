REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Sys: Startup"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Context: sys
    Note: {
        Originally Rebol's "Mezzanine" init was one function.  In Ren/C's
        philosophy of being "just an interpreter core", many concerns will
        not be in the basic library.  This includes whether "--do" is the
        character sequence on the command-line for executing scripts (or even
        if there *is* a command-line).  It also shouldn't be concerned with
        code for reading embedded scripts out of various ELF or PE file
        formats for encapping.

        Prior to the Atronix un-forking, Ren/C had some progress by putting
        the "--do" handling into the host.  But merging the Atronix code put
        encapping into the core startup.  So this separation will be an
        ongoing process.  For the moment that is done by splitting into two
        functions: a "core" portion for finishing Init_Core(), and a "host"
        portion for finishing RL_Start().

        !!! "The boot binding of this module is SYS then LIB deep.
        Any non-local words not found in those contexts WILL BE
        UNBOUND and will error out at runtime!"
    }
]

finish-init-core: procedure [
    "Completes the boot sequence for Ren-C core."
    boot-mezz [block!]
        {Mezzanine code loaded as part of the boot block in Init_Core()}
    boot-prot [block!]
        {Protocols built into the mezzanine at this time (http, tls)}
][
    ; Make the user's global context.  Remove functions whose names are being
    ; retaken for new functionality--to be kept this way during a deprecation
    ; period.
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

    ; Remove the reference through which this function we are running is
    ; found, so it's invisible to the user and can't run again.
    ;
    finish-init-core: 'done

    ; It was a stated goal at one point that it should be possible to protect
    ; the entire system object and still run the interpreter.  This was
    ; commented out, so the state of that feature is unknown.
    ;
    comment [if :lib/secure [protect-system-object]]

    ; The mezzanine is currently considered part of what Init_Core() will
    ; initialize for all clients.
    ;
    do bind-lib boot-mezz

    ; For now, we also consider initializing the port schemes to be "part of
    ; the core function".  Longer term, it needs to be the host's
    ; responsibility to pick and configure the specific schemes it wishes to
    ; support...or to delegate to the user to load them.
    ;
    init-schemes

    ; Also for now, we consider the boot protocols that are implemented in
    ; user code (http and tls) to be part of the core.  The explicit
    ; parameterization here helps show how they're getting passed in.
    ;
    for-each [spec body] boot-prot [module spec body]
]
