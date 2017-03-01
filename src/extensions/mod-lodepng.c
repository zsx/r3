//
//  File: %mod-lodepng.c
//  Summary: "Native Functions for cryptography"
//  Section: Extension
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
// The original cryptography additions to Rebol were done by Saphirion, at
// a time prior to Rebol's open sourcing.  They had to go through a brittle,
// incomplete, and difficult to read API for extending the interpreter with
// C code.
//
// This contains a simplification of %host-core.c, written directly to the
// native API.  It also includes the longstanding (but not standard, and not
// particularly secure) ENCLOAK and DECLOAK operations from R3-Alpha.
//

#include "png/lodepng.h"

#include "sys-core.h"
#include "sys-ext.h"

#include "tmp-mod-lodepng-first.h"

// !!! This didn't really have anywhere to go.  It came from %host-core.c,
// and it's not part of the historical PNG code, but apparently Saphirion
// found a problem with that in terms of saving (saving only?) which they
// added in lodepng for.  This is unfortunate as lodepng repeats deflate
// code already available in Zlib.
//
// It is used as an override for the encoder from R3-Alpha, which is found
// in %u-png.c as ENCODE-PNG.

//
//  encode-png-lodepng: native [
//
//  {Codec for encoding a PNG image (via LODEPNG, plain ENCODE-PNG is buggy)}
//
//      return: [binary!]
//      image [image!]
// ]
//
REBNATIVE(encode_png_lodepng)
{
    INCLUDE_PARAMS_OF_ENCODE_PNG_LODEPNG;

    REBVAL *image = ARG(image);

    LodePNGState state;
    lodepng_state_init(&state);

    // "disable autopilot"
    state.encoder.auto_convert = LAC_NO;

    // input format
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;

    // output format
    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = 8;

    size_t buffersize;
    REBYTE *buffer = NULL;

    REBINT w = VAL_IMAGE_WIDE(image);
    REBINT h = VAL_IMAGE_HIGH(image);

    unsigned error = lodepng_encode(
        &buffer, // freed with free()...so must be allocated via malloc() ?
        &buffersize,
        SER_DATA_RAW(VAL_SERIES(image)),
        w,
        h,
        &state
    );

    lodepng_state_cleanup(&state);

    if (error != 0) {
        if (buffer != NULL) free(buffer);
        return R_BLANK;
    }

    REBSER *binary = Make_Binary(buffersize);
    memcpy(SER_DATA_RAW(binary), buffer, buffersize);
    SET_SERIES_LEN(binary, buffersize);
    free(buffer);

    Init_Binary(D_OUT, binary);
    return R_OUT;
}

#include "tmp-mod-lodepng-last.h"
