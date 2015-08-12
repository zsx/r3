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
