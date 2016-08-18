//
//  File: %n-native.c
//  Summary: "native functions to implement user natives"
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

#include "sys-core.h"

#if defined(WITH_TCC)
#include "libtcc.h"

extern const REBYTE core_header_source[];
extern const void *rebol_symbols[];

#define CHAR_HEAD(x) cs_cast(BIN_HEAD(x))

static void tcc_error_report(void *ignored, const char *msg)
{
    REBVAL err;
    REBSER *ser = Make_Binary(strlen(msg) + 2);
    Append_Series(ser, cb_cast(msg), strlen(msg));
    Val_Init_String(&err, ser);
    fail(Error(RE_TCC_ERROR_WARN, &err));
}

static int do_add_path(TCCState *state, const RELVAL *path,
                       int (*add) (TCCState *, const char *))
{
    int ret;
    if (!VAL_BYTE_SIZE(path))
        return -1;
    if (IS_FILE(path)) {
        REBSER *lp, *bin;
        lp = Value_To_Local_Path(KNOWN(m_cast(RELVAL*,path)), TRUE);
        bin = Make_UTF8_Binary(UNI_HEAD(lp), SER_LEN(lp), 2, OPT_ENC_UNISRC);
        Free_Series(lp);
        assert(SER_WIDE(bin) == 1);
        ret = add(state, CHAR_HEAD(bin));
        Free_Series(bin);
    }
    else { //ANY_STRING
        ret = add(state, CHAR_HEAD(VAL_SERIES(path)));
    }
    return ret;
}

static void do_set_path(TCCState *state, const RELVAL *path,
                        void (*set) (TCCState *, const char *))
{
    if (!VAL_BYTE_SIZE(path))
        return;
    if (IS_FILE(path)) {
        REBSER *lp, *bin;
        lp = Value_To_Local_Path(KNOWN(m_cast(RELVAL*, path)), TRUE);
        bin = Make_UTF8_Binary(UNI_HEAD(lp), SER_LEN(lp), 2, OPT_ENC_UNISRC);
        Free_Series(lp);
        assert(SER_WIDE(bin) == 1);
        set(state, CHAR_HEAD(bin));
        Free_Series(bin);
    }
    else { //ANY_STRING
        set(state, CHAR_HEAD(VAL_SERIES(path)));
    }
}

static REBCTX* add_path(TCCState *state, const RELVAL *path,
                        int(*add) (TCCState *, const char *),
                        enum REBOL_Errors err_code)
{
    if (path) {
        RELVAL *item;
        if (IS_FILE(path) || IS_STRING(path)) {
            if (do_add_path(state, path, add) < 0) {
                return Error(err_code, path);
            }
        }
        else { /* must be a block */
            for (item = VAL_ARRAY_AT(path); NOT_END(item); ++item) {
                if (IS_FILE(item) || ANY_STRING(item)) {
                    if (do_add_path(state, item, add) < 0) {
                        return Error(err_code, item);
                    }
                }
                else {
                    return Error(err_code, item);
                }
            }
        }
    }

    return NULL;
}

static void cleanup(const REBVAL *val)
{
    assert(IS_HANDLE(val));
    assert(val->payload.handle.code == NULL);
    assert(val->payload.handle.data != NULL);
    tcc_delete(cast(TCCState*, val->payload.handle.data));
}

#endif

