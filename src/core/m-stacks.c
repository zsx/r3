//
//  File: %m-stack.c
//  Summary: "data and function call stack implementation"
//  Section: memory
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


#define CHUNKER_FROM_CHUNK(c) \
    cast(struct Reb_Chunker*, \
        cast(REBYTE*, (c)) - (c)->offset \
        - offsetof(struct Reb_Chunker, payload))

//
//  Init_Stacks: C
//
void Init_Stacks(REBCNT size)
{
    // We always keep one chunker around for the first chunk push, and prep
    // one chunk so that the push and drop routines never worry about testing
    // for the empty case.

    TG_Root_Chunker = cast(struct Reb_Chunker*, Alloc_Mem(BASE_CHUNKER_SIZE + CS_CHUNKER_PAYLOAD));
#if !defined(NDEBUG)
    memset(TG_Root_Chunker, 0xBD, sizeof(struct Reb_Chunker));
#endif
    TG_Root_Chunker->next = NULL;
    TG_Root_Chunker->size = CS_CHUNKER_PAYLOAD;
    TG_Top_Chunk = cast(struct Reb_Chunk*, &TG_Root_Chunker->payload);
    TG_Top_Chunk->prev = NULL;
    TG_Top_Chunk->size.bits = BASE_CHUNK_SIZE; // zero values for initial chunk
    TG_Top_Chunk->offset = 0;

    // Implicit termination trick--see VALUE_FLAG_NOT_END and related notes
    cast(
        struct Reb_Chunk*, cast(REBYTE*, TG_Top_Chunk) + BASE_CHUNK_SIZE
    )->size.bits = 0;
    assert(IS_END(&TG_Top_Chunk->values[0]));

    // Start the data stack out with just one element in it, and make it an
    // unwritable trash for the debug build.  This helps avoid both accidental
    // reads and writes of an empty stack, as well as meaning that indices
    // into the data stack can be unsigned (no need for -1 to mean empty,
    // because 0 can)
    {
        DS_Array = Make_Array(1);
        DS_Movable_Base = KNOWN(ARR_HEAD(DS_Array)); // can't push RELVALs

        SET_TRASH_SAFE(ARR_HEAD(DS_Array));
        MARK_CELL_UNWRITABLE_IF_CPP_DEBUG(ARR_HEAD(DS_Array));

        // The END marker will signal DS_PUSH that it has run out of space,
        // and it will perform the allocation at that time.
        //
        TERM_ARRAY_LEN(DS_Array, 1);
        ASSERT_ARRAY(DS_Array);

        // DS_PUSH checks what you're pushing isn't void, as most arrays can't
        // contain them.  But DS_PUSH_MAYBE_VOID allows you to, in case you
        // are building a context varlist or similar.
        //
        SET_ARR_FLAG(DS_Array, ARRAY_FLAG_VOIDS_LEGAL);

        // Reuse the expansion logic that happens on a DS_PUSH to get the
        // initial stack size.  It requires you to be on an END to run.  Then
        // drop the hypothetical thing pushed.
        //
        DS_Index = 1;
        Expand_Data_Stack_May_Fail(size);
        DS_DROP;
    }

    // Call stack (includes pending functions, parens...anything that sets
    // up a `struct Reb_Frame` and calls Do_Core())  Singly linked.
    //
    TG_Frame_Stack = NULL;
}


//
//  Shutdown_Stacks: C
//
void Shutdown_Stacks(void)
{
    assert(FS_TOP == NULL);

    assert(DSP == 0);
    Free_Array(DS_Array);

    assert(TG_Top_Chunk == cast(struct Reb_Chunk*, &TG_Root_Chunker->payload));

    // Because we always keep one chunker of headroom allocated, and the
    // push/drop is not designed to manage the last chunk, we *might* have
    // that next chunk of headroom still allocated.
    //
    if (TG_Root_Chunker->next)
        Free_Mem(TG_Root_Chunker->next, TG_Root_Chunker->next->size + BASE_CHUNKER_SIZE);

    // OTOH we always have to free the root chunker.
    //
    Free_Mem(TG_Root_Chunker, TG_Root_Chunker->size + BASE_CHUNKER_SIZE);
}


