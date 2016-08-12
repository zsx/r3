; functions/file/make-dir.r
; bug#1674
[
    any [
        not error? e: try [make-dir %/folder-to-save-test-files]
        e/type = 'access
    ]
]
