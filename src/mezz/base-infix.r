REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Infix operator symbol definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0.
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        This defines some infix operators.

        See ops.r for how the "weird" words that have to be done through
        a tricky SET manage to have bindings in the lib context, even
        though they aren't picked up here as SET-WORD!.
    }
]

+: to-infix :add
-: to-infix :subtract
*: to-infix :multiply

set (pick [/] 1) to-infix :divide
set (pick [//] 1) to-infix :remainder

**: to-infix :power
=: to-infix :equal?
=?: to-infix :same?
==: to-infix :strict-equal?
!=: to-infix :not-equal?

set (pick [<>] 1) to-infix :not-equal?

!==: to-infix :strict-not-equal?

set (pick [<] 1) to-infix :lesser?
set (pick [<=] 1) to-infix :lesser-or-equal?
set (pick [>] 1) to-infix :greater?
set (pick [>=] 1) to-infix :greater-or-equal?

and: to-infix :and?
or: to-infix :or?
xor: to-infix :xor?

and*: to-infix :and~
or+: to-infix :or~
xor-: to-infix :xor~

; !!! C-isms that are unlikely to be kept
&: to-infix :and~
|: to-infix :or~
