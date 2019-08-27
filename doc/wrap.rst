.. _wrap:

=====================
Wrapping and wrappers
=====================

The main facilities provided by CTester are wrappers around library functions
and system calls, allowing CTester to perform various operations: statistics,
programmed failures, method overriding, and more.

For this, CTester provides four main "big" structures, defined in ``CTester/wrap.h``:

.. c:type:: struct wrap_monitor_t

   Defines one boolean field per supported function or
   system call (see the table below), with the same name as the function.
   Assigning the field to :code:`true` sets CTester to monitor the function: it will
   gather call statistics, will handle programmed failures, and will allow some
   more advanced and function-specific features (but not specifically enabling
   them).

.. c:type:: struct wrap_fail_t

   Defines three fields per supported function or system
   call (see the table below): an :code:`uint32_t` with the same name as
   the function (say :manpage:`open(2)`), an :code:`int` with :code:`_ret` appended to the name
   of the function (like :code:`open_ret`), and an :code:`int` with :code:`_errno` appended
   to the name (like :code:`open_errno`). Some functions may not have the three
   fields, or may not be present at all. See section :ref:`wrap_failures`.

.. c:type:: struct wrap_stats_t

   Defines one field per supported function or system
   call (see the table below), with the same name as the function. The type of
   the field is generally :code:`struct stats_<function>_t`, with :code:`<function>`
   being the name of the function (e.g. :code:`struct stats_open_t`). This structure
   defines several fields that are described below.

.. c:type:: struct wrap_log_t

   Consists of logging structure that may be defined and
   added by some functions for their internal working.

Each of these 4 structures is then instantiated in the ``CTester/CTester.h``
file to form four global variables:

.. c:var:: struct wrap_monitor_t monitored

   This variable is used to activate the monitoring
   of each supported function: for instance, depending on the (boolean) value of
   :code:`monitored.open`, the :code:`open` system call is monitored or not.

.. c:var:: struct wrap_fail_t failures

   This variable is used to cause programmed failures,
   as described below.

.. c:var:: struct wrap_stats_t stats

   Records statistics for all monitored
   functions and system calls: their last parameters, last return values, number
   of calls, and sometimes more.

.. c:var:: struct wrap_log_t logs

   Contains some logs defined by some functions.

The ``CTester/wrap.h`` header should import all headers (the ``.h``)used to
define wrapping, statistics and failures structures, and should be imported in
all implementations of the wrappers and associated functions (the ``.c``)

Typical workflow
================

A typical workflow for testing the appropriate usage of a function starts by
monitoring the wished function (here, :code:`open`)::

    monitored.open = true;

For the example, we want the student to call that function only one time,
with the filename :code:`"file.txt"`, the flags :code:`O_RDWR | O_CREAT | O_TRUNC`
and the :code:`S_IRUSR | S_IWUSR` permissions mode.

Then, we may want the function to have a successful first call, but fail for
all subsequent calls, for instance, if the student tries to open more than
one file::

    failures.open = FAIL_ALWAYS & ~(FAIL_FIRST);
    failures.open_ret = -1;
    failures.open_errno = EACCESS;

Then, we call the student code within the sandbox::

    int ret = -1;
    SANDBOX_BEGIN;
    ret = my_student_function(params);
    SANDBOX_END;

Only functions that are called within the sandbox will be tracked and monitored!

Then, we can query the statistics of the call, given by the :code:`stats.open`
field:

- If :code:`stats.open.called` is 1, the student called the function. Then, we can
  recover the last arguments through the :code:`stats.open.last_params` field,
  and check that they correspond to the expected parameters.
- If :code:`stats.open.called` is greater than one, we know that the student called
  multiple times the :code:`open` function, which was not expected.
- If :code:`stats.open.called` is still zero, the student didn't call the function.

It is recommanded that you only run a minimal set of tests inside one test
function. Monitorings, statistics, failures and other internal structures are
reset to their default values at the start of the test function; if you need
to reset them again, you need to look up for all the required functions in
the appropriate documentations.

.. _wrap_failures:

Managing failures
=================

CTester allows programmed failures through the :c:data:`failures` global variable:
we can ask CTester to make some function fail, with specified return values
and :code:`errno` values.

To cause a programmed failure, one has to choose at which time a function
should fail: this is the role of the first field, :code:`<function_name>` (the same
name as the function name). This is an :code:`uint32_t` where the n-th bit says
whether the n-th call to the function should fail or not. Several constants
are already available to choose from, but you can also combine them or define
new ones:

- :code:`FAIL_NEVER`, the default and initial value, causes the function to never
  fail, whatever the number of times called;
