//
//  File: %b-init.c
//  Summary: "initialization functions"
//  Section: bootstrap
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
// The primary routine for starting up Rebol is Init_Core().  It runs the
// bootstrap in phases, based on processing various portions of the data in
// %boot-code.r (which is the aggregated code from the %mezz/*.r files, that
// is packed into one file as part of the build preparation)
//
// As part of an effort to lock down the memory usage, Ren-C added a parallel
// Shutdown_Core() routine which would gracefully exit Rebol, with assurances
// that all accounting was done correctly.  This includes being sure that the
// number used to track memory usage for triggering garbage collections would
// balance back out to exactly zero.
//
// (Release builds can instead close only vital resources like files, and
// trust the OS exit() to reclaim memory more quickly.  However Ren-C's goal
// is to be usable as a library that may be initialized and shutdown within
// a process that's not exiting, so the ability to clean up is important.)
//


#include "sys-core.h"
#include "mem-pools.h"

#define EVAL_DOSE 10000


//
//  Assert_Basics: C
//
static void Assert_Basics(void)
{
#if !defined(NDEBUG) && defined(SHOW_SIZEOFS)
    //
    // For debugging ports to some systems
    //
#if defined(__LP64__) || defined(__LLP64__)
    const char *fmt = "%lu %s\n";
#else
    const char *fmt = "%u %s\n";
#endif

    union Reb_Value_Payload *dummy_payload;

    printf(fmt, sizeof(dummy_payload->any_word), "any_word");
    printf(fmt, sizeof(dummy_payload->any_series), "any_series");
    printf(fmt, sizeof(dummy_payload->integer), "integer");
    printf(fmt, sizeof(dummy_payload->decimal), "decimal");
    printf(fmt, sizeof(dummy_payload->character), "char");
    printf(fmt, sizeof(dummy_payload->datatype), "datatype");
    printf(fmt, sizeof(dummy_payload->typeset), "typeset");
    printf(fmt, sizeof(dummy_payload->time), "time");
    printf(fmt, sizeof(dummy_payload->tuple), "tuple");
    printf(fmt, sizeof(dummy_payload->function), "function");
    printf(fmt, sizeof(dummy_payload->any_context), "any_context");
    printf(fmt, sizeof(dummy_payload->pair), "pair");
    printf(fmt, sizeof(dummy_payload->event), "event");
    printf(fmt, sizeof(dummy_payload->library), "library");
    printf(fmt, sizeof(dummy_payload->structure), "struct");
    printf(fmt, sizeof(dummy_payload->gob), "gob");
    printf(fmt, sizeof(dummy_payload->money), "money");
    printf(fmt, sizeof(dummy_payload->handle), "handle");
    printf(fmt, sizeof(dummy_payload->all), "all");
    fflush(stdout);
#endif

#if !defined(NDEBUG)
    //
    // Sanity check the platform byte-ordering sensitive flag macros
    //
    REBUPT flags;

    flags = FLAGIT_LEFT(0);
    unsigned char *ch = (unsigned char*)&flags;
    if (*ch != 128) {
        printf("Expected 128, got %d\n", *ch);
        panic ("Bad leftmost bit setting of platform unsigned integer.");
    }

    flags = FLAGIT_LEFT(0) | FLAGIT_LEFT(1) | FLAGBYTE_RIGHT(13);

    REBCNT left = LEFT_N_BITS(flags, 3); // == 6 (binary `110`)
    REBCNT right = RIGHT_N_BITS(flags, 3); // == 5 (binary `101`)
    if (left != 6 || right != 5) {
        printf("Expected 6 and 5, got %d and %d\n", left, right);
        panic ("Bad composed integer assignment for byte-ordering macro.");
    }
#endif

    // Although the system is designed to be able to function with REBVAL at
    // any size, the optimization of it being 4x(32-bit) on 32-bit platforms
    // and 4x(64-bit) on 64-bit platforms is a rather important performance
    // point.  For the moment we consider it to be essential enough to the
    // intended function of the system that it refuses to run if not true.
    //
    // But if someone is in an odd situation and understands why the size did
    // not work out as designed, it *should* be possible to comment this out
    // and keep running.
    //
    if (sizeof(void*) == 8) {
        if (sizeof(REBVAL) != 32 || sizeof(REBEVT) != 32)
            panic ("size of void* is 8 but REBVAL is not sizeof(void*)*4");

        assert(sizeof(REBGOB) == 88); // !!! REBGOB to be made a REBARR
    }
    else if (sizeof(void*) == 4) {
        if (sizeof(REBVAL) != 16 || sizeof(REBEVT) != 16)
            panic ("size of void* is 4 but REBVAL is not sizeof(void*)*4");

        assert(sizeof(REBGOB) == 64); // !!! REBGOB to be made a REBARR
    }
    else
        panic ("sizeof void* is neither 4 nor 8");

    assert(sizeof(REBDAT) == 4);
    assert(sizeof(REBEVT) == sizeof(REBVAL));

    // The REBSER is designed to place the `info` bits exactly after a REBVAL
    // so they can do double-duty as also a terminator for that REBVAL when
    // enumerated as an ARRAY.
    //
    if (
        offsetof(struct Reb_Series, info)
            - offsetof(struct Reb_Series, content)
        != sizeof(REBVAL)
    ){
        panic ("bad structure alignment for internal array termination");
    }

    // The END marker logic currently uses REB_MAX for the type bits.  That's
    // okay up until the REB_MAX bumps to 256.  If you hit this then some
    // other value needs to be chosen in the debug build to represent the
    // type value for END's bits.  (REB_TRASH is just a poor choice, because
    // you'd like to catch IS_END() tests on trash.)
    //
    assert(REB_MAX < 256);
}


//
//  Do_Global_Block: C
//
// Bind and evaluate a global block.
// Rebind:
//     0: bind set into sys or lib
//    -1: bind shallow into sys (for ACTION)
//     1: add new words to LIB, bind/deep to LIB
//     2: add new words to SYS, bind/deep to LIB
//
// Expects result to be void
//
static void Do_Global_Block(
    REBARR *block,
    REBCNT index,
    REBINT rebind,
    REBVAL *opt_toplevel_word
) {
    RELVAL *item = ARR_AT(block, index);
    struct Reb_State state;

    REBVAL result;

    Bind_Values_Set_Midstream_Shallow(
        item, rebind > 1 ? Sys_Context : Lib_Context
    );

    if (rebind < 0) Bind_Values_Shallow(item, Sys_Context);
    if (rebind > 0) Bind_Values_Deep(item, Lib_Context);
    if (rebind > 1) Bind_Values_Deep(item, Sys_Context);

    // !!! In the block, ACTION words are bound but paths would not bind.
    // So you could do `action [spec]` but not `action/xxx [spec]`
    // because in the later case, it wouldn't have descended into the path
    // to bind `native`.  This is apparently intentional to avoid binding
    // deeply in the system context.
    //
    // Here we hackily walk over all the paths and look for any that start
    // with the symbol of the passed in word (SYM_ACTION) and just overwrite
    // the word with the bound vesion.  :-/  Review.
    //
    if (opt_toplevel_word) {
        item = ARR_AT(block, index);
        for (; NOT_END(item); item++) {
            if (IS_PATH(item)) {
                RELVAL *path_item = VAL_ARRAY_HEAD(item);
                if (
                    IS_WORD(path_item) && (
                        VAL_WORD_SPELLING(path_item)
                        == VAL_WORD_SPELLING(opt_toplevel_word)
                    )
                ) {
                    // Steal binding, but keep the same word type
                    //
                    enum Reb_Kind kind = VAL_TYPE(path_item);
                    *path_item = *opt_toplevel_word;
                    VAL_SET_TYPE_BITS(path_item, kind);
                }
            }
        }
    }

    if (Do_At_Throws(&result, block, index, SPECIFIED))
        panic (&result);

    if (!IS_VOID(&result))
        panic (&result);
}


