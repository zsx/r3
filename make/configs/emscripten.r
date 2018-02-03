REBOL []

os-id: 0.16.01

gcc-path: 

toolset: [
    gcc %emcc
    ld %emcc
]

optimize: "z"

ldflags: reduce [
	unspaced["-O" optimize]
	{-s 'EXTRA_EXPORTED_RUNTIME_METHODS=["ccall", "cwrap"]'}
	{--post-js post.js}
]

extensions: [
    - crypt _
    - process _
    - view _
    - png _
    - gif _
    - jpg _
    - bmp _
    - locale _
    - uuid _
    - odbc _
    - ffi _
    - debugger _
]
