.. _usage:

=============
CTester usage
=============

First of all, CTester is designed to be used in INGInious. This has a lot of
consequences in the design choices and the way to run the programs and the tests.

Global view
===========

The general workflow for creating an exercise in CTester is as follows:

- In  INGInious, you create a task. The ``run`` file should be the file named
  ``run`` at the root of CTester, and should be copied. The rest of CTester
  can be placed either in the same directory (thus, ``run.py`` is next to
  ``run``, as well as the folder ``student``), or in the ``$common/CTester/``
  folder (which should contain the ``student`` folder as well as the ``run.py``
  file), which will be copied in the proper place.
- In the ``student`` folder, you create a file named ``tests.c``, which will
  contain your tests, and two other files named ``student_code.h`` and
  ``student_code.c.tpl``, which will define the functions the student needs
  to fill in. For instance, the header will contain the prototype of
  the function, and the ``.c.tpl`` file will contain the implementation, with
  the string ``@@problem_id@@`` replacing the various places the student will
  need to write code.
- In INGInious, you declare as many subproblems as places where the student
  should fill code in the ``.c.tpl`` file, with corresponding problem ids.
- In the ``tests.c`` file, you include ``student_code.h`` and
  ``CTester/CTester.h``.
- You write one or more test function, with appropriate test metadata, and
  call the functions defined in ``student_code.h`` (which will get a proper
  implementation in the ``student_code.c`` file when the task is sent to run)
  inside the sandbox provided by CTester, and you perform tests on the returned
  values of the functions of the student.
- In the :code:`main` function, you ban functions the student cannot use
  (with :c:func:`BAN_FUNCS()`), and you pass each test function to run to the
  :c:func:`RUN()` function.
- You test on INGInious if your task accepts the proper result and rejects
  wrong answers, iterating with debugs and other tasks.
- You let the students attempt your exercise, and watch them suffer in the
  marvelous world of C programming.

A basic skeleton of such a task is available in the default ``student_code.h``,
``student_code.c.tpl`` and ``tests.c`` files of CTester.

Detailing the different parts
=============================

This section describes in more detail the various steps outlined above.

The ``student_code.h`` and ``student_code.c.tpl`` files
-------------------------------------------------------

CTester expects you to place the code written by the students in these two files.

It is strongly recommanded that you ask students to complete bodies of functions,
and not parts of files, for the following reasons:

- This leads to smaller problems, which are easier to grade;
- This teaches them to think in terms of functions, and to structure their
  programs in functions;
