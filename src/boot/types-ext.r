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

trash       *
unset       *
none        *
handle      *

logic       5
integer     *
decimal     *
percent     *

char        11
pair        *
tuple       *
time        *
date        *

word        17
set-word    *
get-word    *
lit-word    *
refinement  *
issue       *

string      25
file        *
email       *
url         *
tag         *

block       33
group       *
path        *
set-path    *
get-path    *
lit-path    *

binary      41
bitset      *
vector      *
image       *

gob         48

object      49
module      *

