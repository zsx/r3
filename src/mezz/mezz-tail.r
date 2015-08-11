REBOL [
	System: "REBOL [R3] Language Interpreter and Run-time Environment"
	Title: "REBOL 3 Mezzanine: End of Mezz"
	Rights: {
		Copyright 2012 REBOL Technologies
		REBOL is a trademark of REBOL Technologies
	}
	License: {
		Licensed under the Apache License, Version 2.0
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
]

funco: :func ; save it for expert usage

; Final FUNC definition:
func: funco [
	{Defines a user function with given spec and body.}
	spec [block!] {Help string (opt) followed by arg words (and opt type and string)}
	body [block!] {The body block of the function}
][
	make function! copy/deep reduce [spec body] ; (now it deep copies)
]


;; Compatibility routines for pre-Ren/C (temporary)

; This needs a more complete story, controlled by switches to give deprecation
; warnings.  But for starters putting them here.

op?: func [
	"Returns TRUE if the argument is an ANY-FUNCTION? and INFIX?"
	value [any-type!]
][
	either any-function? :value [:infix? :value] false
]
