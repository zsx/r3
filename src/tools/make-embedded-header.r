REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Generate C string for the embeded headers"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "Shixin Zeng"
    Needs: 2.100.100
]

do %common.r
do %common-parsers.r
do %form-header.r

print "------ Building embedded header file"
args: parse-args system/options/args
output-dir: fix-win32-path to file! any [args/OUTDIR %../]
mkdir/deep output-dir/core

inp: read fix-win32-path to file! output-dir/include/sys-core.i
replace/all inp "// #define" "#define"
replace/all inp "// #undef" "#undef"

;write %/tmp/sys-core.i inp
out: make binary! 2048

append out form-header/gen "Embedded sys-core.h" %e-embedded-header.c %make-embedded-header.r

append out rejoin [
    {#include "sys-core.h"^/}
    "extern const REBYTE core_header_source[];^/"
    "const REBYTE core_header_source[] = {^/"
    binary-to-c join inp #{00}
    "};^/"
]

write output-dir/core/e-embedded-header.c  out