- This prevents them from declaring global variables, or from redeclaring
  structures or variables used by CTester. For instance, the :c:data:`stats`
  variable is used to record usage statistics. A simple
  :code:`extern struct wrap_stats_t stats` in the global space of the file
  is sufficient to gain access to this variable (this is a problem of C).
  Therefore, limiting them to functions bodies restricts the risk of them
  interfering with CTester. [#externfunc]_

The functions and their empty bodies should go inside the ``.C.tpl`` file,
and the function prototypes should go inside the ``.h``.
CTester's headers should not be included in these files!
You should put proper problem id references inside the bodies of the functions.

Note that it is possible that you put non-empty bodies inside the functions,
if you want the student to only complete some part of that body (for instance,
if you want to convert the results from the student code to a more
appropriate format for testing).

.. [#externfunc] Actually, :code:`extern` is allowed inside functions, but
   one can hope that the student is not aware of this...

Writing a test function
-----------------------

Writing a test function follows mostly the same schema::

    void test_myfunc() {
        set_test_metadata("myfunc", _("Brief description of the test"), 1);
        int ret = -1;
        SANDBOX_BEGIN;
        ret = myfunc(21); // Suppose myfunc(x) = 2*x
        SANDBOX_END;
        CU_ASSERT_EQUAL(ret, 42);
    }

- The test function should take no argument and return nothing.
- The first line should specify some metadata of the test, namely, the function
  that is tested, what is actaully tested, and the relative weight of the test
  in the score of the student. See :c:func:`set_test_metadata()` below.
- The code written by the student (here, the content of the :code:`myfunc`
  function) should be executed within a sandbox.
- Tests should use CUnit to check that the student functions work as wished.

A very important remark is that you should **not** declare any variable inside
the sandbox: instead, declare them before. Indeed, the sandbox doesn't
guarantee that all the code inside it will be executed: any exception inside
of it will cause subsequent code to never be executed. Variable declarations
inside the sandbox have therefore a risk of being risky. You can, however,
assign variables inside the sandbox: it is even recommanded so that you can
perform tests on the results.

For the same reason that declaring variables inside the sandbox is forbidden,
we recommend initializing the variables that are assigned in the sandbox with
an "incorrect" value, i.e. a value that makes the following tests fail.
That way, tests will not have the risk of succeed if we exit the sandbox,
a precaution that is particularly necessary as it is sadly currently possible
to circumvent the sandbox' handling of exceptions...


It is recommanded that you only run a minimal set of tests inside one test
function. Monitorings, statistics, failures and other internal structures used
for some features are reset at the beginning of the test, just before the call
to the function containing the tests, and so you have the guarantee of having
a clean environment at the beginning of the test, which could become cluttered
as you add code to your test.

Test metadata
~~~~~~~~~~~~~

.. c:function:: void set_test_metadata(char *problem, char *descr, unsigned int weight)

   This function gives CTester various informations (metadata) about the test
   in which this function is called. This information is notably shown
   in the logs of the program, and is the basis of the feedback provided
   to the student. Please avoid special characters like ``#``.

   :param problem: A string giving the name of the problem.
   :param descr:   A string giving a longer description of the problem.
   :param weight:  Weight of the test in the final score of the student.

The sandbox
~~~~~~~~~~~

A sandbox is delimited by two macros, :c:func:`SANDBOX_BEGIN`
and :c:func:`SANDBOX_END`.

Inside the sandbox, the code written by the student can be safely tested:

- Exceptions and other signals are caught by the sandbox and immediately
  cause an exit from the sandbox, without crashing CTester. The cause of error
  (segmentation fault, floating point exception) will be available in the logs.
- Monitoring of functions is enabled within the sandbox. This enables a lot
  of features, like statistics, programmed failures, replacement of functions,
  and more. See :ref:`wrap` for more information. All of these features
  are enabled (activated) solely inside the sandbox.
- There is a time limit for the running time of the code inside the sandbox.
  By default it is equal to 2 seconds. It is not yet possible to configure
  that value.

The tests
~~~~~~~~~

Testing should use CUnit. All the usually required functions, macros
and headers are available from CTester.

You should **not** perform tests inside the sandbox. Any code in the sandbox
has the risk of potentially never be executed, if there is an exception, and so
your tests would also run the risk of not being executed.

It is **extremely** important that your tests don't fail or cause any error,
as this error would be reported to the student as its own error.

Create and manage multiple tests
--------------------------------

All the test created should be put inside the ``tests.c`` file, and should be
called inside the :code:`main` function using the :c:func:`RUN` function.

Suppose that you have two test functions, :code:`test_nonempty_result()`
and :code:`test_correct_result()`, and you want to prevent the use of :code:`printf`. Then, your main should look like this::

    int main(int argc, char **argv)
    {
        BAN_FUNCS(printf);
        RUN(test_nonempty_result, test_correct_result);
    }

Don't add a :code:`return` statement, as it is included done by :c:func:`RUN`
(which is actually a macro).

.. c:type:: typedef void (*void_void_function_t)(void);

   This is a pointer to a function taking void and returning void.

.. c:function:: void RUN(void_void_function_t args[])

   This is a macro that executes the various functions passed in :code:`args`,
   that are functions taking no parameter and returning nothing (exactly
   the type of the testing functions).

   This macro is expanded in a true function call (with the number of functions
   passed too) and with a :code:`return` statement.

   This function essentially sets up the environment for executing
   the functions safely, providing a clean state for each test function
   at its start of execution.

.. c:function:: void BAN_FUNCS(args)

   This is a macro used to specify functions that should be banned for
   the student. Banned functions are not authorized in the student code, and
   will cause the tests to fail immediately.

Additional features
-------------------

There is a few more functions you can call inside your test functions:

.. c:function:: void push_info_msg(char *msg)

   This pushes the :code:`msg` message to the list of messages that will be
   printed in the feedback file.

.. c:function:: void set_tag(char *tag)

   This adds the tag to the test function. The tag will be displayed in the
   feedback file. There is a limit of 20 tags, which must have a length of
   less than 30 characters (including :code:`\0`).

.. _ctester_inginious_copy:

Calling on INGInious
--------------------

CTester is designed to be used on INGInious. As such, it provides a run file,
and the feedback it provides during the execution is parsed to obtain relevant
informations that are then displayed to the student.

To work properly, CTester requires the following files:

- the ``run`` file, without extension, which is run by INGInious;
- the ``run.py`` file, which is called by the ``run`` file and contains most
  of the logic of a standard run file;
- the ``student`` folder, which contains:

  - the ``CTester`` folder, containing the various headers and C files
    used by CTester;
  - the ``Makefile``, used for compiling all the files;
  - the above-described ``student_code.h``, ``student_code.c.tpl`` and ``tests.c`` files.

CTester requires either of the following folder structure:

.. code-block:: none

    root_of_the_course/
    ├─ $common/
    │  └─ ...
    ├─ my_task/
    │  ├─ task.yaml
    │  ├─ run
    │  ├─ run.py
    │  └─ student/
    │     ├─ CTester/
    │     │  ├─ CTester.h
    │     │  └─ ...
    │     ├─ Makefile
    │     ├─ student_code.h
    │     ├─ student_code.c
    │     └─ tests.c
    └─ ...

or

.. code-block:: none

    root_of_the_course/
    ├─ $common/
    │  ├─ CTester/
    │  │  ├─ run.py
    │  │  └─ student/
    │  │     ├─ CTester/
    │  │     │  ├─ CTester.h
    │  │     │  └─ ...
    │  │     └─ Makefile
    │  └─ ...─
    ├─ my_task/
    │  ├─ task.yaml
    │  ├─ run
    │  └─ student/
    │     ├─ student_code.h
    │     ├─ student_code.c
    │     └─ tests.c
    └─ ...

The first version places a copy of CTester inside each task folder.

The second version only has one copy of CTester, in the ``$common`` folder,
which is mounted to the ``/common/`` folder at run time. Each task then only
requires its copy of the ``run`` file (needed by INGInious), as well as
its custom versions of the ``student_code.h``, ``student_code.c.tpl``
and ``tests.c`` files.

The CTester ``run`` file, at run time, first looks for a copy of CTester
in the current folder, then looks for a copy of CTester in the ``/common`` folder.

A "copy of CTester" is merely defined, in the ``run`` file, as the presence
of the three files ``run.py``, ``Makefile`` and a folder named ``CTester/``
at the same time. The absence of any of the three causes the folder to not
be a "copy of CTester".

When copying CTester, the copy doesn't overwrite any file in the task folder.
If you want to customize CTester, by patching it or by adding additional files,
you can simply create a ``CTester`` folder in the ``student`` folder
and put your custom code there. Simply, to guarantee that ``run`` doesn't
consider the current folder as being a full copy, you should not provide
a custom version of ``Makefile`` and ``run.py``. Or, modify the ``run`` file
to your liking, but mention the modifications at the top of the file.

By default, the ``run.py`` script is called without argument; it considers
that a task is successful if the total score is above 50%.
If you want to make the task succeed if the total score is 100%,
just pass any additional argument to the call, different from ``--use-fifty``.

Standard output and error stream redirections
---------------------------------------------

In order to parse standard output and error streams for error messages,
and limit the output to a reasonable amount, CTester modifies these streams
by duplicating them and redirecting them to several other file descriptors
and pipes.

This section describes the various used file descriptors, their role at
different instants. We consider only those related to :code:`STDOUT_FILENO`;
there are similar file descriptors for :code:`STDERR_FILENO`.

- :code:`int STDOUT_FILENO` : the usual file descriptor of the standard output,
  equal to 1 by definition. It is where :code:`printf()` writes to. Inside
  the sandbox, it is redirected to :code:`pipe_stdout[1]` (using
  :manpage:`dup2(2)`), and at the exit from the sandbox it is redirected back
  to :code:`true_stdout`, and so, to the true output.
- :code:`FILE *stdout` : a stream version of :code:`STDOUT_FILENO`, for use
  by :manpage:`fwrite(3)` or :manpage:`fprintf(3)`.
- :code:`int true_stdout` : copies of the real standard output: this is
  the actual "file" referred to by :code:`STDOUT_FILENO` before we change
  :code:`STDOUT_FILENO` using :manpage:`dup2(2)` at the start of the sandbox,
  and is used at the exit of the sandbox to restore :code:`STDOUT_FILENO`
  to point to the true standard output. Writing to it writes to the process'
  :code:`stdout` stream.
- :code:`int pipe_stdout[2]` : this is a pipe, i.e. writing to 
  :code:`pipe_stdout[1]` makes the same data readable at :code:`pipe_stdout[0]`.
  The pipe is non-blocking, meaning that reading it when there is no data
  results in a :code:`EAGAIN` failure, and writing to it when the pipe is full
  results in :code:`EAGAIN` too. Inside the sandbox, writing to the standard
  output actually writes to :code:`pipe_stdout[1]`, making the student code's
  standard output available at :code:`pipe_stdout[0]`. At sandbox end,
  this content is sent to :code:`usr_pipe_stdout[1]` and to
  :code:`STDOUT_FILENO`.
- :code:`int usr_pipe_stdout[2]` : this is also a pipe, and is also
  non-blocking. At sandbox exit, it is filled with the standard output
  from the student code, available for reading. At sandbox start, it is emptied
  so that the sandbox is in a clean state.
- :code:`int stdout_cpy` : same value as :code:`usr_pipe_stdout[0]`;
  it allows the testing program to read the output of the student code
  after the sandbox, if it is necessary.
- :code:`FILE *fstdout` : a stream version of :code:`true_stdout`, similarly
  to :code:`stdout`.

From these file descriptors and streams, only the standard ones
(:code:`STDOUT_FILENO` and :code:`stdout`) and :code:`stdout_cpy` and
:code:`fstdout` are available to the testing program; all the others are
internal to CTester (but it is good to know about the plumbing).

A view at the internal workings of CTester
==========================================

A look at the way CTester works is useful to understand its limitations.

CTester works by setting up various things:

- It redirects the standard output and error streams to pipes, allowing these
  streams to be managed and parsed by CTester.
- It registers handlers for the SIGSEGV and SIGFPE signals, to prevent
  the program from crashing if such a signal is generated by student code,
  and exits the sandbox.
- It registers an alarm and its handler to exit if the student code runs for
  too long (currently 2 seconds).
- It wraps a lot of functions (the list is in the page :ref:`wrap`),
  by using a built-in compiler feature allowing wrapping of functions and
  definitions of wrappers, and builds the program with these wrappers.

Detailed step-by-step execution of CTester
------------------------------------------

When the ``run`` file is called by INGInious, the following happens:

#. CTester is copied from an appropriate location, as in
   :ref:`ctester_inginious_copy`;
#. The ``run.py`` file is run:

   #. The ``.c.tpl`` file template is parsed by INGInious to insert
      the student's answers and generate a ``.c`` file.
   #. The ``make`` command is issued, which compiles and links everything
      together. If the compilation fails, a proper feedback is sent to
      the student.
   #. The banned functions are extracted from the ``tests.c`` file, and the
      function names are searched inside the ``student_code.o`` binary;
      if a match is found, a feedback is sent to the student mentioning
      the bad use of a function.
   #. Source files and ``.o`` files are removed
   #. The translations are imported.
   #. The program, named ``./tests``, is run, using the ``run_student`` command
      with a time limit of 20 seconds:

      #. The :code:`run_tests()` function, defined in ``CTester.c``, is called
         by the :c:func:`RUN()` macro.
      #. Inside that function, the locale (language) is set and initialized,
         if it was provided in the arguments.
      #. Memory is set to be initialized with the :code:`~142` value.
      #. The *glibc* library's memory allocator is configured to print detected
         memory errors (double free etc) on standard error, and to prevent abort
         of the program if such an error is found.
      #. The :code:`true_stdout`, :code:`fstdout`, :code:`, :code:`pipe_stdout`,
         :code:`usr_pipe_stdout`, :code:`stdout_cpy`
         and their :code:`stderr` variants are created and configured as above;
         the pipes are empty, redirection of :code:`stdout` has not yet
         happened.
      #. *glibc* is configured to write its errors to standard error.
      #. A special stack for all signal handlers is allocated and configured.
      #. The signal handlers for the :code:`SIGSEGV`, :code:`SIGFPE` and
         :code:`SIGALRM` signals are configured. These handlers update the tags
         by adding the signal in it (:code:`"timeout"` for :code:`SIGALRM`),
         and jump to the :code:`sigsetjmp` of the start of the sandbox, making
         the test fail.
      #. The ``results.txt`` file is opened.
      #. CUnit is initialized and the default suite is created.
      #. For each test function:

         #. Dynamic loading informations is queried, for use with CUnit.
         #. The test function is added to CUnit as a test, using the above
            queried informations.
         #. :code:`start_test()` is called: this function resets the statistics
            and the various internal structures used by CTester.
         #. The test function is executed by CUnit's :code:`CU_basic_run_test`

            - Usually, the test function sets the test metadata, that are used
              after the test to provide a feedback in the ``results.txt`` file.
            - Usually, the test function will call one or more times
              the sandbox.
              The sandbox works as follows:

              - At :code:`SANDBOX_BEGIN;`, we do two things:

                #. We call :code:`sandbox_begin()`, which sets a timer of
                   2 seconds, redirects :code:`STDOUT_FILENO` to
                   :code:`pipe_stdout[1]` (same for :code:`stderr`), empties
                   the :code:`usr_pipe_stdout` pipes, and activates monitoring.
                #. We set a :manpage:`sigsetjmp(2)` buffer so that we can
                   get back to that point in case of failure. It is called
                   inside an :code:`if` statement.
                #. The code inside the sandbox is run; it is the first, default
                   branch of the :code:`if`.

              - At :code:`SANDBOX_END;`, we end the first branch of the
                :code:`if`; the second branch is executed if a signal was
                caught by the signal handlers, and it makes the CUnit tests
                fail. Then, we call :code:`sandbox_end()`:

                #. It disables the monitoring, restores :code:`STDOUT_FILENO`
                   to the normal behaviour.
                #. It empties the :code:`pipe_stdout` pipe, copying its content
                   to :code:`usr_pipe_stdout`.
                #. It empties the :code:`pipe_stderr` pipe, copying its content
                   to :code:`usr_pipe_stderr`, and checks for the string
                   ``"double free or corruption"`` inside it, setting a tag
                   if found.
                #. It resets the timer to zero, to prevent an accidental call
                   of it.

            - Usually, after the sandbox, the test function will perform
              some tests on the returned data (written in the sandbox),
              using CUnit.

         #. The results from executing that test function are retrieved from
            CUnit, and are written to ``results.txt``, as well as the tags set
            and the pushed information messages.
         #. Some cleanup is done for the next test function.

         Note that any error coming from CTester's setup, CUnit's setup,
         some other calls or writes to the ``results.txt`` file results in
         no feedback written to that file, and an error message written
         to (true) standard error.
      #. The exit code is CUnit's :code:`CU_get_error()`, unless another error
         occured during execution.

   #. If the ``run_student`` command, and thus the test program failed,
      a proper feedback is sent to the student detailing the cause of error.
      Note that this doesn't distinguish an error caused by the student
      from an error caused by the tests!
      Possible errors include floating-point exception, segmentation fault,
      memory limit exceeded, time limit exceeded, or any other error.
   #. The ``results.txt`` file is parsed to get the results of the tests:

      - The global feedback depends on whether all tests are successful;
      - Each test adds its status (success or failure) to the feedback of
        its corresponding problem ID, and the total points are updated according
        to the weight of the test.
      - A problem is successful if all the tests related to it are correct.
      - Tags are applied, if tags were specified in the test.

   #. The MCQ are graded and included in the total score.
   #. The total task is successful if the score is above 50

The case of :code:`exit`
------------------------

The :manpage:`exit(3)` function (which is actually not a system call;
this is :manpage:`exit(2)`) is wrapped by CTester, even though it doesn't
provide statistics nor programmed failure.

The reason is obvious: if the student could call :code:`exit`, he/she could
terminate the test program immediately, bypassing the tests.

As of now, if the student calls :code:`exit`, a :code:`SIGSEGV` signal will be
raised, causing an immediate abort of the function. More precise behaviour
has yet to be implemented.

.. about the pipes: writing too much will just cause the write to fail,
   and so the printf, which are likely to be the culprit, will just fail.
