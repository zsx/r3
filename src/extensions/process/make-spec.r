REBOL []

name: 'Process
source: %process/ext-process.c
init: %process/ext-process-init.reb
modules: [
    [
        name: 'Process
        source: %process/mod-process.c
        includes: copy [
            %prep/extensions/process ;for %tmp-extensions-process-init.inc
        ]
    ]
]

