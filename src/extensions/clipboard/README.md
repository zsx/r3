The `clipboard://` port should ultimately be provided by a "clipboard extension".  But either way, even today, not all platforms offer APIs for the clipboard:

[#2029: Extend clipboard:// implementation to all supported platforms](https://github.com/rebol/rebol-issues/issues/2029)

Since the clipboard is not available on all platforms *(and as an extension, might not be built in even on those platforms where it is available)*, the clipboard-based tests were not guaranteed to succeed.  They are kept here for now, as extension-specific tests should likely live with those extensions in their directory.
