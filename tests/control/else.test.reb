[
    success: <bad>
    if 1 > 2 [success: false] else [success: true]
    success
][
    success: <bad>
    if 1 < 2 [success: true] else [success: false]
    success
][
    success: <bad>
    unless 1 > 2 [success: true] else [success: false]
    success
][
    success: <bad>
    unless 1 < 2 [success: false] else [success: true]
    success
][
    success: <bad>
    if true does [success: true]
    success
][
    success: true
    if false does [success: false]
    success
]

[
    x: y: _
    case [
       1 > 2 [x: false] else [x: true]
       1 < 2 [y: true] else [y: false]
    ]
    and? x y
]
