REBOL []
do %common.r
do %systems.r
file-base: make object! load %file-base.r

rebmake: import %rebmake.r

config-dir: %../../make
print pwd
user-config: make object! load config-dir/default-config.r

; Load user defined config.r
args: parse-args system/options/args
if select args 'CONFIG [
    user-config: make user-config load config-dir/(args/CONFIG)
]

; Allow any of the settings in user-config to be overwritten by command line
; options
for-each [name value] args [
    switch/default name [
        CONFIG [
            ;pass
        ]
        EXTENSIONS [
            use [ext-file user-ext][
                either any [
                    exists? ext-file: to file! value
                    exists? ext-file: to file! config-dir/(value)
                ][
                    user-ext: make object! load ext-file
                    unless all [
                        find? words-of user-ext 'extensions
                        block? user-ext/extensions
                    ][
                        fail ["Malformated extension selection file, it needs to be 'EXTENSIONS: []'" (mold user-ext)]
                    ]
                    user-config/extensions: user-ext/extensions
                ][
                    user-ext: load value
                    unless block? user-ext [
                        fail ["Selected extensions must be a block, not" (type-of user-ext)]
                    ]
                    user-config/extensions: user-ext
                ]
            ]
        ]
    ][
        set in user-config (to-word replace/all to string! name #"_" #"-")
            load value
    ]
]

dump user-config

app-config: make object! [
    cflags: make block! 8
    ldflags: make block! 8
    libraries: make block! 8
    debug: off
    optimization: 2
    definitions: copy []
    includes: copy [%../src/include]
    searches: make block! 8
]

assert-no-blank-inside: proc [
    block [block! blank!]
    <local> e
][
    if blank? block [leave]

    for-each e block [
        if blank? e [
            dump block
            fail "No blanks allowed"
        ]
    ]
]

set-exec-path: proc [
    tool [object!]
    path
][
    if path [
        unless file? path [
            fail "Tool path has to be a file!"
        ]
        tool/exec-file: path
    ]
]

to-obj-path: func [
    file [any-string!]
][
    join-of %objs/ replace (last split-path file) ".c" rebmake/target-platform/obj-suffix
]

gen-obj: func [
    s
    /dir directory [any-string!]
    /D definitions [block!]
    <local>
    flags
][
    flags: make block! 8
    if block? s [
        append flags []
        for-each flag next s [
            append flags opt switch/default flag [
                <no-uninitialized> [
                    [
                        <gnu:-Wno-uninitialized>

                        ;-Wno-unknown-warning seems to only modify the
                        ; immediately following option
                        ;
                        ;<gnu:-Wno-unknown-warning>
                        ;<gnu:-Wno-maybe-uninitialized>

                        <msc:/wd4701> <msc:/wd4703>
                    ]
                ]
                <implicit-fallthru> [
                    [
                        <gnu:-Wno-unknown-warning>
                        <gnu:-Wno-implicit-fallthrough>
                    ]
                ]
                <no-unused-parameter> [
                    <gnu:-Wno-unused-parameter>
                ]
                <no-shift-negative-value> [
                    <gnu:-Wno-shift-negative-value>
                ]
                <no-make-header> [
                    ;for make-header. ignoring
                    _
                ]
                <no-unreachable> [
                    <msc:/wd4702>
                ]
                <no-hidden-local> [
                    <msc:/wd4456>
                ]
                <no-constant-conditional> [
                    <msc:/wd4127>
                ]
            ][
                ensure [string! tag!] flag
                flag
            ]
        ]
        s: s/1
    ]

    make rebmake/object-file-class compose/only [
        source: to-file join-of either dir [directory][%../src/] s
        output: join-of %objs/ replace to string! s ".c" rebmake/target-platform/obj-suffix
        cflags: either empty? flags [_] [flags]
        definitions: (to-value :definitions)
    ]
]

add-project-flags: proc [
    project [object!]
    /I includes
    /D definitions
    /c cflags
    /O optimization
    /g debug
][
    assert [
        find? [
            dynamic-library-class
            object-library-class
            static-library-class
            application-class
        ] project/class-name
    ]

    if D [
        assert-no-blank-inside definitions
        either block? project/definitions [
            append project/definitions definitions
        ][
            project/definitions: definitions
        ]
    ]

    if I [
        assert-no-blank-inside includes
        either block? project/includes [
            either locked? project/includes [
                project/includes: join-of project/includes includes
            ][
                append project/includes includes
            ]
        ][
            project/includes: includes
        ]
    ]
    if c [
        assert-no-blank-inside cflags
        either block? project/cflags [
            append project/cflags cflags
        ][
            project/cflags: cflags
        ]
    ]
    if g [project/debug: debug]
    if O [project/optimization: optimization]
]

process-module: func [
    mod [object!]
    <local>
    s
    ret
][
    assert [mod/class-name = 'module-class]
    ;dump mod

    assert-no-blank-inside mod/includes
    assert-no-blank-inside mod/definitions
    assert-no-blank-inside mod/depends
    if block? mod/libraries [assert-no-blank-inside mod/libraries]
    assert-no-blank-inside mod/cflags
    ret: make rebmake/object-library-class [
        name: mod/name
        depends: map-each s (append reduce [mod/source] opt mod/depends) [
            ;dump s
            case [
                any [file? s block? s][
                    gen-obj/dir s "../src/extensions/"
                ]
                all [object? s
                    find? [
                        object-library-class
                        object-file-class
                    ] s/class-name
                ][
                    s
                    ;object-library-class has already been taken care of above
                    ;if s/class-name = 'object-file-class [s]
                ]
                true [
                    dump s
                    fail [type-of s "can't be a dependency of a module"]
                ]
            ]
        ]
        libraries: to-value if mod/libraries [
            map-each lib mod/libraries [
                case [
                    file? lib [
                        make rebmake/ext-dynamic-class [
                            output: lib
                        ]
                    ]
                    all [object? lib
                        find? [
                            ext-dynamic-class
                            ext-static-class
                        ] lib/class-name [
                            lib
                        ]
                    ]
                    true [
                        dump lib
                        fail "unrecognized module library"
                    ]
                ]
            ]
        ]

        includes: mod/includes
        definitions: mod/definitions
        cflags: mod/cflags
    ]

    ret
]

add-new-obj-folders: procedure [
    objs
    folders
    <local>
    lib
    obj
][
    for-each lib objs [
        switch/default lib/class-name [
            object-file-class [
                lib: reduce [lib]
            ]
            object-library-class [
                lib: lib/depends
            ]
        ][
            dump lib
            fail ["unexpected class"]
        ]

        for-each obj lib [
            ;if blank? obj [continue]
            ;dump obj
            dir: first split-path obj/output
            ;dump dir
            unless find? folders dir [
                append folders dir
            ]
        ]
    ]
]

set [cc cc-exe] any [
    find user-config/toolset 'gcc
    find user-config/toolset 'cl
]

set [linker linker-exe] any [
    find user-config/toolset 'ld
    find user-config/toolset 'link
]

set [strip strip-exe] any [
    find user-config/toolset 'strip
]

switch cc [
    gcc [
        rebmake/default-compiler: rebmake/gcc
        set-exec-path rebmake/gcc cc-exe

        if linker != 'ld [
            fail ["GCC can only work with ld, not" linker]
        ]
        rebmake/default-linker: rebmake/ld
        set-exec-path rebmake/ld linker-exe
    ]
    cl [
        rebmake/default-compiler: rebmake/cl
        set-exec-path rebmake/cl cc-exe

        if linker != 'link [
            fail ["CL can only work with link, not" linker]
        ]
        rebmake/default-linker: rebmake/link
        set-exec-path rebmake/link linker-exe
    ]
][
    fail ["Unrecognized compiler (gcc or cl):" cc]
]

