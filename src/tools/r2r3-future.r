REBOL [
	Title: "Rebol2 and R3-Alpha Future Bridge to Ren/C"
	Rights: {
		Rebol is Copyright 1997-2015 REBOL Technologies
		REBOL is a trademark of REBOL Technologies

		Ren/C is Copyright 2015 MetaEducation
	}
	License: {
		Licensed under the Apache License, Version 2.0
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
	Author: "@HostileFork"
	Purpose: {
		These routines can be run from a Rebol2 or R3-Alpha
		to make them act more like Ren/C (which aims to
		implement a finalized Rebol3 standard).

		!!! Rebol2 support intended but not yet implemented.
	}
]

unless value? 'length [length: :length? unset 'length?]
unless value? 'index-of [index-of: :index? unset 'index?]
unless value? 'offset-of [offset-of: :offset? unset 'offset?]
unless value? 'type-of [type-of: :type? unset 'type?]

unless value? 'for-each [
	for-each: :foreach every: :foreach
	;unset 'foreach ;-- tolerate it (for now, maybe indefinitely?)
]

; It is not possible to make a version of eval that does something other
; than everything DO does in an older Rebol.  Which points to why exactly
; it's important to have only one function like eval in existence.
unless value? 'eval [
    eval: :do
]

unless value? 'fail [
	fail: func [
		{Interrupts execution by reporting an error (a TRAP can intercept it).}
		reason [error! string! block!] "ERROR! value, message string, or failure spec"
	][
		case [
			error? reason [do error]
			string? reason [do make error! reason]
			block? reason [
				for-each item reason [
					unless any [
						scalar? :item
						string? :item
						paren? :item
						all [
							word? :item
							not any-function? get :item
						]
					][
						do make error! (
							"FAIL requires complex expressions to be in a PAREN!"
						)
					]
				]
				do make error! form reduce reason
			]
		]
	]
]