//
//  Expand_Data_Stack_May_Fail: C
//
// The data stack maintains an invariant that you may never push an END to it.
// So each push looks to see if it's pushing to a cell that contains an END
// and if so requests an expansion.
//
// WARNING: This will invalidate any extant pointers to REBVALs living in
// the stack.  It is for this reason that stack access should be done by
// REBDSP "data stack pointers" and not by REBVAL* across *any* operation
// which could do a push or pop.  (Currently stable w.r.t. pop but there may
// be compaction at some point.)
//
void Expand_Data_Stack_May_Fail(REBCNT amount)
{
    REBCNT len_old = ARR_LEN(DS_Array);

    // The current requests for expansion should only happen when the stack
    // is at its end.  Sanity check that.
    //
    assert(IS_END(DS_TOP));
    assert(DS_TOP == KNOWN(ARR_TAIL(DS_Array))); // can't push RELVALs
    assert(DS_TOP - KNOWN(ARR_HEAD(DS_Array)) == len_old);

    // If adding in the requested amount would overflow the stack limit, then
    // give a data stack overflow error.
    //
    if (SER_REST(ARR_SERIES(DS_Array)) + amount >= STACK_LIMIT)
        Trap_Stack_Overflow();

    Extend_Series(ARR_SERIES(DS_Array), amount);

    // Update the global pointer representing the base of the stack that
    // likely was moved by the above allocation.  (It's not necessarily a
    // huge win to cache it, but it turns data stack access from a double
    // dereference into a single dereference in the common case, and it was
    // how R3-Alpha did it).
    //
    DS_Movable_Base = KNOWN(ARR_HEAD(DS_Array)); // must do before using DS_TOP

    // We fill in the data stack with "GC safe trash" (which is void in the
    // release build, but will raise an alarm if VAL_TYPE() called on it in
    // the debug build).  In order to serve as a marker for the stack slot
    // being available, it merely must not be IS_END()...
    //
    REBVAL *value = DS_TOP;
    REBCNT len_new = len_old + amount;
    REBCNT n;
    for (n = len_old; n < len_new; ++n) {
        SET_TRASH_SAFE(value);
        ++value;
    }

    // Update the end marker to serve as the indicator for when the next
    // stack push would need to expand.
    //
    TERM_ARRAY_LEN(DS_Array, len_new);
    assert(value == ARR_TAIL(DS_Array));

    ASSERT_ARRAY(DS_Array);
}


//
//  Pop_Stack_Values: C
//
// Pops computed values from the stack to make a new ARRAY.
//
REBARR *Pop_Stack_Values(REBDSP dsp_start)
{
    REBCNT len = DSP - dsp_start;
    RELVAL *values = ARR_AT(DS_Array, dsp_start + 1);

    // Data stack should be fully specified--no relative values
    //
    REBARR *array = Copy_Values_Len_Shallow(values, SPECIFIED, len);

    DS_DROP_TO(dsp_start);
    return array;
}


//
//  Pop_Stack_Values_Into: C
//
// Pops computed values from the stack into an existing ANY-ARRAY.  The
// index of that array will be updated to the insertion tail (/INTO protocol)
//
void Pop_Stack_Values_Into(REBVAL *into, REBDSP dsp_start) {
    REBCNT len = DSP - dsp_start;
    REBVAL *values = KNOWN(ARR_AT(DS_Array, dsp_start + 1));

    assert(ANY_ARRAY(into));
    FAIL_IF_LOCKED_ARRAY(VAL_ARRAY(into));

    VAL_INDEX(into) = Insert_Series(
        ARR_SERIES(VAL_ARRAY(into)),
        VAL_INDEX(into),
        cast(REBYTE*, values), // stack only holds fully specified REBVALs
        len // multiplied by width (sizeof(REBVAL)) in Insert_Series
    );

    DS_DROP_TO(dsp_start);
}


