.. _limitations:

====================================
CTester limitations and improvements
====================================

CTester has several limitations, bugs, and source of improvements that
you should be aware of.

Limitations
===========

Missing signals
---------------

Only the SIGSEGV and the SIGFPE signals are currently caught and managed.
SIGALRM is also used for the timeout timer.

If any other signal happens (for instance, SIGPIPE), the whole CTester program
will be terminated.

Circumventing signals
---------------------

Currently, the :manpage:`sigaction(2)` system call (and other related calls)
are not wrapped. This means that the student could easily call these functions
and change the signal handlers to something more appropriate for him.

Circumventing the timer
-----------------------

Similarly, :manpage:`setitimer(2)` is not wrapped, and the student could
disable the timer.

No randomization of the structures
----------------------------------

The internal structures are mostly declared "statically", and an :code:`extern`
declaration of the appropriate structure inside the student code can easily
give him full access to that structure, if he knows how it works.

A workaround would be removing that access (dynamic allocation+randomization),
or making impossible the :code:`extern` declaration, e.g. by adding fields
to the structures unknown to the student.

Another case of non-randomization is that the student function is called
by CUnit, which calls the function without any precaution or randomization.
As CTester doesn't currently add any precaution on its side, it means that
the student function is, more or less, directly called by CTester, and that
a clever student could modify some variables in the caller functions' stacks.

Missing functions
-----------------

There is a lot of missing functions, and some of them should be wrapped:

- :manpage:`exit` (actually wrapped, but badly).
- :manpage:`execve(2)`, :manpage:`fork(2)` and others. :manpage:`waitpid(2)`
  should be updated then.
- :manpage:`sigaction(2)`, :manpage:`sigaltstack(2)`, :manpage:`sigemptyset(3)`,
  :manpage:`signal(2)`, :manpage:`raise(3)` and others.
- :manpage:`kill(2)` and related.
- :manpage:`setjmp(2)`, :manpage:`longjmp(2)` and others.
- :manpage:`phtread_create(3)`, :manpage:`pthread_kill(3)`,
  :manpage:`pthread_join(3)` etc.
- :manpage:`sem_init(3)`, :manpage:`sem_open(3)`, :manpage:`sem_wait(3)`,
  :manpage:`sem_post(3)` etc
- :manpage:`pipe(2)`
- :manpage:`fcntl(2)`
- :manpage:`fopen(3)`, :manpage:`fread(3)`, :manpage:`fwrite(3)` etc.

Etc

Incoherent naming of a few functions
------------------------------------

Trap is the perfect example: the functions are badly named.

Some fields of some statistics structures too.

Design inconveniences of the API
--------------------------------

Trap: the pointer to pass for deletion is different than the pointer that
was returned, in a non-obvious way.

:code:`SANDBOX_BEGIN;` needs a semicolon at the end, for some reason.

Limitations to memory access control (including access to internal structures)
------------------------------------------------------------------------------

Again, most of the internal structures are not protected, partly because
it is C after all, and by design the user can do whatever he wants.

Array access, memory access through pointers etc are not properly handled;
any misuse from the student can lead to undefined behaviour, in his code
but also in CTester's code if he accessed structures he should not.

Standard output and standard error streams
------------------------------------------

These are pipes, so they are limited. For instance, there is a maximum
capacity for the pipes, that varies between systems (16 pages, or 65536 bytes,
is the current maximum). If the student code writes too much to either
standard outputs, there is a chance of reaching this limit. As the pipes
are non-blocking, any write to the full pipes will fail with :code:`EAGAIN`,
but it means that some information may be lost (like the double frees).

An improvement would be to redirect these to some local files, and to manually
truncate the writes if it is done from the student code.

Scores and grading
------------------

As of now, a task is successful if the total score is above some threshold.
That threshold is currently limited to a choice between 50% and 100%.

Also, the only way to customize the scoring is by adding correct weights
to the test functions.

Exit
----

It is wrapped by CTester, to prevent the student from exiting the tests.
But it also means that the student can't call that function when it encounters
some error, which may sometimes be needed.

Double free detection
---------------------

It relies on detecting the string "double free or corruption" inside
the redirected standard error stream of the program.

As it relies on :code:`read`, it is very well possible that this string is cut
in parts in the buffer, and that we miss this message.
Currently, the wrapper for :code:`free` doesn't detect double frees either.

Timer race condition
--------------------

It is possible, albeit complicated, that the timeout signal arrives during
the call to the function ending the sandbox, after that this call has reset
the pipes and the file descriptors to a normal state, bu before that this call
has disabled the signal.

It should be easy to fix: move lines in that function so that the timer is
disabled before we do anything else. But maybe that it introduces other errors.

Adding tests
============

It is possible to add some integration tests to CTester, in the folder
``ci``.

You can have a look at the folder structure and the tests themselves
to understand how they are written and how you can write new ones.

It is authorized to update the ``run_ci`` script, if you want to perform
other kinds of tests.

