REBOL []

msvcrt: make library! %msvcrt.dll
getdrives: make-routine msvcrt "_getdrives" compose/deep [
    return: [uint32]
]

maps: getdrives
i: 0
while [i < 26] [
    unless zero? maps and (shift 1 i) [
        print unspaced [to char! (to integer! #"A") + i ":"]
    ]
    i: ++ 1
]
close msvcrt
