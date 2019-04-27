// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// [Guard]
#ifndef _NJS_INTEGRATE_LIBUV_H
#define _NJS_INTEGRATE_LIBUV_H

// [Dependencies]
#include "./njs-api.h"
#include <uv.h>

namespace njs {

// ============================================================================
// [njs::Internal]
// ============================================================================

namespace Internal {
  static NJS_NOINLINE void uvWorkCallback(uv_work_t* uvWork) noexcept;
  static NJS_NOINLINE void uvAfterWorkCallback(uv_work_t* uvWork, int status) noexcept;
} // Internal namespace

// ============================================================================
// [NJS_ASYNC]
// ============================================================================

class Task {
public:
  enum {
    kIndexCallback = 0,
    kIndexExports  = 1,
    kIndexParams   = 2,
    kIndexCustom   = 3
  };

  NJS_NOINLINE Task(Context& ctx, Value data) noexcept
    : _runtime(ctx._runtime) {

    // Initialize the storage.
    ctx.makePersistent(data, _data);

    // Initialize UV data.
    _uvWork.data = this;
    _uvStatus = 0;
  }
  virtual ~Task() noexcept {}

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  virtual void onWork() noexcept = 0;
  virtual void onDone(Context& ctx, Value data) noexcept = 0;
  virtual void onDestroy(Context& ctx) noexcept { delete this; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Basic runtime, needed to setup the `Context` for `OnComplete()`.
  Runtime _runtime;

  //! Object used as a storage of indexed values. Only accessible when the task
  //! is created and/or completed, it's not possible to access it inside `onWork`
  //! from a different thread.
  Persistent _data;

  //! UV work data.
  uv_work_t _uvWork;
  //! UV status - initially zero, changed by `uvAfterWorkCallback`.
  int _uvStatus;
};

static NJS_NOINLINE void PostTask(Task* task) {
  uv_queue_work(
    uv_default_loop(),
    &task->_uvWork,
    Internal::uvWorkCallback,
    Internal::uvAfterWorkCallback);
}

namespace Internal {
  static NJS_NOINLINE void uvWorkCallback(uv_work_t* uvWork) noexcept {
    Task* task = static_cast<Task*>(uvWork->data);
    task->onWork();
  }

  static NJS_NOINLINE void uvAfterWorkCallback(uv_work_t* uvWork, int status) noexcept {
    Task* task = static_cast<Task*>(uvWork->data);

    njs::ScopedContext ctx(task->_runtime);
    njs::Value data = ctx.makeLocal(task->_data);

    task->onDone(ctx, data);
    task->onDestroy(ctx);
  }
} // Internal namespace

} // njs namespace

// [Guard]
#endif // _NJS_INTEGRATE_LIBUV_H
