.. _read_write:

=========================
Controlled read and write
=========================

The files ``CTester/read_write.h`` and ``CTester/read_write.c`` provide an API
allowing a more fine-tuned control of the :manpage:`read(2)` and
:manpage:`write(2)` system calls.

Currently, this API consists of partial read and write.

.. _partial_read_write:

Partial read and write
======================

The :code:`read` system call (and its :code:`recv` relatives), whose prototype is::

    ssize_t read(int fd, void *buf, size_t count);

attempts to read up to :code:`count` bytes from the file descriptor :code:`fd` into
the buffer starting at :code:`buf`, and returns the actual number of bytes read
from :code:`fd` (let's call it :code:`read_count`), or -1 if there was an error.

There is a lot of circumstances that can lead to a number of bytes read that is
less than the requested amount :code:`count`, without any error:
we can possibly read from a pipe, or from a socket, or from a terminal.
Reading less bytes than requested simply means that the underlying device
couldn't provide more bytes at the time, but may give more bytes
at a later call.

For :code:`recv` and related, this is even more true: if the socket refers
to an UDP connection, then the :code:`recv` calls can only return one datagram
at a time. Generally, these calls return as soon as they have at least one byte
available (if they were configured as non-blocking, they return nearly
immediately), and return as many bytes as possible, up to the requested amount,
without waiting for the full amount.

The :code:`write` system call has a similar behaviour, but for different reasons:
there may be not enough space on the underlying medium (eg: disk), the pipe or
buffer may be near-full, or the requested amount is greater than the size of
a packet, datagram or segment.

The student is of course expected to be able to handle these system calls
correctly, and hence to be able to handle these "partial read" and
"partial write". The API here described allows the teacher to test that they
correctly handle these behaviours.

When partial reads are used, CTester replaces the calls to the real :code:`read`
calls to a real file descriptor, with reads from a teacher-supplied buffer,
with pre-determined chunks of custom sizes and custom availability delay.

Currently, partial writes are not supported, as they are ill-defined.

The buffers
-----------

The buffer used in the partial read is defined by the following
structures::

    struct read_bufchunk_t {
        int interval;
        const void *buf;
        size_t buflen;
    };
    struct read_buffer_t {
        int mode;
        size_t nchunks;
        struct read_bufchunk_t *chunks;
    };

.. c:type:: struct read_bufchunk_t

   This structure is used to specify a chunk: a piece of continuous data that,
   when available, can be fully read in one call, if the destination buffer
   is big enough.

    .. c:member:: int interval

       Specifies the delay (in milliseconds) or the time interval before
       the chunk is made available; see :ref:`read_write_read_modes`.

    .. c:member:: const void* buf

       Pointer to the data stored by the chunk.

    .. c:member:: size_t buflen

       Size of the data stored by the chunk.


.. c:type:: struct read_buffer_t

   This structure is used to specify the full content of the file or the stream
   backed by this file descriptor. It consists in an array
   :c:member:`chunks <read_buffer_t.chunks>` of
   :c:member:`nchunks <read_buffer_t.nchunks>` chunks, as defined before.

    .. c:member:: int mode

       Mode of interpretation of the time intervals; see :ref:`read_write_read_modes`.

    .. c:member:: size_t nchunks

       Number of chunks in the buffer.

    .. c:member:: struct read_bufchunk_t* chunks

       Array of chunks

Creating a buffer for the partial reads, that simulates a file, requires
allocating all these structures and filling them appropriately.

Buffer creation helpers
~~~~~~~~~~~~~~~~~~~~~~~

Some utility functions are provided to create a buffer based on an existing
piece of data (the content of the file or of the stream), the wished sizes
of each chunk and the wished intervals:

.. c:function:: struct read_buffer_t *create_read_buffer(void *data, size_t n, off_t *offsets, int *intervals, int mode)

This function allows creating a full buffer. For instance, if called like this::

    void *mydata = malloc(40);
    // mydata is filled with 40 bytes of data
    off_t chunk_sizes[] = {10, 30};
    int intervals[] = {100, 200};
    struct read_buffer_t *rep = create_read_buffer(mydata, 2, chunk_sizes, intervals, READ_WRITE_REAL_INTERVAL);

Then the buffer will be structured like this:

- :code:`rep` will be allocated by the function, or will be :code:`NULL` if there was an error;
- :code:`rep->mode` will be :code:`READ_WRITE_REAL_INTERVAL`;
- :code:`rep->nchunks` will be 2;
- :code:`rep->chunks` will point to an array of 2 :code:`struct read_bufchunk_t`, like this:

  - :code:`rep->chunks[0]` will be the first chunk, with the following fields:

    - :code:`rep->chunks[0].buf` will be equal to :code:`mydata`;
    - :code:`rep->chunks[0].buflen` will be 10, for 10 bytes;
    - :code:`rep->chunks[0].interval` will be 100: this is the wait time for
      the first chunk; see :ref:`read_write_read_modes`.

  - :code:`rep->chunks[1]` will be the second chunk, with the following fields:

    - :code:`rep->chunks[1].buf` will be equal to :code:`mydata + 10`;
    - :code:`rep->chunks[1].buflen` will be 30, for 30 bytes;
    - :code:`rep->chunks[1].interval` will be 200: this is the wait time for
      the second chunk, after the first one was read; see :ref:`read_write_read_modes`.

Note that the size of the full data, 40, is not passed explicitly
to the function; rather, it can be computed by summing up :code:`offsets`.
Also note that the parameter :code:`offsets` doesn't contain offsets, but rather
sizes of each of the corresponding chunk.

.. c:function:: int create_partial_read_buffer(void *data, size_t n, off_t *offsets, int *intervals, struct read_buffer_t *buf)

This function does moestly the same work as :c:func:`create_read_buffer()`,
but it doesn't allocate the :c:type:`read_buffer_t`; rather, it uses the one
pointed to by :code:`buf`. This is especially useful if the buffer is allocated
on the stack. Another difference is that it doesn't set the :code:`mode` field:
this field must be set by the caller.

.. c:function:: void free_read_buffer(struct read_buffer_t *buf)

.. c:function:: void free_partial_read_buffer(struct read_buffer_t *buf)

Those two functions free the structures that were allocated by
:c:func:`create_read_buffer()` and :c:func:`create_partial_read_buffer()`
respectively. Note that they don't free the underlying data.

.. _read_write_read_modes:

The modes
---------

There are three modes, that relate to the way the chunks become accessible
as time goes on:

- :code:`READ_WRITE_REAL_INTERVAL`: in this mode, the chunks become available
  in real time: they become available as time goes on, without depending
  on the pattern of calls to :code:`read` or :code:`recv`.
- :code:`READ_WRITE_AFTER_INTERVAL`: in this mode, the next chunk becomes available
  only after :code:`interval` msec have passed since the previous call to :code:`read`
  (or :code:`recv`) on this file descriptor; the time may have passed between the
  two calls, resulting in no wait. Only one chunk is read each time.
- :code:`READ_WRITE_BEFORE_INTERVAL`: in this mode, the next chunk becomes available
  only after a forced wait of :code:`interval` msec, whatever time has passed since
  the previous call; this is the strictest most of the three.

The modes, and the behaviour of the :code:`read` and :code:`recv` calls depending on
these modes, are described in more details in the :ref:`read_write_working_tracking`
section.

Setting up
----------

To enable the partial read on a file descriptor, we use the following function:

.. c:function:: int set_read_buffer(int fd, const struct read_buffer_t *buf)

This function enables partial read on the file descriptor :code:`fd`, and specifies
the buffer as :code:`buf`. It returns 0 if the file descriptor didn't already have
a buffer (that is, it wasn't in partial read mode); 1 if there was already
a buffer, in which case the buffer was changed to the new one, with reset
statistics of progression; -1 if there was an internal error (like not enough
memory); -2 if there was an argument error (like an incorrect mode).

Don't forget to enable monitoring of the appropriate calls (:code:`read`, :code:`recv`
and relatives) if you want the student code run in the sandbox to experience
this partial read. Outside of these conditions, a standard, native :code:`read`
will be called on the true file descriptor.

To disable the partial read on a file descriptor, simply call this same
function with :code:`buf = NULL`.

It is also possible to reset all file descriptor to the default, no-partial-read
behaviour by calling the following function.

.. c:function:: void reinit_read_fd_table()

This removes all associations between a file descriptor and a read buffer,
disables partial read for all descriptors previously set, but it doesn't free
the structures (as they can be allocated on the stack).

.. _read_write_working_tracking:

Working of the partial read and tracking progress
-------------------------------------------------

Once this is done, the partial read works as follows:

Each time the student calls :code:`read` or :code:`recv` (or its relatives)
(they should be monitored), on the file descriptor that was set up
for partial read, inside the sandbox, CTester (referred to as "we" in the
following lines) will attempt to read up to :code:`count` bytes
(from the parameter of the call) from the buffer.

If there is an "open" chunk (= a chunk that has already been partially read,
with at least one byte read, and with remaining bytes left to read), then
we will first read as much bytes from this chunk as possible and as requested.
If the requested number of bytes can be satisfied using only this chunk, then
the call ends and is successful. If not, then we mark this chunk as "finished"
and "start" the next chunk. If this chunk is already available (based on
the time interval: we're in real time and the time has passed),
then we read it; if not, then the call returns without waiting
for the next chunk to become available. If the chunk is available, we read
as many bytes as necessary, possibly emptying the chunk, and possibly
looking at the next chunk: this is repeated as many times as necessary.

If there is no "open" chunk (= the previous chunk was emptied and we're at
the next chunk), then either the file has been fully read,
or there is a next chunk, which we can start.

The behaviour then depends on the mode. If we're in real-time, then we must
wait for the chunk to become available in real-time. It is possible that
the chunk has already been available for some time before the call, and thus
no wait will take place; otherwise, we'll have to wait. It is also possible
that multiple chunks are available at the time of the call; all of theif the request
available chunks will be used to satisfy the requested amount of bytes, until
the requested amount has been met, or we have to read a chunk that is not yet
available; in the latter case, the call ends without waiting.

If we're in after-time, then we must wait only if the current call has been
issued less than the time interval after the previous call: if the interval
was set to 50msec and the current call is issued 40msec after the previous
one, then we wait only 10msec. We then read only one chunk, up to the
requested amount or the chunk size (whichever comes first), and return,
without waiting for an eventual next chunk.

If we're in before-time, then we must wait the interval time, whatever the time
has passed from the previous call. We then read only one chunk, up to the
requested amount or the chunk size, and return, without waiting for the next.

There is therefore a strong difference between the real-time mode on one side,
and the before-time or after-time modes on the other side: the latter can only
read one chunk at each call, possibly emptying it, possibly waiting for it,
while the former can read multiple chunks if multiple chunks are available,
stopping only when either the requested amount has been satisfied or the next
chunk would need to wait.

Note the strange behaviour of the :code:`read` system call: if there is no data
to read at the moment, then the call blocks (unless we set the file descriptor
to non-blocking, or we call :code:`recv` with the :code:`MSG_DONTWAIT` flag set).
But if there is some data, then the call doesn't block, reads as much data
as requested, but doesn't wait for more data to become available and prefers
returning a lower amount of bytes than requested. This behaviour is reproduced
in CTester.

Tracking progress
~~~~~~~~~~~~~~~~~

It is possible to track the state of the partial read with the following
functions:

.. c:function: ssize_t get_bytes_read(int fd)

This function returns the number of bytes that have Currently been read
on the file descriptor :code:`fd`, or -1 if the file descriptor is not associated
with a read buffer for partial read. If the buffer was completely read,
then the full size of the buffer will be returned.

.. c:function: int get_current_chunk_id(int fd)

This function returns the current chunk id (index, number) of the read buffer
of the file descriptor :code:`fd`, or -1 if there is no read buffer.
If the "read cursor" (read offset) is between two chunks (it has finished one
and has not yet started the next), then it will return the id of that next chunk.
Note that the ids start at 0 and not at 1! If the buffer was completely read,
then the number of chunks will be returned (which is equal to the id of
the last chunk plus 1).

If, at some point, you need to see if some file descriptor has a read buffer,
you can use the following function:

.. c:function: bool fd_is_read_buffered(int fd)

This function returns :code:`true` if the file descriptor :code:`fd` has
a read buffer for partial return.

Applications
------------

The most common (and original) use case for these functions is when the file
descriptor is the one of a socket. Sockets don't behave as files, and are more
prone to partial reads in the real life than for usual files.

For TCP-based, stream sockets, partial reads can happen when the connection
is not yet closed but we're waiting on new segments to arrive (at any point
between the sender and the receiving application, even within the OS!).
This is even more true as TCP reorders the arriving segments, and so may take
some time waiting for out-of-order segments to arrive before sending the full,
in-order segments to the application. The real-time mode is more appropriate.

For UDP-based, datagram sockets, we receive datagrams, which are full messages,
independent of any stream. :code:`recvfrom` and :code:`recvmsg` are preferred,
as the source address of the datagrams can vary each time (:manpage:`connect(2)` can be
used to restrict the socket to only accept and receive datagrams coming from
a specific source). As there is no concept of stream, when one reads on the
socket, he reads only one message, one datagram at a time; when the read
terminates, the datagram is discarded (unless we used the :code:`MSG_PEEK` flag)
from the queue of messages and the next message is available (or not):
requesting too less data can lead to some part of the message being definitely
lost. The before-time or after-time modes are more appropriate. Note that this
is not strictly a partial-return.

Partial writes
--------------

Currently, CTester doesn't handle partial writes.

Partial writes happen less frequently than partial reads: when the underlying
file descriptor refers to a file, it mostly happens when the buffers are full;
when it is a socket, it mostly happens when the networking buffers are full,
or when the data is too large to fit inside one packet or datagram (especially
in UDP, where the max size is usually 65507 on IPv4).

Limitations and improvements
----------------------------

:code:`recvfrom` and :code:`recvmsg`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Only :manpage:`read(2)` and :manpage:`recv(2)` are guaranteed to pass through partial return
if set up. :manpage:`recvfrom(2)` will pass through partial return only if :code:`addr`
and :code:`addrlen` are both :code:`NULL`, otherwise it will go through the real one.
:manpage:`recvmsg(2)`, due to its complicated definition, doesn't support partial return,
but one should not expect a lot of student to use this function...

Note for the potential contributor: :code:`recvfrom` should clearly be improved,
the difficulty comes from what values to put into the :code:`addr` and :code:`addrlen`
fields.

The :code:`MSG_PEEK` flag
~~~~~~~~~~~~~~~~~~~~~~~~~

Currently, the :code:`MSG_PEEK` flag of :code:`recv` and related is not handled:
the read data cannot be retrieved again.

Note for the potential contributor: in real-time mode, this should be easy:
just read the data, but reset the control structures to the original values.

In after-time or before-time mode however, it requires a bit more modifications,
as just resetting the control structures to the original value means that
a subsequent call will *again* possibly block and wait. One should add a field
to the control structure to specify that the data has already been waited for
and we can proceed without waiting.

UDP
~~~

If the socket is using UDP, normally we should pass to the next chunk/datagram
each time, as the remains of the current datagram are discarded after reading
the datagram. But this is not what happens. We should handle this better.

Stability
~~~~~~~~~

Although it has been tested a lot, the amount of code is prone for a lot of bugs.
Don't hesitate to add unit tests for a more thorough verification!

Internal working
----------------

There is two additional data structures::

    struct read_item {
        int fd;
        const struct read_buffer_t *buf;
        unsigned int chunk_id;
        size_t bytes_read;
        struct timespec last_time;
        int64_t interval;
    };
    struct read_fd_table_t {
        size_t n;
        struct read_item *items;
    };

The first structure defines all the control variables needed for te partial reads:

- :code:`fd` is the file descriptor.
- :code:`buf` is the read buffer, never modified.
- :code:`chunk_id` is the current chunk id. It starts at 0 with the first chunk,
  is incremented each time we pass to the next chunk, is equal to :code:`buf->nchunks`
  when we're at the end of the data.
- :code:`bytes_read` is the number of bytes read thus far in the current chunk.
  It is 0 if the chunk has not yet been read (and thus, we may need to wait
  for the chunk to become available). When a chunk is emptied, :code:`chunk_id` is
  incremented to go to the next chunk, and :code:`bytes_read` is reset to 0.
- :code:`last_time` is the last time a call to :code:`read` or :code:`recv` has been made.
  It is updated each time a call is made, and is used in real-time and after-time
  modes to determine if we have to wait, for how long, and for which chunks.
  Note that the structure is precise down to the nanosecond and up to billions
  of seconds.
- :code:`interval` is, in real-time mode, the real wait interval (in nanoseconds)
  for the current chunk. It is relative to :code:`last_time`, which was set during
  the previous call to :code:`read` or :code:`recv`, and is updated each time.
  It can be positive, meaning we need to wait at least :code:`interval` nanoseconds
  since :code:`last_time`, or it can be negative, meaning we don't have to wait,
  but we still need to track the time in order for the subsequent chunks
  to become available in real-time. It is not used in after-time mode or in
  before-time mode.

The second structure is actually a singleton definition used to declare
a variable, :code:`read_fd_table`, which contains one :code:`struct read_item` entry
in the :code:`items` table (of size :code:`n`) for each file descriptor currently set
to partial mode.

There are a number of helper functions defined in the .c file, but the most
important one is the following:

.. c:function: ssize_t read_handle_buffer(int fd, void *buf, size_t len, int flags)

This function is the one that is called by the wrappers of :manpage:`read(2)`, :manpage:`recv(2)`,
:manpage:`recvfrom(2)` and :manpage:`recvmsg(2)` when the file descriptor :code:`fd` is associated
with a read buffer, and this is actually the function we've referred to each
time we talk about "last call to :code:`read` or :code:`recv`".

This function contains a great part of the logic behind the partial read
(actually, the greatest part is in :code:`read_handle_buffer_rt` and :code:`read_handle_buffer_no_rt`,
but those are helper functions and not publicly available). Its arguments are
the same as the ones of :code:`recv`.

Most of the limitations of the current system come from this function, also.

Other functions declared in the .c file include functions manipulating time
structures (:code:`getnanotime`, :code:`get_time_interval`), adding and removing
a file descriptor from the :code:`read_fd_table`, and getting its :code:`read_item`
(:code:`read_get_entry`, :c:func:`fd_is_read_buffered`, :code:`reinit_read_fd_table_item`,
:code:`read_remove_entry`, :code:`read_get_new_entry`), and the core implementation of
:code:`read_handle_buffer` (:code:`read_handle_buffer_rt` and :code:`read_handle_buffer_no_rt`).

If you read the source code: it may not be the cleanest code, nor the most
efficient code, but it gets the job done, and only waits to be improved. ;-)

