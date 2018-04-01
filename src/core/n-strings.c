//
//  File: %n-strings.c
//  Summary: "native functions for strings"
//  Section: natives
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
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

#include "sys-core.h"
#include "sys-deci-funcs.h"

#include "sys-zlib.h"

/***********************************************************************
**
**  Hash Function Externs
**
***********************************************************************/

#if !defined(SHA_DEFINED) && defined(HAS_SHA1)
    // make-headers.r outputs a prototype already, because it is used by cloak
    // (triggers warning -Wredundant-decls)
    // REBYTE *SHA1(REBYTE *, REBCNT, REBYTE *);

    EXTERN_C void SHA1_Init(void *c);
    EXTERN_C void SHA1_Update(void *c, REBYTE *data, REBCNT len);
    EXTERN_C void SHA1_Final(REBYTE *md, void *c);
    EXTERN_C int SHA1_CtxSize(void);
#endif

#if !defined(MD5_DEFINED) && defined(HAS_MD5)
    EXTERN_C void MD5_Init(void *c);
    EXTERN_C void MD5_Update(void *c, REBYTE *data, REBCNT len);
    EXTERN_C void MD5_Final(REBYTE *md, void *c);
    EXTERN_C int MD5_CtxSize(void);
#endif

#ifdef HAS_MD4
    REBYTE *MD4(REBYTE *, REBCNT, REBYTE *);

    EXTERN_C void MD4_Init(void *c);
    EXTERN_C void MD4_Update(void *c, REBYTE *data, REBCNT len);
    EXTERN_ void MD4_Final(REBYTE *md, void *c);
    EXTERN_C int MD4_CtxSize(void);
#endif


// Table of has functions and parameters:
static struct {
    REBYTE *(*digest)(REBYTE *, REBCNT, REBYTE *);
    void (*init)(void *);
    void (*update)(void *, REBYTE *, REBCNT);
    void (*final)(REBYTE *, void *);
    int (*ctxsize)(void);
    REBSYM sym;
    REBCNT len;
    REBCNT hmacblock;
} digests[] = {

#ifdef HAS_SHA1
    {SHA1, SHA1_Init, SHA1_Update, SHA1_Final, SHA1_CtxSize, SYM_SHA1, 20, 64},
#endif

#ifdef HAS_MD4
    {MD4, MD4_Init, MD4_Update, MD4_Final, MD4_CtxSize, SYM_MD4, 16, 64},
#endif

#ifdef HAS_MD5
    {MD5, MD5_Init, MD5_Update, MD5_Final, MD5_CtxSize, SYM_MD5, 16, 64},
#endif

    {NULL, NULL, NULL, NULL, NULL, SYM_0, 0, 0}

};


//
//  delimit: native [
//
//  {Joins a block of values into a new string with delimiters.}
//
//      return: [string!]
//      block [block!]
//      delimiter [blank! char! string!]
//  ]
//
REBNATIVE(delimit)
{
    INCLUDE_PARAMS_OF_DELIMIT;

    REBVAL *block = ARG(block);
    REBVAL *delimiter = ARG(delimiter);

    if (Form_Reduce_Throws(
        D_OUT,
        VAL_ARRAY(block),
        VAL_INDEX(block),
        VAL_SPECIFIER(block),
        delimiter
    )) {
        return R_OUT_IS_THROWN;
    }

    return R_OUT;
}


//
//  spelling-of: native [
//
//  {Gives the delimiter-less spelling of words or strings}
//
//      value [any-word! any-string!]
//  ]
//
REBNATIVE(spelling_of)
{
    INCLUDE_PARAMS_OF_SPELLING_OF;

    REBVAL *value = ARG(value);

    REBSER *series;

    if (ANY_BINSTR(value)) {
        assert(!IS_BINARY(value)); // Shouldn't accept binary types...

        // Grab the data out of all string types, which has no delimiters
        // included (they are added in the forming process)
        //
        series = Copy_String_Slimming(VAL_SERIES(value), VAL_INDEX(value), -1);
    }
    else {
        // turn all words into regular words so they'll have no delimiters
        // during the FORMing process.  Use SET_TYPE and not reset header
        // because the binding bits need to stay consistent
        //
        VAL_SET_TYPE_BITS(value, REB_WORD);
        series = Copy_Mold_Value(value, MOLD_FLAG_0);
    }

    Init_String(D_OUT, series);
    return R_OUT;
}


