.. _trap:

===============
Trapped buffers
===============

The file ``CTester/trap.h`` defines two functions for creating trapped buffers,
:c:func:`trap_buffer()`, and for deleting them, :c:func:`free_trap()`.

.. c:function:: void* trap_buffer(size_t size, int type, int flags, void *data)

.. c:function:: int free_trap(void *ptr, size_t size)

A trapped buffer is a continuous memory area (a buffer) that is bounded, at one
edge, with a memory page that is *trapped* (:code:`PROT_NONE`). The buffer itself
is set with the :code:`flags` (any flag accepted by :manpage:`mprotect(2)`).

The type can be either :code:`TRAP_LEFT` or :code:`TRAP_RIGHT` (defined by an enum).

If :code:`type` is :code:`TRAP_LEFT`, then the buffer has a full page of memory
with :code:`PROT_NONE` access before it. The pointer returned by :c:func:`trap_buffer()`
is :code:`buf_start` and is the start of the buffer, whereas the trapped page
starts at an address called :code:`ptr=buf_start - PAGE_SIZE`. When freeing
the trapped buffer, one should pass to :c:func:`free_trap()` the above defined:code:`ptr`
value, and a size of :code:`(2 + (size_of_buffer / PAGE_SIZE)) * PAGE_SIZE`.

If :code:`type` is :code:`TRAP_RIGHT`, then the buffer has a full page of memory
with :code:`PROT_NONE` access just *after it*. The pointer returned by :c:func:`trap_buffer()`
is :code:`buf_start` and is, again, the start of the buffer; it is not necessarily
aligned with a page boundary. The trapped page starts at :code:`buf_start+size`
and is aligned to a page boundary. The :code:`ptr` value that should be passed
to :c:func:`free_trap()` is then :code:`(buf_start + size) - (1 + size/PAGE_SIZE) * PAGE_SIZE`
and the size is again :code:`(2 + (size_of_buffer / PAGE_SIZE)) * PAGE_SIZE`.

Obviously, the API could be better.

