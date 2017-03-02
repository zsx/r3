REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "System build targets"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        These are the target system definitions used to build REBOL
        with a variety of compilers and libraries.  We prefer to keep it
        simple like this rather than using a complex configuration tool
        that could make it difficult to support REBOL on older platforms.

        Note that these numbers for the OS are the minor numbers at the
        tail of the system/version tuple.  (The first tuple values are
        used for the Rebol code version itself.)

        If you have a comment to make about a build, make it in the
        form of a flag...even if the functionality for that flag is a no-op.
        This keeps the table definition clean and readable.

        This file uses a table format processed by routines in %common.r,
        so be sure to include that via DO before calling CONFIG-SYSTEM.
    }
]

systems: [
    ;-------------------------------------------------------------------------
    [id         os-name         os-base
            build-flags]
    ;-------------------------------------------------------------------------
    0.1.03      amiga           posix
            [BEN LLC HID NPS +SC CMT COP -SP -LM]
    ;-------------------------------------------------------------------------
    0.2.04      osx-ppc         osx
            [BEN LLC +OS NCM -LM NSO]

    0.2.05      osx-x86         osx
            [ARC LEN LLC +O1 NPS PIC NCM HID STX -LM]

    0.2.40      osx-x64         osx
            [LP64 LEN LLC +O1 NPS PIC NCM HID STX -LM]
    ;-------------------------------------------------------------------------
    0.3.01      windows-x86     windows
            [LEN LL? +O2 UNI W32 CON S4M EXE DIR -LM]

    0.3.40      windows-x64     windows
            [LLP64 LEN LL? +O2 UNI W32 CON S4M EXE DIR -LM]
    ;-------------------------------------------------------------------------
    0.4.02      linux-x86       linux
            [M32 LEN LLC +O2 LDL ST1 -LM LC23 UFS NSP NSER]

    0.4.03      linux-x86       linux
            [M32 LEN LLC +O2 LDL ST1 -LM LC25 UFS HID]

    0.4.04      linux-x86       linux
            [M32 LEN LLC +O2 LDL ST1 -LM LC211 HID PIP2]

    0.4.10      linux-ppc       linux
            [BEN LLC +O1 HID LDL ST1 -LM PIP2]

    0.4.11      linux-ppc64     linux
            [LP64 BEN LLC +O1 HID LDL ST1 -LM PIP2]

    0.4.20      linux-arm       linux
            [LEN LLC +O2 HID LDL ST1 -LM PIP2]

    0.4.21      linux-arm       linux
            [LEN LLC +O2 HID LDL ST1 -LM PIE LCB PIP2]

    0.4.22      linux-aarch64       linux
            [LP64 LEN LLC +O2 HID LDL ST1 -LM PIP2]

    0.4.30      linux-mips      linux
            [LEN LLC +O2 HID LDL ST1 -LM PIP2]

    0.4.31      linux-mips32be  linux
            [BEN LLC +O2 HID LDL ST1 -LM PIP2]

    0.4.40      linux-x64       linux
            [LP64 LEN LLC +O2 HID LDL ST1 -LM PIP2]

    0.4.60      linux-axp       linux
            [LP64 LEN LLC +O2 HID LDL ST1 -LM PIP2]

    0.4.61      linux-ia64      linux
            [LP64 LEN LLC +O2 HID LDL ST1 -LM PIP2]
    ;-------------------------------------------------------------------------
    0.5.75      haiku           posix
            [LEN LLC +O2 ST1 NWK]
    ;-------------------------------------------------------------------------
    0.7.02      freebsd-x86     posix
            [LEN LLC +O1 ST1 -LM]

    0.7.40      freebsd-x64     posix
            [LP64 LEN LLC +O1 ST1 -LM]
    ;-------------------------------------------------------------------------
    0.9.04      openbsd         posix
            [LEN LLC +O1 ST1 -LM]

    0.9.40      openbsd         posix
            [LP64 LEN LLC +O1 ST1 -LM]
    ;-------------------------------------------------------------------------
    0.13.01     android-arm     android
            [LEN LLC HID F64 LDL LLOG -LM]
    ;-------------------------------------------------------------------------
    0.13.02     android5-arm        android
            [LEN LLC HID F64 LDL LLOG -LM PIE PIC]
    ;-------------------------------------------------------------------------
    0.14.01     syllable-dtp    posix
            [LEN LLC +O2 HID LDL ST1 -LM LC25]

    0.14.02     syllable-svr    linux
            [M32 LEN LLC +O2 HID LDL ST1 -LM LC211]
]

