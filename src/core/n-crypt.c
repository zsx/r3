//
//  File: %n-crypt.c
//  Summary: "Native Functions for cryptography"
//  Section: natives
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The original cryptography additions to Rebol were done by Saphirion, at
// a time prior to Rebol's open sourcing.  They had to go through a brittle,
// incomplete, and difficult to read API for extending the interpreter with
// C code.
//
// This contains a simplification of %host-core.c, written directly to the
// native API.  It also includes the longstanding (but not standard, and not
// particularly secure) ENCLOAK and DECLOAK operations from R3-Alpha.
//

#include "sys-core.h"

#include "rc4/rc4.h"
#include "rsa/rsa.h" // defines gCryptProv and rng_fd (used in Init/Shutdown)
#include "dh/dh.h"
#include "aes/aes.h"



//
//  Init_Crypto: C
//
void Init_Crypto(void)
{
#ifdef TO_WINDOWS
    if (!CryptAcquireContextW(
        &gCryptProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT
    )) {
        // !!! There is no good way to return failure here as the
        // routine is designed, and it appears that in some cases
        // a zero initialization worked in the past.  Assert in the
        // debug build but continue silently otherwise.
        assert(FALSE);
        gCryptProv = 0;
    }
#else
    rng_fd = open("/dev/urandom", O_RDONLY);
    if (rng_fd == -1) {
        // We don't crash the release client now, but we will later
        // if they try to generate random numbers
        assert(FALSE);
    }
#endif
}


//
//  Shutdown_Crypto: C
//
void Shutdown_Crypto(void)
{
#ifdef TO_WINDOWS
    if (gCryptProv != 0)
        CryptReleaseContext(gCryptProv, 0);
#else
    if (rng_fd != -1)
        close(rng_fd);
#endif
}


// The original Saphirion implementation used OS_ALLOC (basically malloc) to
// leave a potentially dangling memory pointer for the RC4 context.  Ren-C
// has "managed handles" which will clean themselves up when they are no
// longer referenced.
//
static void cleanup_rc4_ctx(const REBVAL *val)
{
    assert(IS_HANDLE(val));
    assert(val->payload.handle.code == NULL);
    assert(val->payload.handle.data != NULL);

    RC4_CTX *rc4_ctx = cast(RC4_CTX*, val->payload.handle.data);
    FREE(RC4_CTX, rc4_ctx);
}


//
//  rc4: native [
//  
//  "Encrypt/decrypt data (modifies) using RC4 algorithm."
//
//      return: [handle!]
//          "Returns stream cipher context handle.
//      /key
//          "Provided only for the first time to get stream HANDLE!"
//      crypt-key [binary!]
//          "Crypt key."
//      /stream
//      ctx [handle!]
//          "Stream cipher context."
//      data [binary!]
//          "Data to encrypt/decrypt."
//  ]
//
REBNATIVE(rc4)
{
    REFINE(1, key);
    PARAM(2, crypt_key);
    REFINE(3, stream);
    PARAM(4, ctx);
    PARAM(5, data);

    if (IS_HANDLE(ARG(ctx))) {
        REBVAL *data = ARG(data);

        RC4_CTX *rc4_ctx = cast(RC4_CTX*, VAL_HANDLE_DATA(ARG(ctx)));

        RC4_crypt(
            rc4_ctx,
            VAL_BIN_AT(data), // input "message"
            VAL_BIN_AT(data), // output (same, since it modifies)
            VAL_LEN_AT(data)
        );

        // In %host-core.c this used to fall through to return the first arg,
        // a refinement, which was true in this case.  :-/
        //
        return R_TRUE;
    }
    
    if (IS_BINARY(ARG(crypt_key))) { // Key defined - setup new context
        RC4_CTX *rc4_ctx = ALLOC_ZEROFILL(RC4_CTX);

        RC4_setup(rc4_ctx, VAL_BIN_AT(ARG(key)), VAL_LEN_AT(ARG(key)));

        Init_Handle_Managed(D_OUT, NULL, rc4_ctx, &cleanup_rc4_ctx);
        return R_OUT;
    }

    assert(FALSE); // was falling through and returning first refinement arg
    return R_TRUE;
}


