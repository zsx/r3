
REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Generate extention native header files"
    File: %make-boot-modules.r ;-- used by EMIT-HEADER to indicate emitting script
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Author: "Shixin Zeng <szeng@atronixengineering.com>"
    Needs: 2.100.100
]

do %common.r
do %common-emitter.r
do %form-header.r

r3: system/version > 2.100.0

args: parse-args system/options/args
output-dir: fix-win32-path to file! any [args/OUTDIR %../]
mkdir/deep output-dir/include

modules: either any-string? args/MODULES [split args/MODULES #","][[]]

emit-header "Boot Modules" output-dir/include/tmp-boot-modules.h
remove-each mod modules [empty? mod] ;SPLIT in r3-a111 gives an empty "" at the end

for-each mod modules [
    emit-line ["MODULE_INIT(" mod ");"]
    emit-line ["MODULE_QUIT(" mod ");"]
]

emit-line []
emit-line ["#define LOAD_BOOT_MODULES do {\"]
for-each mod modules [
    emit-line/indent ["LOAD_MODULE(" mod ");\"]
]
emit-line ["} while(0)"]

emit-line []
emit-line ["#define UNLOAD_BOOT_MODULES do {\"]
for-each mod modules [
    emit-line/indent ["UNLOAD_MODULE(" mod ");\"]
]
emit-line ["} while(0)"]

write-emitted output-dir/include/tmp-boot-modules.h
