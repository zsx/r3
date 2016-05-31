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

+: -: *: ()

set/lookback '+ :add
set/lookback '- :subtract
set/lookback '* :multiply

set/lookback (pick [/] 1) :divide
set/lookback (pick [//] 1) :remainder

**: =: =?: ==: !=: ()

set/lookback '** :power
set/lookback '= :equal?
set/lookback '=? :same?
set/lookback '== :strict-equal?
set/lookback '!= :not-equal?

set/lookback (pick [<>] 1) :not-equal?

!==: ()

set/lookback '!== :strict-not-equal?

set/lookback (pick [<] 1) :lesser?
set/lookback (pick [<=] 1) :lesser-or-equal?
set/lookback (pick [>] 1) :greater?
set/lookback (pick [>=] 1) :greater-or-equal?

; So long as the code wants to stay buildable with R3-Alpha, the mezzanine
; cannot use -> or <-, nor even mention them as words.  So this hack is likely
; to be around for quite a long time.  FIRST, LOAD, INTERN etc. are not
; in the definition order at this point...so PICK MAKE BLOCK! is used.
;
right-arrow: bind (pick make block! "->" 1) context-of 'lambda
left-arrow: bind (pick make block! "<-" 1) context-of 'lambda
left-flag: bind (pick make block! "<|" 1) context-of 'lambda
right-flag: bind (pick make block! "|>" 1) context-of 'lambda

set/lookback right-arrow :lambda
set/lookback left-arrow (specialize :lambda [only: true])
set/lookback left-flag :left-bar
set/lookback right-flag :right-bar

right-arrow: left-arrow: left-flag: right-flag: () ; don't leave stray defs

and: or: xor: nor: nand: ()

set/lookback 'and :and?
set/lookback 'or :or?
set/lookback 'xor :xor?
set/lookback 'nor :nor?
set/lookback 'nand :nand?

and*: or+: xor+: ()

set/lookback 'and* :and~
set/lookback 'or+ :or~
set/lookback 'xor+ :xor~