if strip [
    rebmake/default-strip: rebmake/strip
    rebmake/default-strip/options: [<gnu:-S> <gnu:-x> <gnu:-X>]
    set-exec-path rebmake/default-strip strip-exe
]

system-config: config-system user-config/os-id
dump system-config
rebmake/set-target-platform system-config/os-base

add-app-def: adapt specialize :append [series: app-config/definitions] [
    value: flatten/deep reduce bind value system-definitions
]
add-app-cflags: adapt specialize :append [series: app-config/cflags] [
    value: if block? value [flatten/deep reduce bind value compiler-flags]
]
add-app-lib: adapt specialize :append [series: app-config/libraries] [
    value: either block? value [
        value: flatten/deep reduce bind value system-libraries
        ;dump value
        map-each w flatten value [
            make rebmake/ext-dynamic-class [
                output: w
            ]
        ]
    ][
        assert [any-string? value]
        make rebmake/ext-dynamic-class [
            output: value
        ]
    ]
]

add-app-ldflags: adapt specialize :append [series: app-config/ldflags] [
    value: if block? value [flatten/deep reduce bind value linker-flags]
]

libuuid-objs: map-each s [
    %uuid/libuuid/gen_uuid.c
    %uuid/libuuid/unpack.c
    %uuid/libuuid/pack.c
    %uuid/libuuid/randutils.c
][
    gen-obj/dir s "../src/extensions/"
]

module-class: make object! [
    class-name: 'module-class
    name: _
    depends: _
    source: _ ;main script

    includes: _
    definitions: _
    cflags: _

    searches: _
    libraries: _
    ldflags: _
]

extension-class: make object! [
    class-name: 'extension-class
    name: _
    loadable: yes ;can be loaded at runtime
    modules: _
    source: _
    init: _ ;init-script
]

