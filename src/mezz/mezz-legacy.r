REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Mezzanine: Legacy compatibility"
    Rights: {
        Copyright 1997-2015 REBOL Technologies
        Copyright 2012-2015 Rebol Open Source Contributors

        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Description: {
        These definitions turn the clock backward for Rebol code that was
        written prior to Ren/C, e.g. binaries available on rebolsource.net
        or R3-Alpha binaries from rebol.com.  Some flags which are set
        which affect the behavior of natives and the evaluator ARE ONLY
        ENABLED IN DEBUG BUILDS OF REN/C...so be aware of that.

        Some "legacy" definitions (like `foreach` as synonym of `for-each`)
        are kept by default for now, possibly indefinitely.  For other
        changes--such as variations in behavior of natives of the same
        name--you need to add the following to your code:

            do <r3-legacy>

        (Dispatch for this from DO is in the DO* function of %sys-base.r)

        This statement will do nothing in older Rebols, since executing a
        tag evaluates to just a tag.  Note that the current trick will
        modify the user context directly, and is not module-based...so
        you really are sort of "backdating" the system globally.  A
        more selective version that turns features on and off one at
        a time to ease porting is needed, perhaps like:

            do/args <r3-legacy> [
                new-do: off
                question-marks: on
            ]

        As always, feedback and improvement welcome.  A porting guide Trello
        has been started at:

            https://trello.com/b/l385BE7a/porting-guide
    }
]

; This identifies if r3-legacy mode is has been turned on, useful mostly only
; for turning it off again.
;
r3-legacy-mode: off


; GROUP! is a better name than PAREN! for many reasons.  It's a complete word,
; it's no more characters, it doesn't have the same first two letters as
; PATH! so it mentally and typographically hashes better from one of the two
; other array types, it describes the function of what it does in the
; evaluator (where "BLOCK! blocks evaluation of the contents, the GROUP!
; does normal evaluation but limits it to the group)...
;
; Historically, changing the name of a type was especially burdensome because
; the name would be encoded when you did a TO-WORD on it, such as in order
; to perform a SWITCH.  (Rebol2 added TYPE?/WORD to make this convenient.)
; Now it is possible to use get-words in SWITCH to lookup, so things should
; be easier.
;
; What one uses in one's code is one's own choice.  But the internal canon
; for the term in Ren-C's code and mezzanine is GROUP!
;
paren?: :group?
paren!: :group!
to-paren: :to-group


