![Ren-C Logo][100]

# Ren/C
[![Build Status][101]](https://travis-ci.org/metaeducation/ren-c)


**Ren-C** is an interim fork of the [Apache 2.0 open-sourced][1] [Rebol][2] codebase.  The
goal of the project isn't to be a new language or a different console, rather to provide
a smooth API for embedding a Rebol interpreter in C programs.  This API would offer
nearly the full range of power that is internally offered to the core, making it
easy to write new clients or REPLs using it.

[1]: http://www.rebol.com/cgi-bin/blog.r?view=0519
[2]: https://en.wikipedia.org/wiki/Rebol

Because the API is not fully ready for publication, the current way to explore the new
features of Ren-C is using the `r3` console built by the makefile.  It should function
nearly identically (though it has been extended through user contribution to support a
multi-line continuation method similar to Rebol2.)  For those interested in a more
novel application of the Ren-C library, see the C++ binding and [Ren Garden][3].

[3]: http://rencpp.hostilefork.com

In the process of designing the library, Ren-C also aspires to solve several of the
major outstanding design problems that were left unfinished in the R3-Alpha codebase.
Several of these problems have been solved already--and for progress and notes on
these issues, a [Trello board][4] is frequently updated to reflect a summary of
some of the changes.

[4]: https://trello.com/b/l385BE7a/rebol3-porting-guide-ren-c-branch

In doing this work, the hope is to provide an artifact that would rally common
usage between the [mainline builds][5], community builds, and those made by
[Atronix Engineering][6] and [Saphirion AG][7].

[5]: http://rebolsource.net
[6]: http://www.atronixengineering.com/downloads
[7]: http://development.saphirion.com/rebol/saphir/

For more information, please visit the FAQ:

https://github.com/metaeducation/ren-c/wiki/FAQ

Feel free to add your own questions to the bottom of the list.


## Community

To promote the Rebol community's participation in public forums, development discussion
for Ren-C generally takes place in the [Rebol and Red StackOverflow Chat][8].

[8]: http://rebolsource.net/go/chat-faq

It is also possible to contact the developers through the [Ren-C GitHub Issues][9]
page.  This should be limited to questions regarding the Ren-C builds specifically, as
overall language design wishes and debates are kept in the `rebol-issues` repository
of Rebol's GitHub.

[9]: https://github.com/metaeducation/ren-c/issues


## Building

There are currently two build systems in Ren-C: plain make files for basic features, and CMake for extended features.

* With plain make files

First get the sources -- from cloning the repository with `git`, or downloading a ZIP:

https://github.com/metaeducation/ren-c/archive/master.zip

Next you need to [get a pre-built R3-Alpha interpreter](http://rebolsource.net), rename
it to `r3-make` or `r3-make.exe`, and put it in the `%make/` subdirectory.

Then run:

	make -f makefile.boot

The platform to target will be assumed to be the same as the build type of the
`r3-make` you use.  If your needs are more complex *(such as doing a cross-compilation,
or if the `system/version` in your r3-make doesn't match the target you want)*, refer
to the bootstrap makefile `%src/make/makefile.boot`:

https://github.com/metaeducation/ren-c/blob/master/make/makefile.boot

*(Note: Ren-C's build process cannot be performed with Rebol2.  It requires R3-Alpha
or Ren-C itself.  However, it can build using an old pre-open-source R3-Alpha A111.)*

* With CMake

Please see https://github.com/metaeducation/ren-c/wiki/Building-Ren-C-with-CMake

[100]: https://raw.githubusercontent.com/metaeducation/ren-c/master/ren-c-logo.png
[101]: https://travis-ci.org/metaeducation/ren-c.svg?branch=master
