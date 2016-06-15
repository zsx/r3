REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Base: Debug Functions"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Note: {
        This code is evaluated just after actions, natives, sysobj, and other lower
        levels definitions. This file intializes a minimal working environment
        that is used for the rest of the boot.
    }
]

boot-print: func [
    "Prints during boot when not quiet."
    data
][
    unless system/options/quiet [print :data]
]

loud-print: func [
    "Prints during boot when verbose."
    data
][
    if system/options/flags/verbose [print :data]
]