//
//  checksum: native [
//
//  "Computes a checksum, CRC, or hash."
//
//      data [binary!]
//          "Bytes to checksum"
//      /part
//      limit
//          "Length of data"
//      /tcp
//          "Returns an Internet TCP 16-bit checksum"
//      /secure
//          "Returns a cryptographically secure checksum"
//      /hash
//          "Returns a hash value"
//      size [integer!]
//          "Size of the hash table"
//      /method
//          "Method to use"
//      word [word!]
//          "Methods: SHA1 MD5 CRC32"
//      /key
//          "Returns keyed HMAC value"
//      key-value [binary! string!]
//          "Key to use"
//  ]
//
REBNATIVE(checksum)
{
    INCLUDE_PARAMS_OF_CHECKSUM;

    REBVAL *arg = ARG(data);
    REBYTE *data = VAL_RAW_DATA_AT(arg);
    REBCNT wide = SER_WIDE(VAL_SERIES(arg));
    REBCNT len = 0;

    UNUSED(REF(part)); // checked by if limit is void
    Partial1(arg, ARG(limit), &len);

    REBSYM sym;
    if (REF(method)) {
        sym = VAL_WORD_SYM(ARG(word));
        if (sym == SYM_0) // not in %words.r, no SYM_XXX constant
            fail (Error_Invalid(ARG(word)));
    }
    else
        sym = SYM_SHA1;

    // If method, secure, or key... find matching digest:
    if (REF(method) || REF(secure) || REF(key)) {
        if (sym == SYM_CRC32) {
            if (REF(secure) || REF(key))
                fail (Error_Bad_Refines_Raw());

            // The CRC32() routine returns an unsigned 32-bit number and uses
            // the full range of values.  Yet Rebol chose to export this as
            // a signed integer via checksum.  Perhaps (?) to generate a value
            // that could also be used by Rebol2, as it only had 32-bit
            // signed INTEGER! available.
            //
            REBINT crc32 = cast(REBINT, CRC32(data, len));
            Init_Integer(D_OUT, crc32);
            return R_OUT;
        }

        if (sym == SYM_ADLER32) {
            if (REF(secure) || REF(key))
                fail (Error_Bad_Refines_Raw());

            // adler32() is a Saphirion addition since 64-bit INTEGER! was
            // available in Rebol3, and did not convert the unsigned result
            // of the adler calculation to a signed integer.
            //
            uLong adler = z_adler32(0L, data, len);
            Init_Integer(D_OUT, adler);
            return R_OUT;
        }

        REBCNT i;
        for (i = 0; i < sizeof(digests) / sizeof(digests[0]); i++) {
            if (!SAME_SYM_NONZERO(digests[i].sym, sym))
                continue;

            REBSER *digest = Make_Series(digests[i].len + 1, sizeof(char));

            if (NOT(REF(key)))
                digests[i].digest(data, len, BIN_HEAD(digest));
            else {
                REBVAL *key = ARG(key_value);

                REBCNT blocklen = digests[i].hmacblock;

                REBYTE tmpdigest[20]; // size must be max of all digest[].len

                REBSER *temp;
                REBYTE *keycp;
                REBCNT keylen;
                if (IS_BINARY(key)) {
                    temp = NULL;
                    keycp = VAL_BIN_AT(key);
                    keylen = VAL_LEN_AT(key);
                }
                else {
                    assert(IS_STRING(key));

                    REBCNT index = VAL_INDEX(key);
                    temp = Temp_UTF8_At_Managed(key, &index, &keylen);
                    PUSH_GUARD_SERIES(temp);
                    keycp = BIN_AT(temp, index);
                }

                if (keylen > blocklen) {
                    digests[i].digest(keycp, keylen, tmpdigest);
                    keycp = tmpdigest;
                    keylen = digests[i].len;
                }

                REBYTE ipad[64]; // size must be max of all digest[].hmacblock
                memset(ipad, 0, blocklen);
                memcpy(ipad, keycp, keylen);

                REBYTE opad[64]; // size must be max of all digest[].hmacblock
                memset(opad, 0, blocklen);
                memcpy(opad, keycp, keylen);

                REBCNT j;
                for (j = 0; j < blocklen; j++) {
                    ipad[j] ^= 0x36; // !!! why do people write this kind of
                    opad[j] ^= 0x5c; // thing without a comment? !!! :-(
                }

                char *ctx = ALLOC_N(char, digests[i].ctxsize());
                digests[i].init(ctx);
                digests[i].update(ctx,ipad,blocklen);
                digests[i].update(ctx, data, len);
                digests[i].final(tmpdigest,ctx);
                digests[i].init(ctx);
                digests[i].update(ctx,opad,blocklen);
                digests[i].update(ctx,tmpdigest,digests[i].len);
                digests[i].final(BIN_HEAD(digest),ctx);

                FREE_N(char, digests[i].ctxsize(), ctx);

                if (temp != NULL)
                    DROP_GUARD_SERIES(temp);
            }

            TERM_BIN_LEN(digest, digests[i].len);
            Init_Binary(D_OUT, digest);

            return R_OUT;
        }

        fail (Error_Invalid(ARG(word)));
    }
    else if (REF(tcp)) {
        REBINT ipc = Compute_IPC(data, len);
        Init_Integer(D_OUT, ipc);
    }
    else if (REF(hash)) {
        REBINT sum = VAL_INT32(ARG(size));
        if (sum <= 1)
            sum = 1;

        REBINT hash = Hash_String(data, len, wide) % sum;
        Init_Integer(D_OUT, hash);
    }
    else {
        REBINT crc = Compute_CRC(data, len);
        Init_Integer(D_OUT, crc);
    }

    return R_OUT;
}


