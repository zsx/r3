REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "Infix operator symbol definitions"
    Rights: {
        Copyright 2012 REBOL Technologies
        Copyright 2012-2016 Rebol Open Source Contributors
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
should-be-empty-tag: (pick [<>] 1)

right-arrow: bind (pick make block! "->" 1) context-of 'lambda
left-arrow: bind (pick make block! "<-" 1) context-of 'lambda
left-flag: bind (pick make block! "<|" 1) context-of 'lambda
right-flag: bind (pick make block! "|>" 1) context-of 'lambda


; While Ren-C has no particular concept of "infix OP!s" as a unique datatype,
; a function which is arity-2 and bound lookback to a variable acts similarly. 
; Yet the default is to obey the same lookahead rules as prefix operations
; historically applied.
;
; That would mean that `1 * add 1 2 * 3` behaves the same as `1 * 1 + 2 * 3`.
; To override this and act like ((1 * 1) + 2) * 3) it's necessary for the
; second argument to infix to avoid looking ahead for further lookback
; functions.  This is a per-parameter user-controllable property in Ren-C.
;

+: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    add :arg1 :arg2
]

-: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    subtract :arg1 :arg2
]

*: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    multiply :arg1 :arg2
]

**: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    power :arg1 :arg2
]

set/lookback dv func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    divide :arg1 :arg2
]

set/lookback dvdv func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    remainder :arg1 :arg2
]


; As a break from history, it was considered comparisons in Ren-C force
; "complete expressions" on their left by deferring the left-hand argument,
; while consuming arbitrarily long chains of infix on their right.  This
; permits `10 = 5 + 5` to work the same as `10 = probe 5 + 5`, or even
; `add 2 8 = 5 + 5`.  However, it would mean that `x: 1 = var` would not
; assign x the result of the comparison, because `x: 1` is a complete
; expression.  Similarly `not x = 5` would mean `(not x) = 5`.  While a
; potentially interesting consideration for the future, the historical
; invariant is preserved in these versions of the operators.

=: enfix func [arg1 [<opt> any-value!] arg2 [<defer> <opt> any-value!]] [
    equal? :arg1 :arg2
]

=?: enfix func [arg1 [<opt> any-value!] arg2 [<defer> <opt> any-value!]] [
    same? :arg1 :arg2
]

==: enfix func [arg1 [<opt> any-value!] arg2 [<defer> <opt> any-value!]] [
    strict-equal? :arg1 :arg2
]

!=: enfix func [arg1 [<opt> any-value!] arg2 [<defer> <opt> any-value!]] [
    not-equal? :arg1 :arg2
]

!==: enfix func [arg1 [<opt> any-value!] arg2 [<defer> <opt> any-value!]] [
    strict-not-equal? :arg1 :arg2
]

set/lookback should-be-empty-tag func [
    arg1 [<opt> any-value!]
    arg2 [<defer> <opt> any-value!]
][
    not-equal? :arg1 :arg2
]

set/lookback lt func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    lesser? :arg1 :arg2
]

set/lookback lteq func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    lesser-or-equal? :arg1 :arg2
]

set/lookback gt func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    greater? :arg1 :arg2
]

set/lookback gteq func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    greater-or-equal? :arg1 :arg2
]


; The AND, OR, XOR behavior is being changed so radically that it's worth
; asking what the best rule is.  Are they more like math operators or like
; comparison operators?  Compatibility doesn't apply here, as Ren-C has
; basically retaken the undecorated forms for LOGIC! and not bitwise math.
; But using the compatible infix deferment of right-hand side for now.

and: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    and? :arg1 :arg2
]

or: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    or? :arg1 :arg2
]

xor: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    xor? :arg1 :arg2
]

nor: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    nor? :arg1 :arg2
]

nand: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    nand? :arg1 :arg2
]

and*: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    and~ :arg1 :arg2
]

or+: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    or~ :arg1 :arg2
]

xor+: enfix func [arg1 [any-value!] arg2 [<defer> any-value!]] [
    xor~ :arg1 :arg2
]


; Postfix operator for asking the most existential question of Rebol...is it
; a Rebol value at all?  (non-void)
;
; !!! Originally in Rebol2 and R3-Alpha, ? was a synonym for HELP, which seems
; wasteful for the language as a whole when it's easy enough to type HELP.
; Postfix was not initially considered, because there was no ability of
; enfixed operators to force the left hand side of expressions to be as
; maximal as possible.  Hence `while [take blk ?] [...]` would ask if blk was
; void, not `take blk`.  So it was tried as a prefix operator, which wound
; up looking somewhat junky.

?: enfix function [arg [<defer> <opt> any-value!]] [
    any-value? :arg
]


; ELSE is an experiment to try and allow IF condition [branch1] ELSE [branch2]
; For efficiency it uses references to the branches and does not copy them
; into the body or protect them from mutation.  It is supported by the
; BRANCHER native, which still has the overhead of creating a function at this
; time but is still speedier than a user function.
;
else: enfix :brancher


; Lambdas are experimental quick function generators via a symbol
;
set/lookback right-arrow :lambda
set/lookback left-arrow (specialize :lambda [only: true])


; These usermode expression-barrier like constructs may not necessarily use
; their left-hand arguments...however by being enfixed and declaring their
; left-hand argument to be "<defer>" they are able to force complete
; expressions to their left.

set/lookback left-flag :left-bar
set/lookback right-flag :right-bar
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
