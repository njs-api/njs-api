Neutral JS
==========

  * [Official Repository (kobalicek/njs)](https://github.com/kobalicek/njs)
  * [Public Domain](./LICENSE.md)

Introduction
------------

NJS (Neutral-JS) is a header-only C++ library that abstracts APIs of JavaScript virtual machines (VMs) into a single API that can be used by embedders to write bindings compatible with multiple JS engines. The motivation to start the project were breaking changes of V8 engined and the way V8 bindings are written (too verbose and too annoying to handle all possible errors). Also, attempt to port node.js to use Chakra javascript engine instead of V8 contributed to the idea of creating a single API that can target multiple JS engines, not just V8.

Features:

  - Source-level abstraction layer on top of native VM APIs. Source-level means that you cannot replace JavaScript engines at runtime, recompilation is needed as NSJ wraps VM dependent data structures and classes.
  - High-performance interface that maps to native VMs as closely as hand-written bindings - the compiled code should be very close to a code you would have normally written for a particular VM.
  - High-level interface to decrease the amount of code you need to write and to decrease the size of the resulting compiled code - no more boilerplate for each function you want to bind.
  - Highly secure bindings that should never put the application into an inconsistent state throught JavaScript.
  - Comfortable and meaningful error handling - let users of your bindings know where the problem happened. NJS contains many helpers to make error handling less verbose yet still powerful on C++ side (it turns the most annoying type checks to one-liners).
  - Foundation to build extensions that can be used to implement concepts - concepts are additions that enable data serialization and deserialization based on custom user types.

Disclaimer
----------

NJS is a WORK-IN-PROGRESS that has been published to validate the initial ideas and to design the API with more people. The library started as a separate project when developing [b2d-js](https://github.com/blend2d/b2d-node) to make bindings easier to write, easier to maintain, and also more secure.

TODO
----

Write some documentation and examples.
