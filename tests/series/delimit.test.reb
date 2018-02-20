["" = delimit [] #" "]
["1 2" = delimit [1 2] #" "]

["" = delimit [] _]
["1" = delimit [1] _]
["12" = delimit [1 2] _]

["^/^/" = delimit ["^/" "^/"] #" "]
["1^/^/2" = delimit ["1^/" "2"] #"^/"]