//
//  compress: native [
//
//  "Compresses a string series and returns it."
//
//      return: [binary!]
//      data [binary! string!]
//          "If string, it will be UTF8 encoded"
//      /part
//      limit
//          "Length of data (elements)"
//      /gzip
//          "Use GZIP checksum"
//      /only
//          {Do not store header or envelope information ("raw")}
//  ]
//
REBNATIVE(compress)
{
    INCLUDE_PARAMS_OF_COMPRESS;

    REBCNT len;
    UNUSED(PAR(part)); // checked by if limit is void
    Partial1(ARG(data), ARG(limit), &len);

    REBCNT index;
    REBSER *ser = Temp_UTF8_At_Managed(ARG(data), &index, &len);

    assert(BYTE_SIZE(ser)); // must be BINARY!

    const REBOOL raw = REF(only); // use /ONLY to signal raw too?

    REBCNT out_len;
    void *compressed = rebDeflateAlloc(
        &out_len,
        BIN_AT(ser, index),
        len,
        REF(gzip),
        raw,
        REF(only)
    );

    REBVAL *bin = rebRepossess(compressed, out_len);
    Move_Value(D_OUT, bin);
    rebRelease(bin);

    return R_OUT;
}


//
//  decompress: native [
//
//  "Decompresses data."
//
//      return: [binary!]
//      data [binary!]
//          "Data to decompress"
//      /part
//      lim ;-- /limit was a legacy name for a refinement
//          "Length of compressed data (must match end marker)"
//      /gzip
//          "Use GZIP checksum"
//      /limit
//      max
//          "Error out if result is larger than this"
//      /only
//          {Do not look for header or envelope information ("raw")}
//  ]
//
REBNATIVE(decompress)
{
    INCLUDE_PARAMS_OF_DECOMPRESS;

    REBVAL *data = ARG(data);

    REBINT max;
    if (REF(limit)) {
        max = Int32s(ARG(max), 1);
        if (max < 0)
            return R_BLANK; // !!! Should negative limit be an error instead?
    }
    else
        max = -1;

    REBCNT len;
    UNUSED(REF(part)); // implied by non-void lim
    Partial1(data, ARG(lim), &len);

    // This truncation rule used to be in Decompress, which passed len
    // in as an extra parameter.  This was the only call that used it.
    //
    if (len > BIN_LEN(VAL_SERIES(data)))
        len = BIN_LEN(VAL_SERIES(data));

    const REBOOL raw = REF(only); // use /ONLY to signal raw also?
    REBCNT out_len;
    void *decompressed = rebInflateAlloc(
        &out_len,
        BIN_HEAD(VAL_SERIES(data)) + VAL_INDEX(data),
        len,
        max,
        REF(gzip),
        raw,
        REF(only)
    );

    REBVAL *bin = rebRepossess(decompressed, out_len);
    Move_Value(D_OUT, bin);
    rebRelease(bin);

    return R_OUT;
}


