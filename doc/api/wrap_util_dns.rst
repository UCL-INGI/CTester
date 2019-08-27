.. _wrap_util_dns:

.. |gai| replace:: :code:`getaddrinfo`
.. |gni| replace:: :code:`getnameinfo`
.. |fai| replace:: :code:`freeaddrinfo`

====================================
DNS-related functions and structures
====================================

The DNS-related functions, structures, and definitions are located in the
``CTester/wrap_network_dns.h`` and ``CTester/wrap_network_dns.c`` files.

Wrapped functions
=================

The wrapped functions are

- :manpage:`getaddrinfo(3)`
- :manpage:`freeaddrinfo(3)`
- :manpage:`gai_strerror(3)`
- :manpage:`getnameinfo(3)`

There are some specificities for the statistics structures for these functions,
that are described next.

For |gai|
---------

In addition to the common fields, the :code:`struct stats_getaddrinfo` structure
has an additional field, :code:`addrinfo_list`, which is a list of nodes of type
:code:`struct addrinfo_node_t`, which record the various lists returned by |gai|.

For |fai|
---------

As |fai| doesn't return anything, there is no :code:`last_return` field.
However, there is a :code:`status` field which, when |fai| is checked
(see :ref:`checked_freeaddrinfo`), is set to 1 if the last call didn't succeed
(which happens if the parameter has never been returned by |gai|).

This call cannot be set to fail, unless it is checked, and even then, a failure
would not be visible to the student, hence the reporter function
(see :ref:`freeaddrinfo_reporter`).

For :code:`gai_strerror`
------------------------

There is no structure for the parameters: the sole parameter, an :code:`int`, is
accessible by the :code:`last_params` field (note that the plural is used).

This call cannot be set to fail.

.. _checked_freeaddrinfo:

Checked freeaddrinfo
====================

The standard way of using :code:`getaddrinfo` usually goes along these lines::

    struct addrinfo hints, *result, *rp;
    // set the hints fields
    const char *host, *serv;
    // set the host name and service name
    int sfd = 0; // socket
    int s = getaddrinfo(host, serv, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // Try the address
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) // print the error and continue with the next struct addrinfo
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; // Success
        else
            close(sfd);
    }
    if (rp == NULL) // print error as we didn't get a valid connection
    freeaddrinfo(result);

The parameter passed to the :code:`freeaddrinfo` function is usually the pointer
to the head of the list returned by the previous call to getaddrinfo.
However, it is also possible to pass any other pointer to one of the item
of the list: it will free the list of addrinfo structures starting from
the item at this pointer to the end, but will not free the previous items.

If you want to make sure that the student calls |fai| with a list that has been
previously returned by a call to |gai|, you can use the following function:

.. c:function:: void set_check_freeaddrinfo(bool check)

   When passed :code:`true`, this function enables the recording of every list returned
   by |gai|, and when |fai| is called, its argument is checked against
   the list of recorded lists returned by |gai|
   (each list stored inside a :code:`struct addrinfo_node_t` node).

.. _freeaddrinfo_reporter:

Report the errors
-----------------

.. c:type:: typedef void (*freeaddrinfo_badarg_report_t)()

.. c:function:: void set_freeaddrinfo_badarg_reporter(freeaddrinfo_badarg_report_t reporter)

If the check is enabled, when |fai| is called with a list that has
never been returned by |gai|, it will fail with status 1
(in the statistics structure; the method still returns :code:`void`).
If a non-:code:`NULL` reporter function has been specified using
:c:func:`set_freeaddrinfo_badarg_reporter()`, this function will be called.
This is useful if you plan to provide a detailed feedback.
Please note that |fai| **will still attempt to free its argument**,
which may lead to a premature exit from the sandbox.

Custom |gai| and |gni| wrappers
===============================

The default wrapper for |gai| only provides limited functionnality:
it can only call the real |gai| after counting statistics,
or it can fail with some teacher-specified value.
When the real |gai| is called, it will generally attempt to call
the DNS service of the container, which may or may not be accessible.
In addition, the data returned by |gai| is subject to the *glibc*
implementation, and may cause problems with the order of the results
(it is specified in :rfc:`3484` though).

