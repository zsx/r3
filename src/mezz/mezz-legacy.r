REBOL [
	System: "REBOL [R3] Language Interpreter and Run-time Environment"
	Title: "REBOL 3 Mezzanine: Legacy compatibility"
	Rights: {
		Copyright 1997-2015 REBOL Technologies
		Copyright 2012-2015 Rebol Open Source Contributors

		REBOL is a trademark of REBOL Technologies
	}
	License: {
		Licensed under the Apache License, Version 2.0
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
	Description: {
		These definitions help support Rebol code that was written prior
		to Ren/C.  (See also the OS environment setting R3_LEGACY=1)
		This will eventually be reworked as an optional compatibility
		module that will not be included in the default distribution.

		A porting guide Trello has been started at:

		https://trello.com/b/l385BE7a/porting-guide
	}
]

op?: func [
	"Returns TRUE if the argument is an ANY-FUNCTION? and INFIX?"
	value [any-type!]
][
	either any-function? :value [:infix? :value] false
]


; It would be nicer to use type!/type? instead of datatype!/datatype?  :-/
; The compatibility layer may have to struggle with that change down the line
type?: :type-of

; See also prot-http.r, which has an actor with a LENGTH? "method".  Given
; how actors work, it cannot be overriden here.
length?: :length

index?: :index-of

offset?: :offset-of

sign?: :sign-of


; The distinctions between Rebol's types is important to articulate.
; So using the term "BLOCK" generically to mean any composite
; series--as well as specificially the bracketed block type--is a
; recipe for confusion.
;
; Importantly: it also makes it difficult to get one's bearings
; in the C sources.  It's hard to find where exactly the bits are in
; play that make something a bracketed block...or if that's what you
; are dealing with at all.  Hence some unique name for the typeclass
; is needed.
;
; The search for a new word for the ANY-BLOCK! superclass went on for
; a long time.  LIST! was looking like it might have to be the winner,
; until ARRAY! was deemed more fitting.  The notion that a "Rebol
; Array" contains "Rebol Values" that can be elements of any type
; (including other arrays) separates it from the other series classes
; which can only contain one type of element (codepoints for strings,
; bytes for binaries, numbers for vector).  In the future those may
; have more unification under a typeclass of their own, but ARRAY!
; is for BLOCK!, PAREN!, PATH!, GET-PATH!, SET-PATH!, and LIT-PATH!

any-block!: :any-array!
any-block?: :any-array?


; !!! These have not been renamed yet, because of questions over what they
; actually return.  CONTEXT-OF was a candidate, however the current behavior
; of just returning TRUE from BIND? when the word is linked to a function
; arg or local is being reconsidered to perhaps return the function, and to
; be able to use functions as targets for BIND as well.  So binding-of might
; be the better name, or bind-of as it's shorter.

;bind?
;bound?


; !!! These less common cases still linger as question mark routines that
; don't return LOGIC!, and they seem like they need greater rethinking in
; general. What replaces them (for ones that are kept) might be entirely new.

;encoding?
;file-type?
;speed?
;suffix?
;why?
;info?
;exists?


; In word-space, TRY is very close to ATTEMPT, in having ambiguity about what
; is done with the error if one happens.  It also has historical baggage with
; TRY/CATCH constructs. TRAP does not have that, and better parallels CATCH
; by communicating you seek to "trap errors in this code (with this handler)"
; Here trapping the error suggests you "caught it in a trap" and it is
; in the trap (and hence in your possession) to examine.  /WITH is not only
; shorter than /EXCEPT but it makes much more sense.
;
; !!! This may free up TRY for more interesting uses, such as a much shorter
; word to use for ATTEMPT.
;
try: func [
	<transparent>
	{Tries to DO a block and returns its value or an error.}
	block [block!]
	/except "On exception, evaluate this code block"
	code [block! any-function!]
] [
	either except [trap/with block :code] [trap block]
]