//
//  Init_Datatypes: C
//
// Create the datatypes.
//
static void Init_Datatypes(REBARR *boot_types, REBARR *boot_typespecs)
{
    if (ARR_LEN(boot_types) != REB_MAX - 1)
        panic (boot_types); // Every REB_XXX but REB_0 should have a WORD!

    RELVAL *word = ARR_HEAD(boot_types);

    if (VAL_WORD_SYM(word) != SYM_FUNCTION_X)
        panic (word); // First type should be FUNCTION!

    REBINT n;
    for (n = 1; NOT_END(word); word++, n++) {
        assert(n < REB_MAX);

        REBVAL *value = Append_Context(Lib_Context, KNOWN(word), NULL);
        VAL_RESET_HEADER(value, REB_DATATYPE);
        VAL_TYPE_KIND(value) = cast(enum Reb_Kind, n);
        VAL_TYPE_SPEC(value) = VAL_ARRAY(ARR_AT(boot_typespecs, n - 1));

        // !!! The system depends on these definitions, as they are used by
        // Get_Type and Type_Of.  Although it is convenient to be able to
        // get a REBVAL of a type and not have to worry about its lifetime
        // management or stack-allocate the value and fill it, that's
        // probably not a frequent enough need to justify putting it in
        // a public slot and either leave it mutable (dangerous) or lock it
        // (inflexible).  Better to create a new type value each time.
        //
        // (Another possibility would be to use the "datatypes catalog",
        // which appears to copy this subset out from the Lib_Context.)
        //
        assert(value == Get_Type(cast(enum Reb_Kind, n)));
        SET_VAL_FLAG(CTX_KEY(Lib_Context, 1), TYPESET_FLAG_LOCKED);
    }
}


//
//  Init_True_And_False: C
//
// !!! Rebol is firm on TRUE and FALSE being WORD!s, as opposed to the literal
// forms of logical true and false.  Not only does this frequently lead to
// confusion, but there's not consensus on what a good literal form would be.
// R3-Alpha used #[true] and #[false] (but often molded them as looking like
// the words true and false anyway).  $true and $false have been proposed,
// but would not be backward compatible in files read by bootstrap.
//
// Since no good literal form exists, the %sysobj.r file uses the words.  They
// have to be defined before the point that it runs (along with the natives).
//
static void Init_True_And_False(void)
{
    REBVAL *true_value = Append_Context(Lib_Context, 0, Canon(SYM_TRUE));
    SET_TRUE(true_value);
    assert(VAL_LOGIC(true_value) == TRUE);
    assert(IS_CONDITIONAL_TRUE(true_value));

    REBVAL *false_value = Append_Context(Lib_Context, 0, Canon(SYM_FALSE));
    SET_FALSE(false_value);
    assert(VAL_LOGIC(false_value) == FALSE);
    assert(IS_CONDITIONAL_FALSE(false_value));
}


//
//  action: native [
//
//  {Creates datatype action (for internal usage only).}
//
//      return: [blank!]
//      :verb [set-word! word!]
//      spec [block!]
//  ]
//
REBNATIVE(action)
//
// The `action` native is searched for explicitly by %make-natives.r and put
// in second place for initialization (after the `native` native).
//
// It is designed to be a lookback binding that quotes its first argument,
// so when you write FOO: ACTION [...], the FOO: gets quoted to be the verb.
// The SET/LOOKBACK is done by the bootstrap, after the natives are loaded.
{
    INCLUDE_PARAMS_OF_ACTION;

    REBVAL *spec = ARG(spec);

    // We only want to check the return type in the debug build.  In the
    // release build, we want to have as few argument slots as possible...
    // especially to get the optimization for 1 argument to go in the cell
    // and not need to push arguments.
    //
    REBFLGS flags = MKF_KEYWORDS | MKF_FAKE_RETURN;

    REBFUN *fun = Make_Function(
        Make_Paramlist_Managed_May_Fail(spec, flags),
        &Action_Dispatcher,
        NULL // no underlying function--this is fundamental
    );

    *FUNC_BODY(fun) = *ARG(verb);

    // A lookback quoting function that quotes a SET-WORD! on its left is
    // responsible for setting the value if it wants it to change.
    //
    *SINK_VAR_MAY_FAIL(ARG(verb), SPECIFIED) = *FUNC_VALUE(fun);

    return R_BLANK; // result won't be used if a function left-quotes SET-WORD!
}


//
//  Add_Lib_Keys_R3Alpha_Cant_Make: C
//
// In order for the bootstrap to assign values to library words, they have to
// exist in the bootstrap context.  The way they get into the context is by
// a scan for top-level SET-WORD!s in the %sys-xxx.r and %mezz-xxx.r files.
//
// However, R3-Alpha doesn't allow set-words like /: and <=:  The words can
// be gotten with `pick [/] 1` or similar, but they cannot be SET because
// there's nothing in the context to bind them to...since no SET-WORD! was
// picked up in the scan.
//
// As a workaround, this just adds the words to the context manually.  Then,
// however the words are created, it will be possible to bind them and set
// them to things.
//
// !!! Even as Ren-C becomes more permissive in letting SET-WORDs for these
// items be created, they should not be seen by %make-boot.r so long as the
// code expects to be bootstrapped with R3-Alpha.  This is because as part
// of the bootstrap, the code is loaded/processed and molded out as one
// giant file.  Ren-C being able to read `=>:` would not be able to help
// retroactively make old R3-Alphas read it too.
//
static void Add_Lib_Keys_R3Alpha_Cant_Make(void)
{
    const char *names[] = {
        "<",
        ">",

        "<=", // less than or equal to
        "=>", // no current system meaning

        ">=", // greater than or equal to
        "=<",

        "<>", // may ultimately be targeted for empty tag in Ren-C

        "->", // FUNCTION-style lambda ("reaches in")
        "<-", // FUNC-style lambda ("reaches out"),

        "|>", // Evaluate to next single expression, but do ones afterward
        "<|", // Evaluate to previous expression, but do rest (like ALSO)

        "/",
        "//", // is remainder in R3-Alpha, not ideal

        NULL
    };

    REBINT i = 0;
    while (names[i]) {
        REBSTR *str = Intern_UTF8_Managed(cb_cast(names[i]), strlen(names[i]));
        REBVAL *val = Append_Context(Lib_Context, NULL, str);
        SET_VOID(val); // functions will fill in (no-op, since void already)
        ++i;
    }
}


