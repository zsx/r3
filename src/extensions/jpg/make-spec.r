REBOL []

name: 'JPG
source: %jpg/ext-jpg.c
modules: [
    [
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
]
