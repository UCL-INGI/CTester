.. _wrap_util_inet:

==============================================
Other network-related functions and structures
==============================================

.. _wrap_inet_functions:

File ``wrap_network_inet.h``
============================

The ``CTester/wrap_network_inet.h`` header contains definitions of structures
and wrapper for the following functions:

- :manpage:`htons(3)`
- :manpage:`ntohs(3)`
- :manpage:`htonl(3)`
- :manpage:`ntohl(3)`

Each of the statistics structure contains the last parameter at :code:`last_params`,
and the last return value at :code:`last_return`, as usual.
The parameter structures only contain one field, for the sole parameter
of these functions.

In addition, there is the following function:

.. c:function:: void reinit_network_inet_stats()

   Reset the statistics for each of these functions.

These byte-order functions cannot fail, and as such are not present
in the :code:`struct wrap_fail_t` structure.

In the ``CTester/wrap.h`` file, the macro :code:`MONITOR_ALL_BYTEORDER` is defined.
It can be used to monitor all 4 functions at once, and is called like this::

    MONITOR_ALL_BYTEORDER(monitored, true);

:c:data:`monitored` is the name of the default :code:`struct wrap_monitor_t`.

.. _util_inet_functions:

File ``util_inet.h``
====================

The ``CTester/util_inet.h`` header contains definitions of some utility
functions:

- :code:`htons_tab`
- :code:`ntohs_tab`
- :code:`htonl_tab`
- :code:`ntohl_tab`

All these functions translate each integer in an array from one byte order
format to another, from a source array to a destination array.
For instance, :code:`htons_tab` translates each short, 16-bit unsigned integer
of its source array, assumed to be in host byte order, to the network,
little-endian byte order, and store it in corresponding areas
of the destination array.

