.. _wrap_sockets:

=======================================
Socket-related functions and structures
=======================================

The following functions have a wrapper:

- :manpage:`accept(2)`
- :manpage:`bind(2)`
- :manpage:`connect(2)`
- :manpage:`listen(2)`
- :manpage:`poll(2)`
- :manpage:`recv(2)`
- :manpage:`recvfrom(2)`
- :manpage:`recvmsg(2)`
- :manpage:`select(2)`
- :manpage:`send(2)`
- :manpage:`sendto(2)`
- :manpage:`sendmsg(2)`
- :manpage:`shutdown(2)`
- :manpage:`socket(2)`

Notes on specific calls
=======================

The rest of this page indicated some particularities.

:code:`accept`
--------------

The returned address of :code:`accept` and its length is stored into
a :code:`struct return_accept_t` at fields :code:`addr` and :code:`addrlen`
respectively. This structure can be accessed from the stats
by :code:`stats.accept.last_returns`
(:code:`last_return` contains the return value of the function).

Note that :code:`addr` contains a meaningful value only if the call
to :code:`__real_accept` was successful, and if both the parameters
:code:`addr` and :code:`addrlen` are not :code:`NULL`.
:code:`addrlen` contains a meaningful value only if the :code:`addrlen` parameter
was not :code:`NULL`.

:code:`poll`
------------

The :code:`fds_ptr` field contains the value of the original parameter :code:`fds`,
that is, a pointer to an array of :code:`struct pollfd`.
The :code:`fds_copy` field contains a copy of this array, if the call succeeded
or if it failed for a different reason than a segfault.

.. note:: It is very well possible that the reason for the exit was
          for a different reason than a segfault,
          yet the :code:`fds` parameter is invalid: this is probably a bug.

:code:`recv` and related
------------------------

The three functions :code:`recv`, :code:`recvfrom` and :code:`recvmsg` have overlapping
functionalities, and may be called by the student.
Technically, it is not wrong for the student to call :code:`recvfrom` when we asked
him to use :code:`recv` and didn't ask him not to use :code:`recvfrom`.
So, in a regular, "all-accepting" policy, the three calls have to be monitored,
and their statistics should be aggregated.

In order to do that, CTester provides, in addition to the three regular
statistics structures, an additional :code:`struct stats_recv_all_t` structure
with an only field of :code:`called`, which counts the number of times
any of the three functions has been called: the sum of the respective :code:`called`.

In the ``CTester/wrap.h`` file, a macro :code:`MONITOR_ALL_RECV` is defined. It can
be used to monitor all 3 functions at once. For this, call it like this::

    MONITOR_ALL_RECV(monitored, true);

:c:data:`monitored` is the name of the default :code:`struct wrap_monitor_t`.

:code:`recvfrom`
----------------

The :code:`recvfrom` function returns in its parameter :code:`src_addr` the source
address, and in :code:`src_addrlen` the length of this address.
If the call was successful, the actual address and the actual length are stored
in fields :code:`src_addr` and :code:`src_addrlen` of a :code:`struct return_recvfrom_t`,
which is accessible at :code:`stats.recvfrom.last_returned_addr`.

:code:`recvmsg`
---------------

The :code:`recvmsg` function returns in its parameter :code:`msg` a :code:`struct msghdr`,
which contains a lot of information about the received message.
If the call was successful, the actual received message is copied and stored
in :code:`stats.recvmsg.last_returned_msg`.

Note that this :code:`msghdr` structure contains some dynamically allocated fields,
like :code:`iov`; these inner fields are not copied and may thus have been freed
when you consult the statistics.

:code:`select`
--------------

The four parameters :code:`readfds`, :code:`writefds`, :code:`exceptfds` and :code:`timeout`
are stored in corresponding fields with an :code:`_ptr` attached to it.
In addition, there are four fields :code:`readfds`, :code:`writefds`, :code:`exceptfds`
and :code:`timeout` which contain copies of the corresponding structures.
In sharp contrast with most other functions, the copies are done
even if the call was not successful.

.. note:: This can probably lead to segfaults too.

:code:`send` and related
------------------------

In a similar manner to :code:`recv` and its relatives, there is an additional
:code:`struct stats_send_all_t` accessible at :code:`stats.send_all`,
with a :code:`called` field that tracks the number of times any of the three
functions has been called.

In the ``CTester/wrap.h`` file, a macro :code:`MONITOR_ALL_SEND` is defined. It can
be used to monitor all 3 functions at once. For this, call it like this: ::

    MONITOR_ALL_SEND(monitored, true);

:c:data:`monitored` is the name of the default :code:`struct wrap_monitor_t`.

:code:`sendto` and :code:`sendmsg`
----------------------------------

In sharp contrast with :code:`recvfrom` and :code:`recvmsg`, there is currently
no field to store the actual address or message passed as argument.

Resetting
---------

.. c:function:: void reinit_network_socket_stats()

This function resets the statistics of all the tracked functions listed on this page.
