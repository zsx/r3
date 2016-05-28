REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Extension datatypes"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        Used to build C enums and definitions for extensions.
    }
]

0           *
blank       *
handle      *

logic       4
integer     *
decimal     *
percent     *

char        10
pair        *
tuple       *
time        *
date        *

word        16
set-word    *
get-word    *
lit-word    *
refinement  *
issue       *

string      24
file        *
email       *
url         *
tag         *

block       32
group       *
path        *
set-path    *
get-path    *
lit-path    *

binary      40
bitset      *
vector      *
image       *

gob         47

object      48
module      *

