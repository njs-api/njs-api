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
      "njs_test.cpp",
      "njs_test_p.h"
    ]
  }]
}