//
// Init_Function_Tag: C
//
// !!! It didn't seem there was a "compare UTF8 byte array to arbitrary
// decoded REB_TAG which may or may not be REBUNI" routine, but there was
// an easy way to compare tags to each other.  So pre-fabricating these was
// quick, but a better solution should be reviewed in terms of an overall
// string and UTF8 rethinking.
//
static void Init_Function_Tag(const char *name, REBVAL *slot)
{
    Init_Tag(slot, Make_UTF8_May_Fail(name));
    SET_SER_FLAG(VAL_SERIES(slot), SERIES_FLAG_FIXED_SIZE);
    SET_SER_INFO(VAL_SERIES(slot), SERIES_INFO_LOCKED);
}


//
//  Init_Function_Tags: C
//
// FUNC and PROC search for these tags, like <opt> and <local>.  They are
// natives and run during bootstrap, so these string comparisons are
// needed.  This routine does not use a table directly, because the slots
// it initializes are not constants...and older TCCs don't support local
// struct arrays of that form.
//
static void Init_Function_Tags(void)
{
    Init_Function_Tag("with", ROOT_WITH_TAG);
    Init_Function_Tag("...", ROOT_ELLIPSIS_TAG);
    Init_Function_Tag("opt", ROOT_OPT_TAG);
    Init_Function_Tag("end", ROOT_END_TAG);
    Init_Function_Tag("local", ROOT_LOCAL_TAG);
    Init_Function_Tag("durable", ROOT_DURABLE_TAG);
    Init_Function_Tag("tight", ROOT_TIGHT_TAG);
}


//
//  Init_Natives: C
//
// Create native functions.  In R3-Alpha this would go as far as actually
// creating a NATIVE native by hand, and then run code that would call that
// native for each function.  Ren-C depends on having the native table
// initialized to run the evaluator (for instance to test functions against
// the RETURN native's FUNC signature in definitional returns).  So it
// "fakes it" just by calling a C function for each item...and there is no
// actual "native native".
//
// If there *were* a REBNATIVE(native) this would be its spec:
//
//  native: native [
//      spec [block!]
//      /body
//          {Body of user code matching native's behavior (for documentation)}
//      code [block!]
//  ]
//
static REBVAL *Init_Natives(REBARR *boot_natives)
{
    // !!! See notes on FUNCTION-META in %sysobj.r
    {
        REBCTX *function_meta = Alloc_Context(3);
        Append_Context(function_meta, NULL, Canon(SYM_DESCRIPTION));
        Append_Context(function_meta, NULL, Canon(SYM_RETURN_TYPE));
        Append_Context(function_meta, NULL, Canon(SYM_RETURN_NOTE));
        Append_Context(function_meta, NULL, Canon(SYM_PARAMETER_TYPES));
        Append_Context(function_meta, NULL, Canon(SYM_PARAMETER_NOTES));
        REBVAL *rootvar = CTX_VALUE(function_meta);
        VAL_RESET_HEADER(rootvar, REB_OBJECT);
        rootvar->extra.binding = NULL;
        Init_Object(ROOT_FUNCTION_META, function_meta);
    }

    // !!! Same, we want to have SPECIALIZE before %sysobj.r loaded
    {
        REBCTX *specialized_meta = Alloc_Context(3);
        Append_Context(specialized_meta, NULL, Canon(SYM_DESCRIPTION));
        Append_Context(specialized_meta, NULL, Canon(SYM_SPECIALIZEE));
        Append_Context(specialized_meta, NULL, Canon(SYM_SPECIALIZEE_NAME));
        REBVAL *rootvar = CTX_VALUE(specialized_meta);
        VAL_RESET_HEADER(rootvar, REB_OBJECT);
        rootvar->extra.binding = NULL;
        Init_Object(ROOT_SPECIALIZED_META, specialized_meta);
    }

    RELVAL *item = ARR_HEAD(boot_natives);
    REBCNT n = 0;
    REBVAL *action_word;

    while (NOT_END(item)) {
        if (n >= NUM_NATIVES)
            panic (item);

        // Each entry should be one of these forms:
        //
        //    some-name: native [spec content]
        //
        //    some-name: native/body [spec content] [equivalent user code]
        //
        // If more refinements are added, this code will have to be made
        // more sophisticated.
        //
        // Though the manual building of this table is not as nice as running
        // the evaluator, the evaluator makes comparisons against native
        // values.  Having all natives loaded fully before ever running
        // Do_Core() helps with stability and invariants.

        // Get the name the native will be started at with in Lib_Context
        //
        if (!IS_SET_WORD(item))
            panic (item);

        REBVAL *name = KNOWN(item);
        ++item;

        // See if it's being invoked with NATIVE or NATIVE/BODY
        //
        REBOOL has_body;
        if (IS_WORD(item)) {
            if (VAL_WORD_SYM(item) != SYM_NATIVE)
                panic (item);
            has_body = FALSE;
        }
        else {
            if (
                !IS_PATH(item)
                || VAL_LEN_HEAD(item) != 2
                || !IS_WORD(ARR_HEAD(VAL_ARRAY(item)))
                || VAL_WORD_SYM(ARR_HEAD(VAL_ARRAY(item))) != SYM_NATIVE
                || !IS_WORD(ARR_AT(VAL_ARRAY(item), 1))
                || VAL_WORD_SYM(ARR_AT(VAL_ARRAY(item), 1)) != SYM_BODY
            ) {
                panic (item);
            }
            has_body = TRUE;
        }
        ++item;

        // Grab the spec, and turn any <opt> into _ (the spec processor is low
        // level and does not understand <opt>)
        //
        if (!IS_BLOCK(item))
            panic (item);

        REBVAL *spec = KNOWN(item);
        assert(VAL_INDEX(spec) == 0); // must be at head (we don't copy)
        ++item;

        // With the components extracted, generate the native and add it to
        // the Natives table.  The associated C function is provided by a
        // table built in the bootstrap scripts, `Native_C_Funcs`.

        // We only want to check the return type in the debug build.  In the
        // release build, we want to have as few argument slots as possible...
        // especially to get the optimization for 1 argument to go in the cell
        // and not need to push arguments.
        //
        REBFLGS flags = MKF_KEYWORDS | MKF_FAKE_RETURN;

        REBFUN *fun = Make_Function(
            Make_Paramlist_Managed_May_Fail(KNOWN(spec), flags),
            Native_C_Funcs[n], // "dispatcher" is unique to this "native"
            NULL // no underlying function, this is fundamental
        );

        // If a user-equivalent body was provided, we save it in the native's
        // REBVAL for later lookup.
        //
        if (has_body) {
            if (!IS_BLOCK(item))
                panic (item);
            *FUNC_BODY(fun) = *KNOWN(item); // !!! handle relative?
            ++item;
        }

        Natives[n] = *FUNC_VALUE(fun);

        // Append the native to the Lib_Context under the name given.  Do
        // special case SET/LOOKBACK=TRUE (using Append_Context_Core) so
        // that SOME-ACTION: ACTION [...] allows ACTION to see the SOME-ACTION
        // symbol, and know to use it.
        //
        if (VAL_WORD_SYM(name) == SYM_ACTION) {
            *Append_Context_Core(Lib_Context, name, 0, TRUE) = Natives[n];
             action_word = name; // was bound by Append_Context_Core
        }
        else
            *Append_Context(Lib_Context, name, 0) = Natives[n];

        ++n;
    }

    if (n != NUM_NATIVES)
        panic ("Incorrect number of natives found during processing");

    return action_word;
}


