REBOL []

name: 'Crypt
loadable: no ;tls depends on this, so it has to be builtin
source: %crypt/ext-crypt.c
init: %crypt/ext-crypt-init.reb
modules: [
    [
        name: 'Crypt
        source: %crypt/mod-crypt.c
        includes: reduce [
            ;
            ; Added so `#include "bigint/bigint.h` can be found by %rsa.h
            ; and `#include "rsa/rsa.h" can be found by %dh.c
            ;
            src-dir/extensions/crypt
            %prep/extensions/crypt ;for %tmp-extensions-crypt-init.inc
        ]
        depends: [
            %crypt/aes/aes.c
            %crypt/bigint/bigint.c
            %crypt/dh/dh.c
            %crypt/rc4/rc4.c
            %crypt/rsa/rsa.c
            %crypt/sha256/sha256.c
        ]
    ]
]
