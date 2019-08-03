.. _wrap_misc:

==============================================
Miscellaneous wrapped functions and structures
==============================================

Defined in ``unistd.h``
=======================

:code:`sleep`
-------------

``CTester/wrap_sleep.h`` and ``CTester/wrap_sleep.c`` are the archetypal
wrapper definitions files.
They wrap a very simple function, the :manpage:`sleep(3)` function.

The sole structure declared is the :code:`struct stats_sleep_t` structure,
which has the following fields:

- :code:`called`: number of times the :code:`sleep` function has been called;
- :code:`last_return`: return value of the last call to :code:`sleep`;
- :code:`last_arg`: argument value of the last call to :code:`sleep`; this is **not**
  the usual name for this field, which is usually :code:`last_params`.

As there is only one argument, this function doesn't use a structure
to record its parameters.

There are three functions defined in the header:

.. c:function:: void init_sleep()

   Used to initialize the structures needed for the wrapper;
   in the case of :code:`sleep`, it does nothing.

.. c:function:: void clean_sleep()

   Used to cleanup the structures needed for the wrapper;
   in the case of :code:`sleep`, it does nothing either.

.. c:function:: void resetstats_sleep()

   Used to reset the statistics for the number of calls,
   the last arguments and return values, as initially.

The code in the ``CTester/wrap_sleep.c`` file should give an idea of
the general look of a wrapper for a function.

:code:`getpid`
--------------

The :manpage:`getpid(2)` system call also has its wrapper, and its associated
statistics structure (:code:`struct stats_getpid_t`) and initialization,
cleanup and statistics reset functions, as with :code:`sleep`.
A significant difference with :code:`sleep` is that it doesn't have any argument,
so there is no :code:`last_arg` field.

Defined in ``stdlib.h``
=======================

:code:`exit`
------------

The :manpage:`exit(3)` function is wrapped by CTester, but doesn't provide
any statistics nor programmed failures.
It is wrapped because one should prevent the student from calling :code:`exit`
and thereby stopping the tests.

