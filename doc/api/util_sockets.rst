.. _util_sockets:

================================
Socket-related utility functions
================================

The file ``CTester/util_sockets.h`` contains various functions that may be
useful when testing code related to the socket functions
(see :ref:`wrap_sockets`).

.. _util_sockets_create:

Socket creations
================

Often, one has to call :manpage:`getaddrinfo(3)` to retrieve the proper addresses
of some host and services, then call :manpage:`socket(2)`, then call :manpage:`connect(2)`
or :manpage:`bind(2)` and :manpage:`listen(3)`. The following functions simplify this procedure.

:code:`create_socket`
---------------------

.. c:function:: int create_socket(const char *host, const char *serv, int domain, int type, int flags, int do_bind)

   This function creates and returns a socket, "connected" to the :code:`host`
   and :code:`service` (which should both be numeric; :manpage:`getaddrinfo(3)` will be called
   without DNS, with :code:`AI_NUMERICHOST | AI_NUMERICSERV | flags` as its flags),
   with the specified :code:`domain` (:code:`AF_INET` or :code:`AF_INET6` or :code:`AF_UNSPEC`)
   and the specified :code:`type` (:code:`SOCK_STREAM` or :code:`SOCK_DGRAM`).
   If :code:`do_bind` is :code:`true`, then the resulting socket will be :manpage:`bind(2)` to
   the address, accepting connections, and configured to reuse the addresses
   (necessary as :manpage:`close(2)`-ing a socket doesn't free the address immediately);
   otherwise, :manpage:`connect(2)` will be called. Note that it is possible for the caller
   to pass a non-:code:`NULL` :code:`host` and leaving :code:`AI_PASSIVE` out of the :code:`flags`
   and still call :manpage:`bind(2)`, as it is possible for him to do the inverse and call
   :manpage:`connect(2)` : the incoherence of this is left to the caller.

.. :c:function:: int create_tcp_server_socket(const char *serv, int domain, int type)


   This function creates a server socket with
   :code:`create_socket(NULL, serv, domain, type, AI_PASSIVE, true)`
   and then makes the socket :manpage:`listen(2)`, with a queue size of 1.

.. :c:function:: int create_tcp_client_socket(const char *host, const char *serv, int domain, int type)

   This function creates a client socket with
   :code:`create_socket(host, serv, domain, type, 0, false)`
   and returns it.

Both functions with "tcp" don't enforce the type to :code:`SOCK_STREAM`
but leave it to the caller.

.. c:function:: int create_udp_server_socket(const char *serv, int domain)

   This function creates a UDP server socket with
   :code:`create_socket(NULL, serv, domain, SOCK_DGRAM, AI_PASSIVE, true)`
   and returns it.

.. c:function:: int create_udp_client_socket(const char *host, const char *serv, int domain)

   This function creates a UDP client socket with
   :code:`create_socket(host, serv, domain, SOCK_DGRAM, 0, false)`
   and returns it.

Both functions with "udp" enforce the type to :code:`SOCK_DGRAM`.

Socket connection
-----------------

.. :c:function:: int connect_udp_server_to_client(int sfd, int cfd)

   This function connects the UDP server :code:`sfd` and the UDP client :code:`cfd`
   so that :manpage:`send(2)` and :manpage:`recv(2)` calls from the server send and receive to
   the client. Note that the client must be :manpage:`connect(2)`-ed to the server.

Test server and client creations
================================

It is possible to create test servers and test clients, in UDP or TCP,
using a set of 4 functions in this header.

Structures
----------

Three structures are defined::

    struct cs_network_chunk {
        void *data;
        size_t data_length;
        int type;
    };
    struct cs_network_transaction {
        struct cs_network_chunk *chunks;
        size_t nchunks;
        struct sockaddr *addr;
        socklen_t addrlen;
    };
    struct cs_network_transactions {
        struct cs_network_transaction *transactions;
        size_t ntransactions;
    };

- The first structure defines a chunk of data that may be sent or received
  by the test server of client. If type is :code:`SEND_CHUNK` then the chunk
  is sent to the other end of the connection, and any error is reported.
  If type is :code:`RECV_CHUNK`, then the test server or client will try
  to receive a chunk of data, and will compare it with the data in this chunk,
  reporting any transmission error or any difference. No byte-order conversion
  is done when sending or comparing!
- The second structure defines a transaction: a list of chunks exchanged
  between the test server or client and the other end of the connection.
  A transaction is successful if all to-send chunks have been sent correctly
  (=no error), if all to-recv chunks have been received correctly with
  no difference, and if the test server or client has not been asked to quit.
- The third structure defines a list of transactions, that are executed one
  after the other. This is useful to do successive tests without having
  to recreate a server or client again.

All these functions use :manpage:`recv(2)` and :manpage:`send(2)`, which means that it is possible
to enable partial reads or writes on the sockets (see :ref:`partial_read_write`).

Functions
---------

The four functions are declared as follows::

    int launch_test_tcp_server(struct cs_network_transactions *transactions, const char *serv, int domain, int *server_in, int *server_out, int *spid);
    int launch_test_tcp_client(struct cs_network_transactions *transactions, const char *host, const char *serv, int domain, int *client_in, int *client_out, int *cpid);
    int launch_test_udp_server(struct cs_network_transactions *transactions, const char *serv, int domain, int *server_in, int *server_out, int *spid);
    int launch_test_udp_client(struct cs_network_transactions *transactions, const char *host, const char *serv, int domain, int *client_in, int *client_out, int *cpid);

There are a lot of common parameters, hence they are explained together:

- :code:`transactions` is a pointer to a list of transactions, as defined above;
- :code:`host` and :code:`serv`, for clients, are the address and service to which
  the client has to send and receive its data. Both should be numeric.
- :code:`serv`, for servers, is the service name on which the server should be
  listening. It should be a numeric port.
- :code:`domain` is the domain (IPv4/IPv6) of the server.
- :code:`server_in` and :code:`server_out` are two file descriptors used to communicate
  with the server: the first is the writing end of a pipe that is read by
  the server, the second is the reading end of a pipe written to by the server.
- :code:`client_in` and :code:`client_out` are analoguous to :code:`server_in` and
  :code:`server_out` respectively.
- :code:`spid` is filled by the call with the server process PID.
- :code:`cpid` is filled by the call with the client process PID.

Working
-------

These functions work by forking and creating a new process, where a server
or a client of the appropriate address, port, domain and type is created and
initialised, using the appropriate socket creation function
(see :ref:`util_sockets_create`).

Communications
--------------

We can communicate with this other process by means of the input and output
pipes, and we can wait for its termination by waiting on the PID of the server
or client.

There are a number of codes that the child process can use to communicate
upstream:

- :code:`OK`: indicates that a transaction was finished.
- :code:`RECV_ERROR` indicates that an error occured while receiving;
  it is followed by :code:`sizeof(errno)` bytes representing the value of :code:`errno`
- :code:`SEND_ERROR` indicates that an error occured while sending.
- :code:`TOO_MUCH` indicates that much more bytes were received or sent, followed
  by the number of bytes received or sent.
- :code:`TOO_FEW` indicates that less bytes were received or sent, followed
  by the number of bytes received or sent.
- :code:`NOTHING_RECV` indicates that nothing was received, likely because the
  connection was closed.
- :code:`NOT_SAME_DATA` happens when the data is different.
- :code:`NOT_SAME_ADDR` happens when the address of :code:`recvfrom` differs from
  the one that should have been; the address length follows, and then
  the address.
- :code:`EXIT_PROCESS` happens when the subprocess ends prematurely.
- :code:`EXTEND_MSG` is used to specify that the next octet gives the status;
  it is used to allow extensions to these error codes.

Some of the error codes can also be ORed together.

The parent process can communicate with its child by writing to the downstream
pipe. Currently, anything written to it will cause the subprocess to exit.

Caution
-------

These 4 functions have never been tested, either by unit tests or in an actual
exercise.
There are numberous bugs, bad design choices everywhere.
This documentation is here for the sole purpose of mentioning it exists,
and what its intents were.
Use at your own risk.
Don't hesitate to contribute, fixing bugs, adding tests, or whatever.
