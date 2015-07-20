The hooks that git executes (such as a "pre-commit" hook), are placed
in `.git/hooks`.  These are *not* part of the tracked files under
version control, so you will not get them installed from a `git clone`

So it is necessary for each developer to copy these over after they
have cloned.  The hooks may rely on functions that are not available
on some build systems, so that should be considered.

Improvements are welcome.  For now, the goal is just to keep spaces from
sneaking into files where they shouldn't be, but much more can be
done.  Please at minimum do this if you are going to be contributing:

    cp git-hooks/pre-commit .git/hooks/pre-commit
