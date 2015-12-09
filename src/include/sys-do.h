//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2015 Rebol Open Source Contributors
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
//  Summary: Evaluator "Do State" and Helpers
//  File: %sys-do.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A Reb_Call structure represents the fixed-size portion for a function's
// call frame.  It is stack allocated, and is used by both Do and Apply.
// (If a dynamic allocation is necessary for the call frame, that dynamic
// portion is allocated as an array in `arglist`.)
//
// The contents of the call frame are all the input and output parameters
// for a call to the evaluator--as well as all of the internal state needed
// by the evaluator loop.  The reason that all the information is exposed
// in this way is to make it faster and easier to delegate branches in
// the Do loop--without bearing the overhead of setting up new stack state.
//
