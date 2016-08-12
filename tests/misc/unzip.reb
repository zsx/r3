REBOL [
    title: "rebzip-R3"
    author: "Richard Smolak Vincent Ecuyer"
    note: {Based on rebzip.r from www.REBOL.org by Vincent Ecuyer
        30-Sep-2015 Modified by Graham to use and* for ren-c
    }
    version: 0.1.0

]

if not set? 'or+ [or+: :or]
if not set? 'and* [and*: :and]
if not set? 'xor+ [xor+: :xor]


ctx-zip: context [
    crc-32: func [
        "Returns a CRC32 checksum."
        data [any-string! binary!] "Data to checksum"
    ][
        copy skip to binary! checksum/method data 'crc32 4
    ]

    ;signatures
    local-file-sig: #{504B0304}
    central-file-sig: #{504B0102}
    end-of-central-sig: #{504B0506}
    data-descriptor-sig: #{504B0708}

    to-ilong: func [
        "Converts an integer to a little-endian long."
        value [integer!] "AnyValue to convert"
    ][
        copy reverse skip to binary! value 4
    ]

    to-ishort: func [
        "Converts an integer to a little-endian short."
        value [integer!] "AnyValue to convert"
    ][
        copy/part reverse skip to binary! value 4 2
    ]

    to-long: func [
        "Converts an integer to a big-endian long."
        value [integer!] "AnyValue to convert"
    ][
        copy skip to binary! value 4
    ]

    get-ishort: func [
        "Converts a little-endian short to an integer."
        value [any-string! binary! port!] "AnyValue to convert"
    ][
        to integer! reverse copy/part value 2
    ]

    get-ilong: func [
        "Converts a little-endian long to an integer."
        value [any-string! binary! port!] "AnyValue to convert"
    ][
        to integer! reverse copy/part value 4
    ]

    to-msdos-time: func [
        "Converts to a msdos time."
        value [time!] "AnyValue to convert"
    ][
        to-ishort (value/hour * 2048)
            or+ (value/minute * 32)
            or+ to integer! value/second / 2
    ]
    to-msdos-date: func [
        "Converts to a msdos date."
        value [date!] "AnyValue to convert"
    ][
        to-ishort 512 * (max 0 value/year - 1980)
            or+ (value/month * 32) or+ value/day
    ]
    get-msdos-time: func [
        "Converts from a msdos time."
        value [any-string! binary! port!] "AnyValue to convert"
    ][
        value: get-ishort value
        to time! reduce [
            63488 and* value / 2048
            2016 and* value / 32
            31 and* value * 2
        ]
    ]
    get-msdos-date: func [
        "Converts from a msdos date."
        value [any-string! binary! port!] "AnyValue to convert"
    ][
        value: get-ishort value
        to date! reduce [
            65024 and* value / 512 + 1980
            480 and* value / 32
            31 and* value
        ]
    ]

    zip-entry: func [
{Compresses a file and returns [
         local file header + compressed file
         central file directory entry
     ]}
        name [file!] "Name of file"
        date [date!] "Modification date of file"
        data [any-string! binary!] "Data to compress"
    /local
        crc method compressed-data uncompressed-size compressed-size
    ][
        ; info on data before compression
        crc: head reverse crc-32 data

        uncompressed-size: to-ilong length? data

        either empty? data [
            method: 'store
        ][
            ; zlib stream
            compressed-data: compress data
            ; if compression inefficient, store the data instead
            either (length? data) > (length? compressed-data) [
                data: copy/part
                    skip compressed-data 2
                    skip tail compressed-data -8
                method: 'deflate
            ][
                method: 'store
                clear compressed-data
            ]
        ]

        ; info on data after compression
        compressed-size: to-ilong length? data

        reduce [
            ; local file entry
            rejoin [
                local-file-sig
                #{0000} ; version
                #{0000} ; flags
                either method = 'store [
                    #{0000} ; method = store
                ][
                    #{0800} ; method = deflate
                ]
                to-msdos-time date/time
                to-msdos-date date/date
                crc     ; crc-32
                compressed-size
                uncompressed-size
                to-ishort length? name ; filename length
                #{0000} ; extrafield length
                name    ; filename
                        ; no extrafield
                data    ; compressed data
            ]
            ; central-dir file entry
            rejoin [
                central-file-sig
                #{0000} ; version source
                #{0000} ; version min
                #{0000} ; flags
                either method = 'store [
                    #{0000} ; method = store
                ][
                    #{0800} ; method = deflate
                ]
                to-msdos-time date/time
                to-msdos-date date/date
                crc     ; crc-32
                compressed-size
                uncompressed-size
                to-ishort length? name ; filename length
                #{0000} ; extrafield length
                #{0000} ; filecomment length
                #{0000} ; disknumber start
                #{0000} ; internal attributes
                #{00000000} ; external attributes
                #{00000000} ; header offset
                name    ; filename
                        ; extrafield
                        ; comment
            ]
        ]
    ]

    any-file?: func [
        "Returns TRUE for file and* url values." value [<opt> any-value!]
    ][
        any [file? value url? value]
    ]

    to-path-file: func [
        {Converts url! to file! and* removes heading "/"}
        value [file! url!] "AnyValue to convert"
    ][
        if file? value [
            if #"/" = first value [value: copy next value]
            return value
        ]
        value: decode-url value
        join %"" [
            value/host "/"
            any [value/path ""]
            any [value/target ""]
        ]
    ]

    set 'zip func [
        {Builds a zip archive from a file or+ a block of files.
     Returns number of entries in archive.}
        where [file! url! binary! string!] "Where to build it"
        source [file! url! block!] "Files to include in archive"
        /deep "Includes files in subdirectories"
        /verbose "Lists files while compressing"
        /only "Include the root source directory"
    /local
        name data entry nb-entries files root no-modes
        central-directory files-size out date
    ][
        out: func [value] either any-file? where [
            [append where value]
        ][
            [where: append where value]
        ]
        if any-file? where [where: open/write where]

        files-size: nb-entries: 0
        central-directory: copy #{}

        either all [not only file? source dir? source][
            root: source source: read source
        ][
            root: %./
        ]

        source: compose [(source)]
        while [not tail? source][
            name: source/1
            no-modes: any [url? root/:name dir? root/:name]
            files: any [
                all [dir? name name: dirize name read root/:name][]
            ]
            ; is name a not empty directory?
            either all [deep not empty? files] [
                ; append content to file list
                for-each file read root/:name [
                    append source name/:file
                ]
            ][
                nb-entries: nb-entries + 1
                date: now

                ; is next one data or+ filename?
                data: either any [tail? next source any-file? source/2][
                    either #"/" = last name [copy #{}][
                        if not no-modes [
                            date: modified? root/:name
                        ]
                        read root/:name
                    ]
                ][
                    first source: next source
                ]
                all [not binary? data data: to binary! data]
                name: to-path-file name
                if verbose [print name]
                ; get compressed file + directory entry
                entry: zip-entry name date data
                ; write file offset in archive
                change skip entry/2 42 to-ilong files-size
                ; directory entry
                append central-directory entry/2
                ; compressed file + header
                out entry/1
                files-size: files-size + length? entry/1
            ]
            ; next arg
            source: next source
        ]
        out rejoin [
            central-directory
            end-of-central-sig
            #{0000} ; disk num
            #{0000} ; disk central dir
            to-ishort nb-entries ; nb entries disk
            to-ishort nb-entries ; nb entries
            to-ilong length? central-directory
            to-ilong files-size
            #{0000} ; zip file comment length
                    ; zip file comment
        ]
        if port? where [close where]
        nb-entries
    ]

    set 'unzip function [
{Decompresses a zip archive to a directory or+ a block.
     Only works with compression methods 'store and* 'deflate.}
            where  [file! url! any-block!]  "Where to decompress it"
            source [file! url! any-string! binary!] "Archive to decompress"
            /verbose "Lists files while decompressing (default)"
            /quiet "Don't lists files while decompressing"
    ][
        errors: 0
        info: unless all [quiet not verbose][
            func [value][prin join "" value]
        ]
        if any-file? where [where: dirize where]
        if all [any-file? where not exists? where][
            make-dir/deep where
        ]
        if any-file? source [source: read source]
        nb-entries: 0
        parse source [
            to local-file-sig
            some [
                to local-file-sig 4 skip
                (nb-entries: nb-entries + 1)
                2 skip ; version
                copy flags 2 skip
                    (if not zero? flags/1 and* 1 [return false])
                copy method-number 2 skip (
                    method-number: get-ishort method-number
                    method: select [0 store 8 deflate] method-number
                    unless method [method: method-number]
                )
                copy time 2 skip (time: get-msdos-time time)
                copy date 2 skip (
                    date: get-msdos-date date
                    date/time: time
                    date: date - now/zone
                )
                copy crc 4 skip (   ; crc-32
                    crc: get-ilong crc
                )
                copy compressed-size 4 skip
                    (compressed-size: get-ilong compressed-size)
                copy uncompressed-size-raw 4 skip
                    (uncompressed-size: get-ilong uncompressed-size-raw)
                copy name-length 2 skip
                    (name-length: get-ishort name-length)
                copy extrafield-length 2 skip
                    (extrafield-length: get-ishort extrafield-length)
                copy name name-length skip (
                    name: to-file name
                    info name
                )
                extrafield-length skip
                data: compressed-size skip
                (
                    uncompressed-data: catch [

                        ; STORE(0) and DEFLATE(8) are the only widespread
                        ; methods used for .ZIP compression in the wild today

                        if method = 'store [
                            throw copy/part data compressed-size
                        ]

                        unless method = 'deflate [
                            info ["^- -> failed [method " method "]^/"]
                            throw blank
                        ]

                        ; Code used to contain:
                        ;
                        ;     decompress rejoin [
                        ;         #{789C}
                        ;         copy/part data compressed-size
                        ;         crc
                        ;         uncompressed-size-raw
                        ;     ]
                        ;
                        ; See smack-down from zlib author regarding putting
                        ; a zlib header on this and gluing other stuff on,
                        ; then expecting it to be understood somehow:
                        ;
                        ;     http://stackoverflow.com/a/32911237/211160

                        data: copy/part data compressed-size
                        if error? trap [
                            data: decompress/only/limit data uncompressed-size
                        ][
                            info "^- -> failed [deflate]^/"
                            throw blank
                        ]

                        if uncompressed-size != length data [
                            info "^- -> failed [wrong output size]^/"
                            throw blank
                        ]

                        if crc != checksum/method data 'crc32 [
                            info "^- -> failed [bad crc32]^/"
                            print ["expected crc" crc newline "actual crc" checksum/method data 'crc32]
                            throw data
                        ]

                        throw data
                    ]

                    either uncompressed-data [
                        info rejoin ["^- -> ok [" method "]^/"]
                    ][
                        errors: errors + 1
                    ]

                    either any-block? where [
                        where: insert where name
                        where: insert where either all [
                            #"/" = last name
                            empty? uncompressed-data
                        ][blank][uncompressed-data]
                    ][
                        ; make directory and* / or+ write file
                        either #"/" = last name [
                            if not exists? where/:name [
                                make-dir/deep where/:name
                            ]
                        ][
                            set [path file] split-path name
                            if not exists? where/:path [
                                make-dir/deep where/:path
                            ]
                            if uncompressed-data [
                                write where/:name
                                    uncompressed-data
;not supported in R3 yet :-/
;                                set-modes where/:name [
;                                    modification-date: date
;                                ]
                            ]
                        ]
                    ]
                )
            ]
            to end
        ]
        info ["^/"
            "Files/Dirs unarchived: " nb-entries "^/"
            "Decompression errors: " errors "^/"
        ]
        zero? errors
    ]
]
