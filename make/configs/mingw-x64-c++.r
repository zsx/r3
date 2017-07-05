REBOL []

os-id: 0.3.40
standard: 'c++
toolset: [
    gcc %x86_64-w64-mingw32-g++
    ld %x86_64-w64-mingw32-g++; linking is done via calling g++, not ld
    strip %x86_64-w64-mingw32-strip
]