//
//  debase: native [
//
//  {Decodes binary-coded string (BASE-64 default) to binary value.}
//
//      return: [binary!]
//          ;-- Comment said "we don't know the encoding" of the return binary
//      value [binary! string!]
//          "The string to decode"
//      /base
//          "Binary base to use"
//      base-value [integer!]
//          "The base to convert from: 64, 16, or 2"
//  ]
//
REBNATIVE(debase)
{
    INCLUDE_PARAMS_OF_DEBASE;

    REBCNT index;
    REBCNT len = 0;
    REBSER *ser = Temp_UTF8_At_Managed(ARG(value), &index, &len);

    REBINT base = 64;
    if (REF(base))
        base = VAL_INT32(ARG(base_value));
    else
        base = 64;

    if (!Decode_Binary(D_OUT, BIN_AT(ser, index), len, base, 0))
        fail (Error_Invalid_Data_Raw(ARG(value)));

    return R_OUT;
}


//
//  enbase: native [
//
//  {Encodes data into a binary, hexadecimal, or base-64 ASCII string.}
//
//      return: [string!]
//      value [binary! string!]
//          "If string, will be UTF8 encoded"
//      /base
//          "Binary base to use (BASE-64 default)"
//      base-value [integer!]
//          "The base to convert to: 64, 16, or 2"
//  ]
//
REBNATIVE(enbase)
{
    INCLUDE_PARAMS_OF_ENBASE;

    REBINT base;
    if (REF(base))
        base = VAL_INT32(ARG(base_value));
    else
        base = 64;

    REBVAL *arg = ARG(value);

    // Will convert STRING!s to UTF-8 if necessary.
    //
    REBCNT index;
    REBSER *temp = Temp_UTF8_At_Managed(arg, &index, NULL);
    Init_Any_Series_At(arg, REB_BINARY, temp, index);

    REBSER *ser;
    const REBOOL brk = FALSE;
    switch (base) {
    case 64:
        ser = Encode_Base64(NULL, arg, brk);
        break;

    case 16:
        ser = Encode_Base16(NULL, arg, brk);
        break;

    case 2:
        ser = Encode_Base2(NULL, arg, brk);
        break;

    default:
        fail (Error_Invalid(ARG(base_value)));
    }

    Init_String(D_OUT, ser);

    return R_OUT;
}


