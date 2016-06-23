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

boot-print: function [
    "Prints during boot when not quiet."
    data
    /eval
][
    eval_BOOT_PRINT: eval
    eval: :lib/eval

    unless system/options/quiet [
        print/(if any [eval | semiquoted? 'data] ['eval]) :data
    ]
]

loud-print: function [
    "Prints during boot when verbose."
    data
    /eval
][
    eval_BOOT_PRINT: eval
    eval: :lib/eval

    if system/options/flags/verbose [
        print/(if any [eval | semiquoted? 'data] ['eval]) :data
    ]
]
