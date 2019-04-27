// [NJS-API]
// Native JavaScript API for Bindings.
//
// [License]
// Public Domain <http://unlicense.org>

// This is the main header file that tries to auto-detect the target engine
// including integrations. If it fails or you use a single engine within your
// project then it's much easier to just include the specific engine that you
// are using, like "njs-engine-v8".

#ifndef NJS_API_H
#define NJS_API_H

#include "./njs-base.h"

// ============================================================================
// [NJS - Detect the engine and integration]
// ============================================================================

// Node.js native addons define the `BUILDING_NODE_EXTENSION` macro.
#if defined(BUILDING_NODE_EXTENSION)
# define NJS_INTEGRATE_NODE
# define NJS_INTEGRATE_LIBUV
#else
# error "[njs] Couldn't detect the target environment."
#endif // BUILDING_NODE_EXTENSION

#if defined(NJS_INTEGRATE_NODE)
# include <node.h>
# if defined(JS_ENGINE_MOZJS)
#  define NJS_ENGINE_SM // jxcore - spidermonkey.
# else
#  define NJS_ENGINE_V8 // node/jxcore - v8.
# endif
#endif // NJS_INTEGRATE_NODE

// ============================================================================
// [NJS - Include the detected engine]
// ============================================================================

#if defined(NJS_ENGINE_SM)
# include "./njs-engine-sm.h"
#endif // NJS_ENGINE_SM

#if defined(NJS_ENGINE_V8)
# include "./njs-engine-v8.h"
#endif // NJS_ENGINE_V8

// ============================================================================
// [NJS - Include all detected integrations]
// ============================================================================

#if defined(NJS_INTEGRATE_LIBUV)
# include "./njs-integrate-libuv.h"
#endif // NJS_INTEGRATE_LIBUV

#endif // NJS_API_H
