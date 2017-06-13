REBOL [
    Title: "CALL Extension"
    name: 'Call
    type: 'Extension
    version: 1.0.0
    license: {Apache 2.0}
]

; CALL is a native built by the C code, BROWSE depends on using that, as well
; as some potentially OS-specific detection on how to launch URLs (e.g. looks
; at registry keys on Windows.

browse*: procedure [
    "Open web browser to a URL or local file."

    location [url! file! blank!]
][
    if blank? location [leave]

    ; Note that GET-OS-BROWSERS uses the Windows registry convention of having
    ; %1 be what needs to be substituted.  This may not be ideal, it was just
    ; easy to do rather than have to add processing on the C side.  Review.
    ;
    for-each template get-os-browsers [
        command: replace/all (copy template) "%1" either file? location [to-local-file location][location]
        trap/with [
            call/shell command ; don't use /WAIT
            leave
        ][
            ;-- Just keep trying
        ]
    ]
    fail "Could not open web browser"
]

hijack 'browse :browse*