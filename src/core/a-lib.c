//
//  File: %a-lib.c
//  Summary: "Lightweight Export API (REBVAL as opaque type)"
//  Section: environment
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
// This is the "external" API, and %reb-lib.h contains its exported
// definitions.  That file (and %make-reb-lib.r which generates it) contains
// comments and notes which will help understand it.
//
// What characterizes the external API is that it is not necessary to #include
// the extensive definitions of `struct REBSER` or the APIs for dealing with
// all the internal details (e.g. PUSH_GUARD_SERIES(), which are easy to get
// wrong).  Not only does this simplify the interface, but it also means that
// the C code using the library isn't competing as much for definitions in
// the global namespace.
//
// (That was true of the original RL_API in R3-Alpha, but this later iteration
// speaks in terms of actual REBVAL* cells--vs. creating a new type.  They are
// just opaque pointers to cells whose lifetime is governed by the core.)
//
// Each exported routine here has a name RL_rebXxxYyy.  This is a name by
// which it can be called internally from the codebase like any other function
// that is part of the core.  However, macros for calling it from the core
// are given as `#define rebXxxYyy RL_rebXxxYyy`.  This is a little bit nicer
// and consistent with the way it looks when an external client calls the
// functions.
//
// Then extension clients use macros which have you call the functions through
// a struct-based "interface" (similar to the way that interfaces work in
// something like COM).  Here the macros merely pick the API functions through
// a table, e.g. `#define rebXxxYyy interface_struct->rebXxxYyy`.  This means
// paying a slight performance penalty to dereference that API per call, but
// it keeps API clients from depending on the conventional C linker...so that
// DLLs can be "linked" against a Rebol EXE.
//
// (It is not generically possible to export symbols from an executable, and
// just in general there's no cross-platform assurances about how linking
// works, so this provides the most flexibility.)
//

#include "sys-core.h"
#include "mem-series.h" // needed for SER_SET_BIAS in rebRepossess()


// "Linkage back to HOST functions. Needed when we compile as a DLL
// in order to use the OS_* macro functions."
//
#ifdef REB_API  // Included by C command line
    const REBOL_HOST_LIB *Host_Lib = NULL;
    EXTERN_C REBOL_HOST_LIB Host_Lib_Init;
#endif


static REBRXT Reb_To_RXT[REB_MAX];
static enum Reb_Kind RXT_To_Reb[RXT_MAX];


// !!! Review how much checking one wants to do when calling API routines,
// and what the balance should be of debug vs. release.  Right now, this helps
// in particular notice if the core tries to use an API function before the
// proper moment in the boot.
//
inline static void Enter_Api(void) {
    if (Host_Lib == NULL)
        panic ("rebStartup() not called before API call");
}


//=//// API VALUE ALLOCATION //////////////////////////////////////////////=//
//
// API REBVALs live in "pairings", but aren't kept alive by references from
// other values (the way that a pairing used by a PAIR! is kept alive by its
// references in any array elements or other cells referring to that pairing).
// The API value content is in the paired cell, with the PAIRING_KEY() serving
// as a kind of "meta value" (e.g. a FRAME! that controls the lifetime of
// the API value, or an INTEGER! reference count, etc.)
//
// The long term goal is to have diverse and sophisticated memory management
// options for API handles.  They would default to automatic GC when the
// current function frame goes away, but also permit manual freeing.  At the
// moment they default to manual (hence leak if not `rebRelease()`d) and have
// to be converted to be GC managed with `rebManage()`.
//
// Manual release will always be necessary in *some* places, such as the
// console code: since it is top-level its "owning frame" never goes away.
// So for the moment, it doesn't hurt to be doing more solid bookkeeping with
// a manual default to notice any leaking patterns in the core.
//

// What distinguishes an API value is that it has both the NODE_FLAG_CELL and
// NODE_FLAG_ROOT bits set.
//
inline static REBOOL Is_Api_Value(const RELVAL *v) {
    assert(v->header.bits & NODE_FLAG_CELL);
    return LOGICAL(v->header.bits & NODE_FLAG_ROOT);
}

// !!! The return cell from this allocation is a trash cell which has had some
// additional bits set.  This means it is not "canonized" trash that can be
// detected as distinct from UTF-8 strings, so don't call IS_TRASH_DEBUG() or
// Detect_Rebol_Pointer() on it until it has been further initialized.
//
inline static REBVAL *Alloc_Value(void)
{
    REBVAL *paired = Alloc_Pairing();

    // Note the cell is trash (we're only allocating) so we can't use the
    // SET_VAL_FLAGS macro.
    //
    paired->header.bits |= NODE_FLAG_ROOT;

    Init_Blank(PAIRING_KEY(paired)); // the meta-value of the API handle

    return paired;
}

inline static void Free_Value(REBVAL *v)
{
    assert(Is_Api_Value(v));
    Free_Pairing(v);
}


//=//// SERIES-BACKED ALLOCATORS //////////////////////////////////////////=//
//
// These are replacements for malloc(), realloc(), and free() which use a
// byte-sized REBSER as the backing store for the data.
//
// One benefit of using a series is that it offers more options for automatic
// memory management (such as being freed in case of a fail(), vs. leaked as
// a malloc() would, or perhaps being GC'd when a particular FRAME! ends).
//
// It also has the benefit of helping interface with client code that has
// been stylized to use malloc()-ish hooks to produce data, when the eventual
// target of that data is a Rebol series.  It does this without exposing
// REBSER* internals to the external API, by allowing one to "rebRepossess()"
// the underlying series as a BINARY! REBVAL*.
//


//
//  rebMalloc: RL_API
//
// * Like plain malloc(), if size is zero, the implementation just has to
//   return something that free() will take.  (We pick NULL.)
//
// * Unlike plain malloc(), this never returns NULL.  It will fail() instead.
//
// * It tries to be like malloc() by giving back a pointer "suitably aligned
//   for the size of any fundamental type".  See notes on ALIGN_SIZE.
//
// !!! rebMallocAlign() could exist to take an alignment, which could save
// on wasted bytes when ALIGN_SIZE > sizeof(REBSER*)...or work with "weird"
// large fundamental types that need more alignment than ALIGN_SIZE.
//
void *RL_rebMalloc(size_t size)
{
    Enter_Api();

    if (size == 0)
        return NULL; // 0 case is "implementation-defined behavior"

    REBSER *s = Make_Series_Core(
        ALIGN_SIZE // stores REBSER* (must be at least big enough for void*)
            + size // for the actual data capacity
            + 1, // for termination (even BINARY! has this, review necessity)
        sizeof(REBYTE), // Rebserize() only creates binary series ATM
        SERIES_FLAG_DONT_RELOCATE // direct data pointer is being handed back!
    );

    REBYTE *ptr = BIN_HEAD(s) + ALIGN_SIZE;
    *(cast(REBSER**, ptr) - 1) = s; // save self in bytes *right before* data
    TERM_BIN_LEN(s, ALIGN_SIZE + size); // uninitialized data to start

    POISON_MEMORY(BIN_HEAD(s), ALIGN_SIZE); // let ASAN catch underruns

    // !!! The data is uninitialized, and if it is turned into a BINARY! via
    // rebRepossess() before all bytes are assigned initialized, it could be
    // worse than just random data...MOLDing such a binary and reading those
    // bytes could be bad (due to, for instance, "trap representations"):
    //
    // https://stackoverflow.com/a/37184840
    //
    // It may be that rebMalloc() and rebRealloc() should initialize with 0
    // in the release build to defend against that, but doing so in the debug
    // build would keep address sanitizer from noticing when memory was not
    // initialized.

    return ptr;
}


//
//  rebRealloc: RL_API
//
// * Like plain realloc(), NULL is legal for ptr
//
// * Like plain realloc(), it preserves the lesser of the old data range or
//   the new data range, and memory usage drops if new_size is smaller:
//
// https://stackoverflow.com/a/9575348
//
// * Unlike plain realloc(), this fails instead of returning NULL, hence it is
//   safe to say `ptr = rebRealloc(ptr, new_size)`
//
void *RL_rebRealloc(void *ptr, size_t new_size)
{
    Enter_Api();

    assert(new_size > 0); // realloc() deprecated this as of C11 DR 400

    if (ptr == NULL) // C realloc() accepts NULL
        return rebMalloc(new_size);

    REBYTE *bin_head = cast(REBYTE*, ptr) - ALIGN_SIZE;
    UNPOISON_MEMORY(bin_head, ALIGN_SIZE); // need to underrun to fetch `s`
    UNUSED(bin_head);

    REBSER *old = *(cast(REBSER**, ptr) - 1);

    REBCNT old_size = BIN_LEN(old) - ALIGN_SIZE;
    void *reallocated = rebMalloc(new_size);
    memcpy(reallocated, ptr, old_size < new_size ? old_size : new_size);

    Free_Series(old); // asserts that `old` is unmanaged

    return reallocated;
}


//
//  rebFree: RL_API
//
// * As with free(), NULL is accepted as a no-op.
//
void RL_rebFree(void *ptr)
{
    Enter_Api();

    if (ptr == NULL)
        return;

    REBYTE *bin_head = cast(REBYTE*, ptr) - ALIGN_SIZE;
    UNPOISON_MEMORY(bin_head, ALIGN_SIZE); // need to underrun to see `s`
    UNUSED(bin_head);

    REBSER *s = *(cast(REBSER**, ptr) - 1);
    assert(BYTE_SIZE(s));

    Free_Series(s); // asserts that `s` is unmanaged
}


//
//  rebRepossess: RL_API
//
// Alternative to rebFree() is to take over the underlying series as a
// BINARY!.  The old void* should not be used after the transition, as this
// operation makes the series underlying the memory subject to relocation.
//
// If the passed in size is less than the size with which the series was
// allocated, the overage will be treated as unused series capacity.
//
// !!! All bytes in the allocation are expected to be initialized by this
// point, as failure to do so will mean reads crash the interpreter.  See
// remarks in rebMalloc() about the issue, and possibly doing zero fills.
//
// !!! It might seem tempting to use (BIN_LEN(s) - ALIGN_SIZE).  However,
// some routines make allocations bigger than they ultimately need and do not
// realloc() before converting the memory to a series...rebInflate() and
// rebDeflate() do this.  So a version passing the size will be necessary,
// and since C does not have the size exposed in malloc() and you track it
// yourself, it seems fair to *always* ask the caller to pass in a size.
//
REBVAL *RL_rebRepossess(void *ptr, REBCNT size)
{
    Enter_Api();

    REBYTE *bin_head = cast(REBYTE*, ptr) - ALIGN_SIZE;
    UNPOISON_MEMORY(bin_head, ALIGN_SIZE);
    UNUSED(bin_head);

    REBSER **ps = cast(REBSER**, ptr) - 1;
    REBSER *s = *ps;
    TRASH_POINTER_IF_DEBUG(ps);

    assert(NOT(IS_SERIES_MANAGED(s)));
    assert(size <= BIN_LEN(s) - ALIGN_SIZE);

    assert(GET_SER_FLAG(s, SERIES_FLAG_DONT_RELOCATE));
    CLEAR_SER_FLAG(s, SERIES_FLAG_DONT_RELOCATE);

    if (GET_SER_INFO(s, SERIES_INFO_HAS_DYNAMIC)) {
        //
        // Dynamic series have the concept of a "bias", which is unused
        // allocated capacity at the head of a series.  Bump the "bias" to
        // treat the embedded REBSER* (aligned to REBI64) as unused capacity.
        //
        SER_SET_BIAS(s, ALIGN_SIZE);
        s->content.dynamic.data += ALIGN_SIZE;
        s->content.dynamic.rest -= ALIGN_SIZE;
    }
    else {
        // Data is in REBSER node itself, no bias.  Just slide the bytes down.
        //
        memmove( // src overlaps destination, can't use memcpy()
            BIN_HEAD(s),
            BIN_HEAD(s) + ALIGN_SIZE,
            size
        );
    }

    TERM_BIN_LEN(s, size);
    return Init_Binary(Alloc_Value(), s);
}



//
//  Startup_Api: C
//
// RL_API routines may be used by extensions (which are invoked by a fully
// initialized Rebol core) or by normal linkage (such as from within the core
// itself).  A call to rebInit() won't be needed in the former case.  So
// setup code that is needed to interact with the API needs to be done by the
// core independently.
//
void Startup_Api(void)
{
    // These tables used to be built by overcomplicated Rebol scripts.  It's
    // less hassle to have them built on initialization.

    REBCNT n;
    for (n = 0; n < REB_MAX; ++n)
        Reb_To_RXT[n] = 0;

    // The values are deliberately skewed from REB_XXX based on the idea that
    // these values are not connected.  +10 from the Rebol counterparts at
    // time of writing.
    //
    Reb_To_RXT[REB_0] = 255; // REB_0 is internal use only
    Reb_To_RXT[REB_FUNCTION] = RXT_FUNCTION;
    Reb_To_RXT[REB_WORD] = RXT_WORD;
    Reb_To_RXT[REB_SET_WORD] = RXT_SET_WORD;
    Reb_To_RXT[REB_GET_WORD] = RXT_GET_WORD;
    Reb_To_RXT[REB_LIT_WORD] = RXT_LIT_WORD;
    Reb_To_RXT[REB_REFINEMENT] = RXT_REFINEMENT;
    Reb_To_RXT[REB_ISSUE] = RXT_ISSUE;
    Reb_To_RXT[REB_PATH] = RXT_PATH;
    Reb_To_RXT[REB_SET_PATH] = RXT_SET_PATH;
    Reb_To_RXT[REB_GET_PATH] = RXT_GET_PATH;
    Reb_To_RXT[REB_LIT_PATH] = RXT_LIT_PATH;
    Reb_To_RXT[REB_GROUP] = RXT_GROUP;
    Reb_To_RXT[REB_BLOCK] = RXT_BLOCK;
    Reb_To_RXT[REB_BINARY] = RXT_BINARY;
    Reb_To_RXT[REB_STRING] = RXT_STRING;
    Reb_To_RXT[REB_FILE] = RXT_FILE;
    Reb_To_RXT[REB_EMAIL] = RXT_EMAIL;
    Reb_To_RXT[REB_URL] = RXT_URL;
    Reb_To_RXT[REB_TAG] = RXT_TAG;
    Reb_To_RXT[REB_BITSET] = RXT_BITSET;
    Reb_To_RXT[REB_IMAGE] = RXT_IMAGE;
    Reb_To_RXT[REB_VECTOR] = RXT_VECTOR;
    Reb_To_RXT[REB_MAP] = RXT_MAP;
    Reb_To_RXT[REB_VARARGS] = RXT_VARARGS;
    Reb_To_RXT[REB_OBJECT] = RXT_OBJECT;
    Reb_To_RXT[REB_FRAME] = RXT_FRAME;
    Reb_To_RXT[REB_MODULE] = RXT_MODULE;
    Reb_To_RXT[REB_ERROR] = RXT_ERROR;
    Reb_To_RXT[REB_PORT] = RXT_PORT;
    Reb_To_RXT[REB_BAR] = RXT_BAR;
    Reb_To_RXT[REB_LIT_BAR] = RXT_LIT_BAR;
    Reb_To_RXT[REB_BLANK] = RXT_BLANK;
    Reb_To_RXT[REB_LOGIC] = RXT_LOGIC;
    Reb_To_RXT[REB_INTEGER] = RXT_INTEGER;
    Reb_To_RXT[REB_DECIMAL] = RXT_DECIMAL;
    Reb_To_RXT[REB_PERCENT] = RXT_PERCENT;
    Reb_To_RXT[REB_MONEY] = RXT_MONEY;
    Reb_To_RXT[REB_CHAR] = RXT_CHAR;
    Reb_To_RXT[REB_PAIR] = RXT_PAIR;
    Reb_To_RXT[REB_TUPLE] = RXT_TUPLE;
    Reb_To_RXT[REB_TIME] = RXT_TIME;
    Reb_To_RXT[REB_DATE] = RXT_DATE;
    Reb_To_RXT[REB_DATATYPE] = RXT_DATATYPE;
    Reb_To_RXT[REB_TYPESET] = RXT_TYPESET;
    Reb_To_RXT[REB_GOB] = RXT_GOB;
    Reb_To_RXT[REB_EVENT] = RXT_EVENT;
    Reb_To_RXT[REB_HANDLE] = RXT_HANDLE;
    Reb_To_RXT[REB_STRUCT] = RXT_STRUCT;
    Reb_To_RXT[REB_LIBRARY] = RXT_LIBRARY;

  #if !defined(NDEBUG)
    for (n = 1; n < REB_MAX; ++n)
        assert(Reb_To_RXT[n] != 0); // make sure all have a value
  #endif

    for (n = 1; n < REB_MAX; ++n)
        RXT_To_Reb[Reb_To_RXT[n]] = cast(enum Reb_Kind, n); // reverse lookup
}


//
//  Shutdown_Api: C
//
// See remarks on Startup_Api() for the difference between this idea and
// rebShutdown.
//
void Shutdown_Api(void)
{
    assert(Host_Lib != NULL);
    Host_Lib = NULL;
}


//
//  rebVersion: RL_API
//
// Obtain current REBOL interpreter version information.
//
// Returns:
//     A byte array containing version, revision, update, and more.
// Arguments:
//     vers - a byte array to hold the version info. First byte is length,
//         followed by version, revision, update, system, variation.
// Notes:
//     In the original RL_API, this function was to be called before any other
//     initialization to determine version compatiblity with the caller.
//     With the massive changes in Ren-C and the lack of RL_API clients, this
//     check is low priority.  This is how it was originally done:
//
//          REBYTE vers[8];
//          vers[0] = 5; // len
//          RL_Version(&vers[0]);
//
//          if (vers[1] != RL_VER || vers[2] != RL_REV)
//              rebPanic ("Incompatible reb-lib DLL");
//
void RL_rebVersion(REBYTE vers[])
{
    // [0] is length
    vers[1] = REBOL_VER;
    vers[2] = REBOL_REV;
    vers[3] = REBOL_UPD;
    vers[4] = REBOL_SYS;
    vers[5] = REBOL_VAR;
}


//
//  rebStartup: RL_API
//
// This function will allocate and initialize all memory structures used by
// the REBOL interpreter. This is an extensive process that takes time.
//
// `lib` is the host lib table (OS_XXX functions) which Rebol core does not
// take for granted--and assumes a host must provide to operate.  An example
// of this would be that getting the current UTC date and time varies from OS
// to OS, so for the NOW native to be implemented it has to call something
// outside of standard C...e.g. OS_GET_TIME().  So even though NOW is in the
// core, it will be incomplete without having that function supplied.
//
// !!! Increased modularization of the core, and new approaches, are making
// this concept obsolete.  For instance, the NOW native might not even live
// in the core, but be supplied by a "Timer Extension" which is considered to
// be sandboxed and non-core enough that having platform-specific code in it
// is not a problem.  Also, hooks can be supplied in the form of natives that
// are later HIJACK'd by some hosts (see rebPanic() and rebFail()), as a
// way of injecting richer platform-or-scenario-specific code into a more
// limited default host operation.  It is expected that the OS_XXX functions
// will eventually disappear completely.
//
void RL_rebStartup(const void *lib)
{
    if (Host_Lib != NULL)
        panic ("rebStartup() called when it's already started");

    Host_Lib = cast(const REBOL_HOST_LIB*, lib);

    if (Host_Lib->size < HOST_LIB_SIZE)
        panic ("Host-lib wrong size");

    if (((HOST_LIB_VER << 16) + HOST_LIB_SUM) != Host_Lib->ver_sum)
        panic ("Host-lib wrong version/checksum");

    Startup_Core();
}


//
//  rebInit: RL_API
//
// Initialize the REBOL interpreter with Host_Lib_Init
//
void RL_rebInit(void)
{
    rebStartup(&Host_Lib_Init);
}


//
//  rebShutdown: RL_API
//
// Shut down a Rebol interpreter (that was initialized via RL_Init).
//
// Returns:
//     nothing
// Arguments:
//     clean - whether you want Rebol to release all of its memory
//     accrued since initialization.  If you pass false, then it will
//     only do the minimum needed for data integrity (assuming you
//     are planning to exit the process, and hence the OS will
//     automatically reclaim all memory/handles/etc.)
//
void RL_rebShutdown(REBOOL clean)
{
    Enter_Api();

    // At time of writing, nothing Shutdown_Core() does pertains to
    // committing unfinished data to disk.  So really there is
    // nothing to do in the case of an "unclean" shutdown...yet.

    if (clean) {
    #ifdef NDEBUG
        // Only do the work above this line in an unclean shutdown
        return;
    #else
        // Run a clean shutdown anyway in debug builds--even if the
        // caller didn't need it--to see if it triggers any alerts.
        //
        Shutdown_Core();
    #endif
    }
    else {
        Shutdown_Core();
    }
}


// !!! This is a helper routine for producing arrays from a va_list.  It has
// a test of putting "UNEVAL" instructions before each spliced item, in order
// to prevent automatic evaluation.  This can be used by routines like print
// so that this would not try to run LABEL:
//
//     REBVAL *label = rebWord("label");
//     rebPrint("{The label is}", label, END);
//
// Inserting extra words is not how this would be done long term.  But the
// concept being reviewed is that top-level entities to some functions passed
// to va_list be "inert" by default.  It's difficult to implement in a
// consistent fashion because the moment one crosses into a nested BLOCK!,
// there is nowhere to store the "unevaluated" bit--since it is not a generic
// value flag that should be leaked.  For now, it's a test of the question of
// if some routines...like rebRun() and rebPrint()...would not handle splices
// as evaluative:
//
// https://forum.rebol.info/t/371
//
static REBARR* Array_From_Vaptr_Maybe_Null(
    const void *p,
    va_list* vaptr,
    REBOOL uneval_hack
){
    REBDSP dsp_orig = DSP;

    enum Reb_Pointer_Detect detect;

    while ((detect = Detect_Rebol_Pointer(p)) != DETECTED_AS_END) {
        if (p == NULL)
            fail ("use END to terminate rebPrint(), not NULL");

        switch (detect) {
        case DETECTED_AS_UTF8: {
            const REBYTE *utf8 = cast(const REBYTE*, p);
            const REBUPT start_line = 1;
            REBCNT size = LEN_BYTES(utf8);

            SCAN_STATE ss;
            Init_Scan_State(&ss, Intern("rebPrint()"), start_line, utf8, size);
            Scan_To_Stack(&ss);
            break; }

        case DETECTED_AS_SERIES:
            fail ("no complex instructions in rebPrint() yet");

        case DETECTED_AS_FREED_SERIES:
            panic (p);

        case DETECTED_AS_VALUE: {
            if (uneval_hack) {
                //
                // !!! By convention, these are supposed to be "spliced", and
                // not evaluated.  Unfortunately, we aren't really using the
                // variadic machinery here yet, and it's illegal to put
                // VALUE_FLAG_EVAL_FLIP in blocks.  Cheat by inserting UNEVAL.
                //
                DS_PUSH_TRASH;
                Init_Word(DS_TOP, Intern("uneval"));
            }

            DS_PUSH(cast(const REBVAL*, p));

            break; }

        case DETECTED_AS_END:
            assert(FALSE); // checked by while loop
            break;

        case DETECTED_AS_TRASH_CELL:
            panic (p);
        }

        p = va_arg(*vaptr, const void*);
    }

    REBARR *a = Pop_Stack_Values_Core(dsp_orig, NODE_FLAG_MANAGED);
    return a;
}


