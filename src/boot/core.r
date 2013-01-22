REBOL [
	Title: "REBOL Core"
	Name: core
	Type: extension
	Exports: [] ; added by make-host-ext.r
]

words: [
	n			;modulus
	e			;public exponent
	d			;private exponent
	p			;prime num 1
	q			;prime num 2
	dp			;CRT exponent 1
	dq			;CRT exponent 2
	qinv		;CRT coefficient
]

init-words: command [
	words [block!]
]

init-words words

show-console: command [
	"Opens console window"
]

console-output: command [
	"Enables/Diables printing to console"
	state [logic!]
]

hide-console: command [
	"Hides console window if already open"
]

to-png: command [
	"Save an image to PNG format"
	image [image!]
]

<no-export> req-dir: command [
	"low-level command used by REQUEST-DIR"
	/title
		text [string!]
	/path
		dir [string!]
]

request-dir: funct [
	"Asks user to select a directory and returns it as file path"
	/title
		"Custom dialog title text"
		text [string!]
	/path
		"Default directory path"
		dir [file!]
][
	if dir [dir: lib/replace/all to-local-file dir "/" "//"]
	if result: apply :req-dir [title text path dir] [
		return to-rebol-file result
	]
]

rc4: command [
	"Encrypt/decrypt data(modifies) using RC4 algorithm. Returns stream cipher context handle."
	data [binary! none!] "Data to encrypt/decrypt. Or NONE to close the cipher stream."
	/key
		crypt-key [binary!] "Crypt key. Have to be provided only for the first time."
	/stream
		ctx [handle!] "Stream cipher context."
]

rsa-make-key: func [
	"Creates a key object for RSA algorithm."
][
	make object! [
		n:			;modulus
		e:			;public exponent
		d:			;private exponent
		p:			;prime num 1
		q:			;prime num 2
		dp:			;CRT exponent 1
		dq:			;CRT exponent 2
		qinv:		;CRT coefficient
		none
	]
]

rsa: command [
	"Encrypt/decrypt data using the RSA algorithm (with PKCS#1 padding)."
	data [binary!]
	key-object [object!]
	/decrypt "Decrypts the data (default is to encrypt)"
	/private "Uses an RSA private key (default is a public key)"
]
