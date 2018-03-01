; datatypes/unset.r
[void? ()]
[blank? type of ()]
[not void? 1]

[#68 | void? trap/with [a: ()] [_]]

[error? trap [a: () a]]
[not error? trap [set/only 'a ()]]
