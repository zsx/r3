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


+: enfix :add
-: enfix :subtract
*: enfix :multiply


**: enfix :power
=: enfix :equal?
=?: enfix :same?
==: enfix :strict-equal?
!=: enfix :not-equal?

!==: enfix :strict-not-equal?

||: enfix :once-bar ;-- not mechanically infix (it's ENDFIX?)

and: enfix :and?
or: enfix :or?
xor: enfix :xor?
nor: enfix :nor?
nand: enfix :nand?

and*: enfix :and~
or+: enfix :or~
xor+: enfix :xor~


; ELSE is an experiment to try and allow IF condition [branch1] ELSE [branch2]
; For efficiency it uses references to the branches and does not copy them
; into the body or protect them from mutation.  It is supported by the
; BRANCHER native, which still has the overhead of creating a function at this
; time but is still speedier than a user function.
;
else: enfix :brancher


; So long as the code wants to stay buildable with R3-Alpha, the mezzanine
; cannot use -> or <-, nor even mention them as words.  So this hack is likely
; to be around for quite a long time.  FIRST, LOAD, INTERN etc. are not
; in the definition order at this point...so PICK MAKE BLOCK! is used.
;

set/lookback (pick [/] 1) :divide
set/lookback (pick [//] 1) :remainder

set/lookback (pick [<>] 1) :not-equal?

set/lookback (pick [<] 1) :lesser?
set/lookback (pick [<=] 1) :lesser-or-equal?
set/lookback (pick [>] 1) :greater?
set/lookback (pick [>=] 1) :greater-or-equal?

right-arrow: bind (pick make block! "->" 1) context-of 'lambda
left-arrow: bind (pick make block! "<-" 1) context-of 'lambda
left-flag: bind (pick make block! "<|" 1) context-of 'lambda
right-flag: bind (pick make block! "|>" 1) context-of 'lambda

set/lookback right-arrow :lambda
set/lookback left-arrow (specialize :lambda [only: true])
set/lookback left-flag :left-bar
set right-flag :right-bar ;-- not mechanically infix (punctuator)


right-arrow: left-arrow: left-flag: right-flag: () ; don't leave stray defs
