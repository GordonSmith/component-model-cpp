# Spec alignment tracker

This document tracks alignment between the upstream Component Model spec/reference (in `ref/component-model`) and this library’s C++ entrypoints.

- Upstream references:
  - `ref/component-model/design/mvp/CanonicalABI.md`
  - `ref/component-model/design/mvp/Concurrency.md`
  - `ref/component-model/design/mvp/canonical-abi/definitions.py`
- C++ namespace: `cmcpp`

## Canonical built-ins inventory

| Spec built-in | C++ entrypoint | Status | Notes |
|---|---|---|---|
| `context.get` | `cmcpp::canon_context_get` | implemented | |
| `context.set` | `cmcpp::canon_context_set` | implemented | |
| `backpressure.inc` | `cmcpp::canon_backpressure_inc` | implemented | |
| `backpressure.dec` | `cmcpp::canon_backpressure_dec` | implemented | |
| `backpressure.set` | `cmcpp::canon_backpressure_set` | implemented | Spec notes this as deprecated in favor of `backpressure.{inc,dec}`. |
| `thread.yield` | `cmcpp::canon_thread_yield` | implemented | |
| `thread.yield-to` | `cmcpp::canon_thread_yield_to` | implemented | |
| `thread.resume-later` | `cmcpp::canon_thread_resume_later` | implemented | |
| `thread.index` | `cmcpp::canon_thread_index` | implemented | |
| `thread.suspend` | `cmcpp::canon_thread_suspend` | implemented | |
| `task.return` | `cmcpp::canon_task_return` | implemented | |
| `task.cancel` | `cmcpp::canon_task_cancel` | implemented | |
| `task.wait` | `cmcpp::canon_task_wait` | implemented | |
| `waitable-set.new` | `cmcpp::canon_waitable_set_new` | implemented | |
| `waitable-set.drop` | `cmcpp::canon_waitable_set_drop` | implemented | |
| `waitable-set.wait` | `cmcpp::canon_waitable_set_wait` | implemented | |
| `waitable-set.poll` | `cmcpp::canon_waitable_set_poll` | implemented | |
| `waitable.join` | `cmcpp::canon_waitable_join` | implemented | |
| `stream.new` | `cmcpp::canon_stream_new` | implemented | |
| `stream.read` | `cmcpp::canon_stream_read` | implemented | |
| `stream.write` | `cmcpp::canon_stream_write` | implemented | |
| `stream.cancel-read` | `cmcpp::canon_stream_cancel_read` | implemented | |
| `stream.cancel-write` | `cmcpp::canon_stream_cancel_write` | implemented | |
| `stream.drop-readable` | `cmcpp::canon_stream_drop_readable` | implemented | |
| `stream.drop-writable` | `cmcpp::canon_stream_drop_writable` | implemented | |
| `future.new` | `cmcpp::canon_future_new` | implemented | |
| `future.read` | `cmcpp::canon_future_read` | implemented | |
| `future.write` | `cmcpp::canon_future_write` | implemented | |
| `future.cancel-read` | `cmcpp::canon_future_cancel_read` | implemented | |
| `future.cancel-write` | `cmcpp::canon_future_cancel_write` | implemented | |
| `future.drop-readable` | `cmcpp::canon_future_drop_readable` | implemented | |
| `future.drop-writable` | `cmcpp::canon_future_drop_writable` | implemented | |
| `resource.new` | `cmcpp::canon_resource_new` | implemented | |
| `resource.rep` | `cmcpp::canon_resource_rep` | implemented | |
| `resource.drop` | `cmcpp::canon_resource_drop` | implemented | |
| `error-context.new` | `cmcpp::canon_error_context_new` | implemented | |
| `error-context.debug-message` | `cmcpp::canon_error_context_debug_message` | implemented | |
| `error-context.drop` | `cmcpp::canon_error_context_drop` | implemented | |

## Thread built-ins (upstream)

These are referenced by the upstream spec/reference but do not currently have direct C++ canonical built-in entrypoints in this repo:

- `thread.new_ref` / `thread.new-ref` (implemented: `cmcpp::canon_thread_new_ref`)
- `thread.new-indirect` (implemented: `cmcpp::canon_thread_new_indirect`)
- `thread.spawn-ref` (implemented: `cmcpp::canon_thread_spawn_ref`)
- `thread.spawn-indirect` (implemented: `cmcpp::canon_thread_spawn_indirect`)
- `thread.switch-to` (implemented: `cmcpp::canon_thread_switch_to`)
- `thread.available-parallelism` (implemented: `cmcpp::canon_thread_available_parallelism`)