//
//  rsa: native [
//
//  "Encrypt/decrypt data using the RSA algorithm."
//
//      data [binary!]
//      key-object [object!]
//      /decrypt
//         "Decrypts the data (default is to encrypt)"
//      /private
//         "Uses an RSA private key (default is a public key)"
//      /padding
//          "Selects the type of padding to use"
//      padding-type [word! blank!]
//          "Type of padding. Available values: PKCS1 or NONE"
//  ]
//
REBNATIVE(rsa)
{
    PARAM(1, data);
    PARAM(2, key_object);
    REFINE(3, decrypt);
    REFINE(4, private);
    REFINE(5, padding);
    PARAM(6, padding_type);

    REBOOL padding;
    if (REF(padding))
        padding = NOT(IS_BLANK(ARG(padding_type)));
    else
        padding = TRUE; // PKCS1 is on by default

    REBYTE *n = NULL;
    REBYTE *e = NULL;
    REBYTE *d = NULL;
    REBYTE *p = NULL;
    REBYTE *q = NULL;
    REBYTE *dp = NULL;
    REBYTE *dq = NULL;
    REBYTE *qinv = NULL;

    REBINT n_len = 0;
    REBINT e_len = 0;
    REBINT d_len = 0;
    REBINT p_len = 0;
    REBINT q_len = 0;
    REBINT dp_len = 0;
    REBINT dq_len = 0;
    REBINT qinv_len = 0;

    REBCTX *obj = VAL_CONTEXT(ARG(key_object));

    REBVAL *key = CTX_KEYS_HEAD(obj);
    REBVAL *var = CTX_VARS_HEAD(obj);

    for (; NOT_END(key); ++key, ++var) {
        if (!IS_BINARY(var))
            continue; // if not binary then what?

        switch (VAL_KEY_SYM(key)) {
        case SYM_N:
            n = VAL_BIN_AT(var);
            n_len = VAL_LEN_AT(var);
            break;

        case SYM_E:
            e = VAL_BIN_AT(var);
            e_len = VAL_LEN_AT(var);
            break;

        case SYM_D:
            d = VAL_BIN_AT(var);
            d_len = VAL_LEN_AT(var);
            break;

        case SYM_P:
            p = VAL_BIN_AT(var);
            p_len = VAL_LEN_AT(var);
            break;

        case SYM_Q:
            q = VAL_BIN_AT(var);
            q_len = VAL_LEN_AT(var);
            break;

        case SYM_DP:
            dp = VAL_BIN_AT(var);
            dp_len = VAL_LEN_AT(var);
            break;

        case SYM_DQ:
            dq = VAL_BIN_AT(var);
            dq_len = VAL_LEN_AT(var);
            break;

        case SYM_QINV:
            qinv = VAL_BIN_AT(var);
            qinv_len = VAL_LEN_AT(var);
            break;

        default:
            fail (Error(RE_MISC));
        }
    }

    if (!n || !e)
        return R_BLANK;

    RSA_CTX *rsa_ctx = NULL;

    REBINT binary_len;
    if (REF(private)) {
        if (!d)
            return R_BLANK;

        RSA_priv_key_new(
            &rsa_ctx, n, n_len, e, e_len, d, d_len,
            p, p_len, q, q_len, dp, dp_len, dq, dq_len, qinv, qinv_len
        );
        binary_len = d_len;
    }
    else {
        RSA_pub_key_new(&rsa_ctx, n, n_len, e, e_len);
        binary_len = n_len;
    }

    REBYTE *dataBuffer = VAL_BIN_AT(ARG(data));
    REBINT data_len = VAL_LEN_AT(ARG(data));

    BI_CTX *bi_ctx = rsa_ctx->bi_ctx;
    bigint *data_bi = bi_import(bi_ctx, dataBuffer, data_len);

    REBSER *binary = Make_Binary(binary_len);

    if (REF(decrypt)) {
        binary_len = RSA_decrypt(
            rsa_ctx,
            dataBuffer,
            BIN_HEAD(binary),
            REF(private) ? 1 : 0,
            padding ? 1 : 0
        );

        if (binary_len == -1) {
            bi_free(rsa_ctx->bi_ctx, data_bi);
            RSA_free(rsa_ctx);

            Free_Series(binary);
            return R_BLANK;
        }
    }
    else {
        if (
            -1 == RSA_encrypt(
                rsa_ctx,
                dataBuffer,
                data_len,
                BIN_HEAD(binary),
                REF(private) ? 1 : 0,
                padding ? 1 : 0
            )
        ){
            bi_free(rsa_ctx->bi_ctx, data_bi);
            RSA_free(rsa_ctx);

            Free_Series(binary);
            return R_BLANK;
        }
    }

    SET_SERIES_LEN(binary, binary_len);

    bi_free(rsa_ctx->bi_ctx, data_bi);
    RSA_free(rsa_ctx);

    Val_Init_Binary(D_OUT, binary);
    return R_OUT;
}


