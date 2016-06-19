REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Boot Base: Function Constructors"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Note: {
        This code is evaluated just after actions, natives, sysobj, and other lower
        levels definitions. This file intializes a minimal working environment
        that is used for the rest of the boot.
    }
]

; Due to wanting R3-Alpha to be able to run the bootstrap build, these objects
; could not unset these fields.  (make object! [x: ()] fails in R3-Alpha)
;
system/standard/function-meta/description:
system/standard/function-meta/parameter-types:
system/standard/function-meta/parameter-notes:
system/standard/specialized-meta/description:
system/standard/specialized-meta/specializee:
system/standard/specialized-meta/specializee-name:
system/standard/adapted-meta/description:
system/standard/adapted-meta/adaptee:
system/standard/adapted-meta/adaptee-name:
system/standard/chained-meta/description:
system/standard/chained-meta/chainees:
system/standard/chained-meta/chainee-names:
system/standard/hijacked-meta/description:
system/standard/hijacked-meta/hijackee:
system/standard/hijacked-meta/hijackee-name:
    ()


does: func [
    {A shortcut to define a function that has no arguments or locals.}
    body [block!] {The body block of the function}
][
    func [] body
]


make-action: func [
    {Internal generator used by FUNCTION and PROCEDURE specializations.}
    generator [function!]
        {Arity-2 "lower"-level function generator to use (e.g. FUNC or PROC)}
    spec [block!]
        {Help string (opt) followed by arg words (and opt type and string)}
    body [block!]
        {The body block of the function}
    /with
        {Define or use a persistent object (self)}
    object [object! block! map!]
        {The object or spec}
    /extern
        {Provide explicit list of external words}
    words [block!]
        {These words are not local.}
][
    if with [
        unless object? object [object: make object! object]
        body: copy/deep body
        bind body object
    ]

    ; Gather the SET-WORD!s in the body, ignoring some excluded candidates.
    ;
    words: collect-words/deep/set/ignore body compose [
        (spec) ;-- ignore the ANY-WORD!s already in the spec block

        (if with [words-of object]) ;-- ignore fields of /WITH object (if any)

        (if with ['self]) ; !!! REVIEW: ignore self too if binding to object?

        (:words) ;-- ignore explicit words given as an argument (if any)
    ]

    ; !!! The words that come back from COLLECT-WORDS are all WORD!, but we
    ; need SET-WORD! to specify pure locals to the generators.  Review the
    ; COLLECT-WORDS interface to efficiently give this result.
    ;
    spec: copy spec
    for-next words [
        append spec to set-word! words/1
    ]

    generator spec body
]

;-- These are "redescribed" after REDESCRIBE is created
;
function: specialize :make-action [generator: :func]
procedure: specialize :make-action [generator: :proc]


; Functions can be chained, adapted, and specialized--repeatedly.  The meta
; information from which HELP is determined can be inherited through links
; in that meta information.  Though in order to mutate the information for
; the purposes of distinguishing a derived function, it must be copied.
;
dig-function-meta-fields: function [value [function!]] [
    meta: meta-of :value

    underlying: is function! any [
        :meta/specializee
        :meta/adaptee
        :meta/hijackee
        all [block? :meta/chainees | first meta/chainees]
    ]

    fields: all [:underlying | dig-function-meta-fields :underlying]

    inherit-frame: function [parent [blank! frame!]] [
        if blank? parent [return blank]

        child: make frame! :value
        for-each param child [
            if ? select parent param [
                child/(param): copy parent/(param)
            ]
        ]
        return child
    ]

    return make system/standard/function-meta [
        description: (
            is string! any [
                select meta 'description
                all [fields | copy fields/description]
            ]
        )
        parameter-types: (
            is frame! any [
                select meta 'parameter-types
                all [fields | inherit-frame :fields/parameter-types]
            ]
        )
        parameter-notes: (
            is frame! any [
                select meta 'parameter-notes
                all [fields | inherit-frame :fields/parameter-notes]
            ]
        )
    ]
]

redescribe: function [
    {Mutate function description with new title and/or new argument notes.}

    spec [block!]
        {Either a string description, or a spec block (without types).}
    value [function!]
][
    meta: meta-of :value
    notes: _

    ; For efficiency, objects are only created on demand by hitting the
    ; required point in the PARSE.  Hence `redescribe [] :foo` will not tamper
    ; with the meta information at all, while `reescribe [{stuff}] :foo` will
    ; only manipulate the description.not created unless they are needed by
    ; hitting the required point in the PARSE of the spec.

    on-demand-meta: func [] [
        case/all [
            not meta [
                meta: copy system/standard/function-meta
                set-meta :value meta
            ]

            not find meta 'description [
                fail [{archetype META-OF doesn't have DESCRIPTION slot} meta]
            ]

            not notes: any [:meta/parameter-notes] [
                return () ; specialized or adapted, HELP uses original notes
            ]

            not frame? notes [
                fail [{PARAMETER-NOTES in META-OF is not a FRAME!} notes]
            ]

            :value != function-of notes [
                fail [{PARAMETER-NOTES in META-OF frame mismatch} notes]
            ]
        ]
    ]

    ; !!! SPECIALIZEE and SPECIALIZEE-NAME will be lost if a REDESCRIBE is
    ; done of a specialized function that needs to change more than just the
    ; main description.  Same with ADAPTEE and ADAPTEE-NAME in adaptations.
    ;
    ; (This is for efficiency to not generate new keylists on each describe
    ; but to reuse archetypal ones.  Also to limit the total number of
    ; variations that clients like HELP have to reason about.)
    ;
    on-demand-notes: func [] [
        on-demand-meta

        if find meta 'parameter-notes [return ()]

        fields: dig-function-meta-fields :value

        meta: blank ;-- need to get a parameter-notes field in the OBJECT!
        on-demand-meta ;-- ...so this loses SPECIALIZEE, etc.

        description: meta/description: fields/description
        notes: meta/parameter-notes: fields/parameter-notes
        types: meta/parameter-types: fields/parameter-types
    ]

    unless parse spec [
        opt [
            set description: string! (
                either all [equal? description {} | not meta] [
                    ; No action needed (no meta to delete old description in)
                ][
                    on-demand-meta
                    meta/description: if not equal? description {} [
                        description
                    ]
                ]
            )
        ]
        any [
            set param: [word! | get-word! | lit-word! | refinement!]

            ; It's legal for the redescribe to name a parameter just to
            ; show it's there for descriptive purposes without adding notes.
            ; But if {} is given as the notes, that's seen as a request
            ; to delete a note.
            ;
            opt [[set note: string!] (
                on-demand-meta
                if (not equal? note {}) or notes [
                    on-demand-notes

                    unless find notes to word! param [
                        fail [param "not found in frame to describe"]
                    ]

                    actual: first find words-of :value param
                    unless strict-equal? param actual [
                        fail [param {doesn't match word type of} actual]
                    ]

                    notes/(to word! param): if not equal? note {} [note]
                ]
            )]
        ]
    ][
        fail [{REDESCRIBE specs should be STRING! and ANY-WORD! only:} spec]
    ]

    ; If you kill all the notes then they will be cleaned up.  The meta
    ; object will be left behind, however.
    ;
    if all [notes | every [param note] notes [not set? 'note]] [
        meta/parameter-notes: ()
    ]

    :value ;-- should have updated the meta
]


redescribe [
    {Define an action with set-words as locals, that returns a value.}
] :function

redescribe [
    {Define an action with set-words as locals, that doesn't return a value.}
] :procedure


; LOGIC VERSIONS OF CONTROL STRUCTURES
;
; Control structures evaluate to either void (if no branches taken) or the
; last value of any evaluated blocks.  This applies to everything from IF
; to CASE to WHILE.  The ? versions are tailored to return whether a branch
; was taken at all, and always return either TRUE or FALSE.

if?: redescribe [
    {Variation of IF which returns TRUE if the branch runs, FALSE if not}
](
    specialize 'if [?: true]
)

unless?: redescribe [
    {Variation of UNLESS which returns TRUE if the branch runs, FALSE if not}
](
    specialize 'unless [?: true]
)

while?: redescribe [
    {Variation of WHILE which returns TRUE if the body ever runs, FALSE if not}
](
    specialize 'while [?: true]
)

case?: redescribe [
    {Variation of CASE which returns TRUE if any cases run, FALSE if not}
](
    specialize 'case [?: true]
)

switch?: redescribe [
    {Variation of SWITCH which returns TRUE if any cases run, FALSE if not}
](
    specialize 'switch [?: true]
)

trap?: redescribe [
    {Variation of TRAP which returns TRUE if an error traps, FALSE if not}
](
    specialize 'trap [?: true]
)

catch?: redescribe [
    {Variation of CATCH which returns TRUE if a throw is caught, FALSE if not}
](
    specialize 'catch [?: true]
)

any?: redescribe [
    {Shortcut OR, ignores voids. Unlike plain ANY, forces result to LOGIC!}
](
    chain [:any :true?]
)

all?: redescribe [
    {Shortcut AND, ignores voids. Unlike plain ALL, forces result to LOGIC!}
](
    chain [:all :true?]
)

find?: redescribe [
    {Variant of FIND that returns TRUE if present and FALSE if not.}
](
    chain [:find :true?]
)

select?: redescribe [
    {Variant of SELECT that returns TRUE if a value was selected, else FALSE.}
](
    chain [:select :any-value?]
)

; To help for discoverability, there is SET-INFIX and INFIX?.  However, the
; term can be a misnomer if the function is more advanced, and using the
; "lookback" capabilities in another way.  Hence these return descriptive
; errors when people are "outside the bounds" of assurance RE:infixedness.

arity-of: function [
    value [any-word! any-path! function!]
][
    if path? :value [fail "arity-of for paths is not yet implemented."]

    unless function? :value [
        value: get value
        unless function? :value [return 0]
    ]

    if variadic? :value [
        fail "arity-of cannot give reliable answer for variadic functions"
    ]

    ; !!! Should willingness to take endability cause a similar error?
    ; Arguably the answer tells you an arity that at least it *will* accept,
    ; so it's not completely false.

    arity: 0
    for-each param reflect :value 'words [
        if refinement? :param [
            return arity
        ]
        arity: arity + 1
    ]
    arity
]

nfix?: function [
    n [integer!]
    name [string!]
    source [any-word! any-path!]
][
    case [
        not lookback? source [false]
        equal? n arity: arity-of source [true]
        n < arity [
            ; If the queried arity is lower than the arity of the function,
            ; assume it's ok...e.g. PREFIX? callers know INFIX? exists (but
            ; we don't assume INFIX? callers know PREFIX?/ENDFIX? exist)
            false
        ]
        'default [
            fail [
                name "used on lookback function with arity" arity
                | "Use LOOKBACK? for generalized (tricky) testing"
            ]
        ]
    ]
]

endfix?: redescribe [
    {TRUE if a no-argument function is SET/LOOKBACK to not allow right infix.}
](
    specialize :nfix? [n: 0 | name: "ENDFIX?"]
)

postfix?: redescribe [
    {TRUE if an arity 1 function is SET/LOOKBACK to act as postfix.}
](
    specialize :nfix? [n: 1 | name: "POSTFIX?"]
)

infix?: redescribe [
    {TRUE if an arity 2 function is SET/LOOKBACK to act as infix.}
](
    specialize :nfix? [n: 2 | name: "INFIX?"]
)


set-nfix: function [
    n [integer!]
    name [string!]
    target [any-word! any-path!]
    value [function!]
][
    unless equal? n arity-of :value [
        fail [name "requires arity" n "functions, see SET/LOOKAHEAD"]
    ]
    set/lookback target :value
]

set-endfix: redescribe [
    {Convenience wrapper for SET/LOOKBACK that ensures function is arity 0.}
](
    specialize :set-nfix [n: 0 | name: "SET-ENDFIX"]
)

set-postfix: redescribe [
    {Convenience wrapper for SET/LOOKBACK that ensures a function is arity 1.}
](
    specialize :set-nfix [n: 1 | name: "SET-POSTFIX"]
)

set-infix: redescribe [
    {Convenience wrapper for SET/LOOKBACK that ensures a function is arity 2.}
](
    specialize :set-nfix [n: 2 | name: "SET-INFIX"]
)


lambda: function [
    {Convenience variadic wrapper for FUNCTION constructors}
    args [<end> word! block!]
        {Block of argument words, or a single word (passed via LIT-WORD!)}
    :body [any-value! <...>]
        {Block that serves as the body or variadic elements for the body}
    /only
        {Use FUNC and do not run locals-gathering on the body}
][
    f: either only :func :function
    f case [
        not set? 'args [[]]
        word? args [reduce [args]]
        'default [args]
    ] case [
        block? first body [take body]
        'default [make block! body]
    ]
]


left-bar: func [
    {Expression barrier that evaluates to left side but executes right.}
    left [<opt> any-value!]
    right [<opt> any-value! <...>]
][
    while [not tail? right] [take right]
    :left
]

right-bar: func [
    <punctuates>
    {Expression barrier that evaluates to first expression on right.}
    right [<opt> any-value! <...>]
][
    also take right (while [not tail? right] [take right])
]

once-bar: func [
    <punctuates>
    {Expression barrier that's willing to only run one expression after it}
    right [<opt> any-value! <...>]
    :lookahead [any-value! <...>]
    look:
][
    also take right (
        unless any [
            tail? right
            bar? look: first lookahead
            all [
                find [word! function!] type-of :look
                punctuates? :look
            ]
        ][
            ; Can't tell if a PATH! is punctuating w/o risking execution.
            ; Be conservative. <punctuating> might not be the attribute
            ; sought after anyway, e.g. `1 + 2 || 3 + 4 print "Hi"` probably
            ; ought to be an error.  "barrier-like" may be the quality.
            ;
            fail [
                "|| expected punctuating expression, found" :look
            ]
        ]
    )
]


use: func [
    {Defines words local to a block.}
    vars [block! word!] {Local word(s) to the block}
    body [block!] {Block to evaluate}
][
    ; We are building a FUNC out of the body that was passed to us, and that
    ; body may have RETURN words with bindings in them already that we do
    ; not want to disturb with the definitional bindings in the new code.
    ; So that means either using MAKE FUNCTION! (which wouldn't disrupt
    ; RETURN bindings) or using the more friendly FUNC with <no-return>
    ; (they do the same thing, just FUNC is arity-2)
    ;
    ; <durable> is used so that the data for the locals will still be
    ; available if any of the words leak out and are accessed after the
    ; execution is finished.
    ;
    eval func compose [<durable> <no-return> /local (vars)] body
]

; !!! Historically OBJECT was essentially a synonym for CONTEXT with the
; ability to tolerate a spec of `[a:]` by transforming it to `[a: none].
; The tolerance of ending with a set-word has been added to the context
; native, which avoids the need to mutate (or copy) the spec to add the none.
;
; !!! Ren-C intends to grow object into a richer construct with a spec.
;
object: :context

module: func [
    "Creates a new module."
    spec [block! object!] "The header block of the module (modified)"
    body [block!] "The body block of the module (modified)"
    /mixin "Mix in words from other modules"
    mixins [object!] "Words collected into an object"
    /local hidden w mod
][
    mixins: to-value :mixins

    ; !!! Is it a good idea to mess with the given spec and body bindings?
    ; This was done by MODULE but not seemingly automatically by MAKE MODULE!
    ;
    unbind/deep body

    ; Convert header block to standard header object:
    ;
    if block? :spec [
        unbind/deep spec
        spec: attempt [construct/with :spec system/standard/header]
    ]

    ; Validate the important fields of header:
    assert/type [
        spec object!
        body block!
        mixins [object! blank!]
        spec/name [word! blank!]
        spec/type [word! blank!]
        spec/version [tuple! blank!]
        spec/options [block! blank!]
    ]

    ; In Ren-C, MAKE MODULE! acts just like MAKE OBJECT! due to the generic
    ; facility for SET-META.

    mod: make module! 7 ; arbitrary starting size

    if find spec/options 'extension [
        append mod 'lib-base ; specific runtime values MUST BE FIRST
    ]

    unless spec/type [spec/type: 'module] ; in case not set earlier

    ; Collect 'export keyword exports, removing the keywords
    if find body 'export [
        unless block? select spec 'exports [
            repend spec ['exports make block! 10]
        ]

        ; Note: 'export overrides 'hidden, silently for now
        parse body [while [
            to 'export remove skip opt remove 'hidden opt
            [
                set w any-word! (
                    unless find spec/exports w: to word! w [
                        append spec/exports w
                    ]
                )
            |
                set w block! (
                    append spec/exports collect-words/ignore w spec/exports
                )
            ]
        ] to end]
    ]

    ; Collect 'hidden keyword words, removing the keywords. Ignore exports.
    hidden: _
    if find body 'hidden [
        hidden: make block! 10
        ; Note: Exports are not hidden, silently for now
        parse body [while [
            to 'hidden remove skip opt
            [
                set w any-word! (
                    unless find select spec 'exports w: to word! w [
                        append hidden w]
                )
            |
                set w block! (
                    append hidden collect-words/ignore w select spec 'exports
                )
            ]
        ] to end]
    ]

    ; Add hidden words next to the context (performance):
    if block? hidden [bind/new hidden mod]

    if block? hidden [protect/hide/words hidden]

    set-meta mod spec

    ; Add exported words at top of context (performance):
    if block? select spec 'exports [bind/new spec/exports mod]

    either find spec/options 'isolate [
        ;
        ; All words of the module body are module variables:
        ;
        bind/new body mod

        ; The module keeps its own variables (not shared with system):
        ;
        if object? mixins [resolve mod mixins]

        comment [resolve mod sys] ; no longer done -Carl

        resolve mod lib
    ][
        ; Only top level defined words are module variables.
        ;
        bind/only/set body mod

        ; The module shares system exported variables:
        ;
        bind body lib

        comment [bind body sys] ; no longer done -Carl

        if object? mixins [bind body mixins]
    ]

    bind body mod ;-- redundant?
    do body

    ;print ["Module created" spec/name spec/version]
    mod
]

cause-error: func [
    "Causes an immediate error throw with the provided information."
    err-type [word!]
    err-id [word!]
    args
][
    ; Make sure it's a block:
    args: compose [(:args)]
    ; Filter out functional values:
    for-next args [
        if function? first args [
            change/only args meta-of first args
        ]
    ]
    ; Build and throw the error:
    fail make error! [
        type: err-type
        id:   err-id
        arg1: first args
        arg2: second args
        arg3: third args
    ]
]

default: func [
    "Set a word to a default value if it hasn't been set yet."
    'word [word! set-word! lit-word!] "The word (use :var for word! values)"
    value "The value" ; void not allowed on purpose
][
    unless all [set? word | not blank? get word] [set word :value] :value
]


ensure: func [
    {Pass through data that isn't VOID? or FALSE?, but FAIL otherwise}
    arg [<opt> any-value!]
    /value
        {Only check for ANY-VALUE? (FALSE and NONE ok, but not void)}
    /type
    types [block! datatype! typeset!]
        {FAIL only if not one of these types (block converts to TYPESET!)}

    ; !!! To be rewritten as a native once behavior is pinned down.
][
    unless any-value? :arg [
        unless type [fail "ENSURE did not expect value to be void"]
    ]

    unless type [
        unless any [arg value] [
            fail ["ENSURE did not expect arg to be" (mold :arg)]
        ]
        return :arg
    ]

    unless find (case [
        block? :types [make typeset! types]
        typeset? :types [types]
        datatype? :types [reduce [types]] ;-- we'll find DATATYPE! in a block
        fail 'unreachable
    ]) type-of :arg [
        fail ["ENSURE did not expect arg to have type" (type-of :arg)]
    ]
    :arg
]


secure: func ['d] [boot-print "SECURE is disabled"]