//
//  make-native: native [
//  
//  {Parse the spec and create user native}
//      specs [block!] {
//              Pair of [name spec] that are in the form of:
//              name [any-string!] {C function name that implements this native, in the form of "N_xxx"} 
//              spec [block!] "The spec of the native"
//          }
//      source [any-string!] "C source of the native implementation"
//      /opt
//      flags [block!]
//      {
//          The block supports the following dialect:
//          include [block! path!] "include path"
//          define [block!] {define preprocessor symbols, in the form of "VAR=VAL" or "VAR"}
//          debug "Add debuging information to the generated code?"
//      }
//  ]
//
REBNATIVE(make_native)
{
#if !defined(WITH_TCC)
    fail(Error(RE_NOT_TCC_BUILD));
#else
    PARAM(1, specs);
    PARAM(2, source);
    REFINE(3, opt);
    PARAM(4, flags);

    TCCState *TCC_state = NULL;

    RELVAL *spec = NULL, *inc = NULL, *lib = NULL, *libdir = NULL,
           *options=NULL, *rundir = NULL;
    const char* c_src = CHAR_HEAD(VAL_SERIES(ARG(source)));
    REBOOL debug = FALSE; //FIXME: not implemented yet
    REBCTX * err = NULL;
    RELVAL *item;
    REBARR *natives;
    const void **sym = &rebol_symbols[0];
    REBCNT head_len;
    REBCNT src_len;
    REBSER *combined_src;
    const char *prolog = "\n# 0 \"user-source\" 1\n";
    REBARR *singular;
    RELVAL *v;

    if (REF(opt)) {
        RELVAL *val = VAL_ARRAY_AT(ARG(flags));

        for (; NOT_END(val); ++val) {
            if (IS_WORD(val)) {
                switch (VAL_WORD_SYM(val)) {
                    case SYM_INCLUDE:
                        ++val;
                        if (!(IS_BLOCK(val) || IS_FILE(val) || ANY_STRING(val))) {
                            fail(Error(RE_TCC_INVALID_INCLUDE, val));
                        }
                        inc = val;
                        break;
                    case SYM_DEBUG:
                        debug = TRUE;
                        break;
                    case SYM_OPTIONS:
                        ++val;
                        if (!ANY_STRING(val) || !VAL_BYTE_SIZE(val)) {
                            fail(Error(RE_TCC_INVALID_OPTIONS, val));
                        }
                        options = val;
                        break;
                    case SYM_RUNTIME_PATH:
                        ++val;
                        if (!(IS_FILE(val) || ANY_STRING(val))) {
                            fail(Error(RE_TCC_INVALID_LIBRARY_PATH, val));
                        }
                        rundir = val;
                        break;
                    case SYM_LIBRARY_PATH:
                        ++val;
                        if (!(IS_BLOCK(val) || IS_FILE(val) || ANY_STRING(val))) {
                            fail(Error(RE_TCC_INVALID_LIBRARY_PATH, val));
                        }
                        libdir = val;
                        break;
                    case SYM_LIBRARY:
                        ++val;
                        if (!(IS_BLOCK(val) || IS_FILE(val) || ANY_STRING(val))) {
                            fail(Error(RE_TCC_INVALID_LIBRARY, val));
                        }
                        lib = val;
                        break;
                    default:
                        fail(Error(RE_TCC_NOT_SUPPORTED_OPT, val));
                }
            } else {
                fail(Error(RE_TCC_EXPECT_WORD, val));
            }
        }
    }

    // sanity check
    if (VAL_LEN_AT(ARG(specs)) == 0)
        fail(Error(RE_TCC_EMPTY_SPEC));
    if (VAL_LEN_AT(ARG(specs)) % 2 != 0) // specs must be [name spec] pairs
        fail(Error(RE_TCC_INVALID_SPEC_LENGTH, ARG(specs)));

    if (VAL_LEN_AT(ARG(source)) == 0)
        fail(Error(RE_TCC_EMPTY_SOURCE));

    head_len = strlen(cs_cast(core_header_source));
    src_len = strlen(c_src);
    combined_src = Make_Series(src_len + head_len + strlen(prolog) + 1, 1, MKS_NONE);
    Append_Series(combined_src, core_header_source, head_len);
    Append_Series(combined_src, cb_cast(prolog), strlen(prolog));
    Append_Series(combined_src, cb_cast(c_src), src_len + 1); // +1 for the string terminator

    TCC_state = tcc_new();
    if (!TCC_state) {
        fail(Error(RE_TCC_CONSTRUCTION));
    }

    singular = Alloc_Singular_Array();
    ARR_SERIES(singular)->misc.cleaner = cleanup;

    v = ARR_HEAD(singular);
    VAL_RESET_HEADER(v, REB_HANDLE);
    v->extra.singular = singular;
    v->payload.handle.code = NULL;
    v->payload.handle.data = TCC_state;

    MANAGE_ARRAY(singular);

    tcc_set_error_func(TCC_state, NULL, tcc_error_report);

    if (options) {
        if (tcc_set_options(TCC_state, CHAR_HEAD(VAL_SERIES(options))) < 0) {
            fail(Error(RE_TCC_SET_OPTIONS));
        }
    }

    if ((err = add_path(TCC_state, inc, tcc_add_include_path, RE_TCC_INCLUDE))) {
        fail(err);
    }

    if (tcc_set_output_type(TCC_state, TCC_OUTPUT_MEMORY) < 0) {
        fail(Error(RE_TCC_OUTPUT_TYPE));
    }
    if (tcc_compile_string(TCC_state, CHAR_HEAD(combined_src)) < 0) {
        fail(Error(RE_TCC_COMPILE, ARG(source)));
    }
    Free_Series(combined_src);

#if defined(TO_WINDOWS)
    for (; *sym != NULL; sym += 2) {
        if (tcc_add_symbol(TCC_state, cast(char*, *sym), *(sym + 1)) < 0) {
            fail(Error(RE_TCC_RELOCATE));
        }
    }
#endif

    if ((err = add_path(TCC_state, libdir, tcc_add_library_path, RE_TCC_LIBRARY_PATH))) {
        fail(err);
    }
    if ((err = add_path(TCC_state, lib, tcc_add_library, RE_TCC_LIBRARY))) {
        fail(err);
    }

    if (rundir) {
        do_set_path(TCC_state, rundir, tcc_set_lib_path);
    }

    if (tcc_relocate(TCC_state, TCC_RELOCATE_AUTO) < 0) {
        fail(Error(RE_TCC_RELOCATE));
    }
    natives = Make_Array(VAL_LEN_AT(ARG(specs)) / 2);

    for (item = VAL_ARRAY_AT(ARG(specs)); NOT_END(item); ++item) {
        if (ANY_STRING(item)) {
            const char* c_name = CHAR_HEAD(VAL_SERIES(item));
            if (IS_BLOCK(++item)) {
                REBFUN *fun;
                RELVAL *body;
                REBNAT c_func = cast(REBNAT, tcc_get_symbol(TCC_state, c_name));
                if (!c_func) {
                    fail(Error(RE_TCC_SYM_NOT_FOUND, (item - 1)));
                }

                fun = Make_Function(
                    Make_Paramlist_Managed_May_Fail(KNOWN(item), 0),
                    c_func, // "dispatcher" is unique to this "native"
                    NULL // no underlying function, this is fundamental
                );
                Append_Value(natives, FUNC_VALUE(fun));
                body = FUNC_BODY(fun);
                VAL_RESET_HEADER(body, REB_HANDLE);
                body->extra.singular = singular;
            }
            else {
                fail(Error(RE_MALCONSTRUCT, item));
            }
        }
        else {
            fail(Error(RE_TCC_INVALID_NAME, item));
        }
    }
    Val_Init_Block(D_OUT, natives);

    return R_OUT;
#endif
}