//
//  Init_Actions: C
//
static void Init_Actions(REBARR *boot_actions, REBVAL *action_word)
{
    // With the natives registered (including ACTION), it's now safe to
    // run the evaluator to register the actions.
    //
    Do_Global_Block(boot_actions, 0, -1, action_word);

    // Sanity check the symbol transformation
    //
    if (0 != strcmp("open", cs_cast(STR_HEAD(Canon(SYM_OPEN)))))
        panic (Canon(SYM_OPEN));
}


//
//  Init_Root_Context: C
//
// Hand-build the root context where special REBOL values are
// stored. Called early, so it cannot depend on any other
// system structures or values.
//
// Note that the Root_Vars's word table is unset!
// None of its values are exported.
//
static void Init_Root_Context(void)
{
    REBCTX *root = Alloc_Context(ROOT_MAX - 1);
    PG_Root_Context = root;

    SET_SER_FLAG(CTX_VARLIST(root), SERIES_FLAG_FIXED_SIZE);
    Root_Vars = cast(ROOT_VARS*, ARR_HEAD(CTX_VARLIST(root)));

    // Get rid of the keylist, we will make another one later in the boot.
    // (You can't ASSERT_CONTEXT(PG_Root_Context) until that happens.)  The
    // new keylist will be managed so we manage the varlist to match.
    //
    // !!! We let it get GC'd, which is a bit wasteful, but the interface
    // for Alloc_Context() wants to manage the keylist in general.  This
    // is done for convenience.
    //
    /*Free_Array(CTX_KEYLIST(root));*/
    /*INIT_CTX_KEYLIST_UNIQUE(root, NULL);*/ // can't use with NULL
    AS_SERIES(CTX_VARLIST(root))->link.keylist = NULL;
    MANAGE_ARRAY(CTX_VARLIST(root));

    // !!! Also no `stackvars` (or `spec`, not yet implemented); revisit
    //
    VAL_RESET_HEADER(CTX_VALUE(root), REB_OBJECT);
    CTX_VALUE(root)->extra.binding = NULL;

    // Set all other values to blank
    {
        REBINT n = 1;
        REBVAL *var = CTX_VARS_HEAD(root);
        for (; n < ROOT_MAX; n++, var++) SET_BLANK(var);
        TERM_ARRAY_LEN(CTX_VARLIST(root), ROOT_MAX);
    }

    // These values are simple isolated VOID, NONE, TRUE, and FALSE values
    // that can be used in lieu of initializing them.  They are initialized
    // as two-element series in order to ensure that their address is not
    // treated as an array.
    //
    // They should only be accessed by macros which retrieve their values
    // as `const`, to avoid the risk of accidentally changing them.  (This
    // rule is broken by some special system code which `m_cast`s them for
    // the purpose of using them as directly recognizable pointers which
    // also look like values.)
    //
    // It is presumed that these types will never need to have GC behavior,
    // and thus can be stored safely in program globals without mention in
    // the root set.  Should that change, they could be explicitly added
    // to the GC's root set.

    SET_VOID(&PG_Void_Cell[0]);
    SET_TRASH_IF_DEBUG(&PG_Void_Cell[1]);

    SET_BLANK(&PG_Blank_Value[0]);
    SET_TRASH_IF_DEBUG(&PG_Blank_Value[1]);

    SET_BAR(&PG_Bar_Value[0]);
    SET_TRASH_IF_DEBUG(&PG_Bar_Value[1]);

    SET_FALSE(&PG_False_Value[0]);
    SET_TRASH_IF_DEBUG(&PG_False_Value[1]);

    SET_TRUE(&PG_True_Value[0]);
    SET_TRASH_IF_DEBUG(&PG_True_Value[1]);

    // We can't actually put an end value in the middle of a block, so we poke
    // this one into a program global.  It is not legal to bit-copy an
    // END (you always use SET_END), so we can make it unwritable.
    //
    PG_End_Cell.header.bits = NODE_FLAG_END; // read-only end
    assert(IS_END(END_CELL)); // sanity check that it took

    // The EMPTY_BLOCK provides EMPTY_ARRAY.  It is locked for protection.
    //
    Init_Block(ROOT_EMPTY_BLOCK, Make_Array(0));
    SET_SER_INFO(VAL_SERIES(ROOT_EMPTY_BLOCK), SERIES_INFO_LOCKED);
    SET_SER_FLAG(VAL_SERIES(ROOT_EMPTY_BLOCK), SERIES_FLAG_FIXED_SIZE);

    REBSER *empty_series = Make_Binary(1);
    *BIN_AT(empty_series, 0) = '\0';
    Init_String(ROOT_EMPTY_STRING, empty_series);
    SET_SER_INFO(VAL_SERIES(ROOT_EMPTY_STRING), SERIES_INFO_LOCKED);
    SET_SER_FLAG(VAL_SERIES(ROOT_EMPTY_STRING), SERIES_FLAG_FIXED_SIZE);

    // Used by REBNATIVE(print)
    //
    SET_CHAR(ROOT_SPACE_CHAR, ' ');

    // Can't ASSERT_CONTEXT here; no keylist yet...
}


//
//  Set_Root_Series: C
//
// Used to set block and string values in the ROOT context.
//
void Set_Root_Series(REBVAL *value, REBSER *ser)
{
    // Note that the Val_Init routines call Manage_Series and make the
    // series GC Managed.  They will hence be freed on shutdown
    // automatically when the root set is removed from consideration.

    if (Is_Array_Series(ser))
        Init_Block(value, AS_ARRAY(ser));
    else {
        assert(SER_WIDE(ser) == 1 || SER_WIDE(ser) == 2);
        Init_String(value, ser);
    }
}


