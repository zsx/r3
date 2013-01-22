REBOL [
	Title: "R3 Patches"
	Type: module
	Author: "Brian Hawley" ; BrianH
	Date: 10-Sep-2012
	License: MIT
]
; Do any exports manually, at least until exporting is fixed.
; This module isn't named so it won't be saved to system/modules.

replace-export: func [
	"Replace a value in lib and all tracable exports."
	'name [word!] value [any-type!] /local old new m
] [
	if in lib name [
		set/any 'old get/any in lib name
		set/any in lib name :value
		m: system/modules ; These are only the named modules
		forskip m 3 [
			if all [new: in :m/2 name same? :old get/any new] [set/any new :value]
		]
		if all [
			new: in system/contexts/user name same? :old get/any new
		] [set/any new :value]
	]
]

tmp: reduce [spec-of :sys/load-ext-module body-of :sys/load-ext-module]
; Add multi-module support (http://issue.cc/r3/1877)
unless find tmp/1 'end [append tmp/1 'end]
if attempt [none? :tmp/2/36/3] [
	append tmp/2/6 [end:]
	insert at tmp/2 28 [
		if all [not empty? end same? head code head end] [
			code: to block! copy/part code end
		]
	]
	append last tmp/2 'end
]
; Remake the function
bind bind tmp/2 lib sys
sys/load-ext-module: make function! tmp

fix: false
tmp: reduce [spec-of :sys/load-module body-of :sys/load-module]
; Workaround for resolve/extend/only crash (http://issue.cc/r3/1865)
if attempt ['resolve/extend/only = :tmp/2/9/49/2/8/1] [
	fix: true
	tmp/2/9/49/2/8: [
		resolve/only lib mod bind/new/only/copy hdr/exports lib
	]
]
; Add multi-module support (http://issue.cc/r3/1877)
unless find tmp/1 'end [append tmp/1 'end]
if attempt [none? :tmp/2/11/6] [
	fix: true
	append tmp/2/7/12/5/7/8/2/2 [end:]
	append tmp/2/7/12/5/7/8/5 [end:]
	append tmp/2/7/12/5/7/8/12/2 [end:]
	append tmp/2/9/9/2 [end:]
	append tmp/2/9/41/3 'end
	insert at tmp/2/9/46/2 8 [
		all [not empty? end same? head code head end] [code: to block! copy/part code end]
	]
	append last tmp/2 'end
]
; Remake the function
if fix [
	bind bind tmp/2 lib sys
	sys/load-module: make function! tmp
]

tmp: body-of :sys/export-words
; Workaround for resolve/extend/only crash (http://issue.cc/r3/1865)
if attempt ['resolve/extend/only = :tmp/3/1] [
	tmp/3: [
		words: bind/new/only bind/new/only/copy words lib system/contexts/user
		resolve/only lib ctx words
		resolve/only system/contexts/user lib words
	]
	sys/export-words: make function! reduce [spec-of :sys/export-words bind tmp lib]
]

tmp: body-of :lib/script?
; Fix script? string (http://issue.cc/r3/1885)
if attempt ['to-binary = :tmp/4/5/2] [
	change/part next :tmp/4/5 [to binary!] 1
	replace-export script? make function! reduce [spec-of :lib/script? bind tmp lib]
]

fix: false
tmp: body-of :lib/save
; Fix save/header where data true (http://issue.cc/r3/1907)
if attempt ['block? = :tmp/11/8] [
	fix: true
	tmp/11/8: 'object?
	tmp/11/9: tmp/11/10/2: tmp/11/11/2: to get-word! :tmp/11/9
	swap at :tmp/11 10 at :tmp/11 11
]
; Fix save where any-function (http://issue.cc/r3/1908)
if attempt [same? unbind 'value :tmp/4/6] [
	fix: true
	tmp/4/6: tmp/11/5/3/2/2: to get-word! :tmp/4/6
	; Note: first+ can't take a get-word, but doesn't need to.
]
if fix [
	replace-export save make function! reduce [spec-of :lib/save bind tmp lib]
]

; TODO: Fixing decode-url (http://issue.cc/r3/1644)

; Unset local variables just in case this context stays referenced somehow
tmp: fix: none