//
//  rebBlock: RL_API
//
// This constructs a block variadically from its arguments, which can be runs
// of UTF-8 data or REBVAL*.
//
// !!! Currently this does no binding of the data; hence any UTF-8 parts will
// be completely unbound, and any spliced values will keep their bindings.
//
REBVAL *RL_rebBlock(const void *p, ...) {
    va_list va;
    va_start(va, p);

    const REBOOL uneval_hack = FALSE;
    REBARR *a = Array_From_Vaptr_Maybe_Null(p, &va, uneval_hack);

    va_end(va);

    if (a == NULL)
        return NULL;

    return Init_Block(Alloc_Value(), a);
}


//
//  rebRun: RL_API
//
// C variadic function which calls the evaluator on multiple pointers.
// Each pointer may either be a REBVAL* or a UTF-8 string which will be
// scanned to reflect one or more values in the sequence.
//
// All REBVAL* are spliced in inert by default, as if they were an evaluative
// product already.  Use rebEval() to "retrigger" them (which wraps them in
// a singular REBARR*, another type of detectable pointer.)
//
REBVAL *RL_rebRun(const void *p, ...)
{
    Enter_Api();

    va_list va;
    va_start(va, p);

    DECLARE_LOCAL (temp); // so a fail() won't leak a handle...
    REBIXO indexor = Do_Va_Core(
        temp,
        p, // opt_first (preloads value)
        &va,
        DO_FLAG_EXPLICIT_EVALUATE | DO_FLAG_TO_END
    );
    va_end(va);

    if (indexor == THROWN_FLAG)
        fail (Error_No_Catch_For_Throw(temp));

    return Move_Value(Alloc_Value(), temp);
}


//
//  rebTrap: RL_API
//
// Behaves like rebRun() except traps errors.  Any throws/halts/quits will
// also be converted to an ERROR! and returned as a value.  As with the TRAP
// native when used without a /WITH clause, any non-raised errors that are
// evaluated to will return void...and voids turned into blanks.
//
REBVAL *RL_rebTrap(const void * const p, ...) {

    Enter_Api();

    struct Reb_State state;
    REBCTX *error_ctx;

    PUSH_TRAP(&error_ctx, &state);

    // The first time through the following code 'error' will be NULL, but...
    // `fail` can longjmp here, so 'error' won't be NULL *if* that happens!
    //
    if (error_ctx != NULL)
        return Init_Error(Alloc_Value(), error_ctx);

    va_list va;
    va_start(va, p);

    REBVAL *result = Alloc_Value();
    REBIXO indexor = Do_Va_Core(
        result,
        p, // opt_first (preloads value)
        &va,
        DO_FLAG_EXPLICIT_EVALUATE | DO_FLAG_TO_END
    );
    va_end(va);

    if (indexor == THROWN_FLAG) {
        REBCTX *error = Error_No_Catch_For_Throw(result);
        Free_Value(result);

        fail (error); // throws to above
    }

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    // Analogous to how TRAP works, if you don't have a handler for the
    // error case then you can't return an ERROR!, since all errors indicate
    // a failure.
    //
    // !!! Is returning rebVoid() too "quiet" a response?  Should it fail?
    // Returning NULL seems like it would be prone to creating surprise
    // crashes if the caller didn't expect NULLs, or used them to signal
    // some other purpose.
    //
    if (IS_ERROR(result)) {
        rebRelease(result);
        return rebVoid();
    }

    if (IS_VOID(result)) {
        rebRelease(result);
        return rebBlank();
    }

    return result;
}


//
//  rebElide: RL_API
//
// Variant of rebRun() which assumes you don't need the result.  This saves on
// allocating an API handle, or the caller needing to manage its lifetime.
//
void RL_rebElide(const void *p, ...)
{
    Enter_Api();

    va_list va;
    va_start(va, p);

    DECLARE_LOCAL (elided);
    REBIXO indexor = Do_Va_Core(
        elided,
        p, // opt_first (preloads value)
        &va,
        DO_FLAG_EXPLICIT_EVALUATE | DO_FLAG_TO_END
    );
    va_end(va);

    if (indexor == THROWN_FLAG)
        fail (Error_No_Catch_For_Throw(elided));
}


//
//  rebRunInline: RL_API
//
// Non-variadic function which takes a single argument which must be a single
// value that is a BLOCK! or GROUP!.  The goal is that it not add an extra
// stack level the way calling DO would.  This is important for instance in
// the console, so that BACKTRACE does not look up and see a Rebol function
// like DO on the stack.
//
// !!! This may be replaceable with `rebRun(rebInline(v), END);` or something
// similar.
//
REBVAL *RL_rebRunInline(const REBVAL *array)
{
    Enter_Api();

    if (NOT(IS_BLOCK(array)) && NOT(IS_GROUP(array)))
        fail ("rebRunInline() only supports BLOCK! and GROUP!");

    DECLARE_LOCAL (group);
    Move_Value(group, array);
    VAL_SET_TYPE_BITS(group, REB_GROUP);

    return rebRun(rebEval(NAT_VALUE(eval)), group, END);
}


//
//  rebPrint: RL_API
//
// Call through to the Rebol PRINT logic.
//
REBOOL RL_rebPrint(const void *p, ...)
{
    Enter_Api();

    REBVAL *print = CTX_VAR(
        Lib_Context,
        Find_Canon_In_Context(Lib_Context, STR_CANON(Intern("print")), TRUE)
    );

    va_list va;
    va_start(va, p);

    const REBOOL uneval_hack = TRUE; // !!! see notes in Array_From_Vaptr
    REBARR *a = Array_From_Vaptr_Maybe_Null(p, &va, uneval_hack);
    va_end(va);

    if (a == NULL)
        return FALSE;

    Deep_Freeze_Array(a);

    // !!! See notes in rebRun() on this particular choice of binding.  For
    // internal usage of PRINT (e.g. calls from PARSE) it really should not
    // be binding into user!
    //
    REBCTX *user_context = VAL_CONTEXT(
        Get_System(SYS_CONTEXTS, CTX_USER)
    );
    Bind_Values_Set_Midstream_Shallow(ARR_HEAD(a), user_context);
    Bind_Values_Deep(ARR_HEAD(a), Lib_Context);

    DECLARE_LOCAL (block);
    Init_Block(block, a);

    REBVAL *result = rebRun(rebEval(print), block, END);
    if (result == NULL)
        return FALSE;

    rebRelease(result);
    return TRUE;
}