//
//  Push_Ended_Trash_Chunk: C
//
// This doesn't necessarily call Alloc_Mem, because chunks are allocated
// sequentially inside of "chunker" blocks, in their ordering on the stack.
// Allocation is only required if we need to step into a new chunk (and even
// then only if we aren't stepping into a chunk that we are reusing from
// a prior expansion).
//
// The "Ended" indicates that there is no need to manually put an end in the
// `num_values` slot.  Chunks are implicitly terminated by their layout,
// because the low bit of subsequent chunks is set to 0, for data that does
// double-duty as a END marker.
//
REBVAL* Push_Ended_Trash_Chunk(REBCNT num_values) {
    const REBCNT size = BASE_CHUNK_SIZE + num_values * sizeof(REBVAL);

    // an extra Reb_Header is placed at the very end of the array to
    // denote a block terminator without a full REBVAL
    const REBCNT size_with_terminator = size + sizeof(struct Reb_Header);

    struct Reb_Chunker *chunker = CHUNKER_FROM_CHUNK(TG_Top_Chunk);

    struct Reb_Chunk *chunk;

    // Establish invariant where 'chunk' points to a location big enough to
    // hold the data (with data's size accounted for in chunk_size).  Note
    // that TG_Top_Chunk is never NULL, due to the initialization leaving
    // one empty chunk at the beginning and manually destroying it on
    // shutdown (this simplifies Push)
    const REBCNT payload_left = chunker->size - TG_Top_Chunk->offset
          - TG_Top_Chunk->size.bits;

    assert(chunker->size >= CS_CHUNKER_PAYLOAD);

    if (payload_left >= size_with_terminator) {
        //
        // Topmost chunker has space for the chunk *and* a pointer with the
        // END marker bit (e.g. last bit 0).  So advance past the topmost
        // chunk (whose size will depend upon num_values)
        //
        chunk = cast(struct Reb_Chunk*,
            cast(REBYTE*, TG_Top_Chunk) + TG_Top_Chunk->size.bits
        );

        // top's offset accounted for previous chunk, account for ours
        //
        chunk->offset = TG_Top_Chunk->offset + TG_Top_Chunk->size.bits;
    }
    else {
        //
        // Topmost chunker has insufficient space
        //

        REBOOL need_alloc = TRUE;
        if (chunker->next) {
            //
            // Previously allocated chunker exists already, check if it is big
            // enough
            //
            assert(!chunker->next->next);
            if (chunker->next->size >= size_with_terminator)
                need_alloc = FALSE;
            else
                Free_Mem(chunker->next, chunker->next->size + BASE_CHUNKER_SIZE);
        }
        if (need_alloc) {
            // No previously allocated chunker...we have to allocate it
            //
            const REBCNT payload_size = BASE_CHUNKER_SIZE
                + (size_with_terminator < CS_CHUNKER_PAYLOAD ?
                    CS_CHUNKER_PAYLOAD : (size_with_terminator << 1));
            chunker->next = cast(struct Reb_Chunker*, Alloc_Mem(payload_size));
            chunker->next->next = NULL;
            chunker->next->size = payload_size - BASE_CHUNKER_SIZE;
        }

        assert(chunker->next->size >= size_with_terminator);

        chunk = cast(struct Reb_Chunk*, &chunker->next->payload);
        chunk->offset = 0;
    }

    // The size does double duty to terminate the previous chunk's REBVALs
    // so that a full-sized REBVAL that is largely empty isn't needed to
    // convey IS_END().  It must yield its lowest two bits as zero to serve
    // this purpose, so WRITABLE_MASK_DEBUG and NOT_END_MASK will both
    // be false.  Our chunk should be a multiple of 4 bytes in total size,
    // but check that here with an assert.
    //
    // The memory address for the chunk size matches that of a REBVAL's
    // `header`, but since a `struct Reb_Chunk` is distinct from a REBVAL
    // they won't necessarily have write/read coherence, even though the
    // fields themselves are the same type.  Taking the address of the size
    // creates a pointer, which without a `restrict` keyword is defined as
    // being subject to "aliasing".  Hence a write to the pointer could affect
    // *any* other value of that type.  This is necessary for the trick.
    {
        struct Reb_Header *alias = &chunk->size;
        assert(size % 4 == 0);
        alias->bits = size;
    }

    // Set size also in next element to 0, so it can serve as a terminator
    // for the data range of this until it gets its real size (if ever)
    {
        // See note above RE: aliasing.
        //
        struct Reb_Header *alias = &cast(
            struct Reb_Chunk*,
            cast(REBYTE*, chunk) + size)->size;
        alias->bits = 0;
        assert(IS_END(&chunk->values[num_values]));
    }

    chunk->prev = TG_Top_Chunk;

    TG_Top_Chunk = chunk;

#if !defined(NDEBUG)
    //
    // In debug builds we make sure we put in GC-unsafe trash in the chunk.
    // This helps make sure that the caller fills in the values before a GC
    // ever actually happens.  (We could set it to void or something
    // GC-safe, but that might wind up being wasted work if unset is not
    // what the caller was wanting...so leave it to them.)
    {
        REBCNT index;
        for (index = 0; index < num_values; index++)
            INIT_CELL_IF_DEBUG(&chunk->values[index]);
    }
#endif

    assert(CHUNK_FROM_VALUES(&chunk->values[0]) == chunk);
    return &chunk->values[0];
}