compiler-flags: context [
    M32: "-m32"                     ; use 32-bit memory model
    ARC: "-arch i386"               ; x86 32 bit architecture (OSX)

    LP64: "-D__LP64__"              ; 64-bit, and 'void *' is sizeof(long)
    LLP64: "-D__LLP64__"            ; 64-bit, and 'void *' is sizeof(long long)

    BEN: "-DENDIAN_BIG"             ; big endian byte order
    LEN: "-DENDIAN_LITTLE"          ; little endian byte order

    LLC: "-DHAS_LL_CONSTS"          ; supports e.g. 0xffffffffffffffffLL
    LL?: ""                         ; might have LL consts, reb-config.h checks

    +OS: "-Os"                      ; size optimize
    +O1: "-O1"                      ; optimize for minimal size
    +O2: "-O2"                      ; optimize for maximum speed

    UNI: "-DUNICODE"                ; win32 wants it
    HID: "-fvisibility=hidden"      ; all syms are hidden
    F64: "-D_FILE_OFFSET_BITS=64"   ; allow larger files
    NPS: "-Wno-pointer-sign"        ; OSX fix
    PIC: "-fPIC"                    ; position independent (used for libs)
    PIE: "-fPIE"                    ; position independent (executables)
    NCM: "-fno-common"              ; lib cannot have common vars

    ; There are variations in what functions different compiler versions will
    ; wind up linking in to support the same standard C functions.  This
    ; means it is not possible to a-priori know what libc version that
    ; compiler's build product will depend on when using a shared libc.so
    ;
    ; To get a list of the glibc stubs your build depends on, run this:
    ;
    ;     objdump -T ./r3 | fgrep GLIBC
    ;
    ; Notably, increased security measures caused functions like poll() and
    ; longjmp() to link to checked versions available only in later libc,
    ; or to automatically insert stack_chk calls for stack protection:
    ;
    ; http://stackoverflow.com/a/35404501/211160
    ; http://unix.stackexchange.com/a/92780/118919
    ;
    ; As compilers evolve, the workarounds to make them effectively cross
    ; compile to older versions of the same platform will become more complex.
    ; Switches that are needed to achieve this compilation may not be
    ; supported by old compilers.  This simple build system is not prepared
    ; to handle both "platform" and "compiler" variations; each OS_ID is
    ; intended to be used with the standard compiler for that platform.
    ;
    NSP: "-fno-stack-protector"     ; stack protect pulls in glibc 2.4 calls
    PIP2: "-DUSE_PIPE2_NOT_PIPE"    ; pipe2() linux only, glibc 2.9 or later
    UFS: "-U_FORTIFY_SOURCE"        ; don't link to _chk variants of C calls
    NSER:                           ; strerror_r() in glibc 2.3.4, not 2.3.0
        "-DUSE_STRERROR_NOT_STRERROR_R"
]

linker-flags: context [
    M32: "-m32"                     ; use 32-bit memory model (Linux x64)
    ARC: "-arch i386"               ; x86 32 bit architecture (OSX)

    NSO: ""                         ; no shared libs
    LDL: "-ldl"                     ; link with dynamic lib lib
    LLOG: "-llog"                   ; on Android, link with liblog.so

    W32: "-lwsock32 -lcomdlg32"
    CON: "-mconsole"                ; build as Windows Console binary
    S4M: "-Wl,--stack=4194300"
    -LM: "-lm"                      ; Math library (Haiku has it in libroot)
    NWK: "-lnetwork"                ; Needed by HaikuOS
    
    PIE: "-pie"

    ; Which libc is used is commentary, it has to be influenced by other
    ; flags.  See notes above about NSP, PIP1, UFS which are used to try and
    ; actually control these outcomes.
    ;
    LC23: ""                        ; libc 2.3
    LC25: ""                        ; libc 2.5
    LC211: ""                       ; libc 2.11
    LCB: ""                         ; bionic (Android)
]

other-flags: context [
    +SC: ""                         ; has smart console
    -SP: ""                         ; non standard paths
    COP: ""                         ; use COPY as cp program
    DIR: ""                         ; use DIR as ls program
    ST1: "-s"                       ; strip flags...
    STX: "-x"
    CMT: "-R.comment"
    EXE: ""                         ; use %.exe as binary file suffix
]

; A little bit of sanity-checking on the systems table
use [rec unknown-flags used-flags] [
    ;
    ; !!! See notes about RETURN from FOR-EACH-RECORD in its definition.
    ;
    used-flags: copy []
    for-each-record rec systems [
        assert [tuple? rec/id]
        assert [(to-string rec/os-name) == (lowercase to-string rec/os-name)]
        assert [(to-string rec/os-base) == (lowercase to-string rec/os-base)]
        assert [not find (to-string rec/os-base) charset [#"-" #"_"]]
        assert [block? rec/build-flags]
        for-each flag rec/build-flags [assert [word? flag]]

        ; Exclude should mutate (CC#2222), but this works either way
        unknown-flags: exclude (unknown_flags: copy rec/build-flags) compose [
            (words-of compiler-flags)
            (words-of linker-flags)
            (words-of other-flags)
        ]
        if not empty? unknown-flags [
            print mold unknown-flags
            fail "Unknown flag used in %systems.r specification"
        ]

        used-flags: union used-flags rec/build-flags
    ]

    unused-flags: exclude compose [
        (words-of compiler-flags)
        (words-of linker-flags)
        (words-of other-flags)
    ] used-flags

    if not empty? unused-flags [
        print mold unused-flags
        fail "Unused flags in %systems.r specifications"
    ]
]

config-system: function [
    {Return build configuration information}
    hint [blank! string! tuple!]
        {Version ID (blank means guess)}
][
    version: case [
        blank? hint [
            ;
            ; Try same version as this r3-make was built with
            ;
            to tuple! reduce [0 system/version/4 system/version/5]
        ]
        string? hint [
            load hint
        ]
    ]

    unless tuple? version [
        fail [
            "Expected platform id (tuple like 0.3.1), not:" version
        ]
    ]

    unless result: find-record-unique systems 'id version [
        fail [
            {No table entry for} version {found in systems.r}
        ]
    ]

    result
]