//
//  rebEval: RL_API
//
// When rebRun() receives a REBVAL*, the default is to assume it should be
// spliced into the input stream as if it had already been evaluated.  It's
// only segments of code supplied via UTF-8 strings, that are live and can
// execute functions.
//
// This instruction is used with rebRun() in order to mark a value as being
// evaluated.  So `rebRun(rebEval(some_word), ...)` will execute that word
// if it's bound to a FUNCTION! and dereference if it's a variable.
//
void *RL_rebEval(const REBVAL *v)
{
    Enter_Api();

    if (IS_VOID(v))
        fail ("Cannot pass voids to rebEval()");

    // !!! The presence of the VALUE_FLAG_EVAL_FLIP is a pretty good
    // indication that it's an eval instruction.  So it's not necessary to
    // fill in the ->link or ->misc fields.  But if there were more
    // instructions like this, there'd probably need to be a misc->opcode or
    // something to distinguish them.
    //
    REBARR *result = Alloc_Singular_Array();
    Move_Value(KNOWN(ARR_SINGLE(result)), v);
    SET_VAL_FLAG(ARR_SINGLE(result), VALUE_FLAG_EVAL_FLIP);

    // !!! The intent for the long term is that these rebEval() instructions
    // not tax the garbage collector and be freed as they are encountered
    // while traversing the va_list.  Right now an assert would trip if we
    // tried that.  It's a good assert in general, so rather than subvert it
    // the instructions are just GC managed for now.
    //
    MANAGE_ARRAY(result);
    return result;
}


//
//  rebVoid: RL_API
//
REBVAL *RL_rebVoid(void)
{
    Enter_Api();
    return Init_Void(Alloc_Value());
}


//
//  rebBlank: RL_API
//
REBVAL *RL_rebBlank(void)
{
    Enter_Api();
    return Init_Blank(Alloc_Value());
}


//
//  rebLogic: RL_API
//
// !!! Uses libRed convention that it takes a long where 0 is false and all
// other values are true, for the moment.  REBOOL is standardized to only hold
// 0 or 1 inside the core, so taking a foreign REBOOL is risky and would
// require normalization anyway.
//
REBVAL *RL_rebLogic(long logic)
{
    Enter_Api();
    return Init_Logic(Alloc_Value(), LOGICAL(logic));
}


//
//  rebInteger: RL_API
//
// !!! Should there be rebSigned() and rebUnsigned(), in order to catch cases
// of using out of range values?
//
REBVAL *RL_rebInteger(REBI64 i)
{
    Enter_Api();
    return Init_Integer(Alloc_Value(), i);
}


//
//  rebDecimal: RL_API
//
REBVAL *RL_rebDecimal(REBDEC dec)
{
    Enter_Api();
    return Init_Decimal(Alloc_Value(), dec);
}


//
//  rebTimeHMS: RL_API
//
REBVAL *RL_rebTimeHMS(
    unsigned int hour,
    unsigned int minute,
    unsigned int second
){
    Enter_Api();

    REBVAL *result = Alloc_Value();
    VAL_RESET_HEADER(result, REB_TIME);
    VAL_NANO(result) = SECS_TO_NANO(hour * 3600 + minute * 60 + second);
    return result;
}


//
//  rebTimeNano: RL_API
//
REBVAL *RL_rebTimeNano(long nanoseconds) {
    Enter_Api();

    REBVAL *result = Alloc_Value();
    VAL_RESET_HEADER(result, REB_TIME);
    VAL_NANO(result) = nanoseconds;
    return result;
}


//
//  rebDateYMD: RL_API
//
REBVAL *RL_rebDateYMD(
    unsigned int year,
    unsigned int month,
    unsigned int day
){
    Enter_Api();

    REBVAL *result = Alloc_Value();
    VAL_RESET_HEADER(result, REB_DATE); // no time or time zone flags
    VAL_YEAR(result) = year;
    VAL_MONTH(result) = month;
    VAL_DAY(result) = day;
    return result;
}


//
//  rebDateTime: RL_API
//
REBVAL *RL_rebDateTime(const REBVAL *date, const REBVAL *time)
{
    Enter_Api();

    if (NOT(IS_DATE(date)))
        fail ("rebDateTime() date parameter must be DATE!");

    if (NOT(IS_TIME(time)))
        fail ("rebDateTime() time parameter must be TIME!");

    // if we had a timezone, we'd need to set DATE_FLAG_HAS_ZONE and
    // then INIT_VAL_ZONE().  But since DATE_FLAG_HAS_ZONE is not set,
    // the timezone bitfield in the date is ignored.

    REBVAL *result = Alloc_Value();
    VAL_RESET_HEADER(result, REB_DATE);
    SET_VAL_FLAG(result, DATE_FLAG_HAS_TIME);
    VAL_YEAR(result) = VAL_YEAR(date);
    VAL_MONTH(result) = VAL_MONTH(date);
    VAL_DAY(result) = VAL_DAY(date);
    VAL_NANO(result) = VAL_NANO(time);
    return result;
}


//
//  rebHalt: RL_API
//
// Signal that code evaluation needs to be interrupted.
//
// Returns:
//     nothing
// Notes:
//     This function sets a signal that is checked during evaluation
//     and will cause the interpreter to begin processing an escape
//     trap. Note that control must be passed back to REBOL for the
//     signal to be recognized and handled.
//
void RL_rebHalt(void)
{
    Enter_Api();

    SET_SIGNAL(SIG_HALT);
}


//
//  rebEvent: RL_API
//
// Appends an application event (e.g. GUI) to the event port.
//
// Returns:
//     Returns TRUE if queued, or FALSE if event queue is full.
// Arguments:
//     evt - A properly initialized event structure. The
//         contents of this structure are copied as part of
//         the function, allowing use of locals.
// Notes:
//     Sets a signal to get REBOL attention for WAIT and awake.
//     To avoid environment problems, this function only appends
//     to the event queue (no auto-expand). So if the queue is full
//
// !!! Note to whom it may concern: REBEVT would now be 100% compatible with
// a REB_EVENT REBVAL if there was a way of setting the header bits in the
// places that generate them.
//
int RL_rebEvent(REBEVT *evt)
{
    Enter_Api();

    REBVAL *event = Append_Event();     // sets signal

    if (event) {                        // null if no room left in series
        VAL_RESET_HEADER(event, REB_EVENT); // has more space, if needed
        event->extra.eventee = evt->eventee;
        event->payload.event.type = evt->type;
        event->payload.event.flags = evt->flags;
        event->payload.event.win = evt->win;
        event->payload.event.model = evt->model;
        event->payload.event.data = evt->data;
        return 1;
    }

    return 0;
}


