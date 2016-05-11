Neutral JS
==========

  * [Official Repository (kobalicek/njs)](https://github.com/kobalicek/njs)
  * [Public Domain](./LICENSE.md)

Disclaimer
----------

NJS is a WORK-IN-PROGRESS that has been published to validate the initial ideas and to design the API with more people. The library started as a separate project when developing [b2d-js](https://github.com/blend2d/b2d-node) to make bindings easier to write, easier to maintain, and also more secure.

Introduction
------------

NJS (Neutral JavaScript) is a header-only C++ library that abstracts JavaScript virtual machines (VM)s into a neutral APIs, which can be used by embedders to write their C++ to JavaScript bindings. There are several reasons why NJS has been written and why you should consider using it:

  - APIs of VMs are different, but provide generally the same subset of features available for embedders. Some VMs expose a lot of details to the embedder (like V8) whereas some VMs try to hide as many details as possible (ChakraCore). The purpose of NJS is to hide some implementation details and to provide APIs that the embedder can use in a consistent and safe way.
  - APIs of VMs break from time to time, and these breaks are sometimes painful (for example V8 changed its APIs several times in a very drastic way).

The goals of NJS can be summarized in the following points:

  - Provide a source-level abstraction over native VM APIs (source-level means that you cannot replace JavaScript VM at runtime, recompilation is needed).
  - Provide a high-performance interface that maps to native VMs (the compiled code should be very close to a code you would have normally written for a particular VM).
  - Provide high-level interface to decrease the amount of code you need to write.
  - Provide a foundation to build extensions that can be used to implement high-level concepts.