; The TYPE?/WORD primitive is adjusted to give back PAREN! as the word if
; the legacy switch for it is enabled.
;
; The previous /WORD refinement was common because things like SWITCH
; statements wanted to operate on the word for the datatype, since in
; unevaluated contexts INTEGER! would be a WORD! and not a DATATYPE!.  With
; change to allow lax equality comparisons to see a datatype and its word
; as being equal, this is not as necessary.  Cases that truly need it can
; use TO-WORD TYPE-OF.
;
type?: function [
    "Returns the datatype of a value <r3-legacy>."
    value [<opt> any-value!]
    /word "No longer in TYPE-OF, as WORD! and DATATYPE! can be EQUAL?"
][
    case [
        not word [
            type-of :value
        ]

        all [
            group? :value
            system/options/paren-instead-of-group
        ][
            ; For legacy compatibility, type?/word will return PAREN!
            ; instead of Ren-C standard GROUP! if the switch is on.
            ;
            'paren!
        ]

        void? :value [
            ;
            ; UNSET! is no longer a real value type, and TYPE-OF will
            ; return a value of type NONE! (not a datatype).  But the
            ; word conversion will say it's UNSET!.
            ;
            'unset!
        ]

        blank? :value ['none!]

        'default [to-word type-of :value]
    ]
]


; See also prot-http.r, which has an actor with a LENGTH? "method".  Given
; how actors work, it cannot be overriden here.
length?: :length

index?: :index-of

offset?: :offset-of

sign?: :sign-of

suffix?: :suffix-of

; While `foreach` may have been comfortable for some as a word, its hard
; not to see the word `reach` inside of it.  Once you recognize that it's
; not a word and see it with fresh eyes, it looks bad...and also not part
; of the family of other -each functions like `remove-each` and `map-each`.
; The need for the hyphen for `for-each` isn't that bad, but the hyphen
; does break the rhythm a little bit.  `every` was selected as a
; near-synonym of `for-each` (with a different return result).
;
; Because there is no eager need to retake foreach for any other purpose
; and it doesn't convey any fundamentally incorrect idea, it is low on
; the priority list to eliminate completely.  It may be retained as a
; synonym, and `each` may be considered as well.  The support of synonyms
; is controversial and should be balanced against the value of standards.
;
foreach: :for-each


; Ren-C's FOR-NEXT is a more lenient version of FORALL, which allows you to
; switch the variable being iterated to any other type of series--NONE will
; terminate the enumeration, so will reaching the tail.  (R3-Alpha let you
; switch series, but only if the type matched what you initially started
; enumerating).
;
; Additionally, it adds FOR-BACK to do a reverse enumeration.  Both of these
; are simply variations of FOR-SKIP with 1 and -1 respectively.
;
forall: :for-next
forskip: :for-skip


; The distinctions between Rebol's types is important to articulate.
; So using the term "BLOCK" generically to mean any composite
; series--as well as specificially the bracketed block type--is a
; recipe for confusion.
;
; Importantly: it also makes it difficult to get one's bearings
; in the C sources.  It's hard to find where exactly the bits are in
; play that make something a bracketed block...or if that's what you
; are dealing with at all.  Hence some unique name for the typeclass
; is needed.
;
; The search for a new word for the ANY-BLOCK! superclass went on for
; a long time.  LIST! was looking like it might have to be the winner,
; until ARRAY! was deemed more fitting.  The notion that a "Rebol
; Array" contains "Rebol Values" that can be elements of any type
; (including other arrays) separates it from the other series classes
; which can only contain one type of element (codepoints for strings,
; bytes for binaries, numbers for vector).  In the future those may
; have more unification under a typeclass of their own, but ARRAY!
; is for BLOCK!, PAREN!, PATH!, GET-PATH!, SET-PATH!, and LIT-PATH!

any-block!: :any-array!
any-block?: :any-array?


; Similarly to the BLOCK! and ANY-BLOCK! problem for understanding the inside
; and outside of the system, ANY-CONTEXT! is a better name for the superclass
; of OBJECT!, ERROR!, PORT! and (likely to be killed) MODULE!

any-object!: :any-context!
any-object?: :any-context?


; By having typesets prefixed with ANY-*, it helps cement the realization
; on the user's part that they are not dealing with a concrete type...so
; even though this is in the function prototype, they will not get back
; that answer from TYPE-OF, nor could they pass it to MAKE or TO.  Because
; typsets cannot be returned in that way, they're easier than most things
; to make synonyms for the old ANY-less variations.

number!: :any-number!
number?: :any-number?

scalar!: :any-scalar!
scalar?: :any-scalar?

series!: :any-series!
series?: :any-series?

; ANY-TYPE! had an ambiguity in it, which was that with DATATYPE! around
; (possibly being renamed to TYPE! but maybe not) it sounded as if it might
; mean ANY-DATATYPE!--which is more narrow than what it meant to say which
; is "really, any legal Rebol value, type or otherwise".  So ANY-VALUE! is
; the better word for that.  Added for backwards compatibility.
;
; Because UNSET! was a datatype in R3-Alpha and Rebol2, ANY-TYPE! included
; it...which was the way of achieving "optional" arguments.  However, this
; concept of "no type" being included isn't necessarily a typeset feature,
; but a feature of function arguments (like being variadic).  Hence using
; the _ here to pass in a literal blank may not be a long term feature, as
; allowing no type could be for function specs only--not general typesets.
;
any-type!: make typeset! [_ any-value!]

; BIND? and BOUND? didn't fit the naming convention of returning LOGIC! if
; they end in a question mark.  Also, CONTEXT-OF is more explicit about the
; type of the return result, which makes it more useful than BINDING-OF or
; BIND-OF as a name.  (Result can be an ANY-CONTEXT!, including FRAME!)
;
bound?: :context-of
bind?: :context-of

; !!! These less common cases still linger as question mark routines that
; don't return LOGIC!, and they seem like they need greater rethinking in
; general. What replaces them (for ones that are kept) might be entirely new.

;encoding?
;file-type?
;speed?
;why?
;info?
;exists?


; !!! Technically speaking all frames should be "selfless" in the sense that
; the system does not have a particular interest in the word "self" as
; applied to objects.  Generators like OBJECT may choose to establish a
; self-bearing protocol.
;
selfless?: func [context [any-context!]] [
    fail {selfless? no longer has meaning (all frames are "selfless")}
]

unset!: does [
    fail [
        {UNSET! is not a datatype in Ren-C.}
        | {You can test with VOID? (), but the TYPE-OF () is a NONE! *value*}
        | {So NONE? TYPE-OF () will be TRUE.}
    ]
]

unset?: does [
    fail [
        {UNSET? is reserved in Ren-C for future use}
        | {(Will mean VOID? GET, like R3-Alpha VALUE?, only for WORDs/PATHs}
        | {Use VOID? for a similar test, but be aware there is no UNSET! type}
        | {If running in <r3-legacy> mode, old UNSET? meaning is available}
    ]
]

value?: does [
    fail [
        {VALUE? is reserved in Ren-C for future use}
        | {(It will be a shorthand for ANY-VALUE! a.k.a. NOT VOID?)}
        | {SET? is similar to R3-Alpha VALUE?--but SET? only takes words}
        | {If running in <r3-legacy> mode, old VALUE? meaning is available.}
    ]
]

none-of: :none ;-- reduce mistakes for now by renaming NONE out of the way

none?: none!: none: does [
    fail [
        {NONE is reserved in Ren-C for future use}
        | {(It will act like NONE-OF, e.g. NONE [a b] => ALL [not a not b])}
        | {_ is now a "BLANK! literal", with BLANK? test and BLANK the word.}
        | {If running in <r3-legacy> mode, old NONE meaning is available.}
    ]
]

; There were several different strata of equality checks, and one was EQUIV?
; as well as NOT-EQUIV?.  With changes to make comparisons inside the system
; indifferent to binding (unless SAME? is used), these have been shaken up
; instead focusing on getting more foundational comparisons working.
;
; These aren't correct but placeholders for putting in the real functionality
; if it actually matters.
;
equiv?: :equal?
not-equiv?: :not-equal?


; The legacy PRIN construct is equivalent to PRINT/ONLY of a reduced value
; (since PRIN of a block would historically execute it).
;
prin: function [
    "Print value, no line break, reducing blocks.  <r3-legacy>, use PRINT/ONLY"

    value [<opt> any-value!]
][
    print/only either block? :value [reduce value] [:value]
]


; BREAK/RETURN was supplanted by BREAK/WITH.  The confusing idea of involving
; the word RETURN in the refinement (return from where, who?) became only
; more confusing with the introduction of definitional return.
;
; Renaming rationale: https://trello.com/c/c4T1UZEE
; New features of WITH: https://trello.com/c/cOgdiOAD
;
lib-break: :break ; overwriting lib/break for now
break: func [
    {Exit the current iteration of a loop and stop iterating further.}

    /with
        {Act as if loop body finished current evaluation with a value}
    value [<opt> any-value!]

    /return ;-- Overrides RETURN!
        {(deprecated: mostly /WITH synonym, use THROW+CATCH if not)}
    return-value [<opt> any-value!]
][
    lib-break/with either return :return-value :value
]


; SET has a refinement called /ANY which doesn't communicate as well in the
; Ren-C world as OPT.  OPT is the marker on functions to mark parameters as
; optional...OPT is the function to convert NONE! to UNSET! while passing
; all else through.  It has a narrower and more communicative focus of purpose
; than /ANY does (also ANY is a very common function with a very different
; meaning and sense)
;
lib-set: :set ; overwriting lib/set for now
set: function [
    {Sets a word, path, block of words, or context to specified value(s).}

    target [any-word! any-path! block! any-context!]
        {Word, block of words, path, or object to be set (modified)}
    value [<opt> any-value!]
        "Value or block of values"
    /opt
        "Value is optional, and if no value is provided then unset the word"
    /pad
        {For objects, set remaining words to NONE if block is too short}
    /lookback
        {If value is a function, then make the bound word dispatch infix}
    /any
        "Deprecated legacy synonym for /opt"
][
    set_ANY: any
    any: :lib/any
    set_OPT: opt
    opt: :lib/opt

    apply :lib-set [
        target: target
        value: :value
        opt: true? any [set_ANY set_OPT]
        pad: pad
        lookback: lookback
    ]
]


; This version of get supports the legacy /ANY switch that has been replaced
; by /OPT (but since the switch is new, it would be disruptive to remove it
; entirely immediately... /ANY will be moved to the r3-legacy mode after
; more codebases have adapted to /OPT.  If it is to stay even longer, it may
; need to be done via a thinner legacy proxy which can rename refinements but
; not cost a function body execution.)
;
; Historical GET in Rebol allowed any type that wasn't UNSET!.  If you said
; something like `get 1` this would be passed through as `1`.  Both Ren-C and
; Red have removed that feature, so it's only enabled in legacy mode.  R3-Gui
; was dependent on the fallthrough behavior and other legacy clients may be
; also, so this is a more tolerant variant of LIB/GET for now.
;
; Note: It is questionable to use it as a way of getting the fields of an
; object (likely better suited to reflection)--the SET parallel actually
; assumes a positional ordering of fields, disallowed in PICK (should be the
; same rule, probably neither should work.)
;
lib-get: :get
get: function [
    {Gets the value of a word or path, or values of a context.}
    source
        "Word, path, context to get"
    /opt
        "The source may optionally have no value (allows returning void)"
    /any
        "Deprecated legacy synonym for /OPT"
][
    any_GET: any
    any: :lib/any
    opt_GET: opt
    opt: :lib/opt

    either any [
        blank? :source
        any-word? :source
        any-path? :source
        any-context? :source
    ][
        lib-get/(if any [opt_GET any_GET] 'opt) :source
    ][
        if system/options/get-will-get-anything [:source]
        fail ["GET takes ANY-WORD!, ANY-PATH!, ANY-CONTEXT!, not" (:source)]
    ]
]


; In word-space, TRY is very close to ATTEMPT, in having ambiguity about what
; is done with the error if one happens.  It also has historical baggage with
; TRY/CATCH constructs. TRAP does not have that, and better parallels CATCH
; by communicating you seek to "trap errors in this code (with this handler)"
; Here trapping the error suggests you "caught it in a trap" and it is
; in the trap (and hence in your possession) to examine.  /WITH is not only
; shorter than /EXCEPT but it makes much more sense.
;
; !!! This may free up TRY for more interesting uses, such as a much shorter
; word to use for ATTEMPT.  Even so, this is not a priority at this point in
; time...so TRY is left to linger without needing `do <r3-legacy>`
;
try: func [
    {Tries to DO a block and returns its value or an error.}
    block [block!]
    /except "On exception, evaluate this code block"
    code [block! function!]
][
    either except [trap/with block :code] [trap block]
]


; HAS is targeted for use to be the arity-1 parallel to the arity-2 constructor
; for OBJECT!s (similar to the relationship between DOES and FUNCTION).  The
; working name for the arity-2 form is CONSTRUCT
;
has: func [
    {A shortcut to define a function that has local variables but no arguments.}
    vars [block!] {List of words that are local to the function}
    body [block!] {The body block of the function}
][
    func (head insert copy vars /local) body
]


; With the introduction of FRAME! in Ren-C, the field is opened for more
; possibilities of APPLY-like constructs.  The default APPLY is now based on
; filling frames with parameters by name.  But this implements the R3-Alpha
; variant, showing that more could be written.
;
r3-alpha-apply: function [
    "Apply a function to a reduced block of arguments."

    func [function!]
        "Function value to apply"
    block [block!]
        "Block of args, reduced first (unless /only)"
    /only
        "Use arg values as-is, do not reduce the block"
][
    frame: make frame! :func
    params: words-of :func
    using-args: true

    while [not tail? block] [
        arg: either only [
            also block/1 (block: next block)
        ][
            do/next block 'block
        ]

        either refinement? params/1 [
            using-args: set (in frame params/1) true? :arg
        ][
            if using-args [
                set/opt (in frame params/1) :arg
            ]
        ]

        params: next params
    ]

    comment [
        ;
        ; Too many arguments was not a problem for R3-alpha's APPLY, it would
        ; evaluate them all even if not used by the function.  It may or
        ; may not be better to have it be an error.
        ;
        unless tail? block [
            fail "Too many arguments passed in R3-ALPHA-APPLY block."
        ]
    ]

    do frame ;-- voids are optionals
]


; CLOSURE has been unified with FUNCTION by attacking the two facets that it
; offers separately.  One is the ability for its arguments and locals to
; survive the call, which has been recast using the tag `<durable>`.  The
; other is the specific binding of words in the body to the frame of origin
; vs to whichever call to the function is on the stack.  That is desired to
; be pushed as a feature of FUNCTION! that runs at acceptable cost and one
; never has to ask for.
;
; For the moment, the acceptable-cost version is in mid-design, so `<durable>`
; in the function spec indicates a request for both properties.
;
closure: func [
    {Defines a closure function with all set-words as locals.}
    spec [block!]
        {Help string (opt) followed by arg words (and opt type and string)}
    body [block!]
        {The body block of the function}
    /with
        {Define or use a persistent object (self)}
    object [object! block! map!]
        {The object or spec}
    /extern
        {These words are not local}
    words [block!]
][
    apply :function [
        spec: compose [<durable> (spec)]
        body: body
        if with: with [
            object: object
        ]
        if extern: extern [
            words: :words
        ]
    ]
]

clos: func [
    "Defines a closure function."
    spec [block!]
        {Help string (opt) followed by arg words (and opt type and string)}
    body [block!]
        "The body block of the function"
][
    func compose [<durable> (spec)] body
]

closure!: :function!
closure?: :function?

; All other function classes are also folded into the one FUNCTION! type ATM.

any-function!: :function!
any-function?: :function?

native!: function!
native?: func [f] [all [function? :f | 1 = func-class-of :f]]

;-- If there were a test for user-written functions, what would it be called?
;-- it would be function class 2 ATM

action!: function!
action?: func [f] [all [function? :f | 3 = func-class-of :f]]

command!: function!
command?: func [f] [all [function? :f | 4 = func-class-of :f]]

routine!: function!
routine?: func [f] [all [function? :f | 5 = func-class-of :f]]

callback!: function!
callback?: func [f] [all [function? :f | 6 = func-class-of :f]]

; To bridge legacy calls to MAKE ROUTINE!, MAKE COMMAND!, and MAKE CALLBACK!
; while still letting ROUTINE!, COMMAND!, and CALLBACK! be valid to use in
; typesets invokes the new variadic behavior.  This can only work if the
; source literally wrote out `make routine!` vs an expression that evaluated
; to the routine! datatype (for instance) but should cover most cases.
;
lib-make: :lib/make
make: func [
    "Constructs or allocates the specified datatype."
    :lookahead [any-value! <...>]
    type [<opt> any-value! <...>]
        "The datatype or an example value"
    spec [<opt> any-value! <...>]
        "Attributes or size of the new value (modified)"
][
    switch first lookahead [
        callback! [
            assert [function! = take type]
            make-callback take spec
        ]
        routine! [
            assert [function! = take type]
            make-routine take spec
        ]
        command! [
            assert [function! = take type]
            make-command take spec
        ]
        (lib-make take type take spec)
    ]
]


; To invoke this function, use `do <r3-legacy>` instead of calling it
; directly, as that will be a no-op in older Rebols.  Notice the word
; is defined in sys-base.r, as it needs to be visible pre-Mezzanine
;
; Legacy mode is specifically intended to assist in porting efforts to
; Ren-C, not as a permanent operating mode.  While contributions making it
; work more seamlessly are more than welcome, scheduling of improvements to
; the legacy mode are on a strictly "as-needed" basis.
;
set 'r3-legacy* func [] [

    ; There's no clean "shutdown" of legacy mode at this time to go back to
    ; plain Ren-C.  The code that enables legacy mode also is allowed to use
    ; Ren-C features, which would not be available if running a second time
    ; while legacy mode is on.  Hence make running multiple times a no-op.
    ;
    if r3-legacy-mode [return blank]

    append system/contexts/user compose [

        ; UNSET! as a reified type does not exist in Ren-C.  There is still
        ; a "void" state as the result of `do []` or just `()`, and it can be
        ; passed around transitionally.  Yet this "meta" result cannot be
        ; stored in blocks.
        ;
        ; Over the longer term, UNSET? should be something that takes a word
        ; or path to tell whether a variable is unset... but that is reserved
        ; for NOT SET? until legacy is adapted.
        ;
        unset?: (:void?)

        ; Result from TYPE-OF () is a NONE!, so this should allow one to write
        ; `unset! = type-of ()`.  Also, a NONE! value in a typeset spec is
        ; used to indicate a willingness to tolerate optional arguments, so
        ; `foo: func [x [unset! integer!] x][...]` should work in legacy mode
        ; for making an optional x argument.
        ;
        ; Note that with this definition, `datatype? unset!` will fail.
        ;
        unset!: _

        ; NONE is reserved for NONE-OF in the future
        ;
        none: (:blank)
        none!: (:blank!)
        none?: (:blank?)

        ; The bizarre VALUE? function would look up words, return TRUE if they
        ; were set and FALSE if not.  All other values it returned TRUE.  The
        ; parameter was not optional, so you couldn't say `value?`.
        ;
        value?: (func [
            {If a word, return whether word is set...otherwise TRUE}
            value
        ][
            either any-word? :value [set? value] [true]
        ])

        ; These words do NOT inherit the infixed-ness, and you simply cannot
        ; set things infix through a plain set-word.  We have to do this
        ; after the words are appended to the object.

        and: _

        or: _

        xor: _

        apply: (:r3-alpha-apply)

        ; Not contentious, but trying to excise this ASAP
        funct: (:function)

        ; There are no separate function types for infix in Ren-C.  SET/INFIX
        ; makes that particular binding act infix, but if the function is
        ; dealt with as a function value it will not be infix.
        ;
        op?: (func [
            "Always returns FALSE (lookback? test can only be run on WORD!s)"
            value [<opt> any-value!]
        ][
            false
        ])

        ; R3-Alpha and Rebol2's DO was effectively variadic.  If you gave it
        ; a function, it could "reach out" to grab arguments from after the
        ; call.  While Ren-C permits this in variadic functions, the system
        ; functions should be "well behaved" and there will even likely be
        ; a security setting to turn variadics off (system-wide or per module)
        ;
        ; https://trello.com/c/YMAb89dv
        ;
        ; This legacy bridge is variadic to achieve the result.
        ;
        do: (function [
            {Evaluates a block of source code (variadic <r3-legacy> bridge)}

            source [<opt> blank! block! group! string! binary! url! file! tag!
                error! function!
            ]
            normals [any-value! <...>]
                {Normal variadic parameters if function (<r3-legacy> only)}
            'softs [any-value! <...>]
                {Soft-quote variadic parameters if function (<r3-legacy> only)}
            :hards [any-value! <...>]
                {Hard-quote variadic parameters if function (<r3-legacy> only)}
            /args
                {If value is a script, this will set its system/script/args}
            arg
                "Args passed to a script (normally a string)"
            /next
                {Do next expression only, return it, update block variable}
            var [word! blank!]
                "Variable updated with new block position"
        ][
            next_DO: next
            next: :lib/next

            either function? :source [
                code: reduce [:source]
                params: words-of :source
                while [not tail? params] [
                    append code switch type-of params/1 [
                        :word! [take normals]
                        :lit-word! [take softs]
                        :get-word! [take hards]
                        :set-word! [()] ;-- unset appends nothing (for local)
                        :refinement! [break]
                        (fail ["bad param type" params/1])
                    ]
                    params: next params
                ]
                lib/do code
            ][
                apply :lib/do [
                    source: :source
                    if args: args [
                        arg: :arg
                    ]
                    if next: next_DO [
                        var: :var
                    ]
                ]
            ]
        ])

        ; Ren-C removed the "simple parse" functionality, which has been
        ; superseded by SPLIT.  For the legacy parse implementation, add
        ; it back in (more or less) by delegating to split.
        ;
        ; Also, as an experiment Ren-C has been changed so that a successful
        ; parse returns the input, while an unsuccessful one returns blank.
        ; Historically PARSE returned LOGIC!, this restores that behavior.
        ;
        parse: (function [
            "Parses a string or block series according to grammar rules."

            input [any-series!]
                "Input series to parse"
            rules [block! string! blank!]
                "Rules (string! is <r3-legacy>, use SPLIT)"
            /case
                "Uses case-sensitive comparison"
            /all
                "Ignored refinement for <r3-legacy>"
        ][
            case_PARSE: case
            case: :lib/case

            comment [all_PARSE: all] ;-- Not used
            all: :lib/all

            case [
                blank? rules [
                    split input charset reduce [tab space cr lf]
                ]

                string? rules [
                    split input to-bitset rules
                ]

                true [
                    ; This requires system/options/refinements-blank to work.
                    ;
                    ; Note that the heuristic here is not 100% right,
                    ; but probably works most of the time.  The goal is
                    ; to determine when PARSE would have returned true
                    ; when the new PARSE returns a series on success.  But
                    ; old parse *could* have returned a series as well with
                    ; RETURN...if that happens and the RETURN just so
                    ; happens to be the input, this will return TRUE.
                    ;
                    result: lib/parse/(if case_PARSE 'case) input rules
                    case [
                        blank? result [false]
                        same? result input [true]
                        'default [result]
                    ]
                ]
            ]
        ])

        ; There is a feature in R3-Alpha, used by R3-GUI, which allows an
        ; unusual syntax for capturing series positions (like a REPEAT or
        ; FORALL) with a SET-WORD! in the loop words block:
        ;
        ;     >> a: [1 2 3]
        ;     >> foreach [s: i] a [print ["s:" mold s "i:" i]]
        ;
        ;     s: [1 2 3] i: 1
        ;     s: [2 3] i: 2
        ;     s: [3] i: 3
        ;
        ; This feature was removed from Ren-C due to it not deemed to be
        ; "Quality", adding semantic questions and complexity to the C loop
        ; implementation.  (e.g. `foreach [a:] [...] [print "infinite loop"]`)
        ; That interferes with the goal of "modify with confidence" and
        ; simplicity.
        ;
        ; This shim function implements the behavior in userspace.  Should it
        ; arise that MAP-EACH is used similarly in a legacy scenario then the
        ; code could be factored and shared, but it is not likely that the
        ; core construct will be supporting this in FOR-EACH or EVERY.
        ;
        ; Longer-term, a rich LOOP dialect like Lisp's is planned:
        ;
        ;    http://www.gigamonkeys.com/book/loop-for-black-belts.html
        ;
        foreach: (function [
            "Evaluates a block for value(s) in a series w/<r3-legacy> 'extra'."

            'vars [word! block!]
                "Word or block of words to set each time (local)"
            data [any-series! any-context! map! blank!]
                "The series to traverse"
            body [block!]
                "Block to evaluate each time"
        ][
            if any [
                not block? vars
                lib/foreach item vars [
                    if set-word? item [break/with false]
                    true
                ]
            ][
                ; a normal FOREACH
                return lib/foreach :vars data body
            ]

            ; Otherwise it's a weird FOREACH.  So handle a block containing at
            ; least one set-word by doing a transformation of the code into
            ; a while loop.
            ;
            use :vars [
                position: data
                while [not tail? position] compose [
                    (collect [
                        every item vars [
                            case [
                                set-word? item [
                                    keep compose [(item) position]
                                ]
                                word? item [
                                    keep compose [
                                        (to-set-word :item) position/1
                                        position: next position
                                    ]
                                ]
                                true [
                                    fail "non SET-WORD?/WORD? in FOREACH vars"
                                ]
                            ]
                        ]
                    ])
                    (body)
                ]
            ]
        ])

        ; REDUCE has been changed to evaluate single-elements if those
        ; elements do not require arguments (so effectively a more limited
        ; form of EVAL).  The old behavior was to just pass through non-blocks
        ;
        reduce: (function [
            {Evaluates expressions and returns multiple results.}
            value
            /no-set
                "Keep set-words as-is. Do not set them."
            /only
                "Only evaluate words and paths, not functions"
            words [block! blank!]
                "Optional words that are not evaluated (keywords)"
            /into
                {Output results into a series with no intermediate storage}
            target [any-block!]
        ][
            unless block? :value [return :value]

            apply :lib/reduce [
                | value: :value
                | no-set: no-set
                | if only: only [words: :words]
                | if into: into [target: :target]
            ]
        ])

        ; because reduce has been changed but lib/reduce is not in legacy
        ; mode, this means the repend and join function semantics are
        ; different.  This snapshots their implementation.

        repend: (function [
            "Appends a reduced value to a series and returns the series head."
            series [series! port! map! gob! object! bitset!]
                {Series at point to insert (modified)}
            value
                {The value to insert}
            /part
                {Limits to a given length or position}
            length [number! series! pair!]
            /only
                {Inserts a series as a series}
            /dup
                {Duplicates the insert a specified number of times}
            count [number! pair!]
        ][
            ;-- R3-alpha REPEND with block behavior called out

            apply :append [
                | series: series
                | value: either block? :value [reduce :value] [value]
                | if part: part [limit: length]
                | only: only
                | if dup: dup [count: count]
            ]
        ])

        join: (function [
            "Concatenates values."
            value "Base value"
            rest "Value or block of values"
        ][
            ;-- double-inline of R3-alpha `repend value :rest`
            apply :append [
                | series: either series? :value [copy value] [form :value]
                | value: either block? :rest [reduce :rest] [rest]
            ]
        ])

        ; The name STACK is a noun that really suits a variable name, and
        ; the interim choice for this functionality in Ren-C is "backtrace".
        ; Because it is a debug-privilege API it might wind up under DEBUG
        ; or some part of a REFLECT dialect over the long term.
        ;
        ; Several refinements have been renamed or are no longer relevant,
        ; and it has a zero-arity default.  This wraps what functionality of
        ; R3-Alpha's STACK corresponds in a terribly inefficient way, which
        ; will likely never matter because odds are no one used it.
        ;
        ; !!! This routine would require review if anyone finds it of
        ; actual interest, it's just here to show how it *could* be done.
        ;
        stack: (function [
            "Returns stack backtrace or other values."
            offset [integer!] "Relative backward offset"
            /block "Block evaluation position"
            /word "Function or object name, if known"
            /func "Function value"
            /args "Block of args (may be modified)"
            /size "Current stack size (in value units)"
            /depth "Stack depth (frames)"
            /limit "Stack bounds (auto expanding)"
        ][
            func_STACK: :func
            func: :lib/func

            ; Get the full backtrace to start with because we need to filter
            ;
            bt: backtrace/limit/:args/(
                either block 'where blank
            )/(
                either func_STACK 'function blank
            ) blank

            ; The backtrace is going to have STACK in it, in slot number 1.
            ; But the caller doesn't want to see STACK.  So remove it.
            ;
            take/part (find bt 1) 2

            if depth [
                count: 0
                for-each item bt [
                    if integer? item [count: count + 1]
                ]
                return count
            ]

            ; Now renumber all the stack entries to bump them down by 1,
            ; accounting for the removal of stack.  If we find a number
            ; that is less than the offset requested, strip the remainder.
            ;
            for-next bt [
                unless integer? bt/1 [continue]

                bt/1: bt/1 - 1 ;; clears new-line marker, have to reset it
                new-line bt true

                if bt/1 < offset [
                    clear bt
                    break
                ]
            ]

            ; If a singular property was requested, then select it out and
            ; return it.
            ;
            case [
                any [block func_STACK args] [
                    prop: select bt offset
                    return to-value if prop [
                        second prop
                    ]
                ]

                word [
                    prop: select bt offset
                    return to-value if prop [
                        first prop
                    ]
                ]

                size [fail "STACK/SIZE no longer supported"]

                limit [fail "STACK/LIMIT no longer supported"]
            ]

            ; Or by default just return the backtrace/only minus the
            ; last thing (the above is more a test for backtrace than
            ; anything...hence very roundabout)
            ;
            bt: backtrace/only
            take/last bt
            bt
        ])
    ]

    ;
    ; set-infix on PATH! instead of WORD! is still TBD
    ;
    set-infix (bind 'and system/contexts/user) :and*
    set-infix (bind 'or system/contexts/user) :or+
    set-infix (bind 'xor system/contexts/user) :xor+

    ; NOTE: these flags only work in debug builds.  A better availability
    ; test for the functionality is needed, as these flags may be expired
    ; at different times on a case-by-case basis.
    ;
    ; (We don't flip these switches until after the above functions have been
    ; created, so that the shims can use Ren-C features like word-valued
    ; refinements/etc.)
    ;
    system/options/lit-word-decay: true
    system/options/broken-case-semantics: true
    system/options/exit-functions-only: true
    system/options/mutable-function-bodies: true
    system/options/refinements-blank: true
    system/options/no-switch-evals: true
    system/options/no-switch-fallthrough: true
    system/options/forever-64-bit-ints: true
    system/options/print-forms-everything: true
    system/options/break-with-overrides: true
    system/options/none-instead-of-voids: true
    system/options/arg1-arg2-arg3-error: true
    system/options/dont-exit-natives: true
    system/options/paren-instead-of-group: true
    system/options/get-will-get-anything: true
    system/options/no-reduce-nested-print: true

    r3-legacy-mode: on
    return blank
]