//
//  Init_Task_Context: C
//
// See above notes (same as root context, except for tasks)
//
static void Init_Task_Context(void)
{
    REBCTX *task = Alloc_Context(TASK_MAX - 1);
    TG_Task_Context = task;

    SET_SER_FLAG(CTX_VARLIST(task), SERIES_FLAG_FIXED_SIZE);
    Task_Vars = cast(TASK_VARS*, ARR_HEAD(CTX_VARLIST(task)));

    // Get rid of the keylist, we will make another one later in the boot.
    // (You can't ASSERT_CONTEXT(TG_Task_Context) until that happens.)  The
    // new keylist will be managed so we manage the varlist to match.
    //
    // !!! We let it get GC'd, which is a bit wasteful, but the interface
    // for Alloc_Context() wants to manage the keylist in general.  This
    // is done for convenience.
    //
    /*Free_Array(CTX_KEYLIST(task));*/
    /*INIT_CTX_KEYLIST_UNIQUE(task, NULL);*/ // can't use with NULL
    AS_SERIES(CTX_VARLIST(task))->link.keylist = NULL;

    MANAGE_ARRAY(CTX_VARLIST(task));

    // !!! Also no `body` (or `spec`, not yet implemented); revisit
    //
    VAL_RESET_HEADER(CTX_VALUE(task), REB_OBJECT);
    CTX_VALUE(task)->extra.binding = NULL;

    // Set all other values to NONE:
    {
        REBINT n = 1;
        REBVAL *var = CTX_VARS_HEAD(task);
        for (; n < TASK_MAX; n++, var++) SET_BLANK(var);
        TERM_ARRAY_LEN(CTX_VARLIST(task), TASK_MAX);
    }

    // Initialize a few fields:
    SET_INTEGER(TASK_BALLAST, MEM_BALLAST);
    SET_INTEGER(TASK_MAX_BALLAST, MEM_BALLAST);

    // The thrown arg is not intended to ever be around long enough to be
    // seen by the GC.
    //
    SET_UNREADABLE_BLANK(&TG_Thrown_Arg);

    // Can't ASSERT_CONTEXT here; no keylist yet...
}


//
//  Init_System_Object: C
//
// Evaluate the system object and create the global SYSTEM word.  We do not
// BIND_ALL here to keep the internal system words out of the global context.
// (See also N_context() which creates the subobjects of the system object.)
//
static void Init_System_Object(
    RELVAL *boot_sysobj,
    RELVAL *boot_natives,
    RELVAL *boot_actions
) {
    // Create the system object from the sysobj block (defined in %sysobj.r)
    //
    REBCTX *system = Make_Selfish_Context_Detect(
        REB_OBJECT, // type
        NULL, // body
        VAL_ARRAY_HEAD(boot_sysobj), // scan for toplevel set-words
        NULL // parent
    );

    Bind_Values_Deep(VAL_ARRAY_HEAD(boot_sysobj), Lib_Context);

    // Bind it so CONTEXT native will work (only used at topmost depth)
    //
    Bind_Values_Shallow(VAL_ARRAY_HEAD(boot_sysobj), system);

    // Evaluate the block (will eval CONTEXTs within).  Expects void result.
    //
    REBVAL result;
    if (Do_At_Throws(&result, VAL_ARRAY(boot_sysobj), 0, SPECIFIED))
        panic (&result);
    if (!IS_VOID(&result))
        panic (&result);

    // Create a global value for it.  (This is why we are able to say `system`
    // and have it bound in lines like `sys: system/contexts/sys`)
    //
    Init_Object(
        Append_Context(Lib_Context, NULL, Canon(SYM_SYSTEM)),
        system
    );

    // We also add the system object under the root, to ensure it can't be
    // garbage collected and be able to access it from the C code.  (Someone
    // could say `system: blank` in the Lib_Context and then it would be a
    // candidate for garbage collection otherwise!)
    //
    Init_Object(ROOT_SYSTEM, system);

    // Create system/datatypes block.  Start at 1 (REB_BLANK), given that 0
    // is REB_0_LOOKBACK and does not correspond to a value type.
    //
    REBARR *array = VAL_ARRAY(Get_System(SYS_CATALOG, CAT_DATATYPES));
    Extend_Series(AS_SERIES(array), REB_MAX - 1);

    REBCNT n;
    for (n = 1; n < REB_MAX; n++) {
        Append_Value(array, CTX_VAR(Lib_Context, n));
    }

    // Create system/catalog/actions block
    //
    Init_Block(
        Get_System(SYS_CATALOG, CAT_ACTIONS),
        Collect_Set_Words(boot_actions)
    );

    // Create system/catalog/natives block
    //
    Init_Block(
        Get_System(SYS_CATALOG, CAT_NATIVES),
        Collect_Set_Words(boot_natives)
    );

    // Create system/codecs object
    //
    {
        REBCTX *codecs = Alloc_Context(10);
        SET_BLANK(CTX_ROOTKEY(codecs));
        VAL_RESET_HEADER(CTX_VALUE(codecs), REB_OBJECT);
        CTX_VALUE(codecs)->extra.binding = NULL;
        Init_Object(Get_System(SYS_CODECS, 0), codecs);
    }
}


//
//  Init_Contexts_Object: C
//
// This sets up the system/contexts object.
//
// !!! One of the critical areas in R3-Alpha that was not hammered out
// completely was the question of how the binding process gets started, and
// how contexts might inherit or relate.
//
// However, the basic model for bootstrap is that the "user context" is the
// default area for new code evaluation.  It starts out as a copy of an
// initial state set up in the lib context.  When native routines or other
// content gets overwritten in the user context, it can be borrowed back
// from `system/contexts/lib` (typically aliased as "lib" in the user context).
//
static void Init_Contexts_Object(void)
{
    DROP_GUARD_CONTEXT(Sys_Context);
    Init_Object(Get_System(SYS_CONTEXTS, CTX_SYS), Sys_Context);

    DROP_GUARD_CONTEXT(Lib_Context);
    Init_Object(Get_System(SYS_CONTEXTS, CTX_LIB), Lib_Context);
    Init_Object(Get_System(SYS_CONTEXTS, CTX_USER), Lib_Context);
}


//
//  Codec_Text: C
//
REBINT Codec_Text(int action, REBCDI *codi)
{
    codi->error = 0;

    if (action == CODI_ACT_IDENTIFY) {
        return CODI_CHECK; // error code is inverted result
    }

    if (action == CODI_ACT_DECODE) {
        return CODI_TEXT;
    }

    if (action == CODI_ACT_ENCODE) {
        return CODI_BINARY;
    }

    codi->error = CODI_ERR_NA;
    return CODI_ERROR;
}

