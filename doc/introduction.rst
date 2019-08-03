.. _introduction:

=======================
Introduction to CTester
=======================

CTester is a library that provides a secure and customizable way of evaluating
C codes written by students.

It provides the following features:

- Sandboxing of student code: securely running codes of students, with no risk
  of crashes of the testing environment;
- Statistics of function calls: records number of calls, last parameter used,
  for a large number of standard functions and system calls;
- Customizable behaviour of some functions: DNS-related functions and
  :code:`read` and :code:`write` functions;
- And even more features!

This documentation describes in details the API and usage of CTester.

It is organized as follows:

- The :ref:`usage` page describes the general usage of CTester: sandboxing,
  a few notes on wrappers, how one should write tests and organize its code
  for efficient testing of student code.
- The :ref:`wrap` page describes in more details the general look of a wrapper,
  and the features it provides: statistics, programmed failures...
- The :ref:`api_index` pages describe in more details the wrappers for each
  function, as well as the additional features provided by CTester.
- The :ref:`limitations` page describes the current limitations, bugs,
  and vulnerabilities of CTester. Yes, there are some, that should be corrected.
  Improvements to CTester should start by fixing these deficiencies.


