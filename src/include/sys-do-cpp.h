//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2016 Rebol Open Source Contributors
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
//  Summary: Optional C++ Checking Classes
//  File: %sys-cpp.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// R3-Alpha was designed to build under ANSI C89.  Though a seemingly archaic
// requirement in 2016, the unique nature of the project and a desire to
// build on as many platforms as possible has kept that a requirement.
//
// The Ren-C branch took the codebase forward to build in standard C99 and
// C11, but also to build and run under C++98, C++11, C++14, and C++17.  At
// the beginning this was just running in the lowest common denominator of
// both C++ and C89 (which is C89 for any practical purposes in the project.)
// Yet this was extended to use in both static and dynamic analysis, where
// the C++ build had classes taking the place of plain values or structs and
// to add more checking.
//
// No features are implemented using C++, and the classes are only used in a
// debug build of the project.  They are strictly for additional checks.
//
// While the C89 feature strives for legacy support, the C++ build does not.
// The build works under C++98 but adds no additional checking features to
// the C89 version.  Additional classes are only enabled for C++11 and above.
//


//
// Reb_Indexor
//
// R3-Alpha wished to encode "magic values" into the integer index which is
// used for stepping through arrays.  Hence 0, 1, 2, etc. would be normal
// indices, but 2,147,483,647 and 2,147,483,648 would be "magic" values
// (for instance) to indicate a status result of THROWN or END of input.
//
// The risk of not having a separate type and methods to check for this is
// that it's very easy to do math and turn a "magic value" into one that
// is not magic, or otherwise pass a flag value unchecked to something that
// only expects valid array indices.  To check this in the C++ build, a
// class that encapsulates the legal operations checks to make sure that
// a magic value never "escapes" or has math performed on it.
//
// Additionally, when the value changes a string is set to what the value
// is supposed to represent.  This way when looking during the debug build
// one can quickly see which magic value a strange number is supposed to
// be representing.
//
// To ensure it's the same size as a REBUPT, the indexor has to do an
// allocation for its contents.
//
struct Reb_Indexor_Data {
    REBUPT bits;
    const char* name;
};

class Reb_Indexor {
    Reb_Indexor_Data *d; // not unique_ptr<>, class must be same size as C type

    static constexpr const char* array_index_name = "(array index)";
    static constexpr const char* end_name = "END_FLAG";
    static constexpr const char* thrown_name = "THROWN_FLAG";
    static constexpr const char* valist_name = "VALIST_FLAG";
    static constexpr const char* valist_incomplete_name
        = "VALIST_INCOMPLETE";

    void Update_Name() {
        if (d->bits == END_FLAG)
            d->name = end_name;
        else if (d->bits == THROWN_FLAG)
            d->name = thrown_name;
        else if (d->bits == VALIST_FLAG)
            d->name = valist_name;
        else if (d->bits == VALIST_INCOMPLETE_FLAG)
            d->name = valist_incomplete_name;
        else
            d->name = array_index_name;
    }

public:
    Reb_Indexor () : d (new Reb_Indexor_Data) {} // simulate C uninitialization
    Reb_Indexor (REBCNT bits) : d (new Reb_Indexor_Data) {
        d->bits = bits;
        Update_Name();
    }
    Reb_Indexor (Reb_Indexor const &other) {
        d = new Reb_Indexor_Data;
        d->bits = other.d->bits;
        d->name = other.d->name;
    }
    void operator=(Reb_Indexor const &rhs) {
        d->bits = rhs.d->bits;
        d->name = rhs.d->name;
    }
    Reb_Indexor (Reb_Indexor && other) {
        d->bits = other.d->bits;
        d->name = other.d->name;
        delete other.d;
        other.d = NULL;
    }
    ~Reb_Indexor() {
        if (d) delete d;
    }

    void operator=(REBCNT rhs) {
        d->bits = rhs;
        Update_Name();
    }
    int operator==(Reb_Indexor const &rhs) {
        return d->bits == rhs.d->bits;
    }
    int operator!=(Reb_Indexor const &rhs) {
        return !((*this) == rhs);
    }

    // Basic check: whenever one tries to get an actual unset integer out of
    // an indexor, it is asserted not to be a magic value.  This is called by
    // the math operations, as well as any explicit `cast(REBCNT, indexor)`
    //
    explicit operator REBCNT() const {
        assert(
            d->bits != END_FLAG && d->bits != THROWN_FLAG &&
            d->bits != VALIST_FLAG && d->bits != VALIST_INCOMPLETE_FLAG
        );
        return d->bits;
    }

    // Subset of operations that are exported to be legal to perform between
    // an unsigned integer and an indexor.  Comparisons for equality and
    // addition and subtraction are allowed.  While more operations could
    // be added, the best course of action is generally that if one is going
    // to do a lot of math on an indexor it is not a special value...so it
    // should be extracted by casting to a REBCNT.
    //
    friend int operator==(REBCNT lhs, const Reb_Indexor &rhs) {
        return lhs == rhs.d->bits;
    }
    friend int operator!=(REBCNT lhs, const Reb_Indexor &rhs) {
        return !(lhs == rhs);
    }
    int operator<(REBCNT rhs) const {
        return cast(REBCNT, *this) < rhs;
    }
    friend int operator<(REBCNT lhs, const Reb_Indexor &rhs) {
        return lhs < rhs.d->bits;
    }
    int operator>(REBCNT rhs) const {
        return cast(REBCNT, *this) > rhs;
    }
    friend int operator>(REBCNT lhs, const Reb_Indexor &rhs) {
        return lhs > rhs.d->bits;
    }
    int operator<=(REBCNT rhs) const {
        return cast(REBCNT, *this) <= rhs;
    }
    friend int operator<=(REBCNT lhs, const Reb_Indexor &rhs) {
        return lhs <= rhs.d->bits;
    }
    REBCNT operator+(REBCNT rhs) const {
        return cast(REBCNT, *this) + rhs;
    }
    friend REBCNT operator+(REBCNT lhs, const Reb_Indexor &rhs) {
        return rhs + lhs;
    }
    REBCNT operator-(REBCNT rhs) const {
        return cast(REBCNT, *this) - rhs;
    }
    friend REBCNT operator-(REBCNT lhs, const Reb_Indexor &rhs) {
        return rhs - lhs;
    }
    REBCNT operator*(REBCNT rhs) const {
        return cast(REBCNT, *this) * rhs;
    }
    friend REBCNT operator*(REBCNT lhs, const Reb_Indexor &rhs) {
        return rhs * lhs;
    }
};