available-modules: reduce [
    ;name module-file other-files
    mod-crypt: make module-class [
        name: 'Crypt
        source: %crypt/mod-crypt.c
        includes: copy [
            ;
            ; Added so `#include "bigint/bigint.h` can be found by %rsa.h
            ; and `#include "rsa/rsa.h" can be found by %dh.c
            ;
            %../src/extensions/crypt
        ]
        depends: [
            %crypt/aes/aes.c
            %crypt/bigint/bigint.c
            %crypt/dh/dh.c
            %crypt/rc4/rc4.c
            %crypt/rsa/rsa.c
            %crypt/sha256/sha256.c
        ]
    ]

    mod-process: make module-class [
        name: 'Process
        source: %process/mod-process.c
    ]

    mod-lodepng: make module-class [
        name: 'LodePNG
        source: %png/mod-lodepng.c
        depends: [
            [
                %png/lodepng.c

                ; The LodePNG module has local scopes with declarations that
                ; alias declarations in outer scopes.  This can be confusing,
                ; so it's avoided in the core, but LodePNG is maintained by
                ; someone else with different standards.
                ;
                ;    declaration of 'identifier' hides previous
                ;    local declaration
                ;
                <msc:/wd4456>
            ]
        ]
    ]

    mod-upng: make module-class [
        name: 'uPNG
        source: [
            %png/u-png.c

            ; This PNG source has a number of static variables that are used
            ; in the encoding process (making it not thread-safe), as well
            ; as local variables with the same name as these global static
            ; variables.
            ;
            ;    declaration of 'identifier' hides global declaration
            ;
            ; !!! This lack of thread safety is a good argument for not using
            ; this vs. LodePNG.
            ;
            <msc:/wd4459>
        ]
    ]

    mod-gif: make module-class [
        name: 'GIF
        source: %gif/mod-gif.c
    ]

    mod-bmp: make module-class [
        name: 'BMP
        source: %bmp/mod-bmp.c
    ]

    mod-locale: make module-class [
        name: 'Locale
        source: [
            %locale/mod-locale.c

            ; The locale module uses non-constant aggregate initialization,
            ; e.g. LOCALE_WORD_ALL is defined as Ext_Canons_Locale[4], but
            ; is assigned as `= {{LOCALE_WORD_ALL, LC_ALL}...}` to a struct.
            ; For the moment, since it's just the locale module, disable the
            ; warning, though we don't want to use nonstandard C as a general
            ; rule in the core.
            ;
            ;    nonstandard extension used : non-constant aggregate
            ;    initializer
            ;
            <msc:/wd4204>
        ]
    ]

    mod-jpg: make module-class [
        name: 'JPG
        source: %jpg/mod-jpg.c
        depends: [
            ;
            ; The JPG sources come from elsewhere; invasive maintenance for
            ; compiler rigor is not worthwhile to be out of sync with original.
            ;
            [
                %jpg/u-jpg.c

                <gnu:-Wno-unused-parameter> <msc:/wd4100>

                <gnu:-Wno-shift-negative-value>

                ; "conditional expression is constant"
                ;
                <msc:/wd4127>
            ]
        ]
    ]

    mod-uuid: make module-class [
        name: 'UUID
        source: %uuid/mod-uuid.c
        includes: [%../src/extensions/uuid/libuuid]
        depends: to-value switch system-config/os-base [
            linux [
                libuuid-objs
            ]
        ]

        libraries: to-value switch system-config/os-base [
            Windows [
                [%rpcrt4]
            ]
        ]
        ldflags: to-value switch system-config/os-base [
            OSX [
                ["-framework CoreFoundation"]
            ]
        ]
    ]

    mod-odbc: make module-class [
        name: 'ODBC
        source: [
            %odbc/mod-odbc.c

            ; ODBCGetTryWaitValue() is prototyped as a C++-style void argument
            ; function, as opposed to ODBCGetTryWaitValue(void), which is the
            ; right way to do it in C.  But we can't change <sqlext.h>, so
            ; disable the warning.
            ;
            ;     'function' : no function prototype given:
            ;     converting '()' to '(void)'
            ;
            <msc:/wd4255>

            ; The ODBC include also uses nameless structs/unions, which are a
            ; non-standard extension.
            ;
            ;     nonstandard extension used: nameless struct/union
            ;
            <msc:/wd4201>
        ]
        libraries: to-value switch/default system-config/os-base [
            Windows [
                [%odbc32]
            ]
        ][
            ; On some systems (32-bit Ubuntu 12.04), odbc requires ltdl
            append-of [%odbc] unless find [no false off _ #[false]] user-config/odbc-requires-ltdl [%ltdl]
        ]
    ]
]

;dump mod-uuid

available-extensions: reduce [
    ext-crypt: make extension-class [
        name: 'Crypt
        loadable: no ;tls depends on this, so it has to be builtin
        modules: reduce [
            mod-crypt
        ]
        source: %crypt/ext-crypt.c
        init: %crypt/ext-crypt-init.reb
    ]

    ext-process: make extension-class [
        name: 'Process
        modules: reduce [
            mod-process
        ]
        source: %process/ext-process.c
        init: %process/ext-process-init.reb
    ]

    ext-png: make extension-class [
        name: 'PNG
        loadable: no ; Depends on symbols, like z_inflate(End), that are not exported
        modules: reduce [
            mod-lodepng
            mod-upng
        ]
        source: %png/ext-png.c
    ]

    ext-gif: make extension-class [
        name: 'GIF
        modules: reduce [
            mod-gif
        ]
        source: %gif/ext-gif.c
    ]

    ext-jpg: make extension-class [
        name: 'JPG
        modules: reduce [
            mod-jpg
        ]
        source: %jpg/ext-jpg.c
    ]

    ext-bmp: make extension-class [
        name: 'BMP
        modules: reduce [
            mod-bmp
        ]
        source: %bmp/ext-bmp.c
    ]

    ext-locale: make extension-class [
        name: 'Locale
        modules: reduce [
            mod-locale
        ]
        source: %locale/ext-locale.c
        init: %locale/ext-locale-init.reb
    ]

    ext-uuid: make extension-class [
        name: 'UUID
        modules: reduce [
            mod-uuid
        ]
        source: %uuid/ext-uuid.c
        init: %uuid/ext-uuid-init.reb
    ]

    ext-odbc: make extension-class [
        name: 'ODBC
        modules: reduce [
            mod-odbc
        ]
        source: %odbc/ext-odbc.c
        init: %odbc/ext-odbc-init.reb
    ]
]

;dump ext-uuid
;for-each m ext-uuid/modules [
;    dump m
;]

cfg-sanitize: false
cfg-symbols: false
switch/default user-config/debug [
    #[false] no false off none [
        append app-config/definitions ["NDEBUG"]
        app-config/debug: off
    ]
    #[true] yes true on [
        app-config/debug: on
    ]
    asserts [
        app-config/debug: on
    ]
    symbols [
        cfg-symbols: true
        app-config/debug: on
    ]
    sanitize [
        app-config/debug: on
        cfg-symbols: true
        cfg-sanitize: true
        append app-config/cflags <gnu:-fsanitize=address>
        append app-config/ldflags <gnu:-fsanitize=address>
    ]

    ; This is the case we really don't like; bugs that only show up in the
    ; non-debug build.  It's the case you need symbols in a release build
    ;
    pathology [
        cfg-symbols: true
        app-config/debug: off
    ]
][
    fail ["unrecognized debug setting:" user-config/debug]
]

switch user-config/optimize [
    #[false] false no off 0 [
        app-config/optimization: false
    ]
    1 2 3 4 "s" [
        app-config/optimization: user-config/optimize
    ]
]


cfg-cplusplus: false
;standard
append app-config/cflags opt switch/default user-config/standard [
    c [
        _
    ]
    gnu89 c99 gnu99 c11 [
        to tag! unspaced ["gnu:--std=" user-config/standard]
    ]
    c++ [
        cfg-cplusplus: true
        [
            <gnu:-x c++>
            <msc:/TP>
        ]
    ]
    c++98 c++0x c++11 c++14 c++17 c++latest [

        ; Note: The C and C++ user-config/standards do not dictate if `char` is signed
        ; or unsigned.  Lest anyone think all environments have settled on
        ; them being signed, they're not... Android NDK uses unsigned:
        ;
        ; http://stackoverflow.com/questions/7414355/
        ;
        ; In order to give the option some exercise, make the C++11 builds
        ; and above use unsigned chars.
        ;
        cfg-cplusplus: true
        reduce [
            <gnu:-x c++>
            to tag! unspaced ["gnu:--std=" user-config/standard]
            <gnu:-funsigned-char>
            <msc:/TP>
            to tag! unspaced ["msc:/std:" lowercase to string! user-config/standard];only supports "c++14/17/latest"
        ]
    ]
][
    fail [
        "STANDARD should be one of [c gnu89 gnu99 c99 c11 c++ c++11 c++14 c++17 c++latest]"
        "not" (user-config/standard)
    ]
]

cfg-rigorous: false
append app-config/cflags opt switch/default user-config/rigorous [
    #[true] yes on true [
        cfg-rigorous: true
        compose [
            <gnu:-Werror> <msc:/WX>;-- convert warnings to errors

            ; If you use pedantic in a C build on an older GNU compiler,
            ; (that defaults to thinking it's a C89 compiler), it will
            ; complain about using `//` style comments.  There is no
            ; way to turn this complaint off.  So don't use pedantic
            ; warnings unless you're at c99 or higher, or C++.
            ;
            (
                if any [
                    cfg-cplusplus | not find [c gnu89] user-config/standard
                ][
                    <gnu:--pedantic>
                ]
            )

            <gnu:-Wextra>
            <gnu:-Wall> <msc:/Wall>

            <gnu:-Wchar-subscripts>
            <gnu:-Wwrite-strings>
            <gnu:-Wundef>
            <gnu:-Wformat=2>
            <gnu:-Wdisabled-optimization>
            <gnu:-Wlogical-op>
            <gnu:-Wredundant-decls>
            <gnu:-Woverflow>
            <gnu:-Wpointer-arith>
            <gnu:-Wparentheses>
            <gnu:-Wmain>
            <gnu:-Wtype-limits>
            <gnu:-Wclobbered>

            ; Neither C++98 nor C89 had "long long" integers, but they
            ; were fairly pervasive before being present in the standard.
            ;
            <gnu:-Wno-long-long>

            ; When constness is being deliberately cast away, `m_cast` is
            ; used (for "m"utability).  However, this is just a plain cast
            ; in C as it has no const_cast.  Since the C language has no
            ; way to say you're doing a mutability cast on purpose, the
            ; warning can't be used... but assume the C++ build covers it.
            ;
            (either cfg-cplusplus [<gnu:-Wcast-qual>] [<gnu:-Wno-cast-qual>])

            ;     'bytes' bytes padding added after construct 'member_name'
            ;
            ; Disable warning C4820; just tells you struct is not an exactly
            ; round size for the platform.
            ;
            <msc:/wd4820>

            ; Without disabling this, you likely get:
            ;
            ;     '_WIN32_WINNT_WIN10_TH2' is not defined as a preprocessor
            ;     macro, replacing with '0' for '#if/#elif'
            ;
            ; Which seems to be some mistake on Microsoft's part, that some
            ; report can be remedied by using WIN32_LEAN_AND_MEAN:
            ;
            ; https://stackoverflow.com/q/11040133/
            ;
            ; But then if you include <winioctl.h> (where the problem occurs)
            ; you'd still have it.
            ;
            <msc:/wd4668>

            ; There are a currently a lot of places in the code where `int` is
            ; passed to REBCNT, where the signs mismatch.  Disable C4365:
            ;
            ;    'action' : conversion from 'type_1' to 'type_2',
            ;    signed/unsigned mismatch
            ;
            ; and C4245:
            ;
            ;    'conversion' : conversion from 'type1' to 'type2',
            ;    signed/unsigned mismatch
            ;
            <msc:/wd4365> <msc:/wd4245>
            <gnu:-Wsign-compare>

            ; The majority of Rebol's C code was written with little
            ; attention to overflow in arithmetic.  There are a lot of places
            ; in the code where a bigger type is converted into a smaller type
            ; without an explicit cast.  (e.g. REBI64 => SQLUSMALLINT,
            ; REBINT => REBYTE).  Disable C4242:
            ;
            ;     'identifier' : conversion from 'type1' to 'type2', possible
            ;     loss of data
            ;
            ; The issue needs systemic review.
            ;
            <msc:/wd4242>
            <gnu:-Wno-conversion> <gnu:-Wno-strict-overflow>
            ;<gnu:-Wstrict-overflow=5>

            ; When an inline function is not referenced, there can be a
            ; warning about this; but it makes little sense to do so since
            ; there are a lot of standard library functions in includes that
            ; are inline which one does not use (C4514):
            ;
            ;     'function' : unreferenced inline function has been removed
            ;
            ; Inlining is at the compiler's discretion, it may choose to
            ; ignore the `inline` keyword.  Usually it won't tell you it did
            ; this, but disable the warning that tells you (C4710):
            ;
            ;     function' : function not inlined
            ;
            ; There's also an "informational" warning telling you that a
            ; function was chosen for inlining when it wasn't requested, so
            ; disable that also (C4711):
            ;
            ;     function 'function' selected for inline expansion
            ;
            <msc:/wd4514>
            <msc:/wd4710>
            <msc:/wd4711>

            ; It's useful to be told when a function pointer is assigned to
            ; an incompatible type of function pointer.  However, Rebol relies
            ; on the ability to have a kind of "void*-for-functions", e.g.
            ; CFUNC, which holds arbitrary function pointers.  There seems to
            ; be no way to enable function pointer type checking that allows
            ; downcasts and upcasts from just that pointer type, so it pretty
            ; much has to be completely disabled (or managed with #pragma,
            ; which we seek to avoid using in the codebase)
            ;
            ;    'operator/operation' : unsafe conversion from
            ;    'type of expression' to 'type required'
            ;
            <msc:/wd4191>

            ; Though we make sure all enum values are handled at least with a
            ; default:, this warning basically doesn't let you use default:
            ; at all...forcing every case to be handled explicitly.
            ;
            ;     enumerator 'identifier' in switch of enum 'enumeration'
            ;     is not explicitly handled by a case label
            ;
            <msc:/wd4061>

            ; setjmp() and longjmp() cannot be combined with C++ objects due
            ; to bypassing destructors.  Yet the Microsoft compiler seems to
            ; think even "POD" (plain-old-data) structs qualify as
            ; "C++ objects", so they run destructors (?)
            ;
            ;     interaction between 'function' and C++ object destruction
            ;     is non-portable
            ;
            ; This is lousy, because it would be a VERY useful warning, if it
            ; weren't as uninformative as "your C++ program is using setjmp".
            ;
            ; https://stackoverflow.com/q/45384718/
            ;
            <msc:/wd4611>

            ; Assignment within conditional expressions is tolerated in the
            ; core so long as parentheses are used.  if ((x = 10) != y) {...}
            ;
            ;     assignment within conditional expression
            ;
            <msc:/wd4706>

            ; gethostbyname() is deprecated by Microsoft, but dealing with
            ; that is not a present priority.  It is supposed to be replaced
            ; with getaddrinfo() or GetAddrInfoW().  This bypasses the
            ; deprecation warning for now via a #define
            ;
            <msc:/D_WINSOCK_DEPRECATED_NO_WARNINGS>

            ; This warning happens a lot in a 32-bit build if you use float
            ; instead of double in Microsoft Visual C++:
            ;
            ;    storing 32-bit float result in memory, possible loss
            ;    of performance
            ;
            <msc:/wd4738>
        ]
    ]
    _ #[false] no off false [
        cfg-rigorous: false
        _
    ]
][
    fail ["RIGOROUS must be yes, no, or logic! not" (user-config/rigorous)]
]