//
//  rebRescue: RL_API
//
// This API abstracts the mechanics by which exception-handling is done.
// While code that knows specifically which form is used can take advantage of
// that knowledge and use the appropriate mechanism without this API, any
// code (such as core code) that wants to be agnostic to mechanism should
// use rebRescue() instead.
//
// There are three current mechanisms which can be built with.  One is to
// use setjmp()/longjmp(), which is extremely dodgy.  But it's what R3-Alpha
// used, and it's the only choice if one is sticking to ANSI C89-99:
//
// https://en.wikipedia.org/wiki/Setjmp.h#Exception_handling
//
// If one is willing to compile as C++ -and- link in the necessary support
// for exception handling, there are benefits to doing exception handling
// with throw/catch.  One advantage is performance: most compilers can avoid
// paying for catch blocks unless a throw occurs ("zero-cost exceptions"):
//
// https://stackoverflow.com/q/15464891/ (description of the phenomenon)
// https://stackoverflow.com/q/38878999/ (note that it needs linker support)
//
// It also means that C++ API clients can use try/catch blocks without needing
// the rebRescue() abstraction, as well as have destructors run safely.
// (longjmp pulls the rug out from under execution, and doesn't stack unwind).
//
// The other abstraction is for JavaScript, where an emscripten build would
// have to painstakingly emulate setjmp/longjmp.  Using inline JavaScript to
// catch and throw is more efficient, and also provides the benefit of API
// clients being able to use normal try/catch of a RebolError instead of
// having to go through rebRescue().
//
// But using rebRescue() internally allows the core to be compiled and run
// compatibly across all these scenarios.  It is named after Ruby's operation,
// which deals with the identical problem:
//
// http://silverhammermba.github.io/emberb/c/#rescue
//
// !!! As a first step, this only implements the setjmp/longjmp logic.
//
REBVAL *RL_rebRescue(
    REBDNG *dangerous, // !!! pure C function only if not using throw/catch!
    void *opaque
){
    Enter_Api();

    struct Reb_State state;
    REBCTX *error_ctx;

    PUSH_TRAP(&error_ctx, &state);

    // The first time through the following code 'error' will be NULL, but...
    // `fail` can longjmp here, so 'error' won't be NULL *if* that happens!
    //
    if (error_ctx != NULL)
        return Init_Error(Alloc_Value(), error_ctx);

    REBVAL *result = (*dangerous)(opaque);

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    if (result == NULL)
        return NULL; // NULL is considered a legal result

    // Analogous to how TRAP works, if you don't have a handler for the
    // error case then you can't return an ERROR!, since all errors indicate
    // a failure.
    //
    // !!! Is returning rebVoid() too "quiet" a response?  Should it fail?
    // Returning NULL seems like it would be prone to creating surprise
    // crashes if the caller didn't expect NULLs, or used them to signal
    // some other purpose.
    //
    if (IS_ERROR(result)) {
        rebRelease(result);
        return rebVoid();
    }

    if (IS_VOID(result)) {
        rebRelease(result);
        return rebBlank();
    }

    return result;
}


//
//  rebRescueWith: RL_API
//
// Variant of rebRescue() with a handler hook (parallels TRAP/WITH, except
// for C code as the protected code and the handler).  More similar to
// Ruby's rescue2 operation.
//
REBVAL *RL_rebRescueWith(
    REBDNG *dangerous, // !!! pure C function only if not using throw/catch!
    REBRSC *rescuer, // errors in the rescuer function will *not* be caught
    void *opaque
){
    Enter_Api();

    struct Reb_State state;
    REBCTX *error_ctx;

    PUSH_TRAP(&error_ctx, &state);

    // The first time through the following code 'error' will be NULL, but...
    // `fail` can longjmp here, so 'error' won't be NULL *if* that happens!
    //
    if (error_ctx != NULL) {
        REBVAL *error = Init_Error(Alloc_Value(), error_ctx);

        REBVAL *result = (*rescuer)(error, opaque); // *not* guarded by trap!

        rebRelease(error);
        return result; // no special handling, may be NULL
    }

    REBVAL *result = (*dangerous)(opaque); // guarded by trap

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    return result; // no special handling, may be NULL
}


//
// !!! These routines are exports of the macros and inline functions which
// rely upon internal definitions that RL_XXX clients are not expected to have
// available.  While this implementation file can see inside the definitions
// of `struct Reb_Value`, the caller has an opaque definition.
//
// These are transitional as part of trying to get rid of RXIARG, RXIFRM, and
// COMMAND! in general.  Though it is not a good "API design" to just take
// any internal function you find yourself needing in a client and export it
// here with "RL_" in front of the name, it's at least understandable--and
// not really introducing any routines that don't already have to exist and
// be tested.
//

inline static REBFRM *Extract_Live_Rebfrm_May_Fail(const REBVAL *frame) {
    if (NOT(IS_FRAME(frame)))
        fail ("Not a FRAME!");

    REBFRM *f = CTX_FRAME_MAY_FAIL(VAL_CONTEXT(frame));

    assert(Is_Function_Frame(f) && NOT(Is_Function_Frame_Fulfilling(f)));
    return f;
}


//
//  rebFrmNumArgs: RL_API
//
REBCNT RL_rebFrmNumArgs(const REBVAL *frame) {
    Enter_Api();

    REBFRM *f = Extract_Live_Rebfrm_May_Fail(frame);
    return FRM_NUM_ARGS(f);
}

//
//  rebFrmArg: RL_API
//
REBVAL *RL_rebFrmArg(const REBVAL *frame, REBCNT n) {
    Enter_Api();

    REBFRM *f = Extract_Live_Rebfrm_May_Fail(frame);
    return FRM_ARG(f, n);
}


//
//  rebTypeOf: RL_API
//
// !!! Among the few concepts from the original host kit API that may make
// sense, it could be a good idea to abstract numbers for datatypes from the
// REB_XXX numbering scheme.  So for the moment, REBRXT is being kept as is.
//
REBRXT RL_rebTypeOf(const REBVAL *v) {
    Enter_Api();

    enum Reb_Kind kind = VAL_TYPE(v);
    return IS_VOID(v) ? 0 : Reb_To_RXT[kind];
}


//
//  rebDid: RL_API
//
REBOOL RL_rebDid(const void *p, ...) {
    Enter_Api();

    va_list va;
    va_start(va, p);

    DECLARE_LOCAL (condition);
    REBIXO indexor = Do_Va_Core(
        condition,
        p, // opt_first (preloads value)
        &va,
        DO_FLAG_EXPLICIT_EVALUATE | DO_FLAG_TO_END
    );
    if (indexor == THROWN_FLAG) {
        va_end(va);
        fail (Error_No_Catch_For_Throw(condition));
    }

    va_end(va);

    if (IS_VOID(condition))
        fail ("rebDid() received void");

    return IS_TRUTHY(condition);
}


//
//  rebUnboxInteger: RL_API
//
long RL_rebUnboxInteger(const REBVAL *v) {
    Enter_Api();
    return VAL_INT64(v);
}

//
//  rebUnboxDecimal: RL_API
//
REBDEC RL_rebUnboxDecimal(const REBVAL *v) {
    Enter_Api();
    return VAL_DECIMAL(v);
}

//
//  rebUnboxChar: RL_API
//
REBUNI RL_rebUnboxChar(const REBVAL *v) {
    Enter_Api();
    return VAL_CHAR(v);
}

//
//  rebNanoOfTime: RL_API
//
long RL_rebNanoOfTime(const REBVAL *v) {
    Enter_Api();
    return VAL_NANO(v);
}


//
//  rebValTupleData: RL_API
//
REBYTE *RL_rebValTupleData(const REBVAL *v) {
    Enter_Api();
    return VAL_TUPLE_DATA(m_cast(REBVAL*, v));
}


//
//  rebValIndex: RL_API
//
REBCNT RL_rebValIndex(const REBVAL *v) {
    Enter_Api();
    return VAL_INDEX(v);
}


//
//  rebInitDate: RL_API
//
// !!! Note this doesn't allow you to say whether the date has a time
// or zone component at all.  Those could be extra flags, or if Rebol values
// were used they could be blanks vs. integers.  Further still, this kind
// of API is probably best kept as calls into Rebol code, e.g.
// RL_Do("make time!", ...); which might not offer the best performance, but
// the internal API is available for clients who need that performance,
// who can call date initialization themselves.
//
REBVAL *RL_rebInitDate(
    int year,
    int month,
    int day,
    int seconds,
    int nano,
    int zone
){
    Enter_Api();

    REBVAL *result = Alloc_Value();
    VAL_RESET_HEADER(result, REB_DATE);
    VAL_YEAR(result) = year;
    VAL_MONTH(result) = month;
    VAL_DAY(result) = day;

    SET_VAL_FLAG(result, DATE_FLAG_HAS_ZONE);
    INIT_VAL_ZONE(result, zone / ZONE_MINS);

    SET_VAL_FLAG(result, DATE_FLAG_HAS_TIME);
    VAL_NANO(result) = SECS_TO_NANO(seconds) + nano;
    return result;
}


