/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Module:  c-error.c
**  Summary: error handling
**  Section: core
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/


#include "sys-core.h"


/***********************************************************************
**
*/	void Push_Trap_Helper(REBOL_STATE *s)
/*
**		Used by both TRY and TRY_ANY, whose differentiation comes
**		from how they react to HALT.
**
***********************************************************************/
{
	assert(Saved_State || (DSP == -1 && !DSF));

	s->dsp = DSP;
	s->dsf = DSF;

	s->series_guard_tail = GC_Series_Guard->tail;
	s->value_guard_tail = GC_Value_Guard->tail;
	s->gc_disable = GC_Disabled;

	s->manuals_tail = SERIES_TAIL(GC_Manuals);

	s->last_state = Saved_State;
	Saved_State = s;

	// !!! garbage collector should probably walk Saved_State stack to
	// keep the error values alive from GC, so use a "safe" trash.
	SET_TRASH_SAFE(&s->error);
}


/***********************************************************************
**
*/	REBOOL Trapped_Helper_Halted(REBOL_STATE *state)
/*
**		This is used by both PUSH_TRAP and PUSH_UNHALTABLE_TRAP to do
**		the work of responding to a longjmp.  (Hence it is run when
**		setjmp returns TRUE.)  Its job is to safely recover from
**		a sudden interruption, though the list of things which can
**		be safely recovered from is finite.  Among the countless
**		things that are not handled automatically would be a memory
**		allocation.
**
**		(Note: This is a crucial difference between C and C++, as
**		C++ will walk up the stack at each level and make sure
**		any constructors have their associated destructors run.
**		*Much* safer for large systems, though not without cost.
**		Rebol's greater concern is not so much the cost of setup
**		for stack unwinding, but being able to be compiled without
**		requiring a C++ compiler.)
**
**		Returns whether the trapped error was a RE_HALT or not.
**
***********************************************************************/
{
	struct Reb_Call *call = CS_Top;
	REBOOL halted;

	// You're only supposed to throw an error.
	assert(IS_ERROR(&state->error));

	halted = VAL_ERR_NUM(&state->error) == RE_HALT;

	// Restore Rebol call stack frame at time of Push_Trap
	while (call != state->dsf) {
		struct Reb_Call *prior = call->prior;
		Free_Call(call);
		call = prior;
	}
	SET_DSF(state->dsf);

	// Restore Rebol data stack pointer at time of Push_Trap
	DS_DROP_TO(state->dsp);

	// Free any manual series that were extant at the time of the error
	// (that were created since this PUSH_TRAP started)
	assert(GC_Manuals->tail >= state->manuals_tail);
	while (GC_Manuals->tail != state->manuals_tail) {
		// Freeing the series will update the tail...
		Free_Series(cast(REBSER**, GC_Manuals->data)[GC_Manuals->tail - 1]);
	}

	GC_Series_Guard->tail = state->series_guard_tail;
	GC_Value_Guard->tail = state->value_guard_tail;

	GC_Disabled = state->gc_disable;

	Saved_State = state->last_state;

	return halted;
}


/***********************************************************************
**
*/	void Convert_Name_To_Thrown_Debug(REBVAL *name, const REBVAL *arg)
/*
**		Debug-only version of CONVERT_NAME_TO_THROWN
**
**		Sets a task-local value to be associated with the name and
**		mark it as the proxy value indicating a THROW().
**
***********************************************************************/
{
	assert(!THROWN(name));
	VAL_SET_OPT(name, OPT_VALUE_THROWN);

	assert(IS_TRASH(TASK_THROWN_ARG));
	assert(!IS_TRASH(arg));

	*TASK_THROWN_ARG = *arg;
}