//
//  Codec_UTF16: C
//
REBINT Codec_UTF16(int action, REBCDI *codi, REBOOL little_endian)
{
    codi->error = 0;

    if (action == CODI_ACT_IDENTIFY) {
        return CODI_CHECK; // error code is inverted result
    }

    if (action == CODI_ACT_DECODE) {
        REBSER *ser = Make_Unicode(codi->len);
        REBINT size = Decode_UTF16(
            UNI_HEAD(ser), codi->data, codi->len, little_endian, FALSE
        );
        SET_SERIES_LEN(ser, size);
        if (size < 0) { //ASCII
            REBSER *dst = Make_Binary((size = -size));
            Append_Uni_Bytes(dst, UNI_HEAD(ser), size);
            ser = dst;
        }
        codi->data = SER_DATA_RAW(ser); // !!! REBUNI?  REBYTE?
        codi->len = SER_LEN(ser);
        codi->w = SER_WIDE(ser);
        return CODI_TEXT;
    }

    if (action == CODI_ACT_ENCODE) {
        u16 * data = ALLOC_N(u16, codi->len);
        if (codi->w == 1) {
            /* in ASCII */
            REBCNT i = 0;
            for (i = 0; i < codi->len; i ++) {
            #ifdef ENDIAN_LITTLE
                if (little_endian) {
                    data[i] = cast(char*, codi->extra.other)[i];
                } else {
                    data[i] = cast(char*, codi->extra.other)[i] << 8;
                }
            #elif defined (ENDIAN_BIG)
                if (little_endian) {
                    data[i] = cast(char*, codi->extra.other)[i] << 8;
                } else {
                    data[i] = cast(char*, codi->extra.other)[i];
                }
            #else
                #error "Unsupported CPU endian"
            #endif
            }
        } else if (codi->w == 2) {
            /* already in UTF16 */
        #ifdef ENDIAN_LITTLE
            if (little_endian) {
                memcpy(data, codi->extra.other, codi->len * sizeof(u16));
            } else {
                REBCNT i = 0;
                for (i = 0; i < codi->len; i ++) {
                    REBUNI uni = cast(REBUNI*, codi->extra.other)[i];
                    data[i] = ((uni & 0xff) << 8) | ((uni & 0xff00) >> 8);
                }
            }
        #elif defined (ENDIAN_BIG)
            if (little_endian) {
                REBCNT i = 0;
                for (i = 0; i < codi->len; i ++) {
                    REBUNI uni = cast(REBUNI*, codi->extra.other)[i];
                    data[i] = ((uni & 0xff) << 8) | ((uni & 0xff00) >> 8);
                }
            } else {
                memcpy(data, codi->extra.other, codi->len * sizeof(u16));
            }
        #else
            #error "Unsupported CPU endian"
        #endif
        } else {
            /* RESERVED for future unicode expansion */
            codi->error = CODI_ERR_NA;
            return CODI_ERROR;
        }

        codi->len *= sizeof(u16);

        return CODI_BINARY;
    }

    codi->error = CODI_ERR_NA;
    return CODI_ERROR;
}


//
//  Codec_UTF16LE: C
//
REBINT Codec_UTF16LE(int action, REBCDI *codi)
{
    return Codec_UTF16(action, codi, TRUE);
}


//
//  Codec_UTF16BE: C
//
REBINT Codec_UTF16BE(int action, REBCDI *codi)
{
    return Codec_UTF16(action, codi, FALSE);
}


//
//  Register_Codec: C
//
// Internal function for adding a codec.
//
void Register_Codec(
    const char *name,
    const void *suffixes, // may be REBARR* or UTF-8 string
    codo dispatcher
) {
    REBVAL word;
    Init_Word(&word, Intern_UTF8_Managed(cb_cast(name), strlen(name)));

    REBVAL handle;
    Init_Handle_Simple(
        &handle,
        cast(CFUNC*, dispatcher), // code
        NULL // data
    );

    REBVAL suffixes_value;
    switch (Guess_Rebol_Pointer(suffixes)) {
    case GUESSED_AS_SERIES:
        Init_Block(&suffixes_value, AS_ARRAY(m_cast(void*, suffixes)));
        break;

    case GUESSED_AS_UTF8:
        Init_File(
            &suffixes_value,
            Make_UTF8_May_Fail(cast(const char*, suffixes))
        );
        break;

    default:
        panic (suffixes);
    }

    REBVAL *value = Append_Context(
        VAL_CONTEXT(Get_System(SYS_CODECS, 0)), &word, NULL
    );

    REBVAL codec;
    if (Apply_Only_Throws(
        value, TRUE, Sys_Func(SYS_CTX_REGISTER_CODEC_P),
        &word, &handle, &suffixes_value, END_CELL
    )) {
        fail (Error_No_Catch_For_Throw(value));
    }

    if (!IS_OBJECT(value))
        panic (value); // should have returned a codec object
}


//
//  Init_Task: C
//
void Init_Task(void)
{
    // Thread locals:
    Trace_Level = 0;
    Saved_State = 0;

    Eval_Cycles = 0;
    Eval_Dose = EVAL_DOSE;
    Eval_Count = Eval_Dose;
    Eval_Signals = 0;
    Eval_Sigmask = ALL_BITS;

    // errors? problem with PG_Boot_Phase shared?

    Init_Pools(-4);
    Init_GC();
    Init_Task_Context();    // Special REBOL values per task

    Init_Raw_Print();
    Init_Stacks(STACK_MIN/4);
    Init_Scanner();
    Init_Mold(MIN_COMMON/4);
    Init_Collector();
}


//
//  Init_Locale: C
//
void Init_Locale(void)
{
    REBCHR *data;

    if ((data = OS_GET_LOCALE(0))) {
        Init_String(
            Get_System(SYS_LOCALE, LOCALE_LANGUAGE),
            Copy_OS_Str(data, OS_STRLEN(data))
        );
        OS_FREE(data);
    }

    if ((data = OS_GET_LOCALE(1))) {
        Init_String(
            Get_System(SYS_LOCALE, LOCALE_LANGUAGE_P),
            Copy_OS_Str(data, OS_STRLEN(data))
        );
        OS_FREE(data);
    }

    if ((data = OS_GET_LOCALE(2))) {
        Init_String(
            Get_System(SYS_LOCALE, LOCALE_LOCALE),
            Copy_OS_Str(data, OS_STRLEN(data))
        );
        OS_FREE(data);
    }

    if ((data = OS_GET_LOCALE(3))) {
        Init_String(
            Get_System(SYS_LOCALE, LOCALE_LOCALE_P),
            Copy_OS_Str(data, OS_STRLEN(data))
        );
        OS_FREE(data);
    }
}


