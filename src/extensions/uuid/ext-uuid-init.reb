REBOL [
    Title: "UUID Extension"
    name: 'UUID
    type: 'Extension
    version: 1.0.0
    license: {Apache 2.0}
]

uuid: import 'uuid
append uuid reduce [
    quote to-string: function [
        "Convert the UUID to the string form ({8-4-4-4-12})"
        uuid [binary!]
    ][
        delimit map-each w reduce [
            copy/part uuid 4
            copy/part (skip uuid 4) 2
            copy/part (skip uuid 6) 2
            copy/part (skip uuid 8) 2
            copy/part (skip uuid 10) 6
        ][
            enbase/base w 16
        ]
        #"-"
    ]
]