//
//  enhex: native [
//
//  "Converts string to use URL-style hex encoding (%XX)"
//
//      return: [any-string!]
//          "See http://en.wikipedia.org/wiki/Percent-encoding"
//      string [any-string!]
//          "String to encode, all non-ASCII or illegal URL bytes encoded"
//  ]
//
REBNATIVE(enhex)
{
    INCLUDE_PARAMS_OF_ENHEX;

    // The details of what ASCII characters must be percent encoded
    // are contained in RFC 3896, but a summary is here:
    //
    // https://stackoverflow.com/a/7109208/
    //
    // Everything but: A-Z a-z 0-9 - . _ ~ : / ? # [ ] @ ! $ & ' ( ) * + , ; =
    //
  #if !defined(NDEBUG)
    const char *no_encode =
        "ABCDEFGHIJKLKMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" \
            "-._~:/?#[]@!$&'()*+,;=";
  #endif

    REBCNT len = VAL_LEN_AT(ARG(string));

    DECLARE_MOLD (mo);
    Push_Mold (mo);

    // !!! For now, we conservatively assume that the mold buffer might need
    // 12x as many characters as the input.  This is based on the worst-case
    // scenario, that each single codepoint might need 4 bytes of UTF-8 data
    // that are turned into %XX%XX%XX%XX in the output stream.
    //
    // It's not that big a deal since the mold buffer sits around with a large
    // capacity anyway, so it probably has enough for the short encodings this
    // does already.  But after the UTF-8 everywhere conversion, molding logic
    // is smarter and expands the buffer on-demand so routines like this don't
    // need to preallocate it.
    //
    Expand_Series(mo->series, mo->start, len * 12);

    REBUNI *dp = UNI_AT(mo->series, mo->start); // ^-- Expand_Series may move!

    REBSER *s = VAL_SERIES(ARG(string));

    REBCNT i = VAL_INDEX(ARG(string));
    for (; i < len; ++i) {
        REBUNI c = GET_ANY_CHAR(s, i);

        REBYTE encoded[4];
        REBCNT encoded_size;

        if (c > 0x80) // all non-ASCII characters *must* be percent encoded
            encoded_size = Encode_UTF8_Char(encoded, c);
        else {
            // "Everything else must be url-encoded".  Rebol's LEX_MAP does
            // not have a bit for this in particular, though maybe it could
            // be retooled to help more with this.  For now just use it to
            // speed things up a little.

            encoded[0] = cast(REBYTE, c);
            encoded_size = 1;

            switch (GET_LEX_CLASS(c)) {
            case LEX_CLASS_DELIMIT:
                switch (GET_LEX_VALUE(c)) {
                case LEX_DELIMIT_LEFT_PAREN:
                case LEX_DELIMIT_RIGHT_PAREN:
                case LEX_DELIMIT_LEFT_BRACKET:
                case LEX_DELIMIT_RIGHT_BRACKET:
                case LEX_DELIMIT_SLASH:
                case LEX_DELIMIT_SEMICOLON:
                    goto leave_as_is;

                case LEX_DELIMIT_SPACE: // includes control characters
                case LEX_DELIMIT_END: // 00 null terminator
                case LEX_DELIMIT_LINEFEED:
                case LEX_DELIMIT_RETURN: // e.g. ^M
                case LEX_DELIMIT_LEFT_BRACE:
                case LEX_DELIMIT_RIGHT_BRACE:
                case LEX_DELIMIT_DOUBLE_QUOTE:
                    goto needs_encoding;

                case LEX_DELIMIT_UTF8_ERROR: // not for c < 0x80
                default:
                    panic ("Internal LEX_DELIMIT table error");
                }
                goto leave_as_is;

            case LEX_CLASS_SPECIAL:
                switch (GET_LEX_VALUE(c)) {
                case LEX_SPECIAL_AT:
                case LEX_SPECIAL_COLON:
                case LEX_SPECIAL_APOSTROPHE:
                case LEX_SPECIAL_PLUS:
                case LEX_SPECIAL_MINUS:
                case LEX_SPECIAL_BLANK:
                case LEX_SPECIAL_PERIOD:
                case LEX_SPECIAL_COMMA:
                case LEX_SPECIAL_POUND:
                case LEX_SPECIAL_DOLLAR:
                    goto leave_as_is;

                default:
                    goto needs_encoding; // what is LEX_SPECIAL_WORD?
                }
                goto leave_as_is;

            case LEX_CLASS_WORD:
                if (
                    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    c == '?' || c == '!' || c == '&'
                    || c == '*' || c == '=' || c == '~'
                ){
                    goto leave_as_is; // this is all that's leftover
                }
                goto needs_encoding;

            case LEX_CLASS_NUMBER:
                goto leave_as_is; // 0-9 needs no encoding.
            }

        leave_as_is:;
          #if !defined(NDEBUG)
            assert(strchr(no_encode, c) != NULL);
          #endif
            *dp++ = c;
            continue;
        }

    needs_encoding:;
      #if !defined(NDEBUG)
        if (c < 0x80)
           assert(strchr(no_encode, c) == NULL);
      #endif

        REBCNT n;
        for (n = 0; n < encoded_size; ++n) {
            *dp++ = '%';

            // Use uppercase hex digits, per RFC 3896 2.1, which is also
            // consistent with JavaScript's encodeURIComponent()
            //
            // https://tools.ietf.org/html/rfc3986#section-2.1
            //
            *dp++ = Hex_Digits[(encoded[n] & 0xf0) >> 4];
            *dp++ = Hex_Digits[encoded[n] & 0xf];
        }
    }

    *dp = '\0';

    Init_Any_Series(
        D_OUT,
        VAL_TYPE(ARG(string)),
        Pop_Molded_String_Len(
            mo,
            cast(REBCNT, dp - UNI_AT(mo->series, mo->start)) // generated size
        )
    );

    return R_OUT;
}


