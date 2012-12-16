import module [
    title: "REBOL 3 TLS protocol scheme"
    version: 0.1.0
    name: tls
] [
    debug:
    none
    emit: func [
        ctx [object!]
        code [block! binary!]
    ] [
        repend ctx/msg code
    ]
    to-bin: func [
        val [integer!]
        width [integer!]
    ] [
        skip tail to-binary val negate width
    ]
    make-tls-error: func [
        "Make an error for the TLS protocol"
        message [string! block!]
    ] [
        if block? message [message: ajoin message]
        make error! [
            type: 'Access
            id: 'Protocol
            arg1: message
        ]
    ]
    tls-error: func [
        "Throw an error for the TLS protocol"
        message [string! block!]
    ] [
        do make-tls-error message
    ]
    universal-tags: [
        eoc
        boolean
        integer
        bit-string
        octet-string
        null
        object-identifier
        object-descriptor
        external
        real
        enumerated
        embedded-pdv
        utf8string
        relative-oid
        undefined
        undefined
        sequence
        set
        numeric-string
        printable-string
        t61-string
        videotex-string
        ia5-string
        utc-time
        generalized-time
        graphic-string
        visible-string
        general-string
        universal-string
        character-string
        bmp-string
    ]
    class-types: [universal application context-specific private]
    parse-asn: func [
        data [binary!]
        /local
        mode d constructed? class tag ln length result val
    ] [
        result: copy []
        mode: 'type
        while [d: data/1] [
            switch mode [
                type [
                    constructed?: to logic! (d and 32)
                    class: pick class-types 1 + shift d -6
                    switch class [
                        universal [
                            tag: pick universal-tags 1 + (d and 31)
                        ]
                        context-specific [
                            tag: class
                            val: d and 31
                        ]
                    ]
                    mode: 'length
                ]
                length [
                    length: d and 127
                    unless zero? (d and 128) [
                        ln: length
                        length: to integer! copy/part next data length
                        data: skip data ln
                    ]
                    either zero? length [
                        append/only result compose/deep [(tag) [(either constructed? ["constructed"] ["primitive"]) (index? data) (length) none]]
                        mode: 'type
                    ] [
                        mode: 'value
                    ]
                ]
                value [
                    switch class [
                        universal [
                            val: copy/part data length
                            append/only result compose/deep [(tag) [(either constructed? ["constructed"] ["primitive"]) (index? data) (length) (either constructed? [none] [val])]]
                            if constructed? [
                                poke second last result 4 parse-asn val
                            ]
                        ]
                        context-specific [
                            append/only result compose/deep [(tag) [(val) (length)]]
                            parse-asn copy/part data length
                        ]
                    ]
                    data: skip data length - 1
                    mode: 'type
                ]
            ]
            data: next data
        ]
        result
    ]
    read-proto-states: [
        client-hello [server-hello]
        server-hello [certificate]
        certificate [server-hello-done]
        server-hello-done []
        finished [change-cipher-spec alert]
        change-cipher-spec [encrypted-handshake]
        application [application alert]
        alert []
    ]
    write-proto-states: [
        server-hello-done [client-key-exchange]
        client-key-exchange [change-cipher-spec]
        change-cipher-spec [finished]
        encrypted-handshake [application]
    ]
    get-next-proto-state: func [
        ctx [object!]
        /write-state "default is read state"
        /local
        next-state
    ] [
        all [
            next-state: select/only/skip either write-state [write-proto-states] [read-proto-states] ctx/protocol-state 2
            not empty? next-state
            next-state
        ]
    ]
    update-proto-state: func [
        ctx [object!]
        new-state [word!]
        /write-state
        /local
        next-state
    ] [
        debug [ctx/protocol-state "->" new-state write-state]
        either any [
            none? ctx/protocol-state
            all [
                next-state: apply :get-next-proto-state [ctx write-state]
                find next-state new-state
            ]
        ] [
            debug ["new-state ->" new-state]
            ctx/protocol-state: new-state
        ] [
            do make error! "invalid protocol state"
        ]
    ]
    client-hello: func [
        ctx [object!]
        /local
        beg len
    ] [
        ctx/client-random: to-bin to-integer difference now/precise 1-Jan-1970 4
        random/seed now/time/precise
        loop 28 [append ctx/client-random (random/secure 256) - 1]
        beg: length? ctx/msg
        emit ctx [
            #{16}
            ctx/version
            #{0000}
            #{01}
            #{000000}
            ctx/version
            ctx/client-random
            #{00}
            #{0002}
            ctx/cipher-suite
            #{01}
            #{00}
        ]
        change at ctx/msg beg + 7 to-bin len: length? at ctx/msg beg + 10 3
        change at ctx/msg beg + 4 to-bin len + 4 2
        append clear ctx/handshake-messages copy at ctx/msg beg + 6
        return ctx/msg
    ]
    client-key-exchange: func [
        ctx [object!]
        /local
        rsa-key pms-enc beg len
    ] [
        ctx/pre-master-secret: copy ctx/version
        random/seed now/time/precise
        loop 46 [append ctx/pre-master-secret (random/secure 256) - 1]
        rsa-key: rsa-make-key
        rsa-key/e: ctx/pub-exp
        rsa-key/n: ctx/pub-key
        pms-enc: rsa ctx/pre-master-secret rsa-key
        beg: length? ctx/msg
        emit ctx [
            #{16}
            ctx/version
            #{0000}
            #{10}
            #{000000}
            to-bin length? pms-enc 2
            pms-enc
        ]
        change at ctx/msg beg + 7 to-bin len: length? at ctx/msg beg + 10 3
        change at ctx/msg beg + 4 to-bin len + 4 2
        append ctx/handshake-messages copy at ctx/msg beg + 6
        make-master-secret ctx ctx/pre-master-secret
        make-key-block ctx
        ctx/client-mac-key: copy/part ctx/key-block ctx/hash-size
        ctx/server-mac-key: copy/part skip ctx/key-block ctx/hash-size ctx/hash-size
        ctx/client-crypt-key: copy/part skip ctx/key-block 2 * ctx/hash-size ctx/crypt-size
        ctx/server-crypt-key: copy/part skip ctx/key-block 2 * ctx/hash-size + ctx/crypt-size ctx/crypt-size
        return ctx/msg
    ]
    change-cipher-spec: func [
        ctx [object!]
    ] [
        emit ctx [
            #{14}
            ctx/version
            #{0001}
            #{01}
        ]
        return ctx/msg
    ]
    encrypted-handshake-msg: func [
        ctx [object!]
        message [binary!]
        /local
        plain-msg
    ] [
        plain-msg: message
        message: encrypt-data/type ctx message #{16}
        emit ctx [
            #{16}
            ctx/version
            to-bin length? message 2
            message
        ]
        append ctx/handshake-messages plain-msg
        return ctx/msg
    ]
    application-data: func [
        ctx [object!]
        message [binary! string!]
    ] [
        message: encrypt-data ctx to-binary message
        emit ctx [
            #{17}
            ctx/version
            to-bin length? message 2
            message
        ]
        return ctx/msg
    ]
    finished: func [
        ctx [object!]
    ] [
        ctx/seq-num: 0
        return rejoin [
            #{14}
            #{00000C}
            prf ctx/master-secret either ctx/server? ["server finished"] ["client finished"] rejoin [
                checksum/method ctx/handshake-messages 'md5 checksum/method ctx/handshake-messages 'sha1
            ] 12
        ]
    ]
    encrypt-data: func [
        ctx [object!]
        data [binary!]
        /type
        msg-type [binary!] "application data is default"
        /local
        crypt-data
    ] [
        data: rejoin [
            data
            checksum/method/key rejoin [
                #{00000000} to-bin ctx/seq-num 4
                any [msg-type #{17}]
                ctx/version
                to-bin length? data 2
                data
            ] ctx/hash-method decode 'text ctx/client-mac-key
        ]
        switch ctx/crypt-method [
            rc4 [
                either ctx/encrypt-stream [
                    rc4/stream data ctx/encrypt-stream
                ] [
                    ctx/encrypt-stream: rc4/key data ctx/client-crypt-key
                ]
            ]
        ]
        return data
    ]
    decrypt-data: func [
        ctx [object!]
        data [binary!]
        /local
        crypt-data
    ] [
        switch ctx/crypt-method [
            rc4 [
                either ctx/decrypt-stream [
                    rc4/stream data ctx/decrypt-stream
                ] [
                    ctx/decrypt-stream: rc4/key data ctx/server-crypt-key
                ]
            ]
        ]
        return data
    ]
    protocol-types: [
        20 change-cipher-spec
        21 alert
        22 handshake
        23 application
    ]
    message-types: [
        0 hello-request
        1 client-hello
        2 server-hello
        11 certificate
        12 server-key-exchange
        13 certificate-request
        14 server-hello-done
        15 certificate-verify
        16 client-key-exchange
        20 finished
    ]
    alert-descriptions: [
        0 "Close notify"
        10 "Unexpected message"
        20 "Bad record MAC"
        21 "Decryption failed"
        22 "Record overflow"
        30 "Decompression failure"
        40 "Handshake failure"
        41 "No certificate"
        42 "Bad certificate"
        43 "Unsupported certificate"
        44 "Certificate revoked"
        45 "Certificate expired"
        46 "Certificate unknown"
        47 "Illegal parameter"
        48 "Unknown CA"
        49 "Access denied"
        50 "Decode error"
        51 "Decrypt error"
        60 "Export restriction"
        70 "Protocol version"
        71 "Insufficient security"
        80 "Internal error"
        90 "User cancelled"
        100 "No renegotiation"
        110 "Unsupported extension"
    ]
    parse-protocol: func [
        data [binary!]
        /local proto
    ] [
        unless proto: select protocol-types data/1 [
            do make error! "unknown/invalid protocol type"
        ]
        return context [
            type: proto
            version: pick [ssl-v3 tls-v1.0 tls-v1.1] data/3 + 1
            length: to-integer copy/part at data 4 2
            messages: copy/part at data 6 length
        ]
    ]
    parse-messages: func [
        ctx [object!]
        proto [object!]
        /local
        result data msg-type len clen msg-content mac msg-obj
    ] [
        result: copy []
        data: proto/messages
        if ctx/encrypted? [
            change data decrypt-data ctx data
            debug ["decrypted:" data]
        ]
        debug [ctx/seq-num "-->" proto/type]
        switch proto/type [
            alert [
                append result reduce [
                    context [
                        level: any [pick [warning fatal] data/1 'unknown]
                        description: any [select alert-descriptions data/2 "unknown"]
                    ]
                ]
            ]
            handshake [
                while [data/1] [
                    len: to-integer copy/part at data 2 3
                    append result switch msg-type: select message-types data/1 [
                        server-hello [
                            msg-content: copy/part at data 7 len
                            msg-obj: context [
                                type: msg-type
                                version: pick [ssl-v3 tls-v1.0 tls-v1.1] data/6 + 1
                                length: len
                                server-random: copy/part msg-content 32
                                session-id: copy/part at msg-content 34 msg-content/33
                                cipher-suite: copy/part at msg-content 34 + msg-content/33 2
                                compression-method-length: first at msg-content 36 + msg-content/33
                                compression-method: either compression-method-length = 0 [none] [copy/part at msg-content 37 + msg-content/33 compression-method-length]
                            ]
                            ctx/server-random: msg-obj/server-random
                            msg-obj
                        ]
                        certificate [
                            msg-content: copy/part at data 5 len
                            msg-obj: context [
                                type: msg-type
                                length: len
                                certificates-length: to-integer copy/part msg-content 3
                                certificate-list: copy []
                                while [msg-content/1] [
                                    if 0 < clen: to-integer copy/part skip msg-content 3 3 [
                                        append certificate-list copy/part at msg-content 7 clen
                                    ]
                                    msg-content: skip msg-content 3 + clen
                                ]
                            ]
                            ctx/certificate: parse-asn msg-obj/certificate-list/1
                            ctx/pub-key: parse-asn next
                            ctx/certificate/1/sequence/4/1/sequence/4/7/sequence/4/2/bit-string/4
                            ctx/pub-exp: ctx/pub-key/1/sequence/4/2/integer/4
                            ctx/pub-key: next ctx/pub-key/1/sequence/4/1/integer/4
                            msg-obj
                        ]
                        server-hello-done [
                            context [
                                type: msg-type
                                length: len
                            ]
                        ]
                        client-hello [
                            msg-content: copy/part at data 7 len
                            context [
                                type: msg-type
                                version: pick [ssl-v3 tls-v1.0 tls-v1.1] data/6 + 1
                                length: len
                                content: msg-content
                            ]
                        ]
                        finished [
                            msg-content: copy/part at data 5 len
                            either msg-content <> prf ctx/master-secret either ctx/server? ["client finished"] ["server finished"] rejoin [checksum/method ctx/handshake-messages 'md5 checksum/method ctx/handshake-messages 'sha1] 12 [
                                do make error! "Bad 'finished' MAC"
                            ] [
                                debug "FINISHED MAC verify: OK"
                            ]
                            context [
                                type: msg-type
                                length: len
                                content: msg-content
                            ]
                        ]
                    ]
                    append ctx/handshake-messages copy/part data len + 4
                    data: skip data len + either ctx/encrypted? [
                        mac: copy/part skip data len + 4 ctx/hash-size
                        if mac <> checksum/method/key rejoin [
                            #{00000000} to-bin ctx/seq-num 4
                            #{16}
                            ctx/version
                            to-bin len + 4 2
                            copy/part data len + 4
                        ] ctx/hash-method decode 'text ctx/server-mac-key
                        [
                            do make error! "Bad record MAC"
                        ]
                        4 + ctx/hash-size
                    ] [
                        4
                    ]
                ]
            ]
            change-cipher-spec [
                ctx/seq-num: -1
                ctx/encrypted?: true
                append result context [
                    type: 'ccs-message-type
                ]
            ]
            application [
                append result context [
                    type: 'app-data
                    content: copy/part data (length? data) - ctx/hash-size
                ]
            ]
        ]
        ctx/seq-num: ctx/seq-num + 1
        return result
    ]
    parse-response: func [
        ctx [object!]
        msg [binary!]
        /local
        result proto messages len
    ] [
        result: copy []
        len: 0
        until [
            append result proto: parse-protocol msg
            either empty? messages: parse-messages ctx proto [
                do make error! "unknown/invalid protocol message"
            ] [
                proto/messages: messages
            ]
            tail? msg: skip msg proto/length + 5
        ]
        return result
    ]
    prf: func [
        secret [binary!]
        label [string! binary!]
        seed [binary!]
        output-length [integer!]
        /local
        len mid s-1 s-2 a p-sha1 p-md5
    ] [
        len: length? secret
        mid: to-integer 0.5 * (len + either odd? len [1] [0])
        s-1: copy/part secret mid
        s-2: copy at secret mid + either odd? len [0] [1]
        seed: rejoin [#{} label seed]
        p-md5: copy #{}
        a: seed
        while [output-length > length? p-md5] [
            a: checksum/method/key a 'md5 decode 'text s-1
            append p-md5 checksum/method/key rejoin [a seed] 'md5 decode 'text s-1
        ]
        p-sha1: copy #{}
        a: seed
        while [output-length > length? p-sha1] [
            a: checksum/method/key a 'sha1 decode 'text s-2
            append p-sha1 checksum/method/key rejoin [a seed] 'sha1 decode 'text s-2
        ]
        return ((copy/part p-md5 output-length) xor copy/part p-sha1 output-length)
    ]
    make-key-block: func [
        ctx [object!]
    ] [
        ctx/key-block: prf ctx/master-secret "key expansion" rejoin [ctx/server-random ctx/client-random] 2 * ctx/hash-size + (2 * ctx/crypt-size)
    ]
    make-master-secret: func [
        ctx [object!]
        pre-master-secret [binary!]
    ] [
        ctx/master-secret: prf pre-master-secret "master secret" rejoin [ctx/client-random ctx/server-random] 48
    ]
    do-commands: func [
        ctx [object!]
        commands [block!]
        /no-wait
        /local arg cmd
    ] [
        clear ctx/msg
        parse commands [
            some [
                set cmd [
                    'client-hello (client-hello ctx)
                    | 'client-key-exchange (client-key-exchange ctx)
                    | 'change-cipher-spec (change-cipher-spec ctx)
                    | 'finished (encrypted-handshake-msg ctx finished ctx)
                    | 'application set arg [string! | binary!] (application-data ctx arg)
                ] (
                    debug [ctx/seq-num "<--" cmd]
                    ctx/seq-num: ctx/seq-num + 1
                    update-proto-state/write-state ctx cmd
                )
            ]
        ]
        debug ["writing bytes:" length? ctx/msg]
        ctx/resp: clear []
        write ctx/connection ctx/msg
        unless no-wait [
            unless port? wait [ctx/connection 30] [do make error! "port timeout"]
        ]
        ctx/resp
    ]
    tls-init: func [
        ctx [object!]
    ] [
        ctx/seq-num: 0
        ctx/protocol-state: none
        ctx/encrypted?: false
        switch ctx/crypt-method [
            rc4 [
                if ctx/encrypt-stream [
                    ctx/encrypt-stream: rc4 none ctx/encrypt-stream
                ]
                if ctx/decrypt-stream [
                    ctx/decrypt-stream: rc4 none ctx/decrypt-stream
                ]
            ]
        ]
    ]
    tls-read-data: func [
        ctx [object!]
        port-data [binary!]
        /local
        result data len proto new-state next-state record enc? pp
    ] [
        result: copy #{}
        record: copy #{}
        port-data: append ctx/data-buffer port-data
        clear ctx/connection/data
        while [
            5 = length? data: copy/part port-data 5
        ] [
            unless proto: select protocol-types data/1 [
                do make error! "unknown/invalid protocol type"
            ]
            append clear record data
            len: to-integer copy/part at data 4 2
            port-data: skip port-data 5
            debug ["reading bytes:" len]
            if len > length? port-data [
                ctx/data-buffer: copy skip port-data -5
                debug ["not enough data: read " length? port-data " of " len " bytes needed"]
                debug ["CONTINUE READING..." length? head ctx/data-buffer length? result length? record]
                unless empty? result [append ctx/resp parse-response ctx result]
                return false
                do make error! rejoin ["invalid length data: read " length? port-data "/" len " bytes"]
            ]
            data: copy/part port-data len
            port-data: skip port-data len
            debug ["received bytes:" length? data]
            append record data
            new-state: either proto = 'handshake [
                either enc? [
                    'encrypted-handshake
                ] [
                    select message-types record/6
                ]
            ] [
                proto
            ]
            update-proto-state ctx new-state
            if ctx/protocol-state = 'change-cipher-spec [enc?: true]
            append result record
            next-state: get-next-proto-state ctx
            debug ["State:" ctx/protocol-state "-->" next-state]
            ctx/data-buffer: copy port-data
            unless next-state [
                debug ["READING FINISHED" length? head ctx/data-buffer length? result]
                append ctx/resp parse-response ctx result
                return true
            ]
        ]
        debug ["READ NEXT STATE" length? head ctx/data-buffer length? result]
        append ctx/resp parse-response ctx result
        return false
    ]
    tls-awake: funct [event [event!]] [
        debug ["TLS Awake-event:" event/type]
        port: event/port
        tls-port: port/locals
        tls-awake: :tls-port/awake
        switch/default event/type [
            lookup [
                open port
                tls-init tls-port/state
                insert system/ports/system make event! [type: 'lookup port: tls-port]
                return false
            ]
            connect [
                do-commands tls-port/state [client-hello]
                if tls-port/state/resp/1/type = 'handshake [
                    do-commands tls-port/state [
                        client-key-exchange
                        change-cipher-spec
                        finished
                    ]
                ]
                insert system/ports/system make event! [type: 'connect port: tls-port]
                return false
            ]
            wrote [
                if tls-port/state/protocol-state = 'application [
                    insert system/ports/system make event! [type: 'wrote port: tls-port]
                    return false
                ]
                read port
                return false
            ]
            read [
                debug ["Read" length? port/data "bytes" tls-port/state/protocol-state]
                complete?: tls-read-data tls-port/state port/data
                application?: false
                foreach proto tls-port/state/resp [
                    if proto/type = 'application [
                        foreach msg proto/messages [
                            if msg/type = 'app-data [
                                append tls-port/data msg/content
                                application?: true
                                msg/type: none
                            ]
                        ]
                    ]
                ]
                either application? [
                    insert system/ports/system make event! [type: 'read port: tls-port]
                ] [
                    read port
                ]
                return complete?
            ]
            close [
                insert system/ports/system make event! [type: 'close port: tls-port]
                return true
            ]
        ] [
            print ["Unexpected TLS event:" event/type]
            close port
            return true
        ]
        false
    ]
    sys/make-scheme [
        name: 'tls
        title: "TLS protocol"
        spec: make system/standard/port-spec-net []
        actor: [
            read: func [
                port [port!]
                /local
                resp data msg
            ] [
                debug ["READ" open? port/state/connection]
                read port/state/connection
                return port
            ]
            write: func [port [port!] value [any-type!]] [
                foreach proto port/state/resp [
                    if proto/type = 'handshake [
                        foreach msg proto/messages [
                            if msg/type = 'finished [
                                do-commands/no-wait port/state compose [
                                    application (value)
                                ]
                                return port
                            ]
                        ]
                    ]
                ]
            ]
            open: func [port [port!] /local conn] [
                if port/state [return port]
                if none? port/spec/host [tls-error "Missing host address"]
                port/state: context [
                    data-buffer: copy #{}
                    resp: none
                    version: #{0301}
                    server?: false
                    protocol-state: none
                    hash-method: 'sha1
                    hash-size: 20
                    crypt-method: 'rc4
                    crypt-size: 16
                    cipher-suite: #{0005}
                    client-crypt-key:
                    client-mac-key:
                    server-crypt-key:
                    server-mac-key: none
                    seq-num: 0
                    msg: make binary! 4096
                    handshake-messages: make binary! 4096
                    encrypted?: false
                    client-random: server-random: pre-master-secret: master-secret: key-block: certificate: pub-key: pub-exp: none
                    encrypt-stream: decrypt-stream: none
                    connection: none
                ]
                port/state/connection: conn: make port! [
                    scheme: 'tcp
                    host: port/spec/host
                    port-id: port/spec/port-id
                    ref: rejoin [tcp:// host ":" port-id]
                ]
                port/data: clear #{}
                conn/awake: :tls-awake
                conn/locals: port
                open conn
                port
            ]
            open?: func [port [port!]] [
                found? all [port/state open? port/state/connection]
            ]
            close: func [port [port!]] [
                if port/state [
                    close port/state/connection
                    debug "TLS/TCP port closed"
                    port/state/connection/awake: none
                    port/state: none
                ]
                port
            ]
            copy: func [port [port!]] [
                if port/data [copy port/data]
            ]
            query: func [port [port!]] [
                all [port/state query port/state/connection]
            ]
            length?: func [port [port!]] [
                either port/data [length? port/data] [0]
            ]
        ]
    ]
]
import module [
    title: "REBOL 3 HTTP protocol scheme"
    version: 0.1.1
    name: http
] [
    sync-op: func [port body /local state] [
        unless port/state [open port port/state/close?: yes]
        state: port/state
        state/awake: :read-sync-awake
        do body
        if state/state = 'ready [do-request port]
        unless port? wait [state/connection port/spec/timeout] [http-error "Timeout"]
        body: copy port
        if all [
            select state/info/headers 'Content-Type
            state/info/headers/Content-Type
            parse state/info/headers/Content-Type [
                "text/" thru "; charset=UTF-8"
            ]
        ] [
            body: to string! body
        ]
        if state/close? [close port]
        body
    ]
    read-sync-awake: func [event [event!] /local error] [
        switch/default event/type [
            connect ready [
                do-request event/port
                false
            ]
            done [
                true
            ]
            close [
                true
            ]
            error [
                error: event/port/state/error
                event/port/state/error: none
                do error
            ]
        ] [
            false
        ]
    ]
    http-awake: func [event /local port http-port state awake res] [
        port: event/port
        http-port: port/locals
        state: http-port/state
        if any-function? :http-port/awake [state/awake: :http-port/awake]
        awake: :state/awake
        switch/default event/type [
            read [
                awake make event! [type: 'read port: http-port]
                check-response http-port
            ]
            wrote [
                awake make event! [type: 'wrote port: http-port]
                state/state: 'reading-headers
                read port
                false
            ]
            lookup [open port false]
            connect [
                state/state: 'ready
                awake make event! [type: 'connect port: http-port]
            ]
            close [
                res: switch state/state [
                    ready [
                        awake make event! [type: 'close port: http-port]
                    ]
                    doing-request reading-headers [
                        state/error: make-http-error "Server closed connection"
                        awake make event! [type: 'error port: http-port]
                    ]
                    reading-data [
                        either any [integer? state/info/headers/content-length state/info/headers/transfer-encoding = "chunked"] [
                            state/error: make-http-error "Server closed connection"
                            awake make event! [type: 'error port: http-port]
                        ] [
                            any [
                                awake make event! [type: 'done port: http-port]
                                awake make event! [type: 'close port: http-port]
                            ]
                        ]
                    ]
                ]
                close http-port
                res
            ]
        ] [true]
    ]
    make-http-error: func [
        "Make an error for the HTTP protocol"
        message [string! block!]
    ] [
        if block? message [message: ajoin message]
        make error! [
            type: 'Access
            id: 'Protocol
            arg1: message
        ]
    ]
    http-error: func [
        "Throw an error for the HTTP protocol"
        message [string! block!]
    ] [
        do make-http-error message
    ]
    make-http-request: func [
        "Create an HTTP request (returns string!)"
        method [word! string!] "E.g. GET, HEAD, POST etc."
        target [file! string!] {In case of string!, no escaping is performed (eg. useful to override escaping etc.). Careful!}
        headers [block!] "Request headers (set-word! string! pairs)"
        content [any-string! binary! none!] {Request contents (Content-Length is created automatically). Empty string not exactly like none.}
        /local result
    ] [
        result: rejoin [
            uppercase form method #" "
            either file? target [next mold target] [target]
            " HTTP/1.0" CRLF
        ]
        foreach [word string] headers [
            repend result [mold word #" " string CRLF]
        ]
        if content [
            content: to binary! content
            repend result ["Content-Length: " length? content CRLF]
        ]
        append result CRLF
        result: to binary! result
        if content [append result content]
        result
    ]
    do-request: func [
        "Perform an HTTP request"
        port [port!]
        /local spec info
    ] [
        spec: port/spec
        info: port/state/info
        spec/headers: body-of make make object! [
            Accept: "*/*"
            Accept-Charset: "utf-8"
            Host: either not find [80 443] spec/port-id [
                rejoin [form spec/host #":" spec/port-id]
            ] [
                form spec/host
            ]
            User-Agent: "REBOL"
        ] spec/headers
        port/state/state: 'doing-request
        info/headers: info/response-line: info/response-parsed: port/data:
        info/size: info/date: info/name: none
        write port/state/connection
        make-http-request spec/method to file! any [spec/path %/]
        spec/headers spec/content
    ]
    parse-write-dialect: func [port block /local spec] [
        spec: port/spec
        parse block [[set block word! (spec/method: block) | (spec/method: 'post)]
            opt [set block [file! | url!] (spec/path: block)] [set block block! (spec/headers: block) | (spec/headers: [])] [set block [any-string! | binary!] (spec/content: block) | (spec/content: none)]
        ]
    ]
    check-response: func [port /local conn res headers d1 d2 line info state awake spec] [
        state: port/state
        conn: state/connection
        info: state/info
        headers: info/headers
        line: info/response-line
        awake: :state/awake
        spec: port/spec
        if all [
            not headers
            d1: find conn/data crlfbin
            d2: find/tail d1 crlf2bin
        ] [
            info/response-line: line: to string! copy/part conn/data d1
            info/headers: headers: construct/with d1 http-response-headers
            info/name: to file! any [spec/path %/]
            if headers/content-length [info/size: headers/content-length: to integer! headers/content-length]
            if headers/last-modified [info/date: attempt [to date! headers/last-modified]]
            remove/part conn/data d2
            state/state: 'reading-data
        ]
        unless headers [
            read conn
            return false
        ]
        res: false
        unless info/response-parsed [
            parse/all line [
                "HTTP/1." [#"0" | #"1"] some #" " [
                    #"1" (info/response-parsed: 'info)
                    |
                    #"2" [["04" | "05"] (info/response-parsed: 'no-content)
                        | (info/response-parsed: 'ok)
                    ]
                    |
                    #"3" [
                        "03" (info/response-parsed: 'see-other)
                        |
                        "04" (info/response-parsed: 'not-modified)
                        |
                        "05" (info/response-parsed: 'use-proxy)
                        | (info/response-parsed: 'redirect)
                    ]
                    |
                    #"4" [
                        "01" (info/response-parsed: 'unauthorized)
                        |
                        "07" (info/response-parsed: 'proxy-auth)
                        | (info/response-parsed: 'client-error)
                    ]
                    |
                    #"5" (info/response-parsed: 'server-error)
                ]
                | (info/response-parsed: 'version-not-supported)
            ]
        ]
        switch/all info/response-parsed [
            ok [
                either spec/method = 'head [
                    state/state: 'ready
                    res: awake make event! [type: 'done port: port]
                    unless res [res: awake make event! [type: 'ready port: port]]
                ] [
                    res: check-data port
                    if all [not res state/state = 'ready] [
                        res: awake make event! [type: 'done port: port]
                        unless res [res: awake make event! [type: 'ready port: port]]
                    ]
                ]
            ]
            redirect see-other [
                either spec/method = 'head [
                    state/state: 'ready
                    res: awake make event! [type: 'custom port: port code: 0]
                ] [
                    res: check-data port
                    unless open? port [
                        state/state: 'ready
                    ]
                ]
                if all [not res state/state = 'ready] [
                    either all [
                        any [
                            find [get head] spec/method
                            all [
                                info/response-parsed = 'see-other
                                spec/method: 'get
                            ]
                        ]
                        in headers 'Location
                    ] [
                        res: do-redirect port headers/location
                    ] [
                        state/error: make-http-error "Redirect requires manual intervention"
                        res: awake make event! [type: 'error port: port]
                    ]
                ]
            ]
            unauthorized client-error server-error proxy-auth [
                either spec/method = 'head [
                    state/state: 'ready
                ] [
                    check-data port
                ]
            ]
            unauthorized [
                state/error: make-http-error "Authentication not supported yet"
                res: awake make event! [type: 'error port: port]
            ]
            client-error server-error [
                state/error: make-http-error ["Server error: " line]
                res: awake make event! [type: 'error port: port]
            ]
            not-modified [state/state: 'ready
                res: awake make event! [type: 'done port: port]
                unless res [res: awake make event! [type: 'ready port: port]]
            ]
            use-proxy [
                state/state: 'ready
                state/error: make-http-error "Proxies not supported yet"
                res: awake make event! [type: 'error port: port]
            ]
            proxy-auth [
                state/error: make-http-error "Authentication and proxies not supported yet"
                res: awake make event! [type: 'error port: port]
            ]
            no-content [
                state/state: 'ready
                res: awake make event! [type: 'done port: port]
                unless res [res: awake make event! [type: 'ready port: port]]
            ]
            info [
                info/headers: info/response-line: info/response-parsed: port/data: none
                state/state: 'reading-headers
                read conn
            ]
            version-not-supported [
                state/error: make-http-error "HTTP response version not supported"
                res: awake make event! [type: 'error port: port]
                close port
            ]
        ]
        res
    ]
    crlfbin: #{0D0A}
    crlf2bin: #{0D0A0D0A}
    crlf2: to string! crlf2bin
    http-response-headers: context [
        Content-Length:
        Transfer-Encoding:
        Last-Modified: none
    ]
    do-redirect: func [port [port!] new-uri [url! string! file!] /local spec state] [
        spec: port/spec
        state: port/state
        new-uri: decode-url new-uri
        unless select new-uri 'port-id [
            switch new-uri/scheme [
                'https [append new-uri [port-id: 443]]
                'http [append new-uri [port-id: 80]]
            ]
        ]
        new-uri: construct/with new-uri port/scheme/spec
        unless find [http https] new-uri/scheme [
            state/error: make-http-error {Redirect to a protocol different from HTTP or HTTPS not supported}
            return state/awake make event! [type: 'error port: port]
        ]
        either all [
            new-uri/host = spec/host
            new-uri/port-id = spec/port-id
        ] [
            spec/path: new-uri/path
            do-request port
            false
        ] [
            state/error: make-http-error "Redirect to other host - requires custom handling"
            state/awake make event! [type: 'error port: port]
        ]
    ]
    check-data: func [port /local headers res data out chunk-size mk1 mk2 trailer state conn] [
        state: port/state
        headers: state/info/headers
        conn: state/connection
        res: false
        case [
            headers/transfer-encoding = "chunked" [
                data: conn/data
                out: port/data: make binary! length? data
                until [
                    either parse/all data [
                        copy chunk-size some hex-digits thru crlfbin mk1: to end
                    ] [
                        chunk-size: to integer! to issue! chunk-size
                        either chunk-size = 0 [
                            if parse/all mk1 [
                                crlfbin (trailer: "") to end | copy trailer to crlf2bin to end
                            ] [
                                trailer: construct trailer
                                append headers body-of trailer
                                state/state: 'ready
                                res: state/awake make event! [type: 'custom port: port code: 0]
                                clear data
                            ]
                            true
                        ] [
                            either parse/all mk1 [
                                chunk-size skip mk2: crlfbin to end
                            ] [
                                insert/part tail out mk1 mk2
                                remove/part data skip mk2 2
                                empty? data
                            ] [
                                true
                            ]
                        ]
                    ] [
                        true
                    ]
                ]
                unless state/state = 'ready [read conn]
            ]
            integer? headers/content-length [
                port/data: conn/data
                either headers/content-length <= length? port/data [
                    state/state: 'ready
                    conn/data: make binary! 32000
                    res: state/awake make event! [type: 'custom port: port code: 0]
                ] [
                    read conn
                ]
            ]
            true [
                port/data: conn/data
                read conn
            ]
        ]
        res
    ]
    hex-digits: charset "1234567890abcdefABCDEF"
    sys/make-scheme [
        name: 'http
        title: "HyperText Transport Protocol v1.1"
        spec: make system/standard/port-spec-net [
            path: %/
            method: 'get
            headers: []
            content: none
            timeout: 15
        ]
        info: make system/standard/file-info [
            response-line:
            response-parsed:
            headers: none
        ]
        actor: [
            read: func [
                port [port!]
            ] [
                either any-function? :port/awake [
                    unless open? port [cause-error 'Access 'not-open port/spec/ref]
                    if port/state/state <> 'ready [http-error "Port not ready"]
                    port/state/awake: :port/awake
                    do-request port
                    port
                ] [
                    sync-op port []
                ]
            ]
            write: func [
                port [port!]
                value
            ] [
                unless any [block? :value binary? :value any-string? :value] [value: form :value]
                unless block? value [value: reduce [[Content-Type: "application/x-www-form-urlencoded; charset=utf-8"] value]]
                either any-function? :port/awake [
                    unless open? port [cause-error 'Access 'not-open port/spec/ref]
                    if port/state/state <> 'ready [http-error "Port not ready"]
                    port/state/awake: :port/awake
                    parse-write-dialect port value
                    do-request port
                    port
                ] [
                    sync-op port [parse-write-dialect port value]
                ]
            ]
            open: func [
                port [port!]
                /local conn
            ] [
                if port/state [return port]
                if none? port/spec/host [http-error "Missing host address"]
                port/state: context [
                    state: 'inited
                    connection:
                    error: none
                    close?: no
                    info: make port/scheme/info [type: 'file]
                    awake: :port/awake
                ]
                port/state/connection: conn: make port! compose [
                    scheme: (to lit-word! either port/spec/scheme = 'http ['tcp] ['tls])
                    host: port/spec/host
                    port-id: port/spec/port-id
                    ref: rejoin [tcp:// host ":" port-id]
                ]
                conn/awake: :http-awake
                conn/locals: port
                open conn
                port
            ]
            open?: func [
                port [port!]
            ] [
                found? all [port/state open? port/state/connection]
            ]
            close: func [
                port [port!]
            ] [
                if port/state [
                    close port/state/connection
                    port/state/connection/awake: none
                    port/state: none
                ]
                port
            ]
            copy: func [
                port [port!]
            ] [
                either all [port/spec/method = 'head port/state] [
                    reduce bind [name size date] port/state/info
                ] [
                    if port/data [copy port/data]
                ]
            ]
            query: func [
                port [port!]
                /local error state
            ] [
                if state: port/state [
                    either error? error: state/error [
                        state/error: none
                        error
                    ] [
                        state/info
                    ]
                ]
            ]
            length?: func [
                port [port!]
            ] [
                either port/data [length? port/data] [0]
            ]
        ]
    ]
    sys/make-scheme/with [
        name: 'https
        title: "Secure HyperText Transport Protocol v1.1"
        spec: make spec [
            port-id: 443
        ]
    ] 'http
]
load-gui: func [
    {Download current Spahirion's R3-GUI module from web.}
    /local data
] [
    print "Fetching GUI..."
    either error? data: try [load http://www.saphirion.com/development/downloads-2/files/r3-gui.r3] [
        either data/id = 'protocol [print "Cannot load GUI from web."] [do err]
    ] [
        do data
    ]
    exit
]
encode: funct [
    {Encodes a datatype (e.g. image!) into a series of bytes.}
    type [word!] "Media type (jpeg, png, etc.)"
    data [image! binary! string!] "The data to encode"
    /options opts [block!] "Special encoding options"
] [
    unless all [
        cod: select system/codecs type
        data: switch/default cod/name [
            png [
                to-png data
            ]
        ] [
            do-codec cod/entry 'encode data
        ]
    ] [
        cause-error 'access 'no-codec type
    ]
    data
]
console-output true
import module [
    title: "R3 Patches"
    version: none
    name: none
] [
    replace-export: func [
        "Replace a value in lib and all tracable exports."
        'name [word!] value [any-type!] /local old new m
    ] [
        if in lib name [
            set/any 'old get/any in lib name
            set/any in lib name :value
            m: system/modules
            forskip m 3 [
                if all [new: in :m/2 name same? :old get/any new] [set/any new :value]
            ]
            if all [
                new: in system/contexts/user name same? :old get/any new
            ] [set/any new :value]
        ]
    ]
    tmp: reduce [spec-of :sys/load-ext-module body-of :sys/load-ext-module]
    unless find tmp/1 'end [append tmp/1 'end]
    if attempt [none? :tmp/2/36/3] [
        append tmp/2/6 [end:]
        insert at tmp/2 28 [
            if all [not empty? end same? head code head end] [
                code: to block! copy/part code end
            ]
        ]
        append last tmp/2 'end
    ]
    bind bind tmp/2 lib sys
    sys/load-ext-module: make function! tmp
    fix: false
    tmp: reduce [spec-of :sys/load-module body-of :sys/load-module]
    if attempt ['resolve/extend/only = :tmp/2/9/49/2/8/1] [
        fix: true
        tmp/2/9/49/2/8: [
            resolve/only lib mod bind/new/only/copy hdr/exports lib
        ]
    ]
    unless find tmp/1 'end [append tmp/1 'end]
    if attempt [none? :tmp/2/11/6] [
        fix: true
        append tmp/2/7/12/5/7/8/2/2 [end:]
        append tmp/2/7/12/5/7/8/5 [end:]
        append tmp/2/7/12/5/7/8/12/2 [end:]
        append tmp/2/9/9/2 [end:]
        append tmp/2/9/41/3 'end
        insert at tmp/2/9/46/2 8 [
            all [not empty? end same? head code head end] [code: to block! copy/part code end]
        ]
        append last tmp/2 'end
    ]
    if fix [
        bind bind tmp/2 lib sys
        sys/load-module: make function! tmp
    ]
    tmp: body-of :sys/export-words
    if attempt ['resolve/extend/only = :tmp/3/1] [
        tmp/3: [
            words: bind/new/only bind/new/only/copy words lib system/contexts/user
            resolve/only lib ctx words
            resolve/only system/contexts/user lib words
        ]
        sys/export-words: make function! reduce [spec-of :sys/export-words bind tmp lib]
    ]
    tmp: body-of :lib/script?
    if attempt ['to-binary = :tmp/4/5/2] [
        change/part next :tmp/4/5 [to binary!] 1
        replace-export script? make function! reduce [spec-of :lib/script? bind tmp lib]
    ]
    fix: false
    tmp: body-of :lib/save
    if attempt ['block? = :tmp/11/8] [
        fix: true
        tmp/11/8: 'object?
        tmp/11/9: tmp/11/10/2: tmp/11/11/2: to get-word! :tmp/11/9
        swap at :tmp/11 10 at :tmp/11 11
    ]
    if attempt [same? unbind 'value :tmp/4/6] [
        fix: true
        tmp/4/6: tmp/11/5/3/2/2: to get-word! :tmp/4/6
    ]
    if fix [
        replace-export save make function! reduce [spec-of :lib/save bind tmp lib]
    ]
    tmp: fix: none
]
