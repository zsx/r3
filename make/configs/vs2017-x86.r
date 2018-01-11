REBOL []

os-id: 0.3.1
target: 'visual-studio

toolset: [
    cl %cl.exe
    link %link.exe
]

extensions: [
    ;* UUID _
]

with-ffi: [
    definitions: ["FFI_BUILDING"] ;the prebuilt library is static
    includes: [%../external/ffi-prebuilt/msvc/lib32/libffi-3.2.1/include]
    searches: [%../external/ffi-prebuilt/msvc/lib32/Release] ;Change to .../Debug for debugging build
    libraries: reduce [make rebmake/ext-static-class [output: %libffi.lib]]
]
rebol-tool: %r3-make.exe