append app-config/ldflags opt switch/default user-config/static [
    _ no off false #[false] [
        ;pass
        _
    ]
    yes on #[true] [
        compose [
            <gnu:-static-libgcc>
            (if cfg-cplusplus [<gnu:-static-libstdc++>])
            (if cfg-sanitize [<gnu:-static-libasan>])
        ]
    ]
][
    fail ["STATIC must be yes, no or logic! not" (user-config/static)]
]

;libffi
libffi-cflags: _
either block? user-config/with-ffi [
    ffi-proto: make object! [
        cflags:
        includes:
        definitions:
        ldflags:
        libraries:
        searches: _
    ]
    ffi: make ffi-proto user-config/with-ffi
    for-each word words-of ffi-proto [
        append get in app-config word
            opt get in ffi word
    ]
    append app-config/definitions [ {HAVE_LIBFFI_AVAILABLE} ]
][
    switch/default user-config/with-ffi [
        static dynamic [
            for-each var [includes cflags searches ldflags][
                x: rebmake/pkg-config
                    any [user-config/pkg-config {pkg-config}]
                    var
                    %libffi
                ;dump x
                unless empty? x [
                    append (get in app-config var) x
                ]
            ]

            libs: rebmake/pkg-config
                any [user-config/pkg-config {pkg-config}]
                'libraries
                %libffi

            append app-config/libraries map-each lib libs [
                make rebmake/ext-dynamic-class [
                    output: lib
                    flags: either user-config/with-ffi = 'static [[static]][_]
                ]
            ]
            append app-config/definitions [ {HAVE_LIBFFI_AVAILABLE} ]
        ]
        _ no off false #[false] [
            ;pass
        ]
    ][
        fail [
            "WITH-FFI should be one of [dynamic static no]"
            "not" (user-config/with-ffi)
        ]
    ]
]