//
//  Init_Core: C
//
// Initialize the interpreter core.
//
// !!! This will either succeed or "panic".  Panic currently triggers an exit
// to the OS.  The code is not currently written to be able to cleanly shut
// down from a partial initialization.  (It should be.)
//
// The phases of initialization are tracked by PG_Boot_Phase.  Some system
// functions are unavailable at certain phases.
//
// Though most of the initialization is run as C code, some portions are run
// in Rebol.  For instance, ACTION is a function registered very early on in
// the boot process, which is run from within a block to register more
// functions.
//
// At the tail of the initialization, `finish-init-core` is run.  This Rebol
// function lives in %sys-start.r.   It should be "host agnostic" and not
// assume things about command-line switches (or even that there is a command
// line!)  Converting the code that made such assumptions ongoing.
//
void Init_Core(void)
{
#if defined(TEST_EARLY_BOOT_PANIC)
    //
    // It should be legal to panic at any time (especially given that the
    // bar for success is "crash")
    //
    panic ("early panic test");
#elif defined(TEST_EARLY_BOOT_FAIL)
    //
    // A fail should fall back on panic at this boot phase.
    //
    fail (Error(RE_NO_VALUE, BLANK_VALUE));
#endif

//==//////////////////////////////////////////////////////////////////////==//
//
// INITIALIZE BASIC DIAGNOSTICS
//
//==//////////////////////////////////////////////////////////////////////==//

#ifndef NDEBUG
    PG_Always_Malloc = FALSE;
#endif

    // Globals
    PG_Boot_Phase = BOOT_START;
    PG_Boot_Level = BOOT_LEVEL_FULL;
    PG_Mem_Usage = 0;
    PG_Mem_Limit = 0;
    Reb_Opts = ALLOC(REB_OPTS);
    CLEAR(Reb_Opts, sizeof(REB_OPTS));
    Saved_State = NULL;

    // Thread locals.
    //
    // !!! This code duplicates work done in Init_Task
    //
    Trace_Level = 0;
    Saved_State = 0;
    Eval_Dose = EVAL_DOSE;
    Eval_Count = Eval_Dose;
    Eval_Limit = 0;
    Eval_Signals = 0;
    Eval_Sigmask = ALL_BITS;

    Init_StdIO();

    Assert_Basics();
    PG_Boot_Time = OS_DELTA_TIME(0, 0);

//==//////////////////////////////////////////////////////////////////////==//
//
// INITIALIZE MEMORY AND ALLOCATORS
//
//==//////////////////////////////////////////////////////////////////////==//

    Init_Pools(0);          // Memory allocator
    Init_GC();
    Init_Root_Context();    // Special REBOL values per program
    Init_Task_Context();    // Special REBOL values per task

    Init_Raw_Print();       // Low level output (Print)

//==//////////////////////////////////////////////////////////////////////==//
//
// CREATE GLOBAL OBJECTS
//
//==//////////////////////////////////////////////////////////////////////==//

    Init_Char_Cases();
    Init_CRC();             // For word hashing
    Set_Random(0);
    Init_Words();
    Init_Stacks(STACK_MIN * 4);
    Init_Scanner();
    Init_Mold(MIN_COMMON);  // Output buffer
    Init_Collector();

//==//////////////////////////////////////////////////////////////////////==//
//
// LOAD BOOT BLOCK
//
//==//////////////////////////////////////////////////////////////////////==//

    // The %make-boot.r process takes all the various definitions and
    // mezzanine code and packs it into one compressed string in %b-boot.c
    // which gets embedded into the executable.  This includes the type list,
    // word list, error message templates, system object, mezzanines, etc.

    REBSER *utf8 = Decompress(
        Native_Specs, NAT_COMPRESSED_SIZE, NAT_UNCOMPRESSED_SIZE, FALSE, FALSE
    );
    if (utf8 == NULL || SER_LEN(utf8) != NAT_UNCOMPRESSED_SIZE)
        panic ("decompressed native specs size mismatch (try `make clean`)");

    REBARR *boot_array = Scan_UTF8_Managed(
        BIN_HEAD(utf8), NAT_UNCOMPRESSED_SIZE
    );
    PUSH_GUARD_ARRAY(boot_array); // managed, so must be guarded

    Free_Series(utf8); // don't need decompressed text after it's scanned

    BOOT_BLK *boot = cast(BOOT_BLK*, VAL_ARRAY_HEAD(ARR_HEAD(boot_array)));

    Init_Symbols(VAL_ARRAY(&boot->words));

    // STR_SYMBOL(), VAL_WORD_SYM() and Canon(SYM_XXX) now available

    PG_Boot_Phase = BOOT_LOADED;

    // Get the words of the ROOT context (to avoid it being an exception case)
    //
    REBARR *root_keylist = Collect_Keylist_Managed(
        NULL,
        VAL_ARRAY_HEAD(&boot->root),
        NULL,
        COLLECT_ANY_WORD
    );
    INIT_CTX_KEYLIST_UNIQUE(PG_Root_Context, root_keylist);
    ASSERT_CONTEXT(PG_Root_Context);

    AS_SERIES(CTX_VARLIST(PG_Root_Context))->header.bits
        |= NODE_FLAG_ROOT;

    // Get the words of the TASK context (to avoid it being an exception case)
    //
    REBARR *task_keylist = Collect_Keylist_Managed(
        NULL,
        VAL_ARRAY_HEAD(&boot->task),
        NULL,
        COLLECT_ANY_WORD
    );
    INIT_CTX_KEYLIST_UNIQUE(TG_Task_Context, task_keylist);
    ASSERT_CONTEXT(TG_Task_Context);

    AS_SERIES(CTX_VARLIST(TG_Task_Context))->header.bits
        |= NODE_FLAG_ROOT;

//==//////////////////////////////////////////////////////////////////////==//
//
// CREATE BASIC VALUES
//
//==//////////////////////////////////////////////////////////////////////==//

    // Before any code can start running (even simple bootstrap code), some
    // basic words need to be defined.  For instance: You can't run %sysobj.r
    // unless `true` and `false` have been added to the Lib_Context--they'd be
    // undefined.  And while analyzing the function specs during the
    // definition of natives, things like the <opt> tag are needed as a basis
    // for comparison to see if a usage matches that.

    // !!! Have MAKE-BOOT compute # of words
    //
    Lib_Context = Alloc_Context(600);
    VAL_RESET_HEADER(CTX_VALUE(Lib_Context), REB_OBJECT);
    CTX_VALUE(Lib_Context)->extra.binding = NULL;
    MANAGE_ARRAY(CTX_VARLIST(Lib_Context));
    PUSH_GUARD_CONTEXT(Lib_Context);

    Sys_Context = Alloc_Context(50);
    VAL_RESET_HEADER(CTX_VALUE(Sys_Context), REB_OBJECT);
    CTX_VALUE(Sys_Context)->extra.binding = NULL;
    MANAGE_ARRAY(CTX_VARLIST(Sys_Context));
    PUSH_GUARD_CONTEXT(Sys_Context);

    Init_Datatypes(
        VAL_ARRAY(&boot->types), VAL_ARRAY(&boot->typespecs)
    );
    Init_Typesets();
    Init_True_And_False();
    Init_Function_Tags();
    Add_Lib_Keys_R3Alpha_Cant_Make();

    SET_UNREADABLE_BLANK(&Callback_Error);

//==//////////////////////////////////////////////////////////////////////==//
//
// RUN CODE BEFORE ERROR HANDLING INITIALIZED
//
//==//////////////////////////////////////////////////////////////////////==//

    REBVAL *action_word = Init_Natives(VAL_ARRAY(&boot->natives));
    if (action_word == NULL)
        panic ("ACTION function was not found while processing natives");

    Init_Actions(VAL_ARRAY(&boot->actions), action_word);

    Init_System_Object(
        &boot->sysobj,
        &boot->natives,
        &boot->actions
    );

    Init_Contexts_Object();
    Init_Locale();

    Init_Errors(
        VAL_ARRAY(&boot->errors), // error definition list from %errors.r
        Get_System(SYS_STANDARD, STD_ERROR) // standard error template
    );

    PG_Boot_Phase = BOOT_ERRORS;

#if defined(TEST_MID_BOOT_PANIC)
    //
    // At this point panics should be able to do a reasonable job of giving
    // details on Rebol types.
    //
    panic (EMPTY_ARRAY);
#elif defined(TEST_MID_BOOT_FAIL)
    //
    // With no PUSH_TRAP yet, fail should give a localized assert in a debug
    // build, and panic the release build.
    //
    fail (Error(RE_NO_VALUE, BLANK_VALUE));
#endif

    // Special pre-made errors:
    Init_Error(TASK_STACK_ERROR, Error(RE_STACK_OVERFLOW));
    Init_Error(TASK_HALT_ERROR, Error(RE_HALT));

//==//////////////////////////////////////////////////////////////////////==//
//
// RUN MEZZANINE CODE NOW THAT ERROR HANDLING IS INITIALIZED
//
//==//////////////////////////////////////////////////////////////////////==//

    REBCTX *error;
    struct Reb_State state;

    // With error trapping enabled, set up to catch them if they happen.
    PUSH_UNHALTABLE_TRAP(&error, &state);

// The first time through the following code 'error' will be NULL, but...
// `fail` can longjmp here, so 'error' won't be NULL *if* that happens!

    if (error) {
        //
        // You shouldn't be able to halt during Init_Core() startup.
        // The only way you should be able to stop Init_Core() is by raising
        // an error, at which point the system will Panic out.
        //
        // !!! TBD: Enforce not being *able* to trigger HALT
        //
        assert(ERR_NUM(error) != RE_HALT);
        panic (error);
    }

    // !!! These currently execute using Do_Global_Block instead of through
    // an Apply of a system function.  Review the exact reason why that is
    // necessary (if it is), since all things being equal it's better to
    // run userspace code whenever possible.
    //
    Do_Global_Block(VAL_ARRAY(&boot->base), 0, 1, NULL);
    Do_Global_Block(VAL_ARRAY(&boot->sys), 0, 2, NULL);

    PG_Boot_Phase = BOOT_MEZZ;

    assert(DSP == 0 && FS_TOP == NULL);

    Init_Ports();

    // The FINISH-INIT-CORE function should theoretically do very little.
    // But right now it is where:
    //
    // * the mezzanine definitions are bound to the lib context and DO'd
    // * the various scheme handlers (http://, file://) are registered
    // * protocols implemented in user code (HTTP and TLS) are registered
    //
    // Over time those will be factored out so that only the distributions
    // that want these components will include them.  However, this shows
    // the explicit dependency by passing the mezzanine and the protocols
    // from the boot block to the finalization function as arguments.
    //
    REBVAL result;
    if (Apply_Only_Throws(
        &result,
        TRUE, // generate error if all arguments aren't consumed
        Sys_Func(SYS_CTX_FINISH_INIT_CORE), // %sys-start.r function to call
        &boot->mezz, // boot-mezz argument
        &boot->protocols, // boot-protocol argument
        END_CELL
    )) {
        // You shouldn't be able to throw any uncaught values during
        // Init_Core() startup, including throws implementing QUIT or EXIT.
        // A fail() would just jump up to the error delivery above, so a
        // panic here more clearly indicates the moment of the problem.
        //
        panic (&result);
    }

    if (!IS_VOID(&result)) {
        //
        // !!! `finish-init-core` Rebol code should return void, but it may be
        // that more graceful error delivery than a panic should be given if
        // it does not.  It may be that fairly legitimate circumstances which
        // the user could fix would cause a more ordinary message delivery.
        // For the moment, though, we panic on any non-void return result.
        // 
        panic (&result);
    }

    DROP_GUARD_ARRAY(boot_array);

    assert(DSP == 0 && FS_TOP == NULL);

    DROP_TRAP_SAME_STACKLEVEL_AS_PUSH(&state);

    PG_Boot_Phase = BOOT_DONE;

#if !defined(NDEBUG)
    //
    // This memory check from R3-Alpha is somewhat superfluous, but include a
    // call to it during Init in the debug build, because otherwise no one
    // will think to keep it up to date and working.
    //
    Check_Memory_Debug();
#endif

    Recycle(); // necessary?
}


