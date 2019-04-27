{
  "target_defaults": {
    "default_configuration": "Release",
    "configurations": {
      "Debug"  : { "defines": ["_DEBUG"] },
      "Release": { "defines": ["NDEBUG"] }
    },
    "conditions": [
      ["OS=='linux' or OS=='mac'", {
        "cflags": ["-fvisibility=hidden"]
      }]
    ]
  },
  "targets": [{
    "target_name": "njs-test",
    "sources": [
      "njs-api.h",
      "njs-base.h",
      "njs-engine-sm.h",
      "njs-engine-v8.h",
      "njs-extension-enum.h",
      "njs-integrate-libuv.h",
      "test/test.cpp",
      "test/test_p.h"
    ]
  }]
}
