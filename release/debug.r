320x320 240x194 3.3333332538604736x3.3333332538604736 800x1280 {make object! [
    style: 'area
    facets: make object! [
        border-color: none
        border-size: [0x0 0x0]
        bg-color: 240.240.240
        margin: [0x0 0x0]
        padding: [0x0 0x0]
        spacing: 0x0
        init-size: 416x200
        min-size: 16x50
        max-size: 3.4028234663852886e38x3.4028234663852886e38
        align: 'left
        valign: 'top
        resizes: true
        box-model: 'tight
        gob: make gob! [offset: 5x5 size: 230x153.3333282470703 alpha: 255 draw: [translate 0x0 clip 0x0 230x183.3333282470703 anti-alias false pen false fill-pen 240.240.240 box 0x0 230x153.3333282470703 0 fill-pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true]]
        gob-size: 230x153.3333282470703
        space: [0x0 0x0]
        margin-box: [
            top-left: -0x-0
            top-right: 230x-0
            bottom-left: -0x153.3333282470703
            bottom-right: 230x153.3333282470703
            center: 115x76.66666412353516
        ]
        border-box: [
            top-left: 0x0
            top-right: 230x0
            bottom-left: 0x153.3333282470703
            bottom-right: 230x153.3333282470703
            center: 115x76.66666412353516
        ]
        padding-box: [
            top-left: 0x0
            top-right: 230x0
            bottom-left: 0x153.3333282470703
            bottom-right: 230x153.3333282470703
            center: 115x76.66666412353516
        ]
        viewport-box: [
            top-left: 0x0
            top-right: 230x0
            bottom-left: 0x153.3333282470703
            bottom-right: 230x153.3333282470703
            center: 115x76.66666412353516
        ]
        draw-mode: 'plain
        area-fill: none
        material: none
        min-hint: 'auto
        max-hint: 'auto
        init-hint: 'auto
        layout-mode: 'horizontal
        dividers: none
        mouse-pointers: none
        hints: none
        old-hints: none
        break-after: 2
        names: true
        pane-align: 'left
        pane-valign: 'top
        row-max: 'max
        column-max: 'max
        row-min: 'max
        column-min: 'max
        row-init: 'max
        column-init: 'max
        intern: make object! [
            update?: false
            init-pane: 416x200
            heights: [153.3333296775818]
            init-heights: [200.0]
            min-heights: [50.0]
            max-heights: [3.4028234663852886e38]
            widths: [213.3333282470703 16.666666269302368]
            init-widths: [400.0 16.0]
            min-widths: [0 16.0]
            max-widths: [3.4028234663852886e38 16.0]
            row-minification-index: [1]
            row-magnification-index: [1]
            column-minification-index: [2 1]
            column-magnification-index: [2 1]
            row-init-ratio: 0.7666666412353516
            column-init-ratio: 0.5333333343267441
        ]
    ]
    state: make object! [
        mode: 'up
        over: false
        value: none
    ]
    gob: make gob! [offset: 5x5 size: 230x153.3333282470703 alpha: 255 draw: [translate 0x0 clip 0x0 230x183.3333282470703 anti-alias false pen false fill-pen 240.240.240 box 0x0 230x153.3333282470703 0 fill-pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true]]
    options: make object! [
    ]
    tags: make map! [
        layout true
    ]
    intern: make object! [
        make-dividers: make function! [[
            face [object!]
            dividers [block!]
            /local out c r f a i axis sizes d
        ][
            out: make block! length? dividers
            c: get-facet face 'break-after
            r: to integer! (f: length? faces? face) / (any [all [c > 0 c] 1]) + 0.5
            a: pick [[x y] [y x]] face/facets/layout-mode = 'vertical
            foreach [id specs] dividers [
                unless any [
                    id = 0
                    id = f
                ] [
                    i: id
                    if c > 0 [
                        i: id // c
                    ]
                    either i = 0 [
                        axis: a/1
                        i: id / c
                    ] [
                        axis: a/2
                    ]
                    sizes: pick [[widths init-widths column-init] [heights init-heights row-init]] axis = 'x
                    unless any [
                        i = 0
                        all [d: select out i d/axis = axis]
                    ] [
                        append/only out append compose/only [id (i) size 0 axis (axis) sizes (sizes)] specs
                    ]
                ]
            ]
            if empty? out [return none]
            unless get-facet face 'mouse-pointers [
                set-facet face 'mouse-pointers reduce ['x system-cursors/size-we 'y system-cursors/size-ns]
            ]
            out
        ]]
    ]
    names: make object! [
        text-box: make object! [
            style: 'text-box
            facets: make object! [
                border-color: none
                border-size: [0x0 0x0]
                bg-color: 240.240.240
                margin: [3x3 3x3]
                padding: [3x3 3x3]
                spacing: 0x0
                init-size: 400x200
                min-size: 0x0
                max-size: 3.4028234663852886e38x3.4028234663852886e38
                align: 'left
                valign: 'top
                resizes: true
                box-model: 'tight
                gob: make gob! [offset: 0x0 size: 213.3333282470703x153.3333282470703 alpha: 255 draw: [translate 6x6 clip -6x-6 230x153.3333282470703 anti-alias false pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0 grad-pen linear normal -2x-2 -2x10 90.0 1x1 [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0] box -3x-3 203.3333282470703x143.3333282470703 0]]
                gob-size: 213.3333282470703x153.3333282470703
                space: [6x6 6x6]
                margin-box: [
                    top-left: -6x-6
                    top-right: 207.3333282470703x-6
                    bottom-left: -6x147.3333282470703
                    bottom-right: 207.3333282470703x147.3333282470703
                    center: 100.66666412353516x70.66666412353516
                ]
                border-box: [
                    top-left: -3x-3
                    top-right: 204.3333282470703x-3
                    bottom-left: -3x144.3333282470703
                    bottom-right: 204.3333282470703x144.3333282470703
                    center: 100.66666412353516x70.66666412353516
                ]
                padding-box: [
                    top-left: -3x-3
                    top-right: 204.3333282470703x-3
                    bottom-left: -3x144.3333282470703
                    bottom-right: 204.3333282470703x144.3333282470703
                    center: 100.66666412353516x70.66666412353516
                ]
                viewport-box: [
                    top-left: 0x0
                    top-right: 201.3333282470703x0
                    bottom-left: 0x141.3333282470703
                    bottom-right: 201.3333282470703x141.3333282470703
                    center: 100.66666412353516x70.66666412353516
                ]
                text-edit: "qwe"
                lines: true
                text-style: none
                hide-input: false
                detab: none
                draw-mode: 'normal
                area-fill: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                material: 'field-groove
                focus-color: 255.255.255.0
                materials: make object! [
                    up: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                    down: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                    over: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                ]
            ]
            state: make object! [
                mode: 'up
                over: false
                value: "qwe"
                cursor: ""
                mark-head: none
                mark-tail: none
                caret: make object! [
                    caret: [[""] ""]
                    highlight-start: [[""] ""]
                    highlight-end: [[""] ""]
                ]
                xpos: none
                validity: none
            ]
            gob: make gob! [offset: 0x0 size: 213.3333282470703x153.3333282470703 alpha: 255 draw: [translate 6x6 clip -6x-6 230x153.3333282470703 anti-alias false pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0 grad-pen linear normal -2x-2 -2x10 90.0 1x1 [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0] box -3x-3 203.3333282470703x143.3333282470703 0]]
            options: make object! [
                init-size: 400x200
                bg-color: 240.240.240
            ]
            tags: make map! [
                edit true
                tab true
            ]
            intern: none
            name: 'text-box
            actors: [on-key make function! [[face arg
                    /local
                ][
                    do-actor parent-face? face 'on-key arg
                ]]]
            attached: [make object! [
                    style: 'scroller
                    facets: make object! [
                        border-color: 0.0.0.128
                        border-size: [0x0 0x0]
                        bg-color: 200.233.245
                        margin: [0x0 0x0]
                        padding: [0x0 0x0]
                        spacing: 0x0
                        init-size: 16x16
                        min-size: 16x50
                        max-size: 16x3.4028234663852886e38
                        align: 'left
                        valign: 'top
                        resizes: true
                        box-model: 'tight
                        gob: make gob! [offset: 213.3333282470703x0 size: 16x153.3333282470703 alpha: 255 draw: [translate 0x0 clip -0x0 16.666671752929688x153.3333282470703 anti-alias false pen false pen 0.0.0.128 line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0.128 line-width 1.0 variable grad-pen linear normal 1x1 0x16 0.0 1x1 [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0] box 1x1 15x152.3333282470703 6.0 grad-pen linear normal 1x1 0x16 0.0 1x1 [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8] box 2x16 14x137.3333282470703 6.0 pen false transform 0.0 0x0 0.6000000238418579x0.6000000238418579 8x8 pen 0.0.0 line-cap rounded fill-pen 0.0.0 polygon [-6x5 0x-5 6x5] reset-matrix transform 180.0 0x0 0.6000000238418579x0.6000000238418579 8x144.3333282470703 polygon [-6x5 0x-5 6x5]]]
                        gob-size: 16x153.3333282470703
                        space: [0x0 0x0]
                        margin-box: [
                            top-left: -0x-0
                            top-right: 16x-0
                            bottom-left: -0x153.3333282470703
                            bottom-right: 16x153.3333282470703
                            center: 8x76.66666412353516
                        ]
                        border-box: [
                            top-left: 0x0
                            top-right: 16x0
                            bottom-left: 0x153.3333282470703
                            bottom-right: 16x153.3333282470703
                            center: 8x76.66666412353516
                        ]
                        padding-box: [
                            top-left: 0x0
                            top-right: 16x0
                            bottom-left: 0x153.3333282470703
                            bottom-right: 16x153.3333282470703
                            center: 8x76.66666412353516
                        ]
                        viewport-box: [
                            top-left: 0x0
                            top-right: 16x0
                            bottom-left: 0x153.3333282470703
                            bottom-right: 16x153.3333282470703
                            center: 8x76.66666412353516
                        ]
                        init-length: none
                        orientation: none
                        btn-size: 16x16
                        length-limit: 50
                        all-over: true
                        relay: true
                        material: 'scroller
                        arrow-color: 0.0.0
                        knob-xy: 2x16
                        knob-size: 12x121.33332824707031
                        knob-base: 0x0
                        btn-xy: 1x137.3333282470703
                        angles: [0 180]
                        axis: 'y
                        set-fields: make map! [
                            value [
                                all [
                                    number? arg/2
                                    either percent? arg/2 [
                                        true
                                    ] [
                                        arg/2: to percent! arg/2 / pick face/gob/size face/facets/axis
                                    ]
                                    face/state/value <> val: limit arg/2 000% 100%
                                    face/state/value: val
                                    dirty?: true
                                ]
                            ]
                            delta [
                                if number? arg/2 [
                                    unless percent? arg/2 [arg/2: to percent! arg/2 / pick face/gob/size face/facets/axis]
                                    face/state/delta: limit arg/2 000% 100%
                                    dirty?: true
                                ]
                            ]
                        ]
                        get-fields: make map! [
                            value [face/state/value]
                            delta [face/state/delta]
                        ]
                        materials: make object! [
                            up: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                            down: [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0]
                            over: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                        ]
                        area-fill: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                        target: make object! [...]
                    ]
                    state: make object! [
                        mode: 'up
                        over: false
                        value: 000%
                        delta: 100%
                    ]
                    gob: make gob! [offset: 213.3333282470703x0 size: 16x153.3333282470703 alpha: 255 draw: [translate 0x0 clip -0x0 16.666671752929688x153.3333282470703 anti-alias false pen false pen 0.0.0.128 line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0.128 line-width 1.0 variable grad-pen linear normal 1x1 0x16 0.0 1x1 [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0] box 1x1 15x152.3333282470703 6.0 grad-pen linear normal 1x1 0x16 0.0 1x1 [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8] box 2x16 14x137.3333282470703 6.0 pen false transform 0.0 0x0 0.6000000238418579x0.6000000238418579 8x8 pen 0.0.0 line-cap rounded fill-pen 0.0.0 polygon [-6x5 0x-5 6x5] reset-matrix transform 180.0 0x0 0.6000000238418579x0.6000000238418579 8x144.3333282470703 polygon [-6x5 0x-5 6x5]]]
                    options: make object! [
                    ]
                    tags: make map! [
                        action true
                        part true
                    ]
                    intern: none
                    name: 'scroller
                    targets: [make object! [...]]
                ]]
            draw-text: [
                pen off
                fill-pen 0.0.0
                anti-alias off
                text -1.9974574854132204e18x0 aliased [font make object! [
                        name: "/system/fonts/DroidSans.ttf"
                        style: none
                        size: 12
                        color: 0.0.0
                        offset: 0x0
                        space: 0x0
                        shadow: none
                    ] para make object! [
                        origin: 0x0
                        margin: 0x0
                        indent: 0x0
                        tabs: 40
                        wrap?: true
                        scroll: 0x0
                        align: 'left
                        valign: 'top
                    ] anti-alias off
                    caret make object! [
                        caret: [[""] ""]
                        highlight-start: [[""] ""]
                        highlight-end: [[""] ""]
                    ] "qwe"
                ]
            ]
            attached-face: make object! [
                style: 'scroller
                facets: make object! [
                    border-color: 0.0.0.128
                    border-size: [0x0 0x0]
                    bg-color: 200.233.245
                    margin: [0x0 0x0]
                    padding: [0x0 0x0]
                    spacing: 0x0
                    init-size: 16x16
                    min-size: 16x50
                    max-size: 16x3.4028234663852886e38
                    align: 'left
                    valign: 'top
                    resizes: true
                    box-model: 'tight
                    gob: make gob! [offset: 213.3333282470703x0 size: 16x153.3333282470703 alpha: 255 draw: [translate 0x0 clip -0x0 16.666671752929688x153.3333282470703 anti-alias false pen false pen 0.0.0.128 line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0.128 line-width 1.0 variable grad-pen linear normal 1x1 0x16 0.0 1x1 [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0] box 1x1 15x152.3333282470703 6.0 grad-pen linear normal 1x1 0x16 0.0 1x1 [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8] box 2x16 14x137.3333282470703 6.0 pen false transform 0.0 0x0 0.6000000238418579x0.6000000238418579 8x8 pen 0.0.0 line-cap rounded fill-pen 0.0.0 polygon [-6x5 0x-5 6x5] reset-matrix transform 180.0 0x0 0.6000000238418579x0.6000000238418579 8x144.3333282470703 polygon [-6x5 0x-5 6x5]]]
                    gob-size: 16x153.3333282470703
                    space: [0x0 0x0]
                    margin-box: [
                        top-left: -0x-0
                        top-right: 16x-0
                        bottom-left: -0x153.3333282470703
                        bottom-right: 16x153.3333282470703
                        center: 8x76.66666412353516
                    ]
                    border-box: [
                        top-left: 0x0
                        top-right: 16x0
                        bottom-left: 0x153.3333282470703
                        bottom-right: 16x153.3333282470703
                        center: 8x76.66666412353516
                    ]
                    padding-box: [
                        top-left: 0x0
                        top-right: 16x0
                        bottom-left: 0x153.3333282470703
                        bottom-right: 16x153.3333282470703
                        center: 8x76.66666412353516
                    ]
                    viewport-box: [
                        top-left: 0x0
                        top-right: 16x0
                        bottom-left: 0x153.3333282470703
                        bottom-right: 16x153.3333282470703
                        center: 8x76.66666412353516
                    ]
                    init-length: none
                    orientation: none
                    btn-size: 16x16
                    length-limit: 50
                    all-over: true
                    relay: true
                    material: 'scroller
                    arrow-color: 0.0.0
                    knob-xy: 2x16
                    knob-size: 12x121.33332824707031
                    knob-base: 0x0
                    btn-xy: 1x137.3333282470703
                    angles: [0 180]
                    axis: 'y
                    set-fields: make map! [
                        value [
                            all [
                                number? arg/2
                                either percent? arg/2 [
                                    true
                                ] [
                                    arg/2: to percent! arg/2 / pick face/gob/size face/facets/axis
                                ]
                                face/state/value <> val: limit arg/2 000% 100%
                                face/state/value: val
                                dirty?: true
                            ]
                        ]
                        delta [
                            if number? arg/2 [
                                unless percent? arg/2 [arg/2: to percent! arg/2 / pick face/gob/size face/facets/axis]
                                face/state/delta: limit arg/2 000% 100%
                                dirty?: true
                            ]
                        ]
                    ]
                    get-fields: make map! [
                        value [face/state/value]
                        delta [face/state/delta]
                    ]
                    materials: make object! [
                        up: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                        down: [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0]
                        over: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                    ]
                    area-fill: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                    target: make object! [...]
                ]
                state: make object! [
                    mode: 'up
                    over: false
                    value: 000%
                    delta: 100%
                ]
                gob: make gob! [offset: 213.3333282470703x0 size: 16x153.3333282470703 alpha: 255 draw: [translate 0x0 clip -0x0 16.666671752929688x153.3333282470703 anti-alias false pen false pen 0.0.0.128 line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0.128 line-width 1.0 variable grad-pen linear normal 1x1 0x16 0.0 1x1 [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0] box 1x1 15x152.3333282470703 6.0 grad-pen linear normal 1x1 0x16 0.0 1x1 [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8] box 2x16 14x137.3333282470703 6.0 pen false transform 0.0 0x0 0.6000000238418579x0.6000000238418579 8x8 pen 0.0.0 line-cap rounded fill-pen 0.0.0 polygon [-6x5 0x-5 6x5] reset-matrix transform 180.0 0x0 0.6000000238418579x0.6000000238418579 8x144.3333282470703 polygon [-6x5 0x-5 6x5]]]
                options: make object! [
                ]
                tags: make map! [
                    action true
                    part true
                ]
                intern: none
                name: 'scroller
                targets: [make object! [...]]
            ]
        ]
        scroller: make object! [
            style: 'scroller
            facets: make object! [
                border-color: 0.0.0.128
                border-size: [0x0 0x0]
                bg-color: 200.233.245
                margin: [0x0 0x0]
                padding: [0x0 0x0]
                spacing: 0x0
                init-size: 16x16
                min-size: 16x50
                max-size: 16x3.4028234663852886e38
                align: 'left
                valign: 'top
                resizes: true
                box-model: 'tight
                gob: make gob! [offset: 213.3333282470703x0 size: 16x153.3333282470703 alpha: 255 draw: [translate 0x0 clip -0x0 16.666671752929688x153.3333282470703 anti-alias false pen false pen 0.0.0.128 line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0.128 line-width 1.0 variable grad-pen linear normal 1x1 0x16 0.0 1x1 [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0] box 1x1 15x152.3333282470703 6.0 grad-pen linear normal 1x1 0x16 0.0 1x1 [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8] box 2x16 14x137.3333282470703 6.0 pen false transform 0.0 0x0 0.6000000238418579x0.6000000238418579 8x8 pen 0.0.0 line-cap rounded fill-pen 0.0.0 polygon [-6x5 0x-5 6x5] reset-matrix transform 180.0 0x0 0.6000000238418579x0.6000000238418579 8x144.3333282470703 polygon [-6x5 0x-5 6x5]]]
                gob-size: 16x153.3333282470703
                space: [0x0 0x0]
                margin-box: [
                    top-left: -0x-0
                    top-right: 16x-0
                    bottom-left: -0x153.3333282470703
                    bottom-right: 16x153.3333282470703
                    center: 8x76.66666412353516
                ]
                border-box: [
                    top-left: 0x0
                    top-right: 16x0
                    bottom-left: 0x153.3333282470703
                    bottom-right: 16x153.3333282470703
                    center: 8x76.66666412353516
                ]
                padding-box: [
                    top-left: 0x0
                    top-right: 16x0
                    bottom-left: 0x153.3333282470703
                    bottom-right: 16x153.3333282470703
                    center: 8x76.66666412353516
                ]
                viewport-box: [
                    top-left: 0x0
                    top-right: 16x0
                    bottom-left: 0x153.3333282470703
                    bottom-right: 16x153.3333282470703
                    center: 8x76.66666412353516
                ]
                init-length: none
                orientation: none
                btn-size: 16x16
                length-limit: 50
                all-over: true
                relay: true
                material: 'scroller
                arrow-color: 0.0.0
                knob-xy: 2x16
                knob-size: 12x121.33332824707031
                knob-base: 0x0
                btn-xy: 1x137.3333282470703
                angles: [0 180]
                axis: 'y
                set-fields: make map! [
                    value [
                        all [
                            number? arg/2
                            either percent? arg/2 [
                                true
                            ] [
                                arg/2: to percent! arg/2 / pick face/gob/size face/facets/axis
                            ]
                            face/state/value <> val: limit arg/2 000% 100%
                            face/state/value: val
                            dirty?: true
                        ]
                    ]
                    delta [
                        if number? arg/2 [
                            unless percent? arg/2 [arg/2: to percent! arg/2 / pick face/gob/size face/facets/axis]
                            face/state/delta: limit arg/2 000% 100%
                            dirty?: true
                        ]
                    ]
                ]
                get-fields: make map! [
                    value [face/state/value]
                    delta [face/state/delta]
                ]
                materials: make object! [
                    up: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                    down: [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0]
                    over: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                ]
                area-fill: [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8]
                target: make object! [
                    style: 'text-box
                    facets: make object! [
                        border-color: none
                        border-size: [0x0 0x0]
                        bg-color: 240.240.240
                        margin: [3x3 3x3]
                        padding: [3x3 3x3]
                        spacing: 0x0
                        init-size: 400x200
                        min-size: 0x0
                        max-size: 3.4028234663852886e38x3.4028234663852886e38
                        align: 'left
                        valign: 'top
                        resizes: true
                        box-model: 'tight
                        gob: make gob! [offset: 0x0 size: 213.3333282470703x153.3333282470703 alpha: 255 draw: [translate 6x6 clip -6x-6 230x153.3333282470703 anti-alias false pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0 grad-pen linear normal -2x-2 -2x10 90.0 1x1 [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0] box -3x-3 203.3333282470703x143.3333282470703 0]]
                        gob-size: 213.3333282470703x153.3333282470703
                        space: [6x6 6x6]
                        margin-box: [
                            top-left: -6x-6
                            top-right: 207.3333282470703x-6
                            bottom-left: -6x147.3333282470703
                            bottom-right: 207.3333282470703x147.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        border-box: [
                            top-left: -3x-3
                            top-right: 204.3333282470703x-3
                            bottom-left: -3x144.3333282470703
                            bottom-right: 204.3333282470703x144.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        padding-box: [
                            top-left: -3x-3
                            top-right: 204.3333282470703x-3
                            bottom-left: -3x144.3333282470703
                            bottom-right: 204.3333282470703x144.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        viewport-box: [
                            top-left: 0x0
                            top-right: 201.3333282470703x0
                            bottom-left: 0x141.3333282470703
                            bottom-right: 201.3333282470703x141.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        text-edit: "qwe"
                        lines: true
                        text-style: none
                        hide-input: false
                        detab: none
                        draw-mode: 'normal
                        area-fill: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                        material: 'field-groove
                        focus-color: 255.255.255.0
                        materials: make object! [
                            up: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                            down: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                            over: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                        ]
                    ]
                    state: make object! [
                        mode: 'up
                        over: false
                        value: "qwe"
                        cursor: ""
                        mark-head: none
                        mark-tail: none
                        caret: make object! [
                            caret: [[""] ""]
                            highlight-start: [[""] ""]
                            highlight-end: [[""] ""]
                        ]
                        xpos: none
                        validity: none
                    ]
                    gob: make gob! [offset: 0x0 size: 213.3333282470703x153.3333282470703 alpha: 255 draw: [translate 6x6 clip -6x-6 230x153.3333282470703 anti-alias false pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0 grad-pen linear normal -2x-2 -2x10 90.0 1x1 [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0] box -3x-3 203.3333282470703x143.3333282470703 0]]
                    options: make object! [
                        init-size: 400x200
                        bg-color: 240.240.240
                    ]
                    tags: make map! [
                        edit true
                        tab true
                    ]
                    intern: none
                    name: 'text-box
                    actors: [on-key make function! [[face arg
                            /local
                        ][
                            do-actor parent-face? face 'on-key arg
                        ]]]
                    attached: [make object! [...]]
                    draw-text: [
                        pen off
                        fill-pen 0.0.0
                        anti-alias off
                        text -1.9974574854132204e18x0 aliased [font make object! [
                                name: "/system/fonts/DroidSans.ttf"
                                style: none
                                size: 12
                                color: 0.0.0
                                offset: 0x0
                                space: 0x0
                                shadow: none
                            ] para make object! [
                                origin: 0x0
                                margin: 0x0
                                indent: 0x0
                                tabs: 40
                                wrap?: true
                                scroll: 0x0
                                align: 'left
                                valign: 'top
                            ] anti-alias off
                            caret make object! [
                                caret: [[""] ""]
                                highlight-start: [[""] ""]
                                highlight-end: [[""] ""]
                            ] "qwe"
                        ]
                    ]
                    attached-face: make object! [...]
                ]
            ]
            state: make object! [
                mode: 'up
                over: false
                value: 000%
                delta: 100%
            ]
            gob: make gob! [offset: 213.3333282470703x0 size: 16x153.3333282470703 alpha: 255 draw: [translate 0x0 clip -0x0 16.666671752929688x153.3333282470703 anti-alias false pen false pen 0.0.0.128 line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0.128 line-width 1.0 variable grad-pen linear normal 1x1 0x16 0.0 1x1 [120.139.147.255 0.0 255.255.255.255 0.65 155.178.186.255 1.0] box 1x1 15x152.3333282470703 6.0 grad-pen linear normal 1x1 0x16 0.0 1x1 [155.178.186.255 0.0 80.93.98.255 0.05 130.151.159.255 0.2 205.229.238.255 0.49 175.198.207.255 0.5 155.178.186.255 0.55 100.116.122.255 0.8] box 2x16 14x137.3333282470703 6.0 pen false transform 0.0 0x0 0.6000000238418579x0.6000000238418579 8x8 pen 0.0.0 line-cap rounded fill-pen 0.0.0 polygon [-6x5 0x-5 6x5] reset-matrix transform 180.0 0x0 0.6000000238418579x0.6000000238418579 8x144.3333282470703 polygon [-6x5 0x-5 6x5]]]
            options: make object! [
            ]
            tags: make map! [
                action true
                part true
            ]
            intern: none
            name: 'scroller
            targets: [make object! [
                    style: 'text-box
                    facets: make object! [
                        border-color: none
                        border-size: [0x0 0x0]
                        bg-color: 240.240.240
                        margin: [3x3 3x3]
                        padding: [3x3 3x3]
                        spacing: 0x0
                        init-size: 400x200
                        min-size: 0x0
                        max-size: 3.4028234663852886e38x3.4028234663852886e38
                        align: 'left
                        valign: 'top
                        resizes: true
                        box-model: 'tight
                        gob: make gob! [offset: 0x0 size: 213.3333282470703x153.3333282470703 alpha: 255 draw: [translate 6x6 clip -6x-6 230x153.3333282470703 anti-alias false pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0 grad-pen linear normal -2x-2 -2x10 90.0 1x1 [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0] box -3x-3 203.3333282470703x143.3333282470703 0]]
                        gob-size: 213.3333282470703x153.3333282470703
                        space: [6x6 6x6]
                        margin-box: [
                            top-left: -6x-6
                            top-right: 207.3333282470703x-6
                            bottom-left: -6x147.3333282470703
                            bottom-right: 207.3333282470703x147.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        border-box: [
                            top-left: -3x-3
                            top-right: 204.3333282470703x-3
                            bottom-left: -3x144.3333282470703
                            bottom-right: 204.3333282470703x144.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        padding-box: [
                            top-left: -3x-3
                            top-right: 204.3333282470703x-3
                            bottom-left: -3x144.3333282470703
                            bottom-right: 204.3333282470703x144.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        viewport-box: [
                            top-left: 0x0
                            top-right: 201.3333282470703x0
                            bottom-left: 0x141.3333282470703
                            bottom-right: 201.3333282470703x141.3333282470703
                            center: 100.66666412353516x70.66666412353516
                        ]
                        text-edit: "qwe"
                        lines: true
                        text-style: none
                        hide-input: false
                        detab: none
                        draw-mode: 'normal
                        area-fill: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                        material: 'field-groove
                        focus-color: 255.255.255.0
                        materials: make object! [
                            up: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                            down: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                            over: [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0]
                        ]
                    ]
                    state: make object! [
                        mode: 'up
                        over: false
                        value: "qwe"
                        cursor: ""
                        mark-head: none
                        mark-tail: none
                        caret: make object! [
                            caret: [[""] ""]
                            highlight-start: [[""] ""]
                            highlight-end: [[""] ""]
                        ]
                        xpos: none
                        validity: none
                    ]
                    gob: make gob! [offset: 0x0 size: 213.3333282470703x153.3333282470703 alpha: 255 draw: [translate 6x6 clip -6x-6 230x153.3333282470703 anti-alias false pen false line-width 1.0 variable pen 255.255.255 fill-pen false anti-alias true pen 0.0.0 grad-pen linear normal -2x-2 -2x10 90.0 1x1 [0.0.0.255 0.0 120.120.120.255 0.1 237.237.237.255 0.4 240.240.240.255 1.0] box -3x-3 203.3333282470703x143.3333282470703 0]]
                    options: make object! [
                        init-size: 400x200
                        bg-color: 240.240.240
                    ]
                    tags: make map! [
                        edit true
                        tab true
                    ]
                    intern: none
                    name: 'text-box
                    actors: [on-key make function! [[face arg
                            /local
                        ][
                            do-actor parent-face? face 'on-key arg
                        ]]]
                    attached: [make object! [...]]
                    draw-text: [
                        pen off
                        fill-pen 0.0.0
                        anti-alias off
                        text -1.9974574854132204e18x0 aliased [font make object! [
                                name: "/system/fonts/DroidSans.ttf"
                                style: none
                                size: 12
                                color: 0.0.0
                                offset: 0x0
                                space: 0x0
                                shadow: none
                            ] para make object! [
                                origin: 0x0
                                margin: 0x0
                                indent: 0x0
                                tabs: 40
                                wrap?: true
                                scroll: 0x0
                                align: 'left
                                valign: 'top
                            ] anti-alias off
                            caret make object! [
                                caret: [[""] ""]
                                highlight-start: [[""] ""]
                                highlight-end: [[""] ""]
                            ] "qwe"
                        ]
                    ]
                    attached-face: make object! [...]
                ]]
        ]
    ]
    name: 'a
]}