//
//  Shutdown_Core: C
//
// The goal of Shutdown_Core() is to release all memory and resources that the
// interpreter has accrued since Init_Core().  This is a good "sanity check"
// that there aren't unaccounted-for leaks (or semantic errors which such
// leaks may indicate).
//
// Also, being able to clean up is important for a library...which might be
// initialized and shut down multiple times in the same program run.  But
// clients wishing a speedy exit may force an exit to the OS instead of doing
// a clean shut down.  (Note: There still might be some system resources
// that need to be waited on, such as asynchronous writes.)
//
// While some leaks are detected by the debug build during shutdown, even more
// can be found with a tool like Valgrind or Address Sanitizer.
//
void Shutdown_Core(void)
{
#if !defined(NDEBUG)
    //
    // This memory check from R3-Alpha is somewhat superfluous, but include a
    // call to it during Shutdown in the debug build, because otherwise no one
    // will think to keep it up to date and working.
    //
    Check_Memory_Debug();
#endif

    assert(!Saved_State);

    Shutdown_Stacks();

    // Run Recycle, but the TRUE flag indicates we want every series
    // that is managed to be freed.  (Only unmanaged should be left.)
    // We remove the only two root contexts that the Init_Core process added
    // -however- there may be other roots.  But by this point, the roots
    // created by Alloc_Pairing() with an owning context should be freed.
    //
    AS_SERIES(CTX_VARLIST(PG_Root_Context))->header.bits
        &= (~NODE_FLAG_ROOT);
    AS_SERIES(CTX_VARLIST(TG_Task_Context))->header.bits
        &= (~NODE_FLAG_ROOT);
    Recycle_Core(TRUE);

    Shutdown_Crypto();
    Shutdown_Ports();
    Shutdown_Event_Scheme();
    Shutdown_CRC();
    Shutdown_Mold();
    Shutdown_Scanner();
    Shutdown_Char_Cases();

    assert(PG_Num_Canon_Slots_In_Use - PG_Num_Canon_Deleteds == 0);
    Free_Series(PG_Canons_By_Hash);
    Free_Series(PG_Symbol_Canons);

    Shutdown_GC();

    // !!! Need to review the relationship between Open_StdIO (which the host
    // does) and Init_StdIO...they both open, and both close.

    Shutdown_StdIO();

    FREE(REB_OPTS, Reb_Opts);

    // Shutting down the memory manager must be done after all the Free_Mem
    // calls have been made to balance their Alloc_Mem calls.
    //
    Shutdown_Pools();
}