//
//  dh-generate-key: native [
//
//  "Generates a new DH private/public key pair."
//
//      return: [<opt>]
//      obj [object!]
//         "The Diffie-Hellman key object, with generator(g) and modulus(p)"
//  ]
//
REBNATIVE(dh_generate_key)
{
    PARAM(1, obj);

    DH_CTX dh_ctx;
    memset(&dh_ctx, 0, sizeof(dh_ctx));

    REBCTX *obj = VAL_CONTEXT(ARG(obj));

    REBVAL *key = CTX_KEYS_HEAD(obj);
    REBVAL *var = CTX_VARS_HEAD(obj);

    for (; NOT_END(key); ++key, ++var) {
        if (!IS_BINARY(var))
            continue; // what else would it be?

        switch (VAL_KEY_SYM(key)) {
        case SYM_P:
            dh_ctx.p = VAL_BIN_AT(var);
            dh_ctx.len = VAL_LEN_AT(var);
            break;

        case SYM_G:
            dh_ctx.g = VAL_BIN_AT(var);
            dh_ctx.glen = VAL_LEN_AT(var);
            break;
        }
    }

    if (!dh_ctx.p || !dh_ctx.g)
        return R_VOID;

    // allocate new BINARY! for private key
    //
    REBSER *priv_bin = Make_Binary(dh_ctx.len);
    dh_ctx.x = BIN_HEAD(priv_bin);
    memset(dh_ctx.x, 0, dh_ctx.len);
    SET_SERIES_LEN(priv_bin, dh_ctx.len);

    // allocate new BINARY! for public key
    //
    REBSER *pub_bin = Make_Binary(dh_ctx.len);
    dh_ctx.gx = BIN_HEAD(pub_bin);
    memset(dh_ctx.gx, 0, dh_ctx.len);
    SET_SERIES_LEN(pub_bin, dh_ctx.len);

    DH_generate_key(&dh_ctx);

    // set the object fields

    REBCNT priv_index = Find_Canon_In_Context(obj, Canon(SYM_PRIV_KEY), FALSE);
    if (priv_index == 0)
        fail (Error(RE_MISC));
    Val_Init_Binary(CTX_VAR(obj, priv_index), priv_bin);

    REBCNT pub_index = Find_Canon_In_Context(obj, Canon(SYM_PUB_KEY), FALSE);
    if (pub_index == 0)
        fail (Error(RE_MISC));
    Val_Init_Binary(CTX_VAR(obj, pub_index), pub_bin);

    return R_VOID;
}


//
//  dh-compute-key: native [
//
//  "Computes key from a private/public key pair and the peer's public key."
//
//      return: [binary!]
//          "Negotiated key"
//      obj [object!]
//          "The Diffie-Hellman key object"
//      public-key [binary!]
//          "Peer's public key"
//  ]
//
REBNATIVE(dh_compute_key)
{
    PARAM(1, obj);
    PARAM(2, public_key);

    DH_CTX dh_ctx;
    memset(&dh_ctx, 0, sizeof(dh_ctx));

    REBCTX *obj = VAL_CONTEXT(ARG(obj));

    REBVAL *key = CTX_KEYS_HEAD(obj);
    REBVAL *var = CTX_VARS_HEAD(obj);

    for (; NOT_END(key); ++key, ++var) {
        if (!IS_BINARY(var))
            continue; // what else would it be?

        switch (VAL_KEY_SYM(key)) {
        case SYM_P:
            dh_ctx.p = VAL_BIN_AT(var);
            dh_ctx.len = VAL_LEN_AT(var);
            break;

        case SYM_PRIV_KEY:
            dh_ctx.x = VAL_BIN_AT(var);
            break;
        }
    }

    dh_ctx.gy = VAL_BIN_AT(ARG(public_key));

    if (!dh_ctx.p || !dh_ctx.x || !dh_ctx.gy)
        return R_BLANK;

    REBSER *binary = Make_Binary(dh_ctx.len);
    memset(BIN_HEAD(binary), 0, dh_ctx.len);
    SET_SERIES_LEN(binary, dh_ctx.len);

    dh_ctx.k = BIN_HEAD(binary);

    DH_compute_key(&dh_ctx);

    Val_Init_Binary(D_OUT, binary);
    return R_OUT;
}