//
//  rebMoldAlloc: RL_API
//
// Mold any value and produce a UTF-8 string from it.
//
// !!! Ideally the UTF-8 string returned could use an allocation strategy that
// would make it attach GC to the current FRAME!, while also allowing it to be
// rebRelease()'d.  It might also return a `const char*` to the internal UTF8
// data with a hold on it.
//
char *RL_rebMoldAlloc(REBCNT *len_out, const REBVAL *v)
{
    Enter_Api();

    DECLARE_MOLD (mo);
    Push_Mold(mo);
    Mold_Value(mo, v);

    // !!! In UTF-8 Everywhere, the mold buffer is UTF-8, and could be copied
    // out of directly without these extra steps.
    //
    DECLARE_LOCAL (molded);
    Init_String(molded, Pop_Molded_String(mo));

    REBCNT index = VAL_INDEX(molded);
    REBCNT len = VAL_LEN_AT(molded);
    REBSER *utf8 = Temp_UTF8_At_Managed(molded, &index, &len);

    char *result = cast(char*, rebMalloc(len + 1));
    memcpy(result, BIN_AT(utf8, index), len + 1); // has '\0' terminator

    if (len_out != NULL)
        *len_out = len;

    return result;
}


//
//  rebSpellingOf: RL_API
//
// Extract UTF-8 data from an ANY-STRING! or ANY-WORD!.
//
REBCNT RL_rebSpellingOf(
    char *buf,
    REBCNT buf_chars,
    const REBVAL *v
){
    Enter_Api();

    REBCNT index;
    REBCNT len;
    const REBYTE *utf8;
    if (ANY_STRING(v)) {
        index = VAL_INDEX(v);
        len = VAL_LEN_AT(v);
        REBSER *temp = Temp_UTF8_At_Managed(v, &index, &len);
        utf8 = BIN_AT(temp, index);
    }
    else {
        assert(ANY_WORD(v));
        index = 0;
        utf8 = VAL_WORD_HEAD(v);
        len = LEN_BYTES(utf8);
    }

    if (buf == NULL) {
        assert(buf_chars == 0);
        return len; // caller must allocate a buffer of size len + 1
    }

    REBCNT limit = MIN(buf_chars, len);
    memcpy(buf, cs_cast(utf8), limit);
    buf[limit] = '\0';
    return len;
}


//
//  rebSpellingOfAlloc: RL_API
//
char *RL_rebSpellingOfAlloc(REBCNT *len_out, const REBVAL *v)
{
    Enter_Api();

    REBCNT len = rebSpellingOf(NULL, 0, v);
    char *result = cast(char*, rebMalloc(len + 1));
    rebSpellingOf(result, len, v);
    if (len_out != NULL)
        *len_out = len;
    return result;
}


//
//  rebSpellingOfW: RL_API
//
// Extract UCS2 data from an ANY-STRING! or ANY-WORD!.
//
REBCNT RL_rebSpellingOfW(
    REBWCHAR *buf,
    REBCNT buf_chars, // characters buffer can hold (not including terminator)
    const REBVAL *v
){
    Enter_Api();

    REBCNT index;
    REBCNT len;
    if (ANY_STRING(v)) {
        index = VAL_INDEX(v);
        len = VAL_LEN_AT(v);
    }
    else {
        assert(ANY_WORD(v));
        panic ("extracting wide characters from WORD! not yet supported");
    }

    if (buf == NULL) { // querying for size
        assert(buf_chars == 0);
        return len; // caller must now allocate buffer of len + 1
    }

    REBSER *s = VAL_SERIES(v);

    REBCNT limit = MIN(buf_chars, len);
    REBCNT n = 0;
    for (; index < limit; ++n, ++index)
        buf[n] = GET_ANY_CHAR(s, index);

    buf[limit] = 0;
    return len;
}


//
//  rebSpellingOfAllocW: RL_API
//
REBWCHAR *RL_rebSpellingOfAllocW(REBCNT *len_out, const REBVAL *v)
{
    Enter_Api();

    REBCNT len = rebSpellingOfW(NULL, 0, v);
    REBWCHAR *result = cast(REBWCHAR*, rebMalloc(len + 1));
    rebSpellingOfW(result, len, v);
    if (len_out != NULL)
        *len_out = len;
    return result;
}


//
//  rebValBin: RL_API
//
// Extract binary data from a BINARY!
//
REBCNT RL_rebValBin(
    REBYTE *buf,
    REBCNT buf_chars,
    const REBVAL *binary
){
    Enter_Api();

    if (NOT(IS_BINARY(binary)))
        fail ("rebValBin() only works on BINARY!");

    REBCNT len = VAL_LEN_AT(binary);

    if (buf == NULL) {
        assert(buf_chars == 0);
        return len; // caller must allocate a buffer of size len + 1
    }

    REBCNT limit = MIN(buf_chars, len);
    memcpy(s_cast(buf), cs_cast(VAL_BIN_AT(binary)), limit);
    buf[limit] = '\0';
    return len;
}


//
//  rebValBinAlloc: RL_API
//
REBYTE *RL_rebValBinAlloc(REBCNT *len_out, const REBVAL *binary)
{
    Enter_Api();

    REBCNT len = rebValBin(NULL, 0, binary);
    REBYTE *result = cast(REBYTE*, rebMalloc(len + 1));
    rebValBin(result, len, binary);
    if (len_out != NULL)
        *len_out = len;
    return result;
}


//
//  rebBinary: RL_API
//
REBVAL *RL_rebBinary(void *bytes, size_t size)
{
    Enter_Api();

    REBSER *bin = Make_Binary(size);
    memcpy(BIN_HEAD(bin), bytes, size);
    TERM_BIN_LEN(bin, size);

    return Init_Binary(Alloc_Value(), bin);
}


//
//  rebSizedString: RL_API
//
REBVAL *RL_rebSizedString(const char *utf8, size_t size)
{
    Enter_Api();

    return Init_String(
        Alloc_Value(),
        Append_UTF8_May_Fail(NULL, cast(const REBYTE*, utf8), size)
    );
}


//
//  rebString: RL_API
//
REBVAL *RL_rebString(const char *utf8)
{
    Enter_Api();
    return Init_String(Alloc_Value(), Make_UTF8_May_Fail(cb_cast(utf8)));
}


//
//  rebFile: RL_API
//
REBVAL *RL_rebFile(const char *utf8)
{
    REBVAL *result = rebString(utf8); // Enter_Api() called
    VAL_RESET_HEADER(result, REB_FILE);
    return result;
}


//
//  rebStringW: RL_API
//
REBVAL *RL_rebStringW(const REBWCHAR *wstr)
{
    Enter_Api();

    REBCNT num_chars = 0;
    const REBWCHAR *wtemp = wstr;
    while (*wtemp != '\0') {
        ++num_chars;
        ++wtemp;
    }

    REBSER *ser = Make_Unicode(num_chars);
    memcpy(UNI_HEAD(ser), wstr, sizeof(REBWCHAR) * num_chars);
    TERM_UNI_LEN(ser, num_chars);

    return Init_String(Alloc_Value(), ser);
}