CTester allows the teacher to provide alternative implementations of |gai|,
by using :c:func:`set_getaddrinfo_method` and :c:func:`set_gai_methods`.
It also provides :c:func:`set_getnameinfo_method` to provide an alternate |gni| method.

.. c:type:: typedef int (*getaddrinfo_method_t)(const char *node, const char *service, const struct addrinfo *hints, truct addrinfo **res)

.. c:type:: typedef void (*freeaddrinfo_method_t)(struct addrinfo *res)

.. c:type:: typedef int (*getnameinfo_method_t)(const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags)

.. c:function:: void set_getaddrinfo_method(getaddrinfo_method_t method)

   Sets the alternate |gai| method; :code:`NULL` resets that method to
   the default.

.. c:function:: void set_gai_methods(getaddrinfo_method_t gai_method, freeaddrinfo_method_t fai_method)

   Sets the alternate |gai| and |fai| methods; passing :code:`NULL` for one of
   the parameters resets that function to the default.

.. c:function:: void set_getnameinfo_method(getnameinfo_method_t method)

   Sets the alternate |gni| method; passing :code:`NULL` resets to default.

The first two methods require passing a function with the same signature
as a standard |gai|.
The third method requires passing a function with the same signature
as a standard |gni|.

The difference between :c:func:`set_getaddrinfo_method` and :c:func:`set_gai_methods` is
that the second method also allows specifying the |fai| replacement function,
which must logically have the same signature as the standard |fai| function.

The reason you may want to provide both functions is that the POSIX standard
doesn't specify the actual structure of the list of :code:`struct addrinfo`.

For instance, the *glibc* version constructs this list by allocating a series
of blocks of size :code:`sizeof(struct addrinfo) + sizeof(struct sockaddr_in)`
(if the address is IPv4; replace :code:`sockaddr_in` by :code:`sockaddr_in6` if IPv6),
and then making the :code:`ai_addr` field of the :code:`struct addrinfo` point to
the end of the block, to the attached :code:`struct sockaddr_in`.

Another library may decide to allocate the blocks separately, or may decide
to allocate all the :code:`struct addrinfo` together, or anything else.
This is authorized because the standard only requires |gai| to fill in a list
that can be used to create and bind sockets, and that can be freed by
the corresponding |fai|.

Unless you can garantee that your wrapper function will return the list in
the same format that is expected by |fai|, you may want the possibility
to also provide a corresponding |fai| when you change the wrapped |gai|.

For |gni|, there is only one function, as there is no :code:`freenameinfo`
function in the standard library: |gni| only fills in user-provided buffers,
and only returns one result.

.. c:type:: typedef const char *(*gai_strerror_method_t)(int)

.. c:function:: void set_gai_strerror_method_t(gai_strerror_method_t method)

Finally, it is also possible to replace the :code:`gai_strerror` function
with a custom one, by calling :c:func:`set_gai_strerror_method` with an appropriate
parameter: this is useful if you plan to deliberately cause multiple errors
in |gai| and want to have an easier parsing of stderr experience afterwards.

Note that you can come back to the default, standard-library-backed
functions by calling the above functions with a :code:`NULL` argument.
For the :c:func:`set_gai_methods` functions, specifying only one of the arguments
as NULL will only reset the corresponding function to the default.

Simple replacements for |gai| and |fai|
---------------------------------------

.. c:function:: int simple_getaddrinfo(const char *host, const char *serv, const struct addrinfo *hints, struct addrinfo **res)

.. c:function:: void simple_freeaddrinfo(struct addrinfo *res)

Examples of custom-defined |gai| and |fai| wrappers are available in CTester
as :c:func:`simple_getaddrinfo` and :c:func:`simple_freeaddrinfo`.
Both functions get around the DNS by only accepting numerical arguments
(it assumes :code:`AI_NUMERICHOST` in the flags).

Resetting to defaults
---------------------

.. c:function:: void reset_gai_fai_gstr_gni_methods()

   Resets the 4 methods to their default.