//
//  Drop_Chunk: C
//
// Free an array of previously pushed REBVALs that are protected by GC.  This
// only occasionally requires an actual call to Free_Mem(), due to allocating
// call these arrays sequentially inside of chunks in memory.
//
void Drop_Chunk(REBVAL *opt_head)
{
    struct Reb_Chunk* chunk = TG_Top_Chunk;

    // Passing in `values` is optional, but a good check to make sure you are
    // actually dropping the chunk you think you are.  (On an error condition
    // when dropping chunks to try and restore the top chunk to a previous
    // state, this information isn't available.)
    //
    assert(!opt_head || CHUNK_FROM_VALUES(opt_head) == chunk);

    // Drop to the prior top chunk
    TG_Top_Chunk = chunk->prev;

    if (chunk->offset == 0) {
        // This chunk sits at the head of a chunker.

        struct Reb_Chunker *chunker = CHUNKER_FROM_CHUNK(chunk);

        assert(TG_Top_Chunk);

        // When we've completely emptied a chunker, we check to see if the
        // chunker after it is still live.  If so, we free it.  But we
        // want to keep *this* just-emptied chunker alive for overflows if we
        // rapidly get another push, to avoid Make_Mem()/Free_Mem() costs.

        if (chunker->next) {
            Free_Mem(chunker->next, chunker->next->size + BASE_CHUNKER_SIZE);
            chunker->next = NULL;
        }
    }

    // In debug builds we poison the memory for the chunk... but not the `prev`
    // pointer because we expect that to stick around!
    //
#if !defined(NDEBUG)
    memset(
        cast(REBYTE*, chunk) + sizeof(struct Reb_Chunk*),
        0xBD,
        chunk->size.bits - sizeof(struct Reb_Chunk*)
    );
    assert(IS_END(cast(REBVAL*, chunk)));
#endif
}


#if !defined(NDEBUG)
//
// Slow version, keep temporarily for the double-checking
//
REBFUN *Underlying_Function_Debug(
    REBFUN **specializer_out,
    const RELVAL *value
){
    REBOOL loop;
    do {
        loop = FALSE;

        // A specializer knows its underlying function because it is the
        // function its exemplar was designed for.
        //
        // Only the outermost specialized frame is needed.  It should have
        // taken into account the effects of any deeper specializations at the
        // time of creation, to cache the summation of the specilized args.
        //
        if (IS_FUNCTION_SPECIALIZER(value)) {
            *specializer_out = VAL_FUNC(value);

            REBCTX *exemplar = VAL_CONTEXT(VAL_FUNC_BODY(value));
            return VAL_FUNC(CTX_FRAME_FUNC_VALUE(exemplar));
        }

        while (IS_FUNCTION_ADAPTER(value)) {
            value = VAL_ARRAY_AT_HEAD(VAL_FUNC_BODY(value), 1);
            loop = TRUE;
        }

        while (IS_FUNCTION_CHAINER(value)) {
            value = VAL_ARRAY_AT_HEAD(VAL_FUNC_BODY(value), 0);
            loop = TRUE;
        }

        while (IS_FUNCTION_HIJACKER(value)) {
            //
            // The function that got hijacked needs to report the same
            // underlying function that it did before the hijacking.  The only
            // place that's stored is in the misc field

            REBFUN *underlying
                = ARR_SERIES(VAL_FUNC_PARAMLIST(value))->misc.underlying;

            if (underlying == VAL_FUNC(value))
                break; // hijacking was of a fundamental function

            value = FUNC_VALUE(underlying);
            loop = TRUE;
        }
    } while (loop);

    *specializer_out = NULL; // underlying function is not specializing.

    // A plain function may be a re-relativization (due to hijacking) with
    // what was originally a body made for another function.  In that case,
    // the frame needs to be "for that", so it is the underlying function.

    if (IS_FUNCTION_PLAIN(value)) {
        RELVAL *body = VAL_FUNC_BODY(value);
        assert(IS_RELATIVE(body));
        return VAL_RELATIVE(body);
    }

    return VAL_FUNC(value);
}
#endif


