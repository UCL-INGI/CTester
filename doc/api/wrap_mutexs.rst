.. _wrap_mutexs:

======================================
Mutex-related functions and structures
======================================

The header ``CTester/wrap_mutex.h`` contains the wrappers and structures
for the following functions, handling mutexes:

- :manpage:`pthread_mutex_init(3)`: initializes a mutex;
- :manpage:`phtread_mutex_lock(3)`: locks a mutex;
- :manpage:`pthread_mutex_trylock(3)`: tries to lock a mutex, without blocking;
- :manpage:`pthread_mutex_unlock(3)`: unlocks the mutex;
- :manpage:`pthread_mutex_destroy(3)`: destroys the mutex and frees the allocated structures.

All functions defined here record statistics of the number of times called,
of the arguments and return value of the last calls.

However, as of now, they don't allow programmed failures.

There is a initialization, cleanup and statistics reset function
for each function, as well as three functions for all 5 functions.

As of now, :code:`pthread_mutex_init` statistics only remember the last :code:`mutex`
argument, not the attributes :code:`attr`. This should be fixed.