//
//  dehex: native [
//
//  "Converts URL-style encoded strings, %XX is interpreted as UTF-8 byte."
//
//      return: [any-string!]
//          "Decoded string, with the same string type as the input."
//      string [any-string!]
//          "See http://en.wikipedia.org/wiki/Percent-encoding"
//  ]
//
REBNATIVE(dehex)
{
    INCLUDE_PARAMS_OF_DEHEX;

    REBCNT len = VAL_LEN_AT(ARG(string));

    DECLARE_MOLD (mo);
    Push_Mold(mo);

    // Conservatively assume no %NNs, and output is same length as input.
    //
    Expand_Series(mo->series, mo->start, len);

    REBUNI *dp = UNI_AT(mo->series, mo->start); // ^-- Expand_Series may move!

    // RFC 3986 says the encoding/decoding must use UTF-8.  This temporary
    // buffer is used to hold up to 4 bytes (and a terminator) that need
    // UTF-8 decoding--the maximum one UTF-8 encoded codepoint may have.
    //
    REBYTE scan[5];
    REBSIZ scan_size = 0;

    REBSER *s = VAL_SERIES(ARG(string));

    REBCNT i = VAL_INDEX(ARG(string));

    REBUNI c = GET_ANY_CHAR(s, i);
    while (i < len) {

        if (c != '%') {
            *dp++ = c;
            ++i;
        }
        else {
            if (i + 2 >= len)
               fail ("Percent decode has less than two codepoints after %");

            REBYTE lex1 = Lex_Map[GET_ANY_CHAR(s, i + 1)];
            REBYTE lex2 = Lex_Map[GET_ANY_CHAR(s, i + 2)];
            i += 3;

            // If class LEX_WORD or LEX_NUMBER, there is a value contained in
            // the mask which is the value of that "digit".  So A-F and
            // a-f can quickly get their numeric values.
            //
            REBYTE d1 = lex1 & LEX_VALUE;
            REBYTE d2 = lex2 & LEX_VALUE;

            if (
                lex1 < LEX_WORD || (d1 == 0 && lex1 < LEX_NUMBER)
                || lex2 < LEX_WORD || (d2 == 0 && lex2 < LEX_NUMBER)
            ){
                fail ("Percent must be followed by 2 hex digits, e.g. %XX");
            }

            // !!! We might optimize here for ASCII codepoints, but would
            // need to consider it a "flushing point" for the scan buffer,
            // in order to not gloss over incomplete UTF-8 sequences.
            //
            REBYTE b = (d1 << 4) + d2;
            scan[scan_size++] = b;
        }

        c = GET_ANY_CHAR(s, i); // may be '\0', guaranteed to be if `i == len`

        // If our scanning buffer is full (and hence should contain at *least*
        // one full codepoint) or there are no more UTF-8 bytes coming (due
        // to end of string or the next input not a %XX pattern), then try
        // to decode what we've got.
        //
        if (scan_size > 0 && (c != '%' || scan_size == 4)) {
            assert(i == len ? DID(c == '\0') : TRUE);

        decode_codepoint:
            scan[scan_size] = '\0';
            const REBYTE *next; // goto would cross initialization
            if (scan[0] < 0x80) {
                *dp = scan[0];
                next = &scan[1];
            }
            else {
                next = Back_Scan_UTF8_Char(dp, scan, &scan_size);
                if (next == NULL)
                    fail ("Bad UTF-8 sequence in %XX of dehex");
            }
            ++dp;
            --scan_size; // one less (see why it's called "Back_Scan")

            // Slide any residual UTF-8 data to the head of the buffer
            //
            REBCNT n;
            for (n = 0; n < scan_size; ++n) {
                ++next; // pre-increment (see why it's called "Back_Scan")
                scan[n] = *next;
            }

            // If we still have bytes left in the buffer and no more bytes
            // are coming, this is the last chance to decode those bytes,
            // keep going.
            //
            if (scan_size != 0 && c != '%')
                goto decode_codepoint;
        }
    }

    *dp = '\0';

    Init_Any_Series(
        D_OUT,
        VAL_TYPE(ARG(string)),
        Pop_Molded_String_Len(
            mo,
            cast(REBCNT, dp - UNI_AT(mo->series, mo->start)) // generated size
        )
    );

    return R_OUT;
}


