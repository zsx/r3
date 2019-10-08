REBOL[]

;-------------------------------------------------------------------------------
;-- Resource embedding support

RES_TYPE: [
    ICON         3
    RCDATA       10
    GROUP_ICON   14
    VERSION      16
]

APP_ICO_ID: 1
VS_VERSION_INFO_ID: 1  

;-------------------------------------------------------------------------------


args: parse system/script/args ""
exe: none
payload: none
output: none
as-is: false ;don't compress, in case people try to avoid decompression to speed up bootup
windows?: 3 = fourth system/version

while [not tail? args] [
	arg: first args
	case [
		any [arg = "/rebol"
		     arg = "/r"] [
				 exe: second args
				 args: next args
		]
		any [arg = "/payload"
		     arg = "/p"] [
				 payload: second args
				 args: next args
		]
		any [arg = "/output"
		     arg = "/o"] [
				 output: second args
				 args: next args
		]
		any [arg = "/as-is"
		     arg = "/a"] [
				 as-is: true
		]
	]
	args: next args
]

if any [none? exe
	none? payload
	none? output][
	print ["pack.r"]
	print ["^-/rebol | /r^- path-to-rebol"]
	print ["^-/payload | /p^- path-to-payload"]
	print ["^-/output | /o^- path-to-output"]
	print ["^-/as-is | /a^- Do not compress the script"]
	quit
]

payload-data: read to file! payload
either as-is [
	payload-data: join #{00000000} payload-data
][
	payload-data: join #{01000000} compress payload-data
]
tmp: join payload ".tmp"
write to file! tmp payload-data

either windows? [
	unicodify: function [
		s [string!]
	][
		ret: copy #{}
		foreach c s [
			append ret join to binary! c #{00}
		]
		join ret #{0000} ;NULL terminator
	]

	kernel32: make library! %kernel32

	print ["kernel:" mold kernel32]
	BeginUpdateResource: make routine! compose [[
		filename [pointer]
		delete-existing-resources [int32]
		return: [pointer]
	] (kernel32) "BeginUpdateResourceW"]

	UpdateResource: make routine! compose [[
		hUpdate [pointer]
		lpType [pointer]
		lpName [pointer]
		wLanguage [uint16]
		lpData [pointer]
		cbData [uint32]
		return: [int32]
	] (kernel32) "UpdateResourceW"]

	EndUpdateResource: make routine! compose [[
		hUpdate [pointer]
		fDiscard [uint8]
		return: [int32]
	] (kernel32) "EndUpdateResourceW"]

	GetLastError: make routine! compose [[
		return: [uint32]
	] (kernel32) "GetLastError"]

	write to file! output read to file! exe
	h: BeginUpdateResource unicodify output 0
	if zero? h [
		print ["failed to open exe"]
		halt
	]
	if zero? UpdateResource h 10
		unicodify "EmbEddEdREbol"
		0 payload-data length? payload-data [
		print ["failed to update resource"]
		halt
	]

    ; Update app icon
    icon-data: read %app.ico
	if zero? UpdateResource h RES_TYPE/ICON
		APP_ICO_ID  ; ID  unicodify "APP_ICON"
		0           ; Language neutral
		copy at icon-data 23            ; skip icon header
		subtract length? icon-data 22   ; subtract header length
    [
		print ["failed to update app icon resource"]
		halt
	]
	
    ; This is for a single 32x32 32bpp icon. The first %app.ico is such an
    ; icon. We need to make this work for other icons of course, but this
    ; is a first step.
    ; It's a hack, because I learned about Shixin's struct support
    ; after getting this to work.
    ; Watch endian issues.
    group-icon-data: #{0000 0100 0100 20 20 00 00 0200 2000 10A80000 0100}  ; 10A8 = 4264 = size? data - header-size
    ; Update group icon data. Without this Windows doesn't know there are icons in the file.
	if zero? UpdateResource h RES_TYPE/GROUP_ICON
		APP_ICO_ID  ; ID  unicodify "MAIN_ICON"
		0           ; Language neutral
		group-icon-data
		length? group-icon-data
    [
		print ["failed to update group-icon resource"]
		halt
	]

	if zero? EndUpdateResource h 0 [
		print ["failed to write change back to the file due to " GetLastError]
		halt
	]
][
	magic: ".EmbEddEdREbol"
	call reform ["objcopy -R" magic exe]
	call rejoin ["objcopy --add-section " magic "=" tmp " " exe " " output]
	delete to file! tmp
]