// The original Saphirion implementation used OS_ALLOC (basically malloc) to
// leave a potentially dangling memory pointer for the AES context.  Ren-C
// has "managed handles" which will clean themselves up when they are no
// longer referenced.
//
static void cleanup_aes_ctx(const REBVAL *val)
{
    assert(IS_HANDLE(val));
    assert(val->payload.handle.code == NULL);
    assert(val->payload.handle.data != NULL);

    AES_CTX *aes_ctx = cast(AES_CTX*, val->payload.handle.data);
    FREE(AES_CTX, aes_ctx);
}


//
//  aes: native [
//
//  "Encrypt/decrypt data using AES algorithm."
//
//      return: [handle! binary! logic!]
//          "Stream cipher context handle or encrypted/decrypted data."
//      /key
//          "Provided only for the first time to get stream HANDLE!"
//      crypt-key [binary!]
//          "Crypt key."
//      iv [binary! blank!]
//          "Optional initialization vector."
//      /stream
//      ctx [handle!]
//          "Stream cipher context."
//      data [binary!]
//          "Data to encrypt/decrypt."
//      /decrypt
//          "Use the crypt-key for decryption (default is to encrypt)"
//  ]
//
REBNATIVE(aes)
{
    REFINE(1, key);
    PARAM(2, crypt_key);
    PARAM(3, iv);
    REFINE(4, stream);
    PARAM(5, ctx);
    PARAM(6, data);
    REFINE(7, decrypt);
    
    if (IS_HANDLE(ARG(ctx))) {
        AES_CTX *aes_ctx = cast(AES_CTX*, VAL_HANDLE_DATA(ARG(ctx)));

        REBYTE *dataBuffer = VAL_BIN_AT(ARG(data));
        REBINT len = VAL_LEN_AT(ARG(data));

        if (len == 0)
            return R_BLANK;

        REBINT pad_len = (((len - 1) >> 4) << 4) + AES_BLOCKSIZE;

        REBYTE *pad_data;
        if (len < pad_len) {
            //
            //  make new data input with zero-padding
            //
            pad_data = ALLOC_N(REBYTE, pad_len);
            memset(pad_data, 0, pad_len);
            memcpy(pad_data, dataBuffer, len);
            dataBuffer = pad_data;
        }
        else
            pad_data = NULL;

        REBSER *binaryOut = Make_Binary(pad_len);
        memset(BIN_HEAD(binaryOut), 0, pad_len);

        if (aes_ctx->key_mode == AES_MODE_DECRYPT)
            AES_cbc_decrypt(
                aes_ctx,
                cast(const uint8_t*, dataBuffer),
                BIN_HEAD(binaryOut),
                pad_len
            );
        else
            AES_cbc_encrypt(
                aes_ctx,
                cast(const uint8_t*, dataBuffer),
                BIN_HEAD(binaryOut),
                pad_len
            );

        if (pad_data)
            FREE_N(REBYTE, pad_len, pad_data);

        SET_SERIES_LEN(binaryOut, pad_len);
        Val_Init_Binary(D_OUT, binaryOut);
        return R_OUT;
    }
    
    if (IS_BINARY(ARG(crypt_key))) {
        uint8_t iv[AES_IV_SIZE];

        if (IS_BINARY(ARG(iv))) {
            if (VAL_LEN_AT(ARG(iv)) < AES_IV_SIZE)
                return R_BLANK;

            memcpy(iv, VAL_BIN_AT(ARG(iv)), AES_IV_SIZE);
        }
        else {
            assert(IS_BLANK(ARG(iv)));
            memset(iv, 0, AES_IV_SIZE);
        }

        //key defined - setup new context

        REBINT len = VAL_LEN_AT(ARG(crypt_key)) << 3;
        if (len != 128 && len != 256)
            return R_FALSE;

        AES_CTX *aes_ctx = ALLOC_ZEROFILL(AES_CTX);

        AES_set_key(
            aes_ctx,
            cast(const uint8_t *, VAL_BIN_AT(ARG(crypt_key))),
            cast(const uint8_t *, iv),
            (len == 128) ? AES_MODE_128 : AES_MODE_256
        );

        if (REF(decrypt))
            AES_convert_key(aes_ctx);

        Init_Handle_Managed(D_OUT, NULL, aes_ctx, &cleanup_aes_ctx);
        return R_OUT;
    }

    assert(FALSE); // would have just returned first refinement state
    return R_TRUE;
}