//
//  deline: native [
//
//  {Converts string terminators to standard format, e.g. CRLF to LF.}
//
//      string [any-string!]
//          "Will be modified (unless /LINES used)"
//      /lines
//          {Return block of lines (works for LF, CR, CR-LF endings)}
//  ]
//
REBNATIVE(deline)
{
    INCLUDE_PARAMS_OF_DELINE;

    REBVAL *val = ARG(string);

    if (REF(lines)) {
        Init_Block(D_OUT, Split_Lines(val));
        return R_OUT;
    }

    REBINT len = VAL_LEN_AT(val);

    REBINT n;
    if (VAL_BYTE_SIZE(val)) {
        REBYTE *bp = VAL_BIN_AT(val);
        n = Deline_Bytes(bp, len);
    } else {
        REBUNI *up = VAL_UNI_AT(val);
        n = Deline_Uni(up, len);
    }

    SET_SERIES_LEN(VAL_SERIES(val), VAL_LEN_HEAD(val) - (len - n));

    Move_Value(D_OUT, ARG(string));
    return R_OUT;
}


//
//  enline: native [
//
//  {Converts string terminators to native OS format, e.g. LF to CRLF.}
//
//      series [any-string! block!] "(modified)"
//  ]
//
REBNATIVE(enline)
{
    INCLUDE_PARAMS_OF_ENLINE;

    REBVAL *val = ARG(series);
    REBSER *ser = VAL_SERIES(val);

    if (SER_LEN(ser)) {
        if (VAL_BYTE_SIZE(val))
            Enline_Bytes(ser, VAL_INDEX(val), VAL_LEN_AT(val));
        else
            Enline_Uni(ser, VAL_INDEX(val), VAL_LEN_AT(val));
    }

    Move_Value(D_OUT, ARG(series));
    return R_OUT;
}


//
//  entab: native [
//
//  "Converts spaces to tabs (default tab size is 4)."
//
//      string [any-string!]
//          "(modified)"
//      /size
//          "Specifies the number of spaces per tab"
//      number [integer!]
//  ]
//
REBNATIVE(entab)
{
    INCLUDE_PARAMS_OF_ENTAB;

    REBVAL *val = ARG(string);

    REBCNT len = VAL_LEN_AT(val);

    REBINT tabsize;
    if (REF(size))
        tabsize = Int32s(ARG(number), 1);
    else
        tabsize = TAB_SIZE;

    REBSER *ser;
    if (VAL_BYTE_SIZE(val))
        ser = Entab_Bytes(VAL_BIN(val), VAL_INDEX(val), len, tabsize);
    else
        ser = Entab_Unicode(VAL_UNI(val), VAL_INDEX(val), len, tabsize);

    Init_Any_Series(D_OUT, VAL_TYPE(val), ser);

    return R_OUT;
}


//
//  detab: native [
//
//  "Converts tabs to spaces (default tab size is 4)."
//
//      string [any-string!]
//          "(modified)"
//      /size
//          "Specifies the number of spaces per tab"
//      number [integer!]
//  ]
//
REBNATIVE(detab)
{
    INCLUDE_PARAMS_OF_DETAB;

    REBVAL *val = ARG(string);

    REBCNT len = VAL_LEN_AT(val);

    REBINT tabsize;
    if (REF(size))
        tabsize = Int32s(ARG(number), 1);
    else
        tabsize = TAB_SIZE;

    REBSER *ser;
    if (VAL_BYTE_SIZE(val))
        ser = Detab_Bytes(VAL_BIN(val), VAL_INDEX(val), len, tabsize);
    else
        ser = Detab_Unicode(VAL_UNI(val), VAL_INDEX(val), len, tabsize);

    Init_Any_Series(D_OUT, VAL_TYPE(val), ser);

    return R_OUT;
}


//
//  lowercase: native [
//
//  "Converts string of characters to lowercase."
//
//      string [any-string! char!]
//          "(modified if series)"
//      /part
//          "Limits to a given length or position"
//      limit [any-number! any-string!]
//  ]
//
REBNATIVE(lowercase)
{
    INCLUDE_PARAMS_OF_LOWERCASE;

    UNUSED(REF(part)); // checked by if limit is void
    Change_Case(D_OUT, ARG(string), ARG(limit), FALSE);
    return R_OUT;
}