//
//  Underlying_Function: C
//
// The concept of the "underlying" function is that which has the right
// number of arguments for the frame to be built--and which has the actual
// correct paramlist identity to use for binding in adaptations.
//
// So if you specialize a plain function with 2 arguments so it has just 1,
// and then specialize the specialization so that it has 0, your call still
// needs to be building a frame with 2 arguments.  Because that's what the
// code that ultimately executes--after the specializations are peeled away--
// will expect.
//
// And if you adapt an adaptation of a function, the keylist referred to in
// the frame has to be the one for the inner function.  Using the adaptation's
// parameter list would write variables the adapted code wouldn't read.
//
// For efficiency, the underlying pointer is cached in the function paramlist.
// However, it may take two steps, if there is a specialization to take into
// account...because the specialization is needed to get the exemplar frame.
//
REBFUN *Underlying_Function(
    REBFUN **specializer_out,
    const REBVAL *value
) {
    REBFUN *underlying;

    // If the function is itself a specialization, then capture it and then
    // return its underlying function.
    //
    if (IS_FUNCTION_SPECIALIZER(value)) {
        *specializer_out = VAL_FUNC(value);
        underlying = ARR_SERIES(VAL_FUNC_PARAMLIST(value))->misc.underlying;
        goto return_and_check;
    }

    underlying = ARR_SERIES(VAL_FUNC_PARAMLIST(value))->misc.underlying;

    if (!IS_FUNCTION_SPECIALIZER(FUNC_VALUE(underlying))) {
        //
        // If the function isn't a specialization and its underlying function
        // isn't either, that means there are no specializations in this
        // composition.  Note the underlying function pointer may be itself!
        //
        *specializer_out = NULL;
        goto return_and_check;
    }

    // If the underlying function is a specialization, that means this is
    // an adaptation or chaining of specializations.  The next underlying
    // link should be to the real underlying function, digging under all
    // specializations.

    *specializer_out = underlying;
    underlying = ARR_SERIES(FUNC_PARAMLIST(underlying))->misc.underlying;

return_and_check:

    // This should be the terminal point in the chain of underlyingness, and
    // it cannot itself be a specialization/adaptation/etc.
    //
    assert(
        underlying
        == ARR_SERIES(FUNC_PARAMLIST(underlying))->misc.underlying
    );
    assert(!IS_FUNCTION_SPECIALIZER(FUNC_VALUE(underlying)));
    assert(!IS_FUNCTION_CHAINER(FUNC_VALUE(underlying)));
    assert(!IS_FUNCTION_ADAPTER(FUNC_VALUE(underlying)));

#if !defined(NDEBUG)
    REBFUN* specializer_check;
    REBFUN* underlying_check = Underlying_Function_Debug(
        &specializer_check, value
    );
    assert(underlying == underlying_check);
    assert(*specializer_out == specializer_check);
#endif

    return underlying;
}


