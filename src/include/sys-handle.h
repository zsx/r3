//
//  File: %sys-handle.h
//  Summary: "Definitions for GC-able and non-GC-able Handles"
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
// In Rebol terminology, a HANDLE! is a pointer to a function or data that
// represents an arbitrary external resource.  While such data could also
// be encoded as a BINARY! "blob" (as it might be in XML), the HANDLE! type
// is intentionally "opaque" to user code so that it is a black box.
//
// In the C language, sizeof(void*) may not be the same size as a function
// pointer; hence they can't necessarily be cast between each other.  Since
// there is room for it, each HANDLE! in Ren-C is able to store one of each:
// a code pointer and data pointer.  Or it may use only one or the other.
//
// Additionally, Ren-C added the idea of a garbage collector callback for
// "Managed" handles.  This is implemented by means of making the handle cost
// a single REBSER node shared among its instances, which is a "singular"
// Array containing a canon value of the handle itself.  When there are no
// references left to the handle and the GC runs, it will run a hook stored
// in the ->misc field of the singular array.
//
// As an added benefit of the Managed form, the code and data pointers in the
// value itself are not used; instead preferring the data held in the REBARR.
// This allows one instance of a managed handle to have its code or data
// pointer changed and be reflected in all instances.  The simple form of
// handle however is such that each REBVAL copied instance is independent,
// and changing one won't change the others.
//

inline static CFUNC *VAL_HANDLE_CODE(const RELVAL *v) {
    assert(IS_HANDLE(v));
    if (v->extra.singular)
        return ARR_HEAD(v->extra.singular)->payload.handle.code;
    else
        return v->payload.handle.code;
}

inline static void *VAL_HANDLE_DATA(const RELVAL *v) {
    assert(IS_HANDLE(v));
    if (v->extra.singular)
        return ARR_HEAD(v->extra.singular)->payload.handle.data;
    else
        return v->payload.handle.data;
}

inline static void SET_HANDLE_CODE(RELVAL *v, CFUNC *code) {
    assert(IS_HANDLE(v));
    if (v->extra.singular)
        ARR_HEAD(v->extra.singular)->payload.handle.code = code;
    else
        v->payload.handle.code = code;
}

inline static void SET_HANDLE_DATA(RELVAL *v, void *data) {
    assert(IS_HANDLE(v));
    if (v->extra.singular)
        ARR_HEAD(v->extra.singular)->payload.handle.data = data;
    else
        v->payload.handle.data = data;
}

inline static void Init_Handle_Simple(
    RELVAL *out,
    CFUNC *code,
    void *data
){
    VAL_RESET_HEADER(out, REB_HANDLE);
    out->extra.singular = NULL;
    out->payload.handle.code = code;
    out->payload.handle.data = data;
}

inline static void Init_Handle_Managed(
    RELVAL *out,
    CFUNC *code,
    void *data,
    CLEANUP_FUNC cleaner
){
    REBARR *singular = Alloc_Singular_Array();
    ARR_SERIES(singular)->misc.cleaner = cleaner;

    RELVAL *v = ARR_HEAD(singular);
    VAL_RESET_HEADER(v, REB_HANDLE);
    v->extra.singular = singular;
    v->payload.handle.code = code;
    v->payload.handle.data = data;

    MANAGE_ARRAY(singular);

    // Don't fill the handle properties in the instance if it's the managed
    // form.  This way, you can set the properties in the canon value and
    // effectively update all instances...since the bits live in the shared
    // series component.
    //
    SET_TRASH_IF_DEBUG(out);
    VAL_RESET_HEADER(out, REB_HANDLE);
    out->extra.singular = singular;
}
