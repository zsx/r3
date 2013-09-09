REBOL []

do %make-host-init.r

encap-exe: read %../make/encap-boot.bin

write %../make/encap-boot.r mold/only compose [REBOL [] encap-exe: (encap-exe)]

; Files to include in the host program:
files: [
	%../../framework/dev-tools/include.r
	%../make/encap-boot.r
	%mezz/prot-tls.r
	%mezz/prot-http.r
	%mezz/saphir-patches.r
	%mezz/saphir-encap.r
]

code: load-files files

save %boot/host-init.r code

write-c-file %include/host-init.h code