//
//  Push_Or_Alloc_Args_For_Underlying_Func: C
//
// Allocate the series of REBVALs inspected by a function when executed (the
// values behind D_ARG(1), D_REF(2), etc.)
//
// If the function is a specialization, then the parameter list of that
// specialization will have *fewer* parameters than the full function would.
// For this reason we push the arguments for the "underlying" function.
// Yet if there are specialized values, they must be filled in from the
// exemplar frame.
//
// So adaptations must "dig" in order to find a specialization, to use an
// "exemplar" frame.
//
// Specializations must "dig" in order to find the underlying function.
//
REBFUN *Push_Or_Alloc_Args_For_Underlying_Func(struct Reb_Frame *f) {
    //
    // We need the actual REBVAL of the function here, and not just the REBFUN.
    // This is true even though you can get an archetype REBVAL from a function
    // pointer with FUNC_VALUE().  That archetype--as with RETURN and LEAVE--
    // will not carry the specific `binding` information of a value.
    //
    assert(IS_FUNCTION(f->gotten));

    // The underlying function is whose parameter list must be enumerated.
    // Even though this underlying function can have more arguments than the
    // "interface" function being called from f->gotten, any parameters more
    // than in that interface won't be gathered at the callsite because they
    // will not contain END markers.
    //
    REBFUN *specializer;
    REBFUN *underlying = Underlying_Function(&specializer, f->gotten);
    REBCNT num_args = FUNC_NUM_PARAMS(underlying);
    f->param = FUNC_PARAMS_HEAD(underlying);

    REBVAL *slot;
    if (IS_FUNC_DURABLE(underlying)) {
        //
        // !!! It's hoped that stack frames can be "hybrids" with some pooled
        // allocated vars that survive a call, and some that go away when the
        // stack frame is finished.  The groundwork for this is laid but it's
        // not quite ready--so the classic interpretation is that it's all or
        // nothing (similar to FUNCTION! vs. CLOSURE! in this respect)
        //
        f->stackvars = NULL;
        f->varlist = Make_Array(num_args + 1);
        TERM_ARRAY_LEN(f->varlist, num_args + 1);
        SET_ARR_FLAG(f->varlist, SERIES_FLAG_FIXED_SIZE);

        // Skip the [0] slot which will be filled with the CTX_VALUE
        // !!! Note: Make_Array made the 0 slot an end marker
        //
        SET_TRASH_IF_DEBUG(ARR_AT(f->varlist, 0));
        f->arg = slot = SINK(ARR_AT(f->varlist, 1));
    }
    else {
        // We start by allocating the data for the args and locals on the chunk
        // stack.  However, this can be "promoted" into being the data for a
        // frame context if it becomes necessary to refer to the variables
        // via words or an object value.  That object's data will still be this
        // chunk, but the chunk can be freed...so the words can't be looked up.
        //
        // Note that chunks implicitly have an END at the end; no need to
        // put one there.
        //
        f->varlist = NULL;
        f->stackvars = Push_Ended_Trash_Chunk(num_args);
        assert(CHUNK_LEN_FROM_VALUES(f->stackvars) == num_args);
        f->arg = slot = &f->stackvars[0];
    }

    // Make_Call does not fill the args in the frame--that's up to Do_Core
    // and Apply_Block as they go along.  But the frame has to survive
    // Recycle() during arg fulfillment, slots can't be left uninitialized.
    // END markers are used in the slots, since the array is being built and
    // not yet shown to GC--and can be distinguished from "void" which might
    // be a meaningful value for some specialization forms.

    if (specializer) {
        REBCTX *exemplar = VAL_CONTEXT(FUNC_BODY(specializer));
        REBVAL *special_arg = CTX_VARS_HEAD(exemplar);

        while (num_args) {
            if (IS_VOID(special_arg)) {
                if (f->flags & DO_FLAG_APPLYING)
                    SET_VOID(slot);
                else
                    SET_END(slot);
            }
            else {
                *slot = *special_arg;
            }
            ++slot;
            ++special_arg;
            --num_args;
        }

        f->flags |= DO_FLAG_EXECUTE_FRAME; // void is "unspecialized" not <opt>
    }
    else if (f->flags & DO_FLAG_APPLYING) {
        //
        // The APPLY code is giving users access to the variables with words,
        // and they cannot contain END markers.
        //
        while (num_args) {
            SET_VOID(slot);
            ++slot;
            --num_args;
        }
    }
    else {
        while (num_args) { // memset() to 0 empirically slower than this loop
            SET_END(slot);
            ++slot;
            --num_args;
        }
    }

    assert(IS_END(slot));

    f->func = VAL_FUNC(f->gotten);
    f->binding = VAL_BINDING(f->gotten);

    return underlying;
}