/***********************************************************************
**
*/	void Catch_Thrown_Debug(REBVAL *out, REBVAL *thrown)
/*
**		Debug-only version of TAKE_THROWN_ARG
**
**		Gets the task-local value associated with the thrown,
**		and clears the thrown bit from thrown.
**
**		WARNING: 'out' can be the same pointer as 'thrown'
**
***********************************************************************/
{
	assert(THROWN(thrown));
	VAL_CLR_OPT(thrown, OPT_VALUE_THROWN);

	assert(!IS_TRASH(TASK_THROWN_ARG));

	*out = *TASK_THROWN_ARG;

	SET_TRASH_SAFE(TASK_THROWN_ARG);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Raise_Core(const REBVAL *err)
/*
**		Cause a "trap" of an error by longjmp'ing to the enclosing
**		PUSH_TRAP or PUSH_TRAP_ANY.  Although the error being passed
**		may not be something that strictly represents an error
**		condition (e.g. a BREAK or CONTINUE or THROW), if it gets
**		passed to this routine then it has not been caught by its
**		intended recipient, and is being treated as an error.
**
***********************************************************************/
{
	ASSERT_ERROR(err);

#if !defined(NDEBUG)
	// If we throw the error we'll lose the stack, and if it's an early
	// error we always want to see it (do not use ATTEMPT or TRY on
	// purpose in Init_Core()...)
	if (PG_Boot_Phase < BOOT_DONE) {
		Debug_Fmt("** Error raised during Init_Core(), should not happen!");
		Debug_Fmt("%v", err);
		assert(FALSE);
	}
#endif

	if (!Saved_State) {
		// Print out the error before crashing
		Print_Value(err, 0, FALSE);
		panic Error_0(RE_NO_SAVED_STATE);
	}

	if (Trace_Level) {
		if (THROWN(err)) {
			// !!! Write some kind of error tracer for errors that do not
			// have frames, so you can trace quits/etc.
		} else
			Debug_Fmt(
				cs_cast(BOOT_STR(RS_TRACE, 10)),
				&VAL_ERR_VALUES(err)->type,
				&VAL_ERR_VALUES(err)->id
			);
	}

	// Error may live in a local variable whose stack is going away, or
	// other unstable location.  Copy before the jump.

	Saved_State->error = *err;

	// If a THROWN() was being processed up the stack when the error was
	// raised, then it had the thrown argument set.  We ensure that it is
	// not set any longer (even in release builds, this is needed to keep
	// it from having a hold on the GC of the thrown value).

	SET_TRASH_SAFE(TASK_THROWN_ARG);

	LONG_JUMP(Saved_State->cpu_state, 1);
}


/***********************************************************************
**
*/	void Trap_Stack_Overflow(void)
/*
**		See comments on C_STACK_OVERFLOWING.  This routine is
**		deliberately separate and simple so that it allocates no
**		objects or locals...and doesn't run any code that itself
**		might wind up calling C_STACK_OVERFLOWING.
**
***********************************************************************/
{
	if (!Saved_State) panic Error_0(RE_NO_SAVED_STATE);

	Saved_State->error = *TASK_STACK_ERROR; // pre-allocated

	LONG_JUMP(Saved_State->cpu_state, 1);
}


/***********************************************************************
**
*/	REBCNT Stack_Depth(void)
/*
***********************************************************************/
{
	struct Reb_Call *call = DSF;
	REBCNT count = 0;

	while (call) {
		count++;
		call = PRIOR_DSF(call);
	}

	return count;
}


/***********************************************************************
**
*/	REBSER *Make_Backtrace(REBINT start)
/*
**		Return a block of backtrace words.
**
***********************************************************************/
{
	REBCNT depth = Stack_Depth();
	REBSER *blk = Make_Array(depth - start);
	struct Reb_Call *call;
	REBVAL *val;

	for (call = DSF; call != NULL; call = PRIOR_DSF(call)) {
		if (start-- <= 0) {
			val = Alloc_Tail_Array(blk);
			Val_Init_Word_Unbound(val, REB_WORD, VAL_WORD_SYM(DSF_LABEL(call)));
		}
	}

	return blk;
}


/***********************************************************************
**
*/	REBVAL *Find_Error_For_Code(REBVAL *id_out, REBVAL *type_out, REBCNT code)
/*
**		Find the id word, the error type (category) word, and the error
**		message template block-or-string for a given error number.
**
**		This scans the data which is loaded into the boot file by
**		processing %errors.r
**
**		If the message is not found, return NULL.  Will not write to
**		`id_out` or `type_out` unless returning a non-NULL pointer.
**
***********************************************************************/
{
	// See %errors.r for the list of data which is loaded into the boot
	// file as objects for the "error catalog"
	REBSER *categories = VAL_OBJ_FRAME(Get_System(SYS_CATALOG, CAT_ERRORS));

	REBSER *category;
	REBCNT n;
	REBVAL *message;

	// Find the correct catalog category
	n = code / 100; // 0 for Special, 1 for Internal...
	if (n + 1 > SERIES_TAIL(categories)) // +1 account for SELF
		return NULL;

	// Get frame of object representing the elements of the category itself
	if (!IS_OBJECT(FRM_VALUE(categories, n + 1))) {
		assert(FALSE);
		return NULL;
	}
	category = VAL_OBJ_FRAME(FRM_VALUE(categories, n + 1));

	// Find the correct template in the catalog category (see %errors.r)
	n = code % 100; // 0-based order within category
	if (n + 3 > SERIES_TAIL(category)) // +3 account for SELF, CODE: TYPE:
		return NULL;

	// Sanity check CODE: field of category object
	if (!IS_INTEGER(FRM_VALUE(category, 1))) {
		assert(FALSE);
		return NULL;
	}
	assert(
		cast(REBCNT, VAL_INT32(FRM_VALUE(category, 1))) == (code / 100) * 100
	);

	// Sanity check TYPE: field of category object
	// !!! Same spelling as what we set in VAL_WORD_SYM(type_out))?
	if (!IS_STRING(FRM_VALUE(category, 2))) {
		assert(FALSE);
		return NULL;
	}

	message = FRM_VALUE(category, n + 3);

	// Error message template must be string or block
	assert(IS_BLOCK(message) || IS_STRING(message));

	// Success! Write category word from the category list frame key sym,
	// and specific error ID word from the frame key sym within category
	Val_Init_Word_Unbound(
		type_out,
		REB_WORD,
		VAL_TYPESET_SYM(FRM_KEY(categories, (code / 100) + 1))
	);
	Val_Init_Word_Unbound(
		id_out,
		REB_WORD,
		VAL_TYPESET_SYM(FRM_KEY(category, (code % 100) + 3))
	);

	return message;
}


/***********************************************************************
**
*/	void Val_Init_Error(REBVAL *out, REBSER *frame)
/*
***********************************************************************/
{
	ENSURE_FRAME_MANAGED(frame);

	VAL_SET(out, REB_ERROR);
	VAL_ERR_OBJECT(out) = frame;

	ASSERT_ERROR(out);
}


#if !defined(NDEBUG)

/***********************************************************************
**
*/	static REBSER *Make_Guarded_Arg123_Error_Frame(void)
/*
**	Needed only for compatibility trick to "fake in" ARG1: ARG2: ARG3:
**
**	Rebol2 and R3-Alpha errors were limited to three arguments with
**	fixed names, arg1 arg2 arg3.  (Though R3 comments alluded to
**	the idea that MAKE ERROR! from an OBJECT! would inherit that
**	object's fields, it did not actually work.)  With FAIL and more
**	flexible error creation this is being extended.
**
**	Change is not made to the root error object because there is no
**	"moment" to effect that (e.g. <r3-legacy> mode will not be started
**	at boot time, it happens after).  This allows the stock args to be
**	enabled and disabled dynamically in the legacy settings, at the
**	cost of creating a new error object each time.
**
**	To make code handling it like the regular error frame (and keep that
**	code "relatively uncontaminated" by the #ifdefs), it must behave
**	as GC managed.  So it has to be guarded, thus the client drops the
**	guard and it will wind up being freed since it's not in the root set.
**	This is a bit inefficient but it's for legacy mode only, so best
**	to bend to the expectations of the non-legacy code.
**
***********************************************************************/
{
	REBSER *root_frame = VAL_OBJ_FRAME(ROOT_ERROBJ);
	REBCNT len = SERIES_LEN(root_frame);
	REBSER *frame = Make_Frame(len + 3, TRUE);
	REBVAL *key = FRM_KEY(frame, 0);
	REBVAL *value = FRM_VALUE(frame, 0);
	REBCNT n;

	for (n = 0; n < len; n++, key++, value++) {
		if (n == 0) continue; // skip SELF:
		*key = *FRM_KEY(root_frame, n);
		*value = *FRM_VALUE(root_frame, n);
		assert(IS_TYPESET(key));
	}

	for (n = 0; n < 3; n++, key++, value++) {
		Val_Init_Typeset(key, ALL_64, SYM_ARG1 + n);
		SET_NONE(value);
	}

	SET_END(key);
	SET_END(value);

	frame->tail = len + 3;
	FRM_KEYLIST(frame)->tail = len + 3;

	ASSERT_FRAME(frame);
	MANAGE_FRAME(frame);
	PUSH_GUARD_SERIES(frame);
	return frame;
}

#endif


/***********************************************************************
**
*/	REBFLG Make_Error_Object_Throws(REBVAL *out, REBVAL *arg)
/*
**		Creates an error object from arg and puts it in value.
**		The arg can be a string or an object body block.
**
**		Returns TRUE if a THROWN() value is made during evaluation.
**
**		This function is called by MAKE ERROR!.  Note that most often
**		system errors from %errors.r are thrown by C code using
**		Make_Error(), but this routine accommodates verification of
**		errors created through user code...which may be mezzanine
**		Rebol itself.  A goal is to not allow any such errors to
**		be formed differently than the C code would have made them,
**		and to cross through the point of R3-Alpha error compatibility,
**		which makes this a rather tortured routine.  However, it
**		maps out the existing landscape so that if it is to be changed
**		then it can be seen exactly what is changing.
**
***********************************************************************/
{
	// Frame from the error object template defined in %sysobj.r
	REBSER *root_frame = VAL_OBJ_FRAME(ROOT_ERROBJ);

	REBSER *frame;
	ERROR_OBJ *error_obj;

#if !defined(NDEBUG)
	if (LEGACY(OPTIONS_ARG1_ARG2_ARG3_ERROR))
		root_frame = Make_Guarded_Arg123_Error_Frame();
#endif

	if (IS_ERROR(arg) || IS_OBJECT(arg)) {
		// Create a new error object from another object, including any
		// non-standard fields.  WHERE: and NEAR: will be overridden if
		// used.  If ID:, TYPE:, or CODE: were used in a way that would
		// be inconsistent with a Rebol system error, an error will be
		// raised later in the routine.

		frame = Merge_Frames(
			root_frame,
			IS_ERROR(arg) ? VAL_ERR_OBJECT(arg) : VAL_OBJ_FRAME(arg)
		);
		error_obj = ERR_VALUES(frame);
	}
	else if (IS_BLOCK(arg)) {
		// If a block, then effectively MAKE OBJECT! on it.  Afterward,
		// apply the same logic as if an OBJECT! had been passed in above.

		REBVAL evaluated;

		// Bind and do an evaluation step (as with MAKE OBJECT! with A_MAKE
		// code in REBTYPE(Object) and code in REBNATIVE(construct))

		frame = Make_Object(root_frame, VAL_BLK_DATA(arg));
		Rebind_Frame(root_frame, frame);
		Bind_Values_Deep(VAL_BLK_DATA(arg), frame);

		if (DO_ARRAY_THROWS(&evaluated, arg)) {
			*out = evaluated;

		#if !defined(NDEBUG)
			// Let our fake root_frame that had arg1: arg2: arg3: on it be
			// garbage collected.
			if (LEGACY(OPTIONS_ARG1_ARG2_ARG3_ERROR))
				DROP_GUARD_SERIES(root_frame);
		#endif

			return TRUE;
		}

		error_obj = ERR_VALUES(frame);
	}
	else if (IS_STRING(arg)) {
		// String argument to MAKE ERROR! makes a custom error from user:
		//
		//     code: 1000 ;-- default none
		//     type: 'user
		//     id: 'message
		//     message: "whatever the string was" ;-- default none
		//
		// Minus the code number and message, this is the default state of
		// root_frame if not overridden.

		frame = Copy_Array_Shallow(root_frame);
		MANAGE_SERIES(frame);
		error_obj = ERR_VALUES(frame);

		assert(IS_NONE(&error_obj->code));
		// fill in RE_USER (1000) later if it passes the check

		Val_Init_String(&error_obj->message, Copy_Sequence_At_Position(arg));
	}
	else {
		// No other argument types are handled by this routine at this time.

		raise Error_1(RE_INVALID_ERROR, arg);
	}

	// Validate the error contents, and reconcile message template and ID
	// information with any data in the object.  Do this for the IS_STRING
	// creation case just to make sure the rules are followed there too.

	// !!! Note that this code is very cautious because the goal isn't to do
	// this as efficiently as possible, rather to put up lots of alarms and
	// traffic cones to make it easy to pick and choose what parts to excise
	// or tighten in an error enhancement upgrade.

	if (IS_INTEGER(&error_obj->code)) {
		if (VAL_INT32(&error_obj->code) < RE_USER) {
			// Users can make up anything for error codes allocated to them,
			// but Rebol's historical default is to "own" error codes less
			// than 1000.  If a code is used in the sub-1000 range then make
			// sure any id or type provided do not conflict.

			REBVAL id;
			REBVAL type;
			REBVAL *message;

			if (!IS_NONE(&error_obj->message)) // assume a MESSAGE: is wrong
				raise Error_1(RE_INVALID_ERROR, arg);

			message = Find_Error_For_Code(
				&id,
				&type,
				cast(REBCNT, VAL_INT32(&error_obj->code))
			);

			if (!message)
				raise Error_1(RE_INVALID_ERROR, arg);

			error_obj->message = *message;

			if (!IS_NONE(&error_obj->id)) {
				if (
					!IS_WORD(&error_obj->id)
					|| !SAME_SYM(
						VAL_WORD_SYM(&error_obj->id), VAL_WORD_SYM(&id)
					)
				) {
					raise Error_1(RE_INVALID_ERROR, arg);
				}
			}
			error_obj->id = id; // normalize binding and case

			if (!IS_NONE(&error_obj->type)) {
				if (
					!IS_WORD(&error_obj->id)
					|| !SAME_SYM(
						VAL_WORD_SYM(&error_obj->type), VAL_WORD_SYM(&type)
					)
				) {
					raise Error_1(RE_INVALID_ERROR, arg);
				}
			}
			error_obj->type = type; // normalize binding and case

			// !!! TBD: Check that all arguments were provided!
		}
	}
	else if (IS_WORD(&error_obj->type) && IS_WORD(&error_obj->id)) {
		// If there was no CODE: supplied but there was a TYPE: and ID: then
		// this may overlap a combination used by Rebol where we wish to
		// fill in the code.  (No fast lookup for this, must search.)

		REBSER *categories = VAL_OBJ_FRAME(Get_System(SYS_CATALOG, CAT_ERRORS));
		REBVAL *category;

		assert(IS_NONE(&error_obj->code));

		// Find correct category for TYPE: (if any)
		category = Find_Word_Value(categories, VAL_WORD_SYM(&error_obj->type));
		if (category) {
			REBCNT code;
			REBVAL *message;

			assert(IS_OBJECT(category)); // SELF: 0

			assert(
				SAME_SYM(VAL_TYPESET_SYM(VAL_OBJ_KEY(category, 1)), SYM_CODE)
			);
			assert(IS_INTEGER(VAL_OBJ_VALUE(category, 1)));
			code = cast(REBCNT, VAL_INT32(VAL_OBJ_VALUE(category, 1)));

			assert(
				SAME_SYM(VAL_TYPESET_SYM(VAL_OBJ_KEY(category, 2)), SYM_TYPE)
			);
			assert(IS_STRING(VAL_OBJ_VALUE(category, 2)));

			// Find correct message for ID: (if any)
			message = Find_Word_Value(
				VAL_OBJ_FRAME(category), VAL_WORD_SYM(&error_obj->id)
			);

			if (message) {
				assert(IS_STRING(message) || IS_BLOCK(message));

				if (!IS_NONE(&error_obj->message))
					raise Error_1(RE_INVALID_ERROR, arg);

				error_obj->message = *message;

				SET_INTEGER(&error_obj->code,
					code
					+ Find_Word_Index(frame, VAL_WORD_SYM(&error_obj->id), FALSE)
					- Find_Word_Index(frame, SYM_TYPE, FALSE)
					- 1
				);
			}
			else {
				// At the moment, we don't let the user make a user-ID'd
				// error using a category from the internal list just
				// because there was no id from that category.  In effect
				// all the category words have been "reserved"

				// !!! Again, remember this is all here just to show compliance
				// with what the test suite tested for, it disallowed e.g.
				// it expected the following to be an illegal error because
				// the `script` category had no `set-self` error ID.
				//
				//     make error! [type: 'script id: 'set-self]

				raise Error_1(RE_INVALID_ERROR, arg);
			}
		}
		else {
			// The type and category picked did not overlap any existing one
			// so let it be a user error.
			SET_INTEGER(&error_obj->code, RE_USER);
		}
	}
	else {
		// It's either a user-created error or otherwise.  It may
		// have bad ID, TYPE, or message fields, or a completely
		// strange code #.  The question of how non-standard to
		// tolerate is an open one.

		// For now we just write 1000 into the error code field, if that was
		// not already there.

		if (IS_NONE(&error_obj->code))
			SET_INTEGER(&error_obj->code, RE_USER);
		else if (IS_INTEGER(&error_obj->code)) {
			if (VAL_INT32(&error_obj->code) != RE_USER)
				raise Error_1(RE_INVALID_ERROR, arg);
		}
		else
			raise Error_1(RE_INVALID_ERROR, arg);

		// !!! Because we will experience crashes in the molding logic,
		// we put some level of requirement besides "code # not 0".
		// This is conservative logic and not good for general purposes.

		if (
			!(IS_WORD(&error_obj->id) || IS_NONE(&error_obj->id))
			|| !(IS_WORD(&error_obj->type) || IS_NONE(&error_obj->type))
			|| !(
				IS_BLOCK(&error_obj->message)
				|| IS_STRING(&error_obj->message)
				|| IS_NONE(&error_obj->message)
			)
		) {
			raise Error_1(RE_INVALID_ERROR, arg);
		}
	}

	assert(IS_INTEGER(&error_obj->code));

#if !defined(NDEBUG)
	// Let our fake root_frame that had arg1: arg2: arg3: on it be
	// garbage collected.
	if (LEGACY(OPTIONS_ARG1_ARG2_ARG3_ERROR))
		DROP_GUARD_SERIES(root_frame);
#endif

	Val_Init_Error(out, frame);
	return FALSE;
}


/***********************************************************************
**
*/	REBSER *Make_Error_Core(REBCNT code, const char *c_file, int c_line, va_list *args)
/*
**		(va_list by pointer: http://stackoverflow.com/a/3369762/211160)
**
**		Create and init a new error object.  Should not be able to
**		fail...will Panic if it does (avoids the failure to call
**		va_end on a longjmp).
**
***********************************************************************/
{
	REBSER *root_frame = VAL_OBJ_FRAME(ROOT_ERROBJ);

	REBSER *frame; // Error object frame
	ERROR_OBJ *error_obj; // Error object values
	REBVAL *message;
	REBVAL id;
	REBVAL type;

	REBCNT expected_args;

	assert(code != 0);

#if !defined(NDEBUG)
	if (LEGACY(OPTIONS_ARG1_ARG2_ARG3_ERROR))
		root_frame = Make_Guarded_Arg123_Error_Frame();
#endif

	if (PG_Boot_Phase < BOOT_ERRORS) {
		Panic_Core(code, NULL, c_file, c_line, args);
		DEAD_END;
	}

	message = Find_Error_For_Code(&id, &type, code);
	assert(message);

	if (IS_BLOCK(message)) {
		// For a system error coming from a C vararg call, the # of
		// GET-WORD!s in the format block should match the varargs supplied.

		REBVAL *temp = VAL_BLK_HEAD(message);
		expected_args = 0;
		while (NOT_END(temp)) {
			if (IS_GET_WORD(temp))
				expected_args++;
			else
				assert(IS_STRING(temp));
			temp++;
		}
	}
	else {
		// Just a string, no arguments expected.

		assert(IS_STRING(message));
		expected_args = 0;
	}

#if !defined(NDEBUG)
	// !!! We have the C source file and line information for where the
	// error was triggered (since Make_Error_Core calls all originate
	// from C source, as opposed to the user path where the error
	// is made in the T_Object dispatch).  Add them in the error so
	// they can be seen with PROBE but not when FORM'd to users
	expected_args += 2;
#endif

	if (expected_args == 0) {
		// If there are no arguments, we don't need to make a new keylist...
		// just a new valuelist to hold this instance's settings. (root
		// frame keylist is already managed)

		frame = Copy_Array_Shallow(root_frame);
	}
	else {
		REBVAL *key;
		REBVAL *value;
		REBVAL *temp;

		// Should the error be well-formed, we'll need room for the new
		// expected values *and* their new keys in the keylist.
		//
		frame = Copy_Array_Extra_Shallow(root_frame, expected_args);
		FRM_KEYLIST(frame) = Copy_Array_Extra_Shallow(
			FRM_KEYLIST(root_frame), expected_args
		);

		key = BLK_SKIP(FRM_KEYLIST(frame), SERIES_LEN(root_frame));
		value = BLK_SKIP(frame, SERIES_LEN(root_frame));

	#ifdef NDEBUG
		temp = VAL_BLK_HEAD(message);
	#else
		// Will get here even for a parameterless string due to throwing in
		// the extra "arguments" of the __FILE__ and __LINE__
		temp = IS_STRING(message) ? END_VALUE : VAL_BLK_HEAD(message);
	#endif

		while (NOT_END(temp)) {
			if (IS_GET_WORD(temp)) {
				REBVAL *arg = va_arg(*args, REBVAL*);

				if (!arg) {
				#if defined(NDEBUG)
					// If the C code passed too few args in a debug build,
					// prevent a crash in the release build by filling it
					// in with a NONE.
					arg = NONE_VALUE;
				#else
					Debug_Fmt(
						"too few args passed for error code %d at %s line %d",
						code,
						c_file,
						c_line
					);
					assert(FALSE);

					// !!! Note that we have no way of checking for too *many*
					// args with C's vararg machinery--no NULL termination
					// unless you explicitly put it in (and that's messy, so
					// we don't want to do it)
				}
				#endif

				ASSERT_VALUE_MANAGED(arg);

				Val_Init_Typeset(key, ALL_64, VAL_WORD_SYM(temp));
				*value = *arg;

				key++;
				value++;
			}
			temp++;
		}

	#if !defined(NDEBUG)
		// error/__FILE__ (a FILE! value)
		Val_Init_Typeset(key, ALL_64, SYM___FILE__);
		key++;
		Val_Init_File(
			value,
			Append_UTF8(NULL, cb_cast(c_file), LEN_BYTES(cb_cast(c_file)))
		);
		value++;

		// error/__LINE__ (an INTEGER! value)
		Val_Init_Typeset(key, ALL_64, SYM___LINE__);
		key++;
		SET_INTEGER(value, c_line);
		value++;
	#endif

		SET_END(key);
		SET_END(value);

		// Fix up the tail (was not done so automatically);
		SERIES_TAIL(FRM_KEYLIST(frame)) += expected_args;
		SERIES_TAIL(frame) += expected_args;

		MANAGE_SERIES(FRM_KEYLIST(frame));
	}

	MANAGE_SERIES(frame);

	error_obj = ERR_VALUES(frame);

	// Set error number:
	SET_INTEGER(&error_obj->code, code);

	error_obj->message = *message;
	error_obj->id = id;
	error_obj->type = type;

	// Set backtrace and location information:
	if (DSF) {
		// Where (what function) is the error:
		Val_Init_Block(&error_obj->where, Make_Backtrace(0));
		// Nearby location of the error (in block being evaluated):
		error_obj->nearest = *DSF_WHERE(DSF);
	}

#if !defined(NDEBUG)
	// Let our fake root_frame that had arg1: arg2: arg3: on it be
	// garbage collected.
	if (LEGACY(OPTIONS_ARG1_ARG2_ARG3_ERROR))
		DROP_GUARD_SERIES(root_frame);
#endif

	return frame;
}


/***********************************************************************
**
*/	REBSER *Make_Error(REBCNT num, ...)
/*
***********************************************************************/
{
	va_list args;
	REBSER *frame;

	va_start(args, num);

#ifdef NDEBUG
	frame = Make_Error_Core(num, NULL, 0, &args);
#else
	frame = Make_Error_Core(num, __FILE__, __LINE__, &args);
#endif

	va_end(args);

	return frame;
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Null(REBCNT num, ... /* REBVAL *arg1, REBVAL *arg2, ... NULL */)
/*
**		This is a variadic function which is designed to be the
**		"argument" of either a `raise` or a `panic` "keyword".
**		It can be called directly, or indirectly by another proxy
**		error function.  It takes a number of REBVAL* arguments
**		appropriate for the error number passed.
**
**		Although it is made to look like an argument to an action,
**		this function actually does the Raising or Panic'ing.  The
**		macro keywords only set which failure type to put in effect,
**		and in debug builds that macro also captures the file and
**		line number at the point of invocation.  This routine then
**		reads those global values.
**
**		If no `raise` or `panic` was in effect, Error() will assert
**		regarding the missing instruction.
**
**		(See complete explanation in notes on `raise` and `panic`)
**
***********************************************************************/
{
	va_list args;

	switch (TG_Fail_Prep) {
		// Default ok anywhere: http://stackoverflow.com/questions/3110088/
		default:
			Debug_Fmt("UNKNOWN TG_Fail_Prep VALUE: %d", TG_Fail_Prep);
			assert(FALSE);
			/* fallthrough */ // line can't be blank for Coverity
		case FAIL_UNPREPARED:
			Debug_Fmt("FAIL_UNPREPARED in Error()");
			assert(FALSE);
			/* fallthrough */ // line can't be blank for Coverity
		case FAIL_PREP_PANIC:
			va_start(args, num);
			Panic_Core(num, NULL, TG_Fail_C_File, TG_Fail_C_Line, &args);
			// crashed!

		case FAIL_PREP_RAISE: {
			REBVAL error;

			// Clear fail prep flag so `raise` status doesn't linger
			// (also, Make_Error_Core may Panic--we might assert no prep)
			TG_Fail_Prep = FAIL_UNPREPARED;

			va_start(args, num);
			Val_Init_Error(
				&error,
				Make_Error_Core(num, TG_Fail_C_File, TG_Fail_C_Line, &args)
			);
			va_end(args);

			Raise_Core(&error);
			// longjmp'd!
		}
	}

	DEAD_END;
}


#if !defined(NDEBUG)

/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_0_Debug(REBCNT num)
/*
**		Debug-only version of Error_0 macro that checks arg types.
**
***********************************************************************/
{
	Error_Null(num, NULL);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_1_Debug(REBCNT num, const REBVAL *arg1)
/*
**		Debug-only version of Error_1 macro that checks arg types.
**
***********************************************************************/
{
	Error_Null(num, arg1, NULL);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_2_Debug(REBCNT num, const REBVAL *arg1, const REBVAL *arg2)
/*
**		Debug-only version of Error_2 macro that checks arg types.
**
***********************************************************************/
{
	Error_Null(num, arg1, arg2, NULL);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_3_Debug(REBCNT num, const REBVAL *arg1, const REBVAL *arg2, const REBVAL *arg3)
/*
**		Debug-only version of Error_3 macro that checks arg types.
**
***********************************************************************/
{
	Error_Null(num, arg1, arg2, arg3, NULL);
}

#endif



/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Bad_Func_Def(const REBVAL *spec, const REBVAL *body)
/*
***********************************************************************/
{
	// !!! Improve this error; it's simply a direct emulation of arity-1
	// error that existed before refactoring code out of MT_Function().

	REBVAL def;
	REBSER *series = Make_Array(2);
	Append_Value(series, spec);
	Append_Value(series, body);
	Val_Init_Block(&def, series);

	Error_1(RE_BAD_FUNC_DEF, &def);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_No_Arg(const REBVAL *label, const REBVAL *key)
/*
***********************************************************************/
{
	REBVAL key_word;
	assert(IS_TYPESET(key));
	Val_Init_Word_Unbound(&key_word, REB_WORD, VAL_TYPESET_SYM(key));
	Error_2(RE_NO_ARG, label, &key_word);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Invalid_Datatype(REBCNT id)
/*
***********************************************************************/
{
	REBVAL id_value;
	SET_INTEGER(&id_value, id);
	Error_1(RE_INVALID_DATATYPE, &id_value);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_No_Memory(REBCNT bytes)
/*
***********************************************************************/
{
	REBVAL bytes_value;
	SET_INTEGER(&bytes_value, bytes);
	Error_1(RE_NO_MEMORY, &bytes_value);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Invalid_Arg(const REBVAL *value)
/*
**		This error is pretty vague...it's just "invalid argument"
**		and the value with no further commentary or context.  It
**		becomes a catch all for "unexpected input" when a more
**		specific error would be more useful.
**
***********************************************************************/
{
	Error_1(RE_INVALID_ARG, value);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_No_Catch_For_Throw(REBVAL *thrown)
/*
***********************************************************************/
{
	REBVAL arg;
	assert(THROWN(thrown));
	CATCH_THROWN(&arg, thrown); // clears bit

	if (IS_NONE(thrown))
		Error_1(RE_NO_CATCH, &arg);
	else
		Error_2(RE_NO_CATCH_NAMED, &arg, thrown);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Has_Bad_Type(const REBVAL *value)
/*
**		<type> type is not allowed here
**
***********************************************************************/
{
	Error_1(RE_INVALID_TYPE, Type_Of(value));
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Out_Of_Range(const REBVAL *arg)
/*
**		value out of range: <value>
**
***********************************************************************/
{
	Error_1(RE_OUT_OF_RANGE, arg);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Protected_Key(REBVAL *key)
/*
***********************************************************************/
{
	REBVAL key_name;
	assert(IS_TYPESET(key));
	Val_Init_Word_Unbound(&key_name, REB_WORD, VAL_TYPESET_SYM(key));

	Error_1(RE_LOCKED_WORD, &key_name);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Illegal_Action(REBCNT type, REBCNT action)
/*
***********************************************************************/
{
	REBVAL action_word;
	Val_Init_Word_Unbound(&action_word, REB_WORD, Get_Action_Sym(action));

	Error_2(RE_CANNOT_USE, &action_word, Get_Type(type));
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Math_Args(enum Reb_Kind type, REBCNT action)
/*
***********************************************************************/
{
	REBVAL action_word;
	Val_Init_Word_Unbound(&action_word, REB_WORD, Get_Action_Sym(action));

	Error_2(RE_NOT_RELATED, &action_word, Get_Type(type));
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Unexpected_Type(enum Reb_Kind expected, enum Reb_Kind actual)
/*
***********************************************************************/
{
	assert(expected != REB_END && expected < REB_MAX);
	assert(actual != REB_END && actual < REB_MAX);

	// if (type2 == REB_END) Error_1(errnum, Get_Type(type1));
	raise Error_2(RE_EXPECT_VAL, Get_Type(expected), Get_Type(actual));
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Arg_Type(const struct Reb_Call *call, const REBVAL *param, const REBVAL *arg_type)
/*
**		Function in frame of `call` expected parameter `param` to be
**		a type different than the arg given (which had `arg_type`)
**
***********************************************************************/
{
	REBVAL param_word;
	assert(IS_TYPESET(param));
	Val_Init_Word_Unbound(&param_word, REB_WORD, VAL_TYPESET_SYM(param));

	assert(IS_DATATYPE(arg_type));
	Error_3(RE_EXPECT_ARG, DSF_LABEL(call), &param_word, arg_type);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Bad_Make(REBCNT type, const REBVAL *spec)
/*
***********************************************************************/
{
	Error_2(RE_BAD_MAKE_ARG, Get_Type(type), spec);
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_Cannot_Reflect(REBCNT type, const REBVAL *arg)
/*
***********************************************************************/
{
	Error_2(RE_CANNOT_USE, arg, Get_Type(type));
}


/***********************************************************************
**
*/	ATTRIBUTE_NO_RETURN void Error_On_Port(REBCNT errnum, REBSER *port, REBINT err_code)
/*
***********************************************************************/
{
	REBVAL *spec = OFV(port, STD_PORT_SPEC);
	REBVAL *val;
	REBVAL err_code_value;

	if (!IS_OBJECT(spec)) raise Error_0(RE_INVALID_PORT);

	val = Get_Object(spec, STD_PORT_SPEC_HEAD_REF); // most informative
	if (IS_NONE(val)) val = Get_Object(spec, STD_PORT_SPEC_HEAD_TITLE);

	SET_INTEGER(&err_code_value, err_code);
	Error_2(errnum, val, &err_code_value);
}


/***********************************************************************
**
*/	int Exit_Status_From_Value(REBVAL *value)
/*
**		This routine's job is to turn an arbitrary value into an
**		operating system exit status:
**
**			https://en.wikipedia.org/wiki/Exit_status
**
***********************************************************************/
{
	assert(!THROWN(value));

	if (IS_INTEGER(value)) {
		// Fairly obviously, an integer should return an integer
		// result.  But Rebol integers are 64 bit and signed, while
		// exit statuses don't go that large.
		//
		return VAL_INT32(value);
	}
	else if (IS_UNSET(value) || IS_NONE(value)) {
		// An unset would happen with just QUIT or EXIT and no /WITH,
		// so treating that as a 0 for success makes sense.  A NONE!
		// seems like nothing to report as well, for instance:
		//
		//     exit/with if badthing [badthing-code]
		//
		return 0;
	}
	else if (IS_ERROR(value)) {
		// Rebol errors do have an error number in them, and if your
		// program tries to return a Rebol error it seems it wouldn't
		// hurt to try using that.  They may be out of range for
		// platforms using byte-sized error codes, however...but if
		// that causes bad things OS_EXIT() should be graceful about it.
		//
		return VAL_ERR_NUM(value);
	}

	// Just 1 otherwise.
	//
	return 1;
}


/***********************************************************************
**
*/	void Init_Errors(REBVAL *errors)
/*
***********************************************************************/
{
	REBSER *errs;
	REBVAL *val;

	// Create error objects and error type objects:
	*ROOT_ERROBJ = *Get_System(SYS_STANDARD, STD_ERROR);
	errs = Construct_Object(NULL, VAL_BLK_HEAD(errors), FALSE);

	Val_Init_Object(Get_System(SYS_CATALOG, CAT_ERRORS), errs);

	// Create objects for all error types:
	for (val = BLK_SKIP(errs, 1); NOT_END(val); val++) {
		errs = Construct_Object(NULL, VAL_BLK_HEAD(val), FALSE);
		Val_Init_Object(val, errs);
	}
}


/***********************************************************************
**
*/	REBYTE *Security_Policy(REBCNT sym, REBVAL *name)
/*
**	Given a security symbol (like FILE) and a value (like the file
**	path) returns the security policy (RWX) allowed for it.
**
**	Args:
**
**		sym:  word that represents the type ['file 'net]
**		name: file or path value
**
**	Returns BTYE array of flags for the policy class:
**
**		flags: [rrrr wwww xxxx ----]
**
**		Where each byte is:
**			0: SEC_ALLOW
**			1: SEC_ASK
**			2: SEC_THROW
**			3: SEC_QUIT
**
**	The secuity is defined by the system/state/policies object, that
**	is of the form:
**
**		[
**			file:  [%file1 tuple-flags %file2 ... default tuple-flags]
**			net:   [...]
**			call:  tuple-flags
**			stack: tuple-flags
**			eval:  integer (limit)
**		]
**
***********************************************************************/
{
	REBVAL *policy = Get_System(SYS_STATE, STATE_POLICIES);
	REBYTE *flags;
	REBCNT len;
	REBCNT errcode = RE_SECURITY_ERROR;

	if (!IS_OBJECT(policy)) goto error;

	// Find the security class in the block: (file net call...)
	policy = Find_Word_Value(VAL_OBJ_FRAME(policy), sym);
	if (!policy) goto error;

	// Obtain the policies for it:
	// Check for a master tuple: [file rrrr.wwww.xxxx]
	if (IS_TUPLE(policy)) return VAL_TUPLE(policy); // non-aligned
	// removed A90: if (IS_INTEGER(policy)) return (REBYTE*)VAL_INT64(policy); // probably not used

	// Only other form is detailed block:
	if (!IS_BLOCK(policy)) goto error;

	// Scan block of policies for the class: [file [allow read quit write]]
	len = 0;	// file or url length
	flags = 0;	// policy flags
	for (policy = VAL_BLK_HEAD(policy); NOT_END(policy); policy += 2) {

		// Must be a policy tuple:
		if (!IS_TUPLE(policy+1)) goto error;

		// Is it a policy word:
		if (IS_WORD(policy)) { // any word works here
			// If no strings found, use the default:
			if (len == 0) flags = VAL_TUPLE(policy+1); // non-aligned
		}

		// Is it a string (file or URL):
		else if (ANY_BINSTR(policy) && name) {
			//Debug_Fmt("sec: %r %r", policy, name);
			if (Match_Sub_Path(VAL_SERIES(policy), VAL_SERIES(name))) {
				// Is the match adequate?
				if (VAL_TAIL(name) >= len) {
					len = VAL_TAIL(name);
					flags = VAL_TUPLE(policy+1); // non-aligned
				}
			}
		}
		else goto error;
	}

	if (!flags) {
		errcode = RE_SECURITY;
		policy = name ? name : 0;
error:
		if (!policy) {
			Val_Init_Word_Unbound(DS_TOP, REB_WORD, sym);
			policy = DS_TOP;
		}
		raise Error_1(errcode, policy);
	}

	return flags;
}


/***********************************************************************
**
*/	void Trap_Security(REBCNT flag, REBCNT sym, REBVAL *value)
/*
**		Take action on the policy flags provided. The sym and value
**		are provided for error message purposes only.
**
***********************************************************************/
{
	if (flag == SEC_THROW) {
		if (!value) {
			Val_Init_Word_Unbound(DS_TOP, REB_WORD, sym);
			value = DS_TOP;
		}
		raise Error_1(RE_SECURITY, value);
	}
	else if (flag == SEC_QUIT) OS_EXIT(101);
}


/***********************************************************************
**
*/	void Check_Security(REBCNT sym, REBCNT policy, REBVAL *value)
/*
**		A helper function that fetches the security flags for
**		a given symbol (FILE) and value (path), and then tests
**		that they are allowed.
**
***********************************************************************/
{
	REBYTE *flags;

	flags = Security_Policy(sym, value);
	Trap_Security(flags[policy], sym, value);
}


#if !defined(NDEBUG)

/***********************************************************************
**
*/	void Assert_Error_Debug(const REBVAL *err)
/*
**		Debug-only implementation of ASSERT_ERROR
**
***********************************************************************/
{
	assert(IS_ERROR(err));
	assert(VAL_ERR_NUM(err) != 0);

	ASSERT_FRAME(VAL_ERR_OBJECT(err));
}

#endif
