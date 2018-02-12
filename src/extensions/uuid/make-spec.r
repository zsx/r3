REBOL []

name: 'UUID
source: %uuid/ext-uuid.c
init: %uuid/ext-uuid-init.reb
modules: [
    [
        name: 'UUID
        source: %uuid/mod-uuid.c
        includes: reduce [
            src-dir/extensions/uuid/libuuid
            %prep/extensions/uuid ;for %tmp-extensions-uuid-init.inc
        ]
        depends: to-value switch system-config/os-base [
            linux [
                [
                    %uuid/libuuid/gen_uuid.c
                    %uuid/libuuid/unpack.c
                    %uuid/libuuid/pack.c
                    %uuid/libuuid/randutils.c
                ]
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
]
