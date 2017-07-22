REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Infix operator symbol definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2017 Rebol Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0.
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Purpose: {
        When a variable is set to a function value with SET, there is an option
        to designate that particular binding as /LOOKBACK.  This means that
        when the function is invoked through that variable, its first argument
        will come from the left hand side--before the invoking WORD!.

        If the function has two parameters, this gives the effect of what
        Rebol2 called an "OP!" (infix operator).  However, Ren-C's choice to
        make this a separate flag to SET means it does not require a new
        datatype.  Any FUNCTION! of any arity can be used, and it will just
        get its first argument from the left, with the rest from the right.
        
        This file sets up the common "enfixed" operators.
    }
]

; Due to Rebol's complex division of lexical space, operations like `<` have
; needed special rules in the scanner code.  These rules may have permitted
; use of the WORD! form, but made the SET-WORD! forms illegal (e.g. `<:`).
;
; Ren-C allows more of these things, but if they were used in this file it
; could not be read by R3-Alpha; which is used to process this file for
; bootstrap.  So it brings the operators into existence in %b-init.c in
; the function Add_Lib_Keys_R3Alpha_Cant_Make().
; 
; These hacks are used to get the properly bound WORD!s.  Note that FIRST,
; LOAD, INTERN etc. are not in the definition order at this point...so
; PICK MAKE BLOCK! is used.
;
; Note also the unsets for these at the bottom of the file for "cleanliness."

lt: (pick [<] 1)
lteq: (pick [<=] 1)
gt: (pick [>] 1)
gteq: (pick [>=] 1)
dv: (pick [/] 1) ;-- "slash" is the character #"/"
dvdv: (pick [//] 1)
ltgt: (pick [<>] 1)

right-arrow: bind (pick make block! "->" 1) context-of 'lambda
left-arrow: bind (pick make block! "<-" 1) context-of 'lambda
left-flag: bind (pick make block! "<|" 1) context-of 'lambda
right-flag: bind (pick make block! "|>" 1) context-of 'lambda


; While Ren-C has no particular concept of "infix OP!s" as a unique datatype,
; a function which is arity-2 and "enfix bound" to a variable acts similarly.
;
;     some-infix: enfix func [a b] [...]
;
; However, the default parameter convention for the left argument is to
; consume "one unit of expression" to the left.  So if you have:
;
;     add 1 2 some-infix add 1 2 + 10
;
; This would be interpreted as:
;
;     (add 1 2) some-infix (add 1 2 + 10)
;
; This is different from how OP!s acted in Rebol2 and R3-Alpha. They would
; "greedily" consume the immediate evaluative unit to the left:
;
;     add 1 (2 some-infix add 1 2) + 10
;
; These are two fundamentally different parameter conventions.  Ren-C lets
; you specify you want the latter by using an ISSUE! to indicate the parameter
; class is what is known as "tight":
;
;     some-infix: enfix func [#a #b] [...]
;
; Eventually it will be possible to "re-skin" a function with arbitrary new
; parameter conventions...e.g. to convert a function that doesn't quote one
; of its arguments so that it quotes it, without incurring any additional
; runtime overhead of a "wrapper".  Today a more limited version of this
; optimization is provided via TIGHTEN, which will convert all a function's
; parameters to be tight.  Hence a function with "normal" parameters (like
; ADD) can be translated into an equivalent function with "tight" parameters,
; to be bound to `+` and act compatibly with historical Rebol expectations.
;

+: enfix tighten :add
-: enfix tighten :subtract
*: enfix tighten :multiply
**: enfix tighten :power

set/enfix dv tighten :divide
set/enfix dvdv tighten :remainder

=: enfix tighten :equal?
=?: enfix tighten :same?

==: enfix tighten :strict-equal?
!=: enfix tighten :not-equal?
!==: enfix tighten :strict-not-equal?

set/enfix ltgt tighten :not-equal?

set/enfix lt tighten :lesser?
set/enfix lteq tighten :lesser-or-equal?

set/enfix gt tighten :greater?
set/enfix gteq tighten :greater-or-equal?

and: enfix tighten :and?
or: enfix tighten :or?
xor: enfix tighten :xor?
nor: enfix tighten :nor?

nand: enfix tighten :nand?
and*: enfix tighten :and~
or+: enfix tighten :or~
xor+: enfix tighten :xor~


; Postfix operator for asking the most existential question of Rebol...is it
; a Rebol value at all?  (non-void)
;
; !!! Originally in Rebol2 and R3-Alpha, ? was a synonym for HELP, which seems
; wasteful for the language as a whole when it's easy enough to type HELP.
; Postfix was not initially considered, because there was no ability of
; enfixed operators to force the left hand side of expressions to be as
; maximal as possible.  Hence `while [take blk ?] [...]` would ask if blk was
; void, not `take blk`.  So it was tried as a prefix operator, which wound
; up looking somewhat junky...now it's being tried as working postfix.

?: enfix :any-value?


; THEN and ELSE are "non-TIGHTened" enfix functions which either pass through
; an argument or run a branch, based on void-ness of the argument.  They take
; advantage of the pattern of conditionals such as `if condition [...]` to
; only return void if the branch does not run, and never return void if it
; does run (void branch evaluations are forced to BLANK!)
;
; These could be implemented as specializations of the generic EITHER-TEST
; native.  But due to their common use they are hand-optimized into their own
; specialized natives: EITHER-TEST-VOID and EITHER-TEST-VALUE.

then: enfix redescribe [
    "Evaluate the branch if the left hand side expression is not void"
](
    comment [specialize 'either-test [test: :void?]]
    :either-test-void
)

then*: enfix redescribe [
    "Would be the same as THEN/ONLY, if infix functions dispatched from paths"
](
    specialize 'then [only: true]
)

else: enfix redescribe [
    "Evaluate the branch if the left hand side expression is void"
](
    comment [specialize 'either-test [test: :any-value?]]
    :either-test-value
)

else*: enfix redescribe [
    "Would be the same as ELSE/ONLY, if infix functions dispatched from paths"
](
    specialize 'else [only: true]
)


; Lambdas are experimental quick function generators via a symbol
;
set/enfix right-arrow :lambda
set/enfix left-arrow (specialize :lambda [only: true])


; These usermode expression-barrier like constructs may not necessarily use
; their left-hand arguments...however by being enfixed and not having <tight>
; first args, they are able to force complete expressions to their left.

set/enfix left-flag :left-bar
set/enfix right-flag :right-bar
||: enfix :once-bar


; Clean up the words used to hold things that can't be made SET-WORD!s (or
; perhaps even words) in R3-Alpha

lt:
lteq:
gt:
gteq:
dv:
dvdv:
should-be-empty-tag:
right-arrow:
left-arrow:
left-flag:
right-flag:
    ()
