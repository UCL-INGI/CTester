.. _wrap_file:

==================================================
File-related functions, structures, and statistics
==================================================

This page only talks about the wrappers and the statistics of the file-related
functions. Please check the page :ref:`partial_read_write` for an API allowing
to finely tune the "partial return" behaviour of the :code:`read`
and :code:`write` functions.

The following functions have a wrapper:

- :manpage:`open(2)`
- :manpage:`creat(2)`
- :manpage:`close(2)`
- :manpage:`read(2)`
- :manpage:`write(2)`
- :manpage:`stat(2)`
- :manpage:`fstat(2)`
- :manpage:`lseek(2)`

A particular missing function is :manpage:`fcntl(2)`. Feel free to add support for it!

Notes on specific calls
=======================

The rest of this page only describes the differences with the usual form
of the wrappers.

As always, there is a function to reset the statistics of the wrappers,
:code:`reinit_file_stats`, taking to argument.

:code:`open`
------------

CTester currently only supports the :manpage:`open(2)` system call with three arguments;
if one calls the :code:`open` system call with two arguments, it is likely that
it won't be intercepted (not tested). Simply check that the student called
the right version through the statistics.

:code:`read`
------------

This is the only function that supports the partial read/write from the API
described in the page :ref:`partial_read_write`.

:code:`stat` and :code:`fstat`
------------------------------

The :code:`struct stats_stat_t` and :code:`struct stats_fstat_t` structures [1]_ have
an additional field, :code:`returned_stat`, which is a copy of the :code:`struct stat`
structure which was filled by the last call to :code:`stat` or :code:`fstatt`.

.. [1] Wordy names for structures