//
//  rebFileW: RL_API
//
REBVAL *RL_rebFileW(const REBWCHAR *wstr)
{
    REBVAL *result = rebStringW(wstr); // Enter_Api() called
    VAL_RESET_HEADER(result, REB_FILE);
    return result;
}


//
//  rebManage: RL_API
//
// The "friendliest" default for the API is to assume you want handles to be
// tied to the lifetime of the frame they're in.  Long-running top-level
// processes like the C code running the console would eventually exhaust
// memory if that were the case...so there should be some options for metrics
// as a form of "leak detection" even so.
//
// While the API is being developed and used in core code, the default is to
// be "unfriendly"...and you have to explicitly ask for management.
//
REBVAL *RL_rebManage(REBVAL *v)
{
    Enter_Api();

    assert(Is_Api_Value(v));
    assert(NOT(v->header.bits & CELL_FLAG_STACK));

    if (FS_TOP == NULL)
        fail ("rebManage() temporarily disallowed in top level C for safety");

    REBVAL *key = PAIRING_KEY(v);

    v->header.bits |= CELL_FLAG_STACK;

    // For this situation, the context must be reified.  This is because the
    // way in which lifetime is expected to be managed is that though the
    // frame is alive right now as a REBFRM*, it eventually will not be...and
    // a GC sweep at that time will need something that isn't an invalid
    // pointer to examine.
    //
    Init_Any_Context(
        key,
        REB_FRAME,
        Context_For_Frame_May_Reify_Managed(FS_TOP)
    );

    return v;

}


//
//  rebUnmanage: RL_API
//
// This "unmanages" an API handle.  It doesn't actually make the node flag
// unmanaged (the managed bit on a value is how we discern API handles from
// non-API handles, managed or not).  We just set the meta information
// for the handle to blank.
//
REBVAL *RL_rebUnmanage(REBVAL *v)
{
    Enter_Api();

    REBVAL *key = PAIRING_KEY(v);
    assert(IS_FRAME(key));

    Init_Blank(key);
    return v;
}


//
//  rebRelease: RL_API
//
void RL_rebRelease(REBVAL *v)
{
    Enter_Api();

    if (NOT(Is_Api_Value(v)))
        panic("Attempt to rebRelease() a non-API handle");

    Free_Value(v);
}



//
//  rebError: RL_API
//
REBVAL *RL_rebError(const char *msg)
{
    Enter_Api();
    return Init_Error(Alloc_Value(), Error_User(msg));
}


//
//  rebFail: RL_API [
//      #noreturn
//  ]
//
// rebFail() is a distinct entry point (vs. just using rebElide("fail"))
// because it needs to have the noreturn attribute, so that compiler warnings
// can be enabled and checked.
//
// !!! Would calling it rebFAIL (...) make it stand out more?
//
// Note: Over the long term, one does not want to hard-code error strings in
// the executable.  That makes them more difficult to hook with translations,
// or to identify systemically with some kind of "error code".  However,
// it's a realistic quick-and-dirty way of delivering a more meaningful
// error than just using a RE_MISC error code, and can be found just as easily
// to clean up later.
//
// !!! Should there be a way for the caller to slip their C file and line
// information through as the source of the FAIL?
//
void RL_rebFail(const void *p, const void *p2)
{
    Enter_Api();

    assert(Detect_Rebol_Pointer(p2) == DETECTED_AS_END);

    // !!! There needs to be some sort of story on unmanaged API handles in
    // terms of how they get cleaned up w.r.t. errors.  For now, this is one
    // case where we know we need to make sure the value gets cleaned up
    // when this stack level goes away...so tie the value's lifetime to the
    // topmost frame in effect.
    //
    if (Detect_Rebol_Pointer(p) == DETECTED_AS_VALUE) {
        const REBVAL *v = cast(const REBVAL*, p);
        if (
            (v->header.bits & (NODE_FLAG_ROOT | CELL_FLAG_STACK))
            == NODE_FLAG_ROOT
        ){
            rebManage(m_cast(REBVAL*, v));
        }
    }

    rebElide("fail", p, p2); // should not return...should DO an ERROR!

    panic ("FAIL was called, but continued running!");
}


//
//  rebPanic: RL_API [
//      #noreturn
//  ]
//
// Calls PANIC via rebRun(), but is a separate entry point in order to have
// an attribute saying it doesn't return.
//
void RL_rebPanic(const void *p, const void *end)
{
    Enter_Api();
    assert(Detect_Rebol_Pointer(end) == DETECTED_AS_END); // !!! TBD: variadic
    UNUSED(end);

    rebRun(rebEval(NAT_VALUE(panic)), p, END);

    // !!! Should there be a special bit or dispatcher used on the PANIC and
    // PANIC-VALUE functions that ensures they exit?  If it were a dispatcher
    // then HIJACK would have to be aware of it and preserve it.
    //
    panic ("HIJACK'd PANIC function did not exit Rebol");
}


//
//  rebPanicValue: RL_API [
//      #noreturn
//  ]
//
// Calls PANIC-VALUE via rebRun(), but is a separate entry point in order to
// have an attribute saying it doesn't return.
//
void RL_rebPanicValue(const void *p, const void *end)
{
    Enter_Api();
    assert(Detect_Rebol_Pointer(end) == DETECTED_AS_END); // !!! TBD: variadic
    UNUSED(end);

    rebRun(rebEval(NAT_VALUE(panic_value)), p, END);

    // !!! Should there be a special bit or dispatcher used on the PANIC and
    // PANIC-VALUE functions that ensures they exit?  If it were a dispatcher
    // then HIJACK would have to be aware of it and preserve it.
    //
    panic ("HIJACK'd PANIC-VALUE function did not exit Rebol");
}


//
//  rebFileToLocal: RL_API
//
// This is the API exposure of TO-LOCAL-FILE.  It takes in a FILE! and
// returns a STRING!...using the data type to help guide whether a file has
// had the appropriate transformation applied on it or not.
//
REBVAL *RL_rebFileToLocal(const REBVAL *file, REBOOL full)
{
    Enter_Api();

    if (NOT(IS_FILE(file)))
        fail ("rebFileToLocal() only works on FILE!");

    return Init_String(
        Alloc_Value(),
        Value_To_Local_Path(file, full)
    );
}


//
//  rebLocalToFile: RL_API
//
// This is the API exposure of TO-REBOL-FILE.  It takes in a STRING! and
// returns a FILE!, as with rebFileToLocal() in order to help cue whether the
// translations have been done or not.
//
REBVAL *RL_rebLocalToFile(const REBVAL *string, REBOOL is_dir)
{
    Enter_Api();

    if (NOT(IS_STRING(string)))
        fail ("rebLocalToFile() only works on STRING!");

    return Init_File(
        Alloc_Value(),
        Value_To_REBOL_Path(string, is_dir)
    );
}


//
//  rebEnd: RL_API
//
const REBVAL *RL_rebEnd(void) {return END;}


// We wish to define a table of the above functions to pass to clients.  To
// save on typing, the declaration of the table is autogenerated as a file we
// can include here.
//
// It doesn't make a lot of sense to expose this table to clients via an API
// that returns it, because that's a chicken-and-the-egg problem.  The reason
// a table is being used in the first place is because extensions can't link
// to an EXE (in a generic way).  So the table is passed to them, in that
// extension's DLL initialization function.
//
// !!! Note: if Rebol is built as a DLL or LIB, the story is different.
//
extern RL_LIB Ext_Lib;
#include "tmp-reb-lib-table.inc" // declares Ext_Lib
