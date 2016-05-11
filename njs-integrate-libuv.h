// [NJS]
// Neutral JS interface.
//
// [License]
// Public Domain <http://unlicense.org>

// [Guard]
#ifndef _NJS_INTEGRATE_LIBUV_H
#define _NJS_INTEGRATE_LIBUV_H

// [Dependencies]
#include "./njs.h"
#include <uv.h>

namespace njs {

// ============================================================================
// [njs::internal]
// ============================================================================

namespace internal {
  static NJS_NOINLINE void UVWorkCallback(uv_work_t* uvWork) NJS_NOEXCEPT;
  static NJS_NOINLINE void UVAfterWorkCallback(uv_work_t* uvWork, int status) NJS_NOEXCEPT;
} // internal namespace

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

  NJS_NOINLINE Task(Context& ctx, Value data) NJS_NOEXCEPT
    : _runtime(ctx._runtime) {

    // Initialize the storage.
    ctx.MakePersistent(data, _data);

    // Initialize UV data.
    _uvWork.data = this;
    _uvStatus = 0;
  }
  virtual ~Task() NJS_NOEXCEPT {}

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  virtual void OnWork() NJS_NOEXCEPT = 0;
  virtual void OnDone(Context& ctx, Value data) NJS_NOEXCEPT = 0;
  virtual void OnDestroy(Context& ctx) NJS_NOEXCEPT { delete this; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  // Basic runtime, needed to setup the `Context` for `OnComplete()`.
  Runtime _runtime;

  // Object used as a storage of indexed values. Only accessible when the task
  // is created and/or completed, it's not possible to access it inside `OnWork`
  // from a different thread.
  Persistent _data;

  // UV work data.
  uv_work_t _uvWork;
  // UV status - initially zero, changed by `UVAfterWorkCallback`.
  int _uvStatus;
};

static NJS_NOINLINE void PostTask(Task* task) {
  uv_queue_work(
    uv_default_loop(),
    &task->_uvWork,
    internal::UVWorkCallback,
    internal::UVAfterWorkCallback);
}

namespace internal {
  static NJS_NOINLINE void UVWorkCallback(uv_work_t* uvWork) NJS_NOEXCEPT {
    Task* task = static_cast<Task*>(uvWork->data);
    task->OnWork();
  }

  static NJS_NOINLINE void UVAfterWorkCallback(uv_work_t* uvWork, int status) NJS_NOEXCEPT {
    Task* task = static_cast<Task*>(uvWork->data);

    njs::ScopedContext ctx(task->_runtime);
    njs::Value data = ctx.MakeLocal(task->_data);

    task->OnDone(ctx, data);
    task->OnDestroy(ctx);
  }
} // internal namespace

} // njs namespace

// [Guard]
#endif // _NJS_INTEGRATE_LIBUV_H
