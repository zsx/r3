REBOL []

os-id: 0.3.40
target: 'visual-studio

toolset: [
    cl %cl.exe
    link %link.exe
]

extensions: [
    ;* UUID _
    - FFI _
]

with-ffi: no
rebol-tool: %r3-make.exe

