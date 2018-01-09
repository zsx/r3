REBOL []

ROOT: https://raw.githubusercontent.com/karelzak/util-linux/master/

mkdir %libuuid

pass: func [x][x]

add-config.h: [
    to "/*" thru "*/"
    thru "^/"
    insert {^/#include "config.h"^/}
]
space: charset " ^-^/^M"

;comment out unneeded headers
comment-out-includes: [
    pos: {#include}
    [
        [
            some space [
                exclude-headers
            ] (insert pos {//} pos: skip pos 2)
            | skip
        ] (pos: skip pos 8)
    ] :pos
]


fix-randutils.c: func [
    cnt
][
    exclude-headers: [
        {"c.h"}
    ]

    parse cnt [
        add-config.h
        insert {^/#include <errno.h>^/}

        any [
            comment-out-includes
            
            ;randutils.c:137:12: error: invalid conversion from ‘void*’ to ‘unsigned char*’ 
            | change {cp = buf} {cp = (unsigned char*)buf}

            ; Fix "error: invalid suffix on literal; C++11 requires a space between literal and identifier"
            | change {"PRIu64"} {" PRIu64 "}

            | skip
        ]
    ]

    cnt
]

fix-gen_uuid.c: function [
    cnt
    <with>
    exclude-headers
    comment-out-includes
    add-config.h
    space
][

    exclude-headers: [
        {"all-io.h"}
        | {"c.h"}
        | {"strutils.h"}
        | {"md5.h"}
        | {"sha1.h"}
    ]

    parse cnt [
        add-config.h

        any [
            ;comment out unneeded headers
            comment-out-includes

            ; avoid "unused node_id" warning
            | {get_node_id} thru #"^{" thru "^/" insert {^/^-(void)node_id;^/}

            ; comment out uuid_generate_md5, we don't need this
            | change [
                copy definition: [
                    {void uuid_generate_md5(} thru "^}"
                  ]
                  (target: unspaced [{#if 0^/} to string! definition {^/#endif^/}])
                ]
                target

            ; comment out uuid_generate_sha1, we don't need this
            | change [
                copy definition: [
                    {void uuid_generate_sha1(} thru "^}"
                  ]
                  (target: unspaced [{#if 0^/} to string! definition {^/#endif^/}])
                ]
                target

            ; comment out unused variable variant_bits
            | change [
                copy unused: [
                    {static unsigned char variant_bits[]}
                  ]
                  (target: unspaced [{// } to string! unused])
                ] target

            | skip
        ]
    ]
    cnt
]

files: compose [
    %include/nls.h              _
    %include/randutils.h        _
    %lib/randutils.c            (:fix-randutils.c)
    %libuuid/src/gen_uuid.c     (:fix-gen_uuid.c)    
    %libuuid/src/pack.c         _
    %libuuid/src/unpack.c       _
    %libuuid/src/uuidd.h        _
    %libuuid/src/uuid.h         _
    %libuuid/src/uuidP.h        _
]

for-each [file fix] files [
    unless :fix [fix: :pass]
    trap/with [
       cnt: read url: join-of ROOT file
    ] proc [
        error [error!]
    ][
        print ["Failed to fetch" url]
        dump error
    ]
    write join-of %libuuid/ (last split-path file) fix cnt
]

;write %tmp.c fix-randutils.c read %libuuid/randutils.c