//
//  Context_For_Frame_May_Reify_Core: C
//
// A Reb_Frame does not allocate a REBSER for its frame to be used in the
// context by default.  But one can be allocated on demand, even for a NATIVE!
// in order to have a binding location for the debugger (for instance).
// If it becomes necessary to create words bound into the frame that is
// another case where the frame needs to be brought into existence.
//
// If there's already a frame this will return it, otherwise create it.
//
// The result of this operation will not necessarily give back a managed
// context.  All cases can't be managed because it may be in a partial state
// (of fulfilling function arguments), and may contain bad data in the varlist.
// But if it has already been managed, it will be returned that way.
//
REBCTX *Context_For_Frame_May_Reify_Core(struct Reb_Frame *f) {
    assert(Is_Any_Function_Frame(f)); // varargs reifies while still pending

    REBCTX *context;
    struct Reb_Chunk *chunk;

    if (f->varlist != NULL) {
        if (GET_ARR_FLAG(f->varlist, ARRAY_FLAG_VARLIST))
            return AS_CONTEXT(f->varlist); // already a context!

        // We have our function call's args in an array, but it is not yet
        // a context.  !!! Really this cannot reify if we're in arg gathering
        // mode, calling MANAGE_ARRAY is illegal -- need test for that !!!

        assert(IS_TRASH_DEBUG(ARR_AT(f->varlist, 0))); // we fill this in
        assert(GET_ARR_FLAG(f->varlist, SERIES_FLAG_HAS_DYNAMIC));

        context = AS_CONTEXT(f->varlist);
    }
    else {
        REBSER *series = Make_Series(
            1, // length report will not come from this, but from end marker
            sizeof(REBVAL),
            MKS_NO_DYNAMIC // use the REBVAL in the REBSER--no allocation
        );
        SET_SER_FLAG(series, SERIES_FLAG_ARRAY);

        f->varlist = AS_ARRAY(series);

        assert(!GET_ARR_FLAG(f->varlist, SERIES_FLAG_HAS_DYNAMIC));
        SET_ARR_FLAG(f->varlist, SERIES_FLAG_FIXED_SIZE);

        context = AS_CONTEXT(f->varlist);

        SET_CTX_FLAG(context, CONTEXT_FLAG_STACK);
        SET_CTX_FLAG(context, SERIES_FLAG_ACCESSIBLE);
    }

    SET_ARR_FLAG(CTX_VARLIST(context), ARRAY_FLAG_VARLIST);

    // We do not Manage_Context, because we are reusing a word series here
    // that has already been managed.  The arglist array was managed when
    // created and kept alive by Mark_Call_Frames
    //
    REBFUN *specializer;
    REBFUN *underlying = Underlying_Function(&specializer, FUNC_VALUE(f->func));
    INIT_CTX_KEYLIST_SHARED(context, FUNC_PARAMLIST(underlying));
    ASSERT_ARRAY_MANAGED(CTX_KEYLIST(context));

    // We do not manage the varlist, because we'd like to be able to free
    // it *if* nothing happens that causes it to be managed.  Note that
    // initializing word REBVALs that are bound into it will ensure
    // managedness, as will creating a REBVAL for it.
    //
    assert(NOT(IS_ARRAY_MANAGED(CTX_VARLIST(context))));

    // When in ET_FUNCTION or ET_LOOKBACK, the arglist will be marked safe from
    // GC. It is managed because the pointer makes its way into bindings that
    // ANY-WORD! values may have, and they need to not crash.
    //
    // !!! Note that theoretically pending mode arrays do not need GC
    // access as no running code could get them, but the debugger is
    // able to access this information.  This is under review for how it
    // might be stopped.
    //
    VAL_RESET_HEADER(CTX_VALUE(context), REB_FRAME);
    CTX_VALUE(context)->payload.any_context.varlist = CTX_VARLIST(context);
    INIT_CONTEXT_FRAME(context, f);

    // A reification of a frame for native code should not allow changing
    // the values out from under it, because that could cause it to crash
    // the interpreter.  (Generally speaking, modification should only be
    // possible in the debugger anyway.)  For now, protect unless it's a
    // user function.
    //
    if (NOT(IS_FUNCTION_PLAIN(FUNC_VALUE(f->func))))
        SET_ARR_FLAG(CTX_VARLIST(context), SERIES_FLAG_LOCKED);

    return context;
}


