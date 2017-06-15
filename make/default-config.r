REBOL []
os-id: _

; possible values (words):
; Execution: Build the target directly without generating a Makefile
; makefile: Generate a makefile for GNU make
; nmake: Generate an NMake file for CL
target: 'execution

extensions: [
    ;[+|-|*] ext [modules]
    ;- uuid _
    ;* png _ ;[Lodepng]
]

; use ./r3-make
rebol-tool: _ ;%r3-make.exe

; possible combination:
; [gcc _ ld _]
; [cl _ link _]
toolset: [
    ;name executable-file-path (_ being default)
    gcc _
    ld _
]

;one of 'no', 'assert', 'symbols' or 'sanitize'
debug: no

; one of 'no', 1, 2 or 4
optimize: 2
standard: 'c ;one of: 'c, 'gnu89, 'gnu99, 'c99, 'c11, 'c++, 'c++98, 'c++0x, 'c++11, 'c++14 or 'c++17
rigorous: no

static: no
pkg-config: get-env "PKGCONFIG" ;path to pkg-config, or default
with-ffi: 'dynamic

with-tcc: no

git-commit: _

includes: _
definitions: _
cflags: _
libraries: _
ldflags: _