/*
#define SEED_LEN 10
static REBYTE seed_str[SEED_LEN] = {
    249, 52, 217, 38, 207, 59, 216, 52, 222, 61 // xor "Sassenrath" #{AA55..}
};
//      kp = seed_str; // Any seed constant.
//      klen = SEED_LEN;
*/

//
//  Cloak: C
// 
// Simple data scrambler. Quality depends on the key length.
// Result is made in place (data string).
// 
// The key (kp) is passed as a REBVAL or REBYTE (when klen is !0).
//
REBOOL Cloak(
    REBOOL decode,
    REBYTE *cp,
    REBCNT dlen,
    REBYTE *kp,
    REBCNT klen,
    REBOOL as_is
) {
    REBYTE src[20];
    REBYTE dst[20];

    if (dlen == 0)
        return TRUE;

    REBCNT i;

    // Decode KEY as VALUE field (binary, string, or integer)
    if (klen == 0) {
        REBVAL *val = (REBVAL*)kp;
        REBSER *ser;

        switch (VAL_TYPE(val)) {
        case REB_BINARY:
            kp = VAL_BIN_AT(val);
            klen = VAL_LEN_AT(val);
            break;

        case REB_STRING:
            ser = Temp_Bin_Str_Managed(val, &i, &klen);
            kp = BIN_AT(ser, i);
            break;

        case REB_INTEGER:
            INT_TO_STR(VAL_INT64(val), dst);
            klen = LEN_BYTES(dst);
            as_is = FALSE;
            break;
        }

        if (klen == 0)
            return FALSE;
    }

    if (!as_is) {
        for (i = 0; i < 20; i++)
            src[i] = kp[i % klen];

        SHA1(src, 20, dst);
        klen = 20;
        kp = dst;
    }

    if (decode) {
        for (i = dlen - 1; i > 0; i--)
            cp[i] ^= cp[i - 1] ^ kp[i % klen];
    }

    // Change starting byte based all other bytes.

    REBCNT n = 0xa5;
    for (i = 1; i < dlen; i++)
        n += cp[i];

    cp[0] ^= cast(REBYTE, n);

    if (!decode) {
        for (i = 1; i < dlen; i++)
            cp[i] ^= cp[i - 1] ^ kp[i % klen];
    }

    return TRUE;
}


//
//  decloak: native [
//  
//  {Decodes a binary string scrambled previously by encloak.}
//  
//      data [binary!]
//          "Binary series to descramble (modified)"
//      key [string! binary! integer!]
//          "Encryption key or pass phrase"
//      /with
//          "Use a string! key as-is (do not generate hash)"
//  ]
//
REBNATIVE(decloak)
{
    PARAM(1, data);
    PARAM(2, key);
    REFINE(3, with);
     
    if (!Cloak(
        TRUE,
        VAL_BIN_AT(ARG(data)),
        VAL_LEN_AT(ARG(data)),
        cast(REBYTE*, ARG(key)),
        0,
        REF(with)
    )){
        fail (Error_Invalid_Arg(ARG(key)));
    }

    *D_OUT = *ARG(data);
    return R_OUT;
}


//
//  encloak: native [
//  
//  "Scrambles a binary string based on a key."
//  
//      data [binary!]
//          "Binary series to scramble (modified)"
//      key [string! binary! integer!]
//          "Encryption key or pass phrase"
//      /with
//          "Use a string! key as-is (do not generate hash)"
//  ]
//
REBNATIVE(encloak)
{
    PARAM(1, data);
    PARAM(2, key);
    REFINE(3, with);

    if (!Cloak(
        FALSE,
        VAL_BIN_AT(ARG(data)),
        VAL_LEN_AT(ARG(data)),
        cast(REBYTE*, ARG(key)),
        0,
        REF(with))
    ){
        fail (Error_Invalid_Arg(ARG(key)));
    }

    *D_OUT = *ARG(data);
    return R_OUT;
}