- :code:`FAIL_FIRST` causes the first call to fail; subsequent calls will succeed;
- :code:`FAIL_SECOND` causes the second call to fail; the first one as well as
  subsequent ones will succeed;
- :code:`FAIL_THIRD` causes the third call to fail; the two first ones as well as
  the 4th and subsequent calls will succeed;
- :code:`FAIL_TWICE` causes the first two calls to fail; subsequent calls will
  succeed;
- :code:`FAIL_ALWAYS` causes all calls to fail.

Failures can only be decided based on the number of times it has been called
before, not based on the arguments that are passed. Some function calls add
extra functionalities to have more custom failures: example of :code:`getaddrinfo`.

It is possible to combine these values; for instance, if you want all calls
to fail except the first two ones and except the 4th one, you can get an
appropriate value by doing::

    FAIL_ALWAYS & ~(FAIL_TWICE | 0b1000);

Note that only :code:`FAIL_ALWAYS` will cause all calls to fail: the above value
will only cause calls up to the 32nd to fail, and the 33rd will succeed.
This is because the field is only 32-bits wide.

.. note:: An improvement would be to check that the 32nd bit is set and decide
   that it means that the 33rd and up calls should fail: feel free to add
   support for this!

Once failures have been decided, one can set the return value and the :code:`errno`
value by setting respectively the fields :code:`<function_name>_ret` and
:code:`<function_name>_errno` of the :c:data:`failures` global variable. The values will
be the same for each call failures. In general, the :code:`<function_name>_ret`
field is an :code:`int`, with the notable exception of :manpage:`malloc(3)` and related.

Statistics
==========

Each of the statistics structure for a function follow approximately the same
structure. For instance, the structures for the following fictitious function::

    int my_func(int mode, const void *buf, size_t count, const struct sockaddr *dest_addr, struct sockaddr *source_addr);

would be::

    struct params_my_func_t {
        int mode;
        const void *buf;
        size_t count;
        const struct sockaddr *dest_addr;
        struct sockaddr *source_addr;
    };
    struct stats_my_func_t {
        int called;
        struct params_my_func_t last_params;
        int last_return;
    };

The first structure, :code:`params_my_func_t`, contains the value of all the
parameters of the function.

The second structure contains three fields:

- :code:`called` which counts how many times the function has been called;
- :code:`last_params` which contains a copy of the values of the parameters
  of the last call to the function;
- :code:`last_return` which contains the return value of the last call to the
  function.

Note that only function calls that were done within the sandbox are counted;
outside of the sandbox, CTester doesn't count the calls nor save the parameters
or return values.

Most of the functions and system calls follow this structure, and future
wrapped functions are expected to respect this format.

There are a few exceptions to this pattern:

- Functions that have only one parameter may bypass the :code:`struct params_t`
  completely, and the sole parameter may be directly stored in :code:`last_params`;
  the documentation should precise this choice.
