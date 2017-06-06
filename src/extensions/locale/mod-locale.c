//
//  File: %mod-locale.c
//  Summary: "Native Functions for spawning and controlling processes"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
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

#ifdef TO_WINDOWS
    #include <windows.h>
#endif

// IS_ERROR might be defined in winerror.h and reb-types.h
#ifdef IS_ERROR
#undef IS_ERROR
#endif

#include "sys-core.h"
#include "sys-ext.h"

#include "tmp-mod-locale-first.h"


//
//  locale: native/export [
//      "Get locale specific information"
//      category [word!]
//          {Language: English name of the language,
//          Territory: English name of the country/region,
//          Language*: Full localized primary name of the language
//          Territory*: Full localized name of the country/region}
//  ]
//  new-words: [Language Language* Territory Territory*]
//  new-errors: [
//      invalid-category: [{Invalid locale category:} :arg1]
//  ]
//
REBNATIVE(locale)
//
{
#ifdef TO_WINDOWS
    INCLUDE_PARAMS_OF_LOCALE;
    REBSTR *cat = VAL_WORD_CANON(ARG(category));
    LCTYPE type;
    if (cat == LOCALE_WORD_LANGUAGE) {
        type = LOCALE_SENGLANGUAGE;
    } else if (cat == LOCALE_WORD_LANGUAGE_P) {
        type = LOCALE_SNATIVELANGNAME;
    } else if (cat == LOCALE_WORD_TERRITORY) {
        type = LOCALE_SENGCOUNTRY;
    } else if (cat == LOCALE_WORD_TERRITORY_P) {
        type = LOCALE_SCOUNTRY;
    } else {
        fail (Error(RE_EXT_LOCALE_INVALID_CATEGORY, ARG(category), END));
    }
    int len = GetLocaleInfo(0, type, 0, 0);
    REBSER *data = Make_Unicode(len);
    assert(sizeof(REBUNI) == sizeof(wchar_t));
    len = GetLocaleInfo(0, type, cast(wchar_t*, UNI_HEAD(data)), len);
    SET_UNI_LEN(data, len - 1);

    Init_String(D_OUT, data);

    return R_OUT;
#else
    UNUSED(frame_);
    fail ("Locale not implemented for non-windows");
#endif

}

#include "tmp-mod-locale-last.h"