//
//  Context_For_Frame_May_Reify_Managed: C
//
REBCTX *Context_For_Frame_May_Reify_Managed(struct Reb_Frame *f)
{
    assert(Is_Any_Function_Frame(f) && NOT(Is_Function_Frame_Fulfilling(f)));

    REBCTX *context = Context_For_Frame_May_Reify_Core(f);
    ENSURE_ARRAY_MANAGED(CTX_VARLIST(context));

    // Finally we mark the flags to say this contains a valid frame, so that
    // future calls to this routine will return it instead of making another.
    // This flag must be cleared when the call is finished (as the Reb_Frame
    // will be blown away if there's an error, no concerns about that).
    //
    ASSERT_CONTEXT(context);
    return context;
}


//
//  Drop_Function_Args_For_Frame_Core: C
//
// This routine needs to be shared with the error handling code.  It would be
// nice if it were inlined into Do_Core...but repeating the code just to save
// the function call overhead is second-guessing the optimizer and would be
// a cause of bugs.
//
// Note that in response to an error, we do not want to drop the chunks,
// because there are other clients of the chunk stack that may be running.
// Hence the chunks will be freed by the error trap helper.
//
void Drop_Function_Args_For_Frame_Core(struct Reb_Frame *f, REBOOL drop_chunks)
{
    f->flags &= ~DO_FLAG_EXECUTE_FRAME;

    if (drop_chunks && f->stackvars) {
        Drop_Chunk(f->stackvars);
    }

    if (f->varlist == NULL) goto finished;

    assert(GET_ARR_FLAG(f->varlist, SERIES_FLAG_ARRAY));

    if (NOT(IS_ARRAY_MANAGED(f->varlist))) {
        //
        // It's an array, but hasn't become managed yet...either because
        // it couldn't be (args still being fulfilled, may have bad cells) or
        // didn't need to be (no Context_For_Frame_May_Reify_Managed).  We
        // can just free it.
        //
        Free_Array(f->varlist);
        goto finished;
    }

    // The varlist might have been for indefinite extent variables, or it
    // might be a stub holder for a stack context.

    ASSERT_ARRAY_MANAGED(f->varlist);

    if (NOT(GET_ARR_FLAG(f->varlist, CONTEXT_FLAG_STACK))) {
        //
        // If there's no stack memory being tracked by this context, it
        // has dynamic memory and is being managed by the garbage collector
        // so there's nothing to do.
        //
        assert(GET_ARR_FLAG(f->varlist, SERIES_FLAG_HAS_DYNAMIC));
        goto finished;
    }

    // It's reified but has its data pointer into the chunk stack, which
    // means we have to free it and mark the array inaccessible.

    assert(GET_ARR_FLAG(f->varlist, ARRAY_FLAG_VARLIST));
    assert(NOT(GET_ARR_FLAG(f->varlist, SERIES_FLAG_HAS_DYNAMIC)));

    assert(GET_ARR_FLAG(f->varlist, SERIES_FLAG_ACCESSIBLE));
    CLEAR_ARR_FLAG(f->varlist, SERIES_FLAG_ACCESSIBLE);

finished:

#if !defined(NDEBUG)
    f->stackvars = cast(REBVAL*, 0xDECAFBAD);
    f->varlist = cast(REBARR*, 0xDECAFBAD);
#endif

    return; // needed for release build so `finished:` labels a statement
}


#if !defined(NDEBUG)

//
//  FRM_ARG_Debug: C
// 
// Debug-only version of getting a variable out of a call
// frame, which asserts if you use an index that is higher
// than the number of arguments in the frame.
//
REBVAL *FRM_ARG_Debug(struct Reb_Frame *frame, REBCNT n)
{
    REBVAL *var;
    assert(n != 0 && n <= FRM_NUM_ARGS(frame));

    var = &frame->arg[n - 1];
    assert(!THROWN(var));
    assert(NOT(GET_VAL_FLAG(var, VALUE_FLAG_RELATIVE)));

    return var;
}

#endif