- In case of only one parameter, you should still call the field :code:`last_params`;
  any exception of this rule in the current version of CTester is unfortunate
  but cannot be changed for backward compatibility reasons. [#sleep]_
- Functions that take no parameter should not have that field, as it makes
  no sense for them, and it is discouraged to have empty structures.
- Functions that return nothing (i.e., :code:`void`) similarly should not have the
  :code:`last_return` field.

.. [#sleep] :manpage:`sleep(3)` choosed the field name :code:`last_arg`.

Additional statistics
---------------------

In addition to the above fields and structures, it is possible for a function
to define additional structures and fields.

The first common additional statistics happens when a function takes
a parameter that is a pointer to a structure. For instance, in the above
function, :code:`dest_addr` is a pointer to a constant structure, that is used by
the call for some purpose: the pointer itself only has a meaning if the
structure still exists at the time the statistics are accessed; maybe,
in the time between the call to the function and the moment when we read
the field, the structure has been deallocated and it no longer available.

In that kind of case, one may add an additional field in the :code:`params_t`
structure containing a copy of this structure, like this::

    struct params_my_func_t {
        int mode;
        const void *buf;
        size_t count;
        const struct sockaddr *dest_addr;
        struct dest_addr_val;
        struct sockaddr *source_addr;
    };

The proper way of doing this is by using a field name derived from
the parameter name with the :code:`_val` suffix appended to it [#badptr]_.

There is a notable exception to this pattern: a parameter that points to a data
buffer, or to an array of non-predefined size, should not be copied or saved.
This is the case of the :code:`buf` parameter, which points to a buffer of data
that is modified by the function: we don't know the size
of the buffer of data, and can't allocate it in the structure other than by
allocating a new buffer and copying. If the parameter was pointing to an array
which size doesn't depend on the call (it is pre-defined, for instance
3 items), it is authorized to maintain a copy.

For instance, :manpage:`poll(2)`'s first parameter is an array of structures, whose size
is specified by a parameter of the call, and so should not be copied.
The third parameter of the :manpage:`utimensat(2)` system call, however, is a pointer
to an array of two :code:`struct timespec`, and could be saved.

A second kind of additional statistics happens when a function takes a pointer
to a non-const structure, and it is expected that the function fills in
the value of the structure during the call (this is the case of the :code:`addr`
parameter of the :manpage:`accept(2)` system call). For instance, suppose :code:`source_addr`
will be filled by the :code:`my_func` function during the call. Then, this field
is a kind of "return value", and we may want to save it. The proper way of
doing is by declaring a new structure::

    struct return_my_func_t {
        struct sockaddr source_addr;
    };

and adding it to the :code:`stats_t` structure, with a field name of :code:`last_returns`::

    struct stats_my_func_t {
        int called;
        struct params_my_func_t last_params;
        int last_return;
        struct return_my_func_t last_returns;
    };

``accept`` and ``recvfrom`` are perfect examples of this pattern. ``stat`` also
follows this pattern, bu uses a different field name (bad idea again).

If there is only one field in the :code:`return_t` structure, it is possible
to bypass the structure completely and directly put the return value in
the :code:`stats_t` structure.

.. [#badptr] Some functions to the opposite: the pointer is named :code:`dest_addr_ptr`
   and the value is named :code:`dest_addr`. This is to be avoided, but again,
   we can't correct this for backward compatibility reasons.

Wrapper structure
=================

Each wrapper for a function has mostly the same structure, save for the few
missing features or additional features which are described in the appropriate
pages of the documentation. The structure for the same fictitious function::

    int my_func(int mode, const void *buf, size_t count, struct sockaddr *source_addr);

would be like this::

    // Declared in .c file

    // No need to declare it, the compiler does it
    int __real_my_func(int mode, const void *buf, size_t count, const struct sockaddr *dest_addr, struct sockaddr *source_addr);

    int __wrap_my_func(int mode, const void *buf, size_t count, const struct sockaddr *dest_addr, struct sockaddr *source_addr)
    {
        if (!(wrap_monitoring && monitored.my_func)) {
            return __real_my_func(mode, buf, count, dest_addr, source_addr);
        }
        stats.my_func.called++;
        stats.my_func.last_params = (struct params_my_func_t) {
            .mode = mode,
            .buf = buf,
            .count = count,
            .dest_addr = dest_addr,
            .source_addr = source_addr
        };
        if (FAIL(failures.my_func)) {
            failures.my_func = NEXT(failures.my_func);
            errno = failures.my_func_errno;
            return stats.my_func.last_return = failures.my_func_ret;
        }
        failures.my_func = NEXT(failures.my_func);
        int ret = __real_my_func(mode, buf, count, dest_addr, source_addr);
        /* Copy values of pointed arguments if you want to here */
        return (stats.last_return = ret);
    }

The :code:`FAIL` and :code:`NEXT` functions are defined in ``CTester/wrap.h``.

If the two above-mentioned additional statistics are implemented, they should
go where indicated: this way, a potential segmentation fault when dereferencing
would be caused within :code:`__real_my_func`, and not in the wrapper, and may thus
be caught by some internal mechanism (system calls usually detect segfaults
without crashing the whole application).

Logging
=======

Currently, the :c:type:`wrap_log_t` structure only contains one field, :code:`malloc`,
that contains a :c:type:`malloc_t` structure that is used by :manpage:`malloc(3)` and related
to track the memory areas that are allocated and freed. Consult the
:ref:`wrap_mallocs` section for more informations.

List of wrapped functions and system calls
==========================================

The following table lists the various functions and system calls that currently
have a wrapper and are supported by CTester, as well as their level of support
of some features:

- Monitoring: whether they are available in the :code:`wrap_monitor_t` structure;
  right now there are no exception.
- Call and usage statistics.
- Programmed failures.
- Whether they save a copy of the returned results when those results are
  filled in pointers passed as parameters:
  "No return" indicates that these calls don't return nothing except the return
  value; "None" indicates that these calls don't copy any return value as
  parameter, but there are some parameters that could. Data buffers fileld by
  the call are not considered return values nor results.
- If they support some additional features, and which.

A link to the appropriate API page is also provided.

Not all functions are currently wrapped. The most notable and most embarrassing
example is that of the signal management functions.

.. |fctname| replace:: Function name
.. |mon| replace:: Monitoring
.. |stt| replace:: Statistics
.. |fls| replace:: Failures
.. |cpy| replace:: Copy of returned pointers
.. |adf| replace:: Additional features
.. |prw| replace:: :ref:`partial_read_write`
.. |chckfai| replace:: Checked freeaddrinfo

+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
|       |fctname|       | |mon| | |stt| |  |fls|  |    |cpy|     |    |adf|     |     Documentation      |
+=======================+=======+=======+=========+==============+==============+========================+
| accept                | Yes   | Yes   | Yes     | Yes (addr    | None         | :ref:`wrap_sockets`    |
|                       |       |       |         | and addrlen) |              |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| bind                  | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| calloc                | Yes   | Yes   | Yes (no | No return    | Tracks       | :ref:`wrap_mallocs`    |
|                       |       |       | errno   |              | memory usage |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| close                 | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_file`       |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| connect               | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| creat                 | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_file`       |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| free                  | Yes   | Yes   | Yes     | No return    | Tracks       | :ref:`wrap_mallocs`    |
|                       |       |       | [#fre]_ |              | memory usage |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| fstat                 | Yes   | Yes   | Yes     | Yes (struct  | None         | :ref:`wrap_file`       |
|                       |       |       |         | stat buf)    |              |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| freeaddrinfo          | Yes   | Yes   | No      | No return    | - |chckfai|  | :ref:`wrap_util_dns`   |
|                       |       |       | [#fai]_ |              |              |                        |
+-----------------------+-------+-------+---------+--------------+ - custom     +------------------------+
| getaddrinfo           | Yes   | Yes   | Yes     | See doc      |   wrappers   | :ref:`wrap_util_dns`   |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| gai_strerror          | Yes   | Yes   | Yes     | No return    | custom       | :ref:`wrap_util_dns`   |
|                       |       |       |         |              | wrappers     |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| getnameinfo           | Yes   | Yes   | Yes     | No return    | - custom     | :ref:`wrap_util_dns`   |
|                       |       |       |         |              |   wrappers   |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| getpid                | Yes   | Yes   | No      | No return    | None         | :ref:`wrap_misc`       |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| htonl                 | Yes   | Yes   | No      | No return    | None         | :ref:`wrap_util_inet`  |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| htons                 | Yes   | Yes   | No      | No return    | None         | :ref:`wrap_util_inet`  |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| listen                | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| lseek                 | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_file`       |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| malloc                | Yes   | Yes   | Yes     | No return    | Tracks       | :ref:`wrap_mallocs`    |
|                       |       |       |         |              | memory usage |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| ntohl                 | Yes   | Yes   | No      | No return    | None         | :ref:`wrap_util_inet`  |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| ntohs                 | Yes   | Yes   | No      | No return    | None         | :ref:`wrap_util_inet`  |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| open                  | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_file`       |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| poll                  | Yes   | Yes   | Yes     | None         | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| pthread_mutex_destroy | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_mutexs`     |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| pthread_mutex_init    | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_mutexs`     |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| pthread_mutex_lock    | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_mutexs`     |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| pthread_mutex_trylock | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_mutexs`     |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| pthread_mutex_unlock  | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_mutexs`     |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| read                  | Yes   | Yes   | Yes     | No return    | Supports     | :ref:`wrap_file`       |
|                       |       |       |         |              | partial      |                        |
|                       |       |       |         |              | returns (see |                        |
|                       |       |       |         |              | |prw|        |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| realloc               | Yes   | Yes   | Yes (no | No return    | Tracks       | :ref:`wrap_mallocs`    |
|                       |       |       | errno)  |              | memory usage |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| recv                  | Yes   | Yes   | Yes     | No return    | Support      | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+ partial      |                        |
| recvfrom              | Yes   | Yes   | Yes     | Yes (addr    | returns (see |                        |
|                       |       |       |         | and addrlen) | |prw|)       |                        |
+-----------------------+-------+-------+---------+--------------+--------------+                        |
| recvmsg               | Yes   | Yes   | Yes     | Yes (msg)    | None         |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| select                | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| send                  | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| sendto                | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| sendmsg               | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| shutdown              | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| sleep                 | Yes   | Yes   | Yes (no | No return    | None         | :ref:`wrap_misc`       |
|                       |       |       | errno)  |              |              |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| socket                | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_sockets`    |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| stat                  | Yes   | Yes   | Yes     | Yes (struct  | None         | :ref:`wrap_file`       |
|                       |       |       |         | stat buf)    |              |                        |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+
| write                 | Yes   | Yes   | Yes     | No return    | None         | :ref:`wrap_file`       |
+-----------------------+-------+-------+---------+--------------+--------------+------------------------+

.. [#fre] Although it can fail, we cannot enforce a return value nor a value
   for :code:`errno`, so it is not really useful.

.. [#fai] :manpage:`freeaddrinfo(3)` cannot fail in a detectable way for the student,
   but there is a :code:`status` field that indicates a rough failure status.

