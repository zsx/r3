//
//  File: %d-legacy.h
//  Summary: "Legacy Support Routines for Debug Builds"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2016 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
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
// In order to make porting code from R3-Alpha or Rebol2 easier, Ren-C set
// up several LEGACY() switches and a <r3-legacy> mode.  The switches are
// intended to only be available in debug builds, so that compatibility for
// legacy code will not be a runtime cost in the release build.  However,
// they could be enabled by any sufficiently motivated individual who
// wished to build a version of the interpreter with the old choices in an
// optimized build as well.
//
// Support routines for legacy mode are quarantined here when possible.
//

#include "sys-core.h"


#if !defined(NDEBUG)

//
//  In_Legacy_Function_Debug: C
//
// Determine if a legacy function is "in effect" currently.  To the extent
// that compatibility in debug builds or legacy mode with R3-Alpha is
// "important" this should be used sparingly, because code can be bound and
// passed around in blocks.  So you might be running a legacy function passed
// new code or new code passed legacy code (e.g. a mezzanine that uses DO)
//
REBOOL In_Legacy_Function_Debug(void)
{
    // Find the first bit of code that's actually running ordinarily in
    // the evaluator, and not just dispatching.
    //
    REBFRM *f = FS_TOP;
    if (f != NULL) {
        if (f->flags.bits & DO_FLAG_VA_LIST)
            return FALSE; // no source array to look at

        // whatever's dispatching here, there is a source array
    }

    if (f == NULL)
        return FALSE;

    // Check the flag on the source series
    //
    if (GET_SER_INFO(f->source.array, SERIES_INFO_LEGACY_DEBUG))
        return TRUE;

    return FALSE;
}


//
//  Legacy_Convert_Function_Args: C
//
// R3-Alpha and Rebol2 used BLANK for unused refinements and arguments to
// a refinement which is not present.  Ren-C uses FALSE for unused refinements
// and arguments to unused refinements are not set.
//
// Could be woven in efficiently, but as it's a debug build only feature it's
// better to isolate it into a post-phase.  This improves the readability of
// the mainline code.
//
// Trigger is when OPTIONS_REFINEMENTS_TRUE is set during function creation,
// which will give it FUNC_FLAG_LEGACY_DEBUG--leading to this being used.
//
void Legacy_Convert_Function_Args(REBFRM *f)
{
    REBVAL *param = FUNC_FACADE_HEAD(f->phase);
    REBVAL *arg = f->args_head;

    REBOOL set_blank = FALSE;

    for (; NOT_END(param); ++param, ++arg) {
        switch (VAL_PARAM_CLASS(param)) {
        case PARAM_CLASS_REFINEMENT:
            if (IS_LOGIC(arg)) {
                if (VAL_LOGIC(arg))
                    set_blank = FALSE;
                else {
                    Init_Blank(arg);
                    set_blank = TRUE;
                }
            }
            else assert(FALSE);
            break;

        case PARAM_CLASS_LOCAL:
            assert(IS_VOID(arg)); // keep *pure* locals as void, even in legacy
            break;

        case PARAM_CLASS_RETURN:
        case PARAM_CLASS_LEAVE:
            assert(IS_FUNCTION(arg) || IS_VOID(arg));
            break;

        case PARAM_CLASS_NORMAL:
        case PARAM_CLASS_HARD_QUOTE:
        case PARAM_CLASS_SOFT_QUOTE:
            if (set_blank) {
                assert(IS_VOID(arg));
                Init_Blank(arg);
            }
            break;

        default:
            assert(FALSE);
        }
    }
}

#endif
