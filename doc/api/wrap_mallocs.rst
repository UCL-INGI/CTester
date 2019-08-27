.. _wrap_mallocs:

=======================================
Malloc-related functions and structures
=======================================

This page describes the features provided by the file ``CTester/wrap_malloc.h``
and its accompanying ``CTester/wrap_malloc.c``.

The following functions are wrapped and have their statistics recorded:

- :manpage:`malloc(3)`
- :manpage:`calloc(3)`
- :manpage:`free(3)`
- :manpage:`realloc(3)`

The only notable thing to mention is that, as :code:`free` doesn't return anything,
the corresponding :code:`struct stats_free_t` doesn't have a :code:`last_return` field.

A current misfeature is that :code:`errno` is never set by the wrappers of any of
the 4 functions in case of failure, and the fields are not present in the
:c:data:`failures` variable (of type :code:`struct wrap_fail_t`).

Also, :code:`malloc`, :code:`calloc` and :code:`realloc` have a field :code:`_ret` in the
:c:data:`failures` structure of type :code:`void*` instead of :code:`int`. Beware!

There is a function called :code:`malloc_log` in the header file.
Don't use it, it has no implementation.
Same for the commented :code:`malloc_log_init`.

There is no dedicated function to reset the statistics for these 4 functions.

Memory usage API
================

In order to monitor the memory consumption, several functions and structures
are provided:

- The :code:`struct stats_memory_t` structure has a field :code:`used` that indicates
  how many bytes are allocated thus far in the sandbox.
  This field is updated by each call to one of the 4 functions.
- The :code:`malloc_allocated` function, when called, returns the number of bytes
  that are allocated thus far in the sandbox.
  This value is computed by summing up the sizes of all chunks
  when the function is called.

The two functions provide a different implementation of the same functionality;
they should however always return the same result.

.. note:: Unit tests are missing for this: contributions are welcome!

Note that both meters are updated by :code:`malloc`, even if the call to :code:`malloc`
fails due to a true error (not a simulated one).
In sharp contrast, the values are updated by :code:`realloc` only if the call
succeeded.
This is a current bug that is part of the current API.
Also note that the meters are decremented for a call to :code:`free`, even if the
true call to :code:`free` fails: this is actually normal behaviour.

Also, due to :code:`struct stats_memory_t` using an :code:`int` for :code:`used`, and
:code:`struct malloc_elem_t` using a :code:`size_t` for :code:`size`, some overflows
are possible. Recall that :code:`size_t` is unsigned...

There is no dedicated function to reset the memory usage statistics.

The details of recorded chunks are recorded in the :c:data:`logs` variable,
of type :c:type:`wrap_log_t`, which contains a field, :code:`malloc`, of type
:c:type:`malloc_t`.

.. c:type:: struct malloc_t

   Contains useful statistics for the :code:`malloc` and related functions.

    .. c:member:: int n

       Indicates the number of recorded chunks.

    .. c:member:: struct malloc_elem_t *log

       Array recording each allocated chunks of memory.*

.. c:type:: struct malloc_elem_t

   Defines a chunk of allocated memory.

    .. c:member:: size_t size

       Size of the chunk

    .. c:member:: void *ptr

       Returned pointer*

Checked :code:`free` API
========================

Currently, it is not checked if the addresses passed to :code:`free`
have been previously returned by :code:`malloc` or its derivatives.
This check is left to the underlying library, which may or not
cause a detectable segfault.
This remark also applies to :code:`realloc`, which ususally frees up some memory.

Contributions are welcome!