//
//  uppercase: native [
//
//  "Converts string of characters to uppercase."
//
//      string [any-string! char!]
//          "(modified if series)"
//      /part
//          "Limits to a given length or position"
//      limit [any-number! any-string!]
//  ]
//
REBNATIVE(uppercase)
{
    INCLUDE_PARAMS_OF_UPPERCASE;

    UNUSED(REF(part)); // checked by if limit is void
    Change_Case(D_OUT, ARG(string), ARG(limit), TRUE);
    return R_OUT;
}


//
//  to-hex: native [
//
//  {Converts numeric value to a hex issue! datatype (with leading # and 0's).}
//
//      value [integer! tuple!]
//          "Value to be converted"
//      /size
//          "Specify number of hex digits in result"
//      len [integer!]
//  ]
//
REBNATIVE(to_hex)
{
    INCLUDE_PARAMS_OF_TO_HEX;

    REBVAL *arg = ARG(value);

    REBYTE buffer[(MAX_TUPLE * 2) + 4];  // largest value possible

    REBYTE *buf = &buffer[0];

    REBINT len;
    if (REF(size)) {
        len = cast(REBINT, VAL_INT64(ARG(len)));
        if (len < 0)
            fail (Error_Invalid(ARG(len)));
    }
    else
        len = -1;

    if (IS_INTEGER(arg)) {
        if (len < 0 || len > MAX_HEX_LEN)
            len = MAX_HEX_LEN;

        Form_Hex_Pad(buf, VAL_INT64(arg), len);
    }
    else if (IS_TUPLE(arg)) {
        REBINT n;
        if (
            len < 0
            || len > 2 * cast(REBINT, MAX_TUPLE)
            || len > 2 * VAL_TUPLE_LEN(arg)
        ){
            len = 2 * VAL_TUPLE_LEN(arg);
        }
        for (n = 0; n < VAL_TUPLE_LEN(arg); n++)
            buf = Form_Hex2(buf, VAL_TUPLE(arg)[n]);
        for (; n < 3; n++)
            buf = Form_Hex2(buf, 0);
        *buf = 0;
    }
    else
        fail (Error_Invalid(arg));

    if (NULL == Scan_Issue(D_OUT, &buffer[0], len))
        fail (Error_Invalid(arg));

    return R_OUT;
}


//
//  find-script: native [
//
//  {Find a script header within a binary string. Returns starting position.}
//
//      script [binary!]
//  ]
//
REBNATIVE(find_script)
{
    INCLUDE_PARAMS_OF_FIND_SCRIPT;

    REBVAL *arg = ARG(script);

    REBINT n = What_UTF(VAL_BIN_AT(arg), VAL_LEN_AT(arg));

    if (n != 0 && n != 8)
        return R_BLANK;  // UTF8 only

    if (n == 8)
        VAL_INDEX(arg) += 3;  // BOM8 length

    REBINT offset = Scan_Header(VAL_BIN_AT(arg), VAL_LEN_AT(arg));
    if (offset == -1)
        return R_BLANK;

    VAL_INDEX(arg) += offset;

    Move_Value(D_OUT, ARG(script));
    return R_OUT;
}


//
//  utf?: native [
//
//  {Returns UTF BOM (byte order marker) encoding; + for BE, - for LE.}
//
//      data [binary!]
//  ]
//
REBNATIVE(utf_q)
{
    INCLUDE_PARAMS_OF_UTF_Q;

    REBINT utf = What_UTF(VAL_BIN_AT(ARG(data)), VAL_LEN_AT(ARG(data)));
    Init_Integer(D_OUT, utf);
    return R_OUT;
}


//
//  invalid-utf8?: native [
//
//  {Checks UTF-8 encoding; if correct, returns blank else position of error.}
//
//      data [binary!]
//  ]
//
REBNATIVE(invalid_utf8_q)
{
    INCLUDE_PARAMS_OF_INVALID_UTF8_Q;

    REBVAL *arg = ARG(data);

    REBYTE *bp = Check_UTF8(VAL_BIN_AT(arg), VAL_LEN_AT(arg));
    if (bp == 0)
        return R_BLANK;

    VAL_INDEX(arg) = bp - VAL_BIN_HEAD(arg);

    Move_Value(D_OUT, arg);
    return R_OUT;
}