;TCC
cfg-tcc: _
case [
    any [
        file? user-config/with-tcc
        find? [yes on true #[true]] user-config/with-tcc
    ][
        tcc-rootdir: either file? user-config/with-tcc [
            first split-path user-config/with-tcc
        ][
            %../external/tcc/
        ]
        cfg-tcc: make object! [
            exec-file: join-of tcc-rootdir any [get-env "TCC" %tcc]
            includes: [%../external/tcc]
            searches: reduce [tcc-rootdir]
            libraries: reduce [tcc-rootdir/libtcc1.a tcc-rootdir/libtcc.a]
            cpp-flags: get-env "TCC_CPP_EXTRA_FLAGS" ; extra cpp flags passed to tcc for preprocess %sys-core.i
        ]
        if block? cfg-tcc/libraries [
            cfg-tcc/libraries: map-each lib cfg-tcc/libraries [
                either file? lib [
                    either rebmake/ends-with? lib ".a" [
                        make rebmake/ext-static-class [
                            output: lib
                        ]
                    ][
                        make rebmake/ext-dynamic-class [
                            output: lib
                        ]
                    ]
                ][
                    lib
                ]
            ]
        ]

        for-each word [includes searches libraries] [
            append get in app-config word
                opt get in cfg-tcc word
        ]
        append app-config/definitions [ {WITH_TCC} ]
        append file-base/generated [
            %tmp-symbols.c
            %tmp-embedded-header.c
        ]
    ]
    find? [no off false #[false] _] user-config/with-tcc [
        ;pass
    ]
    true [
        fail [
            "WITH-TCC must be yes or no]"
            "not" (user-config/with-tcc)
        ]
    ]
]


prin ["Sanity checking on user config ... "]
;add user settings
for-each word [definitions includes cflags libraries ldflags][
    assert-no-blank-inside get in user-config word
]
append app-config/definitions opt user-config/definitions
append app-config/includes opt user-config/includes
append app-config/cflags opt user-config/cflags
append app-config/libraries opt user-config/libraries
append app-config/ldflags opt user-config/ldflags
print ["Good"]

;add system settings
prin ["Sanity checking on system config ... "]
for-each word [definitions cflags libraries ldflags][
    assert-no-blank-inside get in system-config word
]
add-app-def copy system-config/definitions
add-app-cflags copy system-config/cflags
add-app-lib copy system-config/libraries
add-app-ldflags copy system-config/ldflags
print ["Good"]

prin ["Sanity checking on app config ... "]
assert-no-blank-inside app-config/definitions
assert-no-blank-inside app-config/includes
assert-no-blank-inside app-config/cflags
assert-no-blank-inside app-config/libraries
assert-no-blank-inside app-config/ldflags
print ["Good"]

print ["definitions:" mold app-config/definitions]
print ["includes:" mold app-config/includes]
print ["libraries:" mold app-config/libraries]
print ["cflags:" mold app-config/cflags]
print ["ldflags:" mold app-config/ldflags]
print ["debug:" mold app-config/debug]
print ["optimization:" mold app-config/optimization]

append app-config/definitions reduce [
    unspaced ["TO_" uppercase to-string system-config/os-base]
    unspaced ["TO_" uppercase replace/all to-string system-config/os-name "-" "_"]
]

libr3-core: make rebmake/object-library-class [
    name: 'libr3-core
    definitions: join-of ["REB_API"] app-config/definitions
    includes: copy app-config/includes ;might be modified by the generator, thus copying
    cflags: copy app-config/cflags ;might be modified by the generator, thus copying
    optimization: app-config/optimization
    debug: app-config/debug
    depends: map-each w append-of file-base/core file-base/generated [
        gen-obj/dir w "../src/core/"
    ]
]

os-file-block: get bind
    (to word! append-of "os-" system-config/os-base)
    file-base

remove-each plus os-file-block [plus = '+] ;remove the '+ sign, we don't care here
remove-each plus file-base/os [plus = '+] ;remove the '+ sign, we don't care here
libr3-os: make libr3-core [
    name: 'libr3-os
    definitions: join-of ["REB_CORE"] app-config/definitions
    includes: copy app-config/includes ;might be modified by the generator, thus copying
    cflags: copy app-config/cflags ;might be modified by the generator, thus copying
    depends: map-each s append copy file-base/os os-file-block [
        gen-obj/dir s "../src/os/"
    ]
]

pthread: make rebmake/ext-dynamic-class [
    output: %pthread
    flags: [static]
]

;extensions
builtin-extensions: copy available-extensions
dynamic-extensions: make block! 8
for-each [action name modules] user-config/extensions [
    switch/default action [
        + [; builtin
            ;pass, default action
        ]
        * - [
            item: _
            for-next builtin-extensions [
                if builtin-extensions/1/name = name [
                    item: take builtin-extensions
                    if all [
                        not item/loadable
                        action = '*
                    ][
                        fail [{Extension} name {is not dynamically loadable}]
                    ]
                ]
            ]
            unless item [
                fail [{Unrecognized extension name:} name]
            ]

            if action = '* [;dynamic extension
                ;dump item
                selected-modules: either blank? modules [
                    ; all modules in the extension
                    item/modules
                ][
                    map-each m item/modules [
                        if find? modules m/name [
                            m
                        ]
                    ]
                ]

                if empty? selected-modules [
                    fail [
                        {No modules are selected,}
                        {check module names or use '-' to remove}
                    ]
                ]
                item/modules: selected-modules
                append dynamic-extensions item
            ]
        ]
    ][
        fail ["Unrecognized extension action:" mold action]
    ]
]

print ["Builtin extensions"]
for-each ext builtin-extensions [
    prin unspaced ["ext: " ext/name ": ["]
    for-each mod ext/modules [
        prin [mod/name space]
    ]
    print ["]"]
]
print ["Dynamic extensions"]
for-each ext dynamic-extensions [
    prin unspaced ["ext: " ext/name ": ["]
    for-each mod ext/modules [
        prin [mod/name space]
    ]
    print ["]"]
]

all-extensions: join-of builtin-extensions dynamic-extensions

ext-objs: make block! 8
for-each ext builtin-extensions [
    mod-obj: _
    for-each mod ext/modules [
        ;extract object-library, because an object-library can't depend on another object-library
        if all [block? mod/depends
            not empty? mod/depends][
            append ext-objs map-each s mod/depends [
                if all [
                    object? s
                    s/class-name = 'object-library-class
                ][
                    s
                ]
            ]
        ]

        append ext-objs mod-obj: process-module mod
        if mod-obj/libraries [
            assert-no-blank-inside mod-obj/libraries
            append app-config/libraries mod-obj/libraries
        ]

        if mod/ldflags [
            ;dump mod/ldflags
            assert-no-blank-inside mod/ldflags
            append app-config/ldflags mod/ldflags
        ]

        ; Modify module properties
        add-project-flags/I/D/c/O/g mod-obj
            app-config/includes
            join-of ["REB_API"] app-config/definitions
            app-config/cflags
            app-config/optimization
            app-config/debug
    ]
    if ext/source [
        append any [all [mod-obj mod-obj/depends] ext-objs] gen-obj/dir ext/source "../src/extensions/"
    ]
]

vars: reduce [
    reb-tool: make rebmake/var-class [
        name: {REBOL_TOOL}
        value: any [
                user-config/rebol-tool
                unspaced [{./r3-make} rebmake/target-platform/exe-suffix]
            ]
    ]
    make rebmake/var-class [
        name: {REBOL}
        value: {$(REBOL_TOOL) -qs}
    ]
    make rebmake/var-class [
        name: {T}
        value: {../src/tools}
    ]
    make rebmake/var-class [
        name: {GIT_COMMIT}
        default: either user-config/git-commit [user-config/git-commit][{unknown}]
    ]
]

prep: make rebmake/entry-class [
    target: "prep"
    commands: compose [
        {$(REBOL) $T/make-natives.r}
        {$(REBOL) $T/make-headers.r}
        (unspaced [ {$(REBOL) $T/make-boot.r OS_ID=} system-config/id { GIT_COMMIT=$(GIT_COMMIT)}])
        {$(REBOL) $T/make-host-init.r}
        {$(REBOL) $T/make-os-ext.r}
        {$(REBOL) $T/make-host-ext.r}
        {$(REBOL) $T/make-reb-lib.r}
        (
            cmds: make block! 8
            for-each ext all-extensions [
                for-each mod ext/modules [
                    append cmds unspaced [
                        {$(REBOL) $T/make-ext-natives.r MODULE=} mod/name { SRC=../extensions/}
                            either file? mod/source [
                                mod/source
                            ][
                                ensure [block!] mod/source
                                first find mod/source file!
                            ]
                        { OS_ID=} system-config/id
                    ]
                ]
                if ext/init [
                    append cmds unspaced [
                        {$(REBOL) $T/make-ext-init.r SRC=../extensions/} ext/init
                    ]
                ]
            ]
            cmds
        )
        (
            unspaced [
                {$(REBOL) $T/make-boot-ext-header.r EXTENSIONS=}
                delimit map-each ext builtin-extensions [
                    to string! ext/name
                ] #","
            ]
        )
        (
            if cfg-tcc [
                sys-core-i: make rebmake/object-file-class [
                    compiler: make rebmake/tcc [
                        exec-file: cfg-tcc/exec-file
                    ]
                    output: %../src/include/sys-core.i
                    source: %../src/include/sys-core.h
                    definitions: join-of app-config/definitions [ {REN_C_STDIO_OK} ]
                    includes: append-of app-config/includes [%../external/tcc %../external/tcc/include]
                    cflags: append-of append-of [ {-dD} {-nostdlib} ] opt libffi-cflags opt cfg-tcc/cpp-flags
                ]
                reduce [
                    sys-core-i/command/E
                    {$(REBOL) $T/make-embedded-header.r}
                ]
            ]
        )
    ]
    depends: reduce [
        reb-tool
    ]
]

;print mold prep/commands

folders: copy [%objs/ %objs/generic/]
if find? [linux android osx] system-config/os-base [append folders [%objs/posix/]]
if find? [android] system-config/os-base [append folders [%objs/linux/]]
append folders join-of %objs/ [system-config/os-base #"/"]

add-new-obj-folders ext-objs folders
;print ["ext-objs: (" length ext-objs ")" mold ext-objs]
;print ["app-config/ldflags:" mold app-config/ldflags]
app: make rebmake/application-class [
    name: 'main
    output: %r3 ;no suffix
    depends: compose [
        (libr3-core)
        (libr3-os)
        (ext-objs)
        (app-config/libraries)
    ]
    post-build-commands: either cfg-symbols [
        _
    ][
        reduce [
            make rebmake/cmd-strip-class [
                file: join-of output opt rebmake/target-platform/exe-suffix
            ]
        ]
    ]

    searches: app-config/searches
    ldflags: app-config/ldflags
    cflags: app-config/cflags
    optimization: app-config/optimization
    debug: app-config/debug
    includes: app-config/includes
    definitions: app-config/definitions
]


dynamic-libs: make block! 8
ext-libs: make block! 8
ext-ldflags: make block! 8
ext-dynamic-objs: make block! 8
for-each ext dynamic-extensions [
    mod-objs: make block! 8
    for-each mod ext/modules [
        ;dump mod
        append mod-objs mod-obj: process-module mod
        append ext-libs opt mod-obj/libraries

        if mod/ldflags [
            assert-no-blank-inside mod/ldflags
            append ext-ldflags mod/ldflags
        ]

        ; Modify module properties
        add-project-flags/I/D/c/O/g mod-obj
            app-config/includes
            join-of ["EXT_DLL"] app-config/definitions
            app-config/cflags
            app-config/optimization
            app-config/debug
    ]

    append ext-dynamic-objs copy mod-objs

    if ext/source [
        append mod-objs gen-obj/dir/D ext/source "../src/extensions/" ["EXT_DLL"]
    ]
    append dynamic-libs ext-proj: make rebmake/dynamic-library-class [
        name: join-of either system-config/os-base = 'windows ["r3-"]["libr3-"]
            lowercase to string! ext/name
        output: to file! name
        depends: append compose [
            (mod-objs)
            (app) ;all dynamic extensions depend on r3
            (app-config/libraries)
        ] ext-libs

        post-build-commands: either cfg-symbols [
            _
        ][
            reduce [
                make rebmake/cmd-strip-class [
                    file: join-of output opt rebmake/target-platform/dll-suffix
                ]
            ]
        ]

        ldflags: append-of either empty? ext-ldflags [[]][ext-ldflags] [<gnu:-Wl,--as-needed>]
    ]

    add-project-flags/I/D/c/O/g ext-proj
        app-config/includes
        join-of ["EXT_DLL"] app-config/definitions
        app-config/cflags
        app-config/optimization
        app-config/debug

    add-new-obj-folders mod-objs folders
]

;dump folders
;print ["dynamic-libs:" mold dynamic-libs]

top: make rebmake/entry-class [
    target: "top"
    depends: flatten reduce [app dynamic-libs]
]

t-folders: make rebmake/entry-class [
    target: "folders"
    commands: map-each dir sort folders [;sort it so that the parent folder gets created first
        make rebmake/cmd-create-class compose [
            file: (dir)
        ]
    ]
]

clean: make rebmake/entry-class [
    target: "clean"
    commands: append flatten reduce [
        make rebmake/cmd-delete-class [file: %objs/]
        make rebmake/cmd-delete-class [file: join-of %r3 opt rebmake/target-platform/exe-suffix]
        make rebmake/cmd-delete-class [file: %../src/include/tmp-*.h]
        use [s][
            map-each s [
                %host-lib.h
                %reb-evtypes.h
                %reb-lib.h
                %reb-lib-lib.h
                %reb-types.h
            ][
                make rebmake/cmd-delete-class [file: join-of %../src/include/ s]
            ]
        ]
        use [s][
            map-each s file-base/generated [
                make rebmake/cmd-delete-class [file: join-of %../src/core/ s]
            ]
        ]
        use [s][
            map-each s dynamic-libs [
                make rebmake/cmd-delete-class [file: join-of s/output opt rebmake/target-platform/dll-suffix]
            ]
        ]
    ] if system-config/os-base != 'Windows [
        [
            {find ../src -name 'tmp-*' -exec rm -f {} \;}
            {grep -l "AUTO-GENERATED FILE" ../src/include/*.h |grep -v sys-zlib.h|xargs rm 2>/dev/null || true}
        ]
    ]
]

check: make rebmake/entry-class [
    target: "check"
    depends: join-of dynamic-libs app
    commands: append reduce [
        make rebmake/cmd-strip-class [
            file: join-of app/output opt rebmake/target-platform/exe-suffix
        ]
    ] map-each s dynamic-libs [
        make rebmake/cmd-strip-class [
            file: join-of s/output opt rebmake/target-platform/dll-suffix
        ]
    ]
]

;print ["check:" mold check]

solution: make rebmake/solution-class [
    name: 'app
    depends: flatten reduce [
        vars
        top
        t-folders
        prep
        ext-objs
        libr3-core
        libr3-os
        app
        dynamic-libs
        ext-dynamic-objs
        check
        clean
    ]
    debug: app-config/debug
]

switch/default user-config/target [
    clean [
        change-dir %../../make
        rebmake/execution/run make rebmake/solution-class [
            depends: reduce [
                clean
            ]
        ]
    ]
    execution [
        change-dir %../../make
        rebmake/execution/run make rebmake/solution-class [
            depends: flatten reduce [
                vars
                prep
                t-folders
                app
                dynamic-libs
            ]
        ]
    ]
    makefile [
        rebmake/makefile/generate %../../make/makefile solution
    ]
    nmake [
        rebmake/nmake/generate %../../make/makefile solution
    ]
    vs2017
    visual-studio [
        rebmake/visual-studio/generate/(all [system-config/os-name = 'Windows-x86 'x86]) %../../make solution
    ]
    vs2015 [
        rebmake/vs2015/generate/(all [system-config/os-name = 'Windows-x86 'x86]) %../../make solution
    ]
][
    fail [
        "Unsupported target (execution, makefile or nmake):"
        user-config/target
    ]
]
