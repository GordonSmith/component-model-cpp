# GitHub Issue Backlog

Draft issues ready to be copy-pasted into GitHub. Each entry lists a suggested title, labels, background, concrete scope, and acceptance criteria aligned with the canonical ABI reference implementation.

---

## Issue: Implement canonical async runtime scaffolding
- **Labels:** `enhancement`, `async`
- **Status:** Done

### Background
The canonical ABI reference (`design/mvp/canonical-abi/definitions.py`) includes a `Store`, `Thread`, and scheduling loop that enable cooperative async execution via `tick()`. The current C++ headers lack any runtime to drive asynchronous component calls.

### Scope
- Add `Store`, `Thread`, `Task`, and related scheduling types to `cmcpp` mirroring the canonical semantics.
- Implement queueing of pending threads and `tick()` to resume ready work.
- Expose hooks for component functions to register `on_start`/`on_resolve` callbacks and support cancellation tokens on the returned `Call` object.
- Provide doctest (or equivalent) coverage that simulates async invocation and verifies correct scheduling behavior.

### Acceptance Criteria
1. New runtime types are available in the public API and documented.
2. Asynchronous component calls can progress via repeated `tick()` calls without blocking the host thread.
3. Unit tests demonstrate thread scheduling, `on_start` argument delivery, and cooperative cancellation.
4. Documentation explains how hosts drive async work.

---

## Issue: Complete resource handle lifecycle
- **Labels:** `enhancement`, `abi`
- **Status:** Done

### Background
`ResourceHandle`, `ResourceType`, and `ComponentInstance.resources` are currently empty shells. The canonical implementation tracks ownership, borrow counts, destructors, and exposes `canon resource.{new,drop,rep}`.

### Scope
- Flesh out `ResourceHandle` with `own`, `borrow_scope`, and `num_lends` semantics.
- Implement `ResourceType` destructor registration, including async callback support.
- Add `canon resource.new`, `canon resource.drop`, and `canon resource.rep` helper functions that update component tables and trap on misuse.
- Write tests covering own/borrow flows, lend counting, and destructor invocation.

### Acceptance Criteria
1. Resource creation traps if the table would overflow or if ownership rules are violated.
2. Dropping resources invokes registered destructors exactly once and respects async/sync constraints.
3. Borrowed handles track lend counts and trap on invalid drops or reps.
4. Tests mirror canonical success and trap cases.

---

## Issue: Add waitable, stream, and future infrastructure
- **Labels:** `enhancement`, `async`

### Background
Canonical ABI defines waitables, waitable-sets, buffers, streams, and futures plus their cancellation behaviors. Our headers only contain empty structs.
- **Status:** Done

### Scope
- Model waitable and waitable-set state, including registration with the component store.
- Implement buffer management for {stream,future} types, covering readable/writable halves.
- Provide APIs for `canon waitable-set.{new,wait,poll,drop}`, `canon waitable.join`, and `canon {stream,future}.{new,read,write,cancel,drop}`.
- Add tests verifying read/write ordering, cancellation pathways, and polling semantics.

### Acceptance Criteria
1. Streams and futures can be created, awaited, and canceled via the new APIs.
2. Waitable sets correctly block/unblock pending tasks and surface readiness in tests.
3. Cancellation propagates to queued operations per canonical rules.
4. Documentation describes how hosts integrate these constructs.

---

## Issue: Implement backpressure and task lifecycle management
- **Labels:** `enhancement`, `async`
- **Status:** Done

### Background
`ComponentInstance` holds flags for `may_leave`, `backpressure`, and call-state tracking but they are unused. Canonical ABI specifies `canon task.{return,cancel}`, `canon yield`, and backpressure counters governing concurrent entry.

### Scope
- Track outstanding synchronous/async calls and enforce `may_leave` invariants when entering/leaving component code.
- Implement `canon task.return` and `canon task.cancel` helpers wired to pending task queues.
- Support `canon yield` to hand control back to the embedder.
- Ensure backpressure counters gate `Store.invoke` while tasks are paused or pending.

### Acceptance Criteria
1. Re-entrant sync calls are rejected per canonical rules (tests cover both allowed and disallowed cases).
2. Tasks marked for cancellation resolve promptly with `on_resolve(None)`.
3. `canon yield` transitions tasks to pending and requires a subsequent `tick()` to resume.
4. Backpressure metrics are exposed for debugging and verified in tests.

---

## Issue: Support context locals and error-context APIs
- **Labels:** `enhancement`, `abi`
- **Status:** Done

### Background
`LiftLowerContext` currently omits instance references and borrow scopes, and `ContextLocalStorage`/`ErrorContext` types are unused. The canonical ABI exposes `canon context.{get,set}` and `canon error-context.{new,debug-message,drop}`.

### Scope
- Extend `LiftLowerContext` to hold the active `ComponentInstance` and scoped borrow variant.
- Implement context-local storage getters/setters with bounds validation.
- Provide error-context creation, debug-message formatting via the host converter, and drop semantics that respect async callbacks.
- Add tests ensuring invalid indices and double drops trap.

### Acceptance Criteria
1. Borrowed resources capture their scope and trap when accessed outside it.
2. Context locals persist across lift/lower calls and reset appropriately between tasks.
3. Error-context debug messages surface through the host trap mechanism.
4. Test coverage includes both success and failure paths for each API.

---

## Issue: Finish function flattening utilities
- **Labels:** `enhancement`, `abi`
- **Status:** Done

### Background
`include/cmcpp/func.hpp` contains commented-out flattening helpers. Canonical ABI requires flattening functions to honor `MAX_FLAT_PARAMS/RESULTS` and spill to heap memory via the provided `realloc`.

### Scope
- Implement `cmcpp::func::flatten`, `pack_flags_into_int`, and the associated load/store helpers.
- Respect max-flat thresholds and ensure out-params allocate via `LiftLowerContext::opts.realloc`.
- Add regression tests covering large tuples, records, and flag types that exceed the flat limit.
- Compare flattened signatures against outputs from the canonical Python definitions for validation.

### Acceptance Criteria
1. Flattened core signatures match canonical expectations for representative WIT signatures.
2. Heap-based lowering is invoked automatically when flat limits are exceeded.
3. Flags marshal correctly between bitsets and flat integers.
4. Tests demonstrate both flat and heap pathways.

---

## Issue: Wire canonical options and callbacks through lift/lower
- **Labels:** `enhancement`, `abi`

### Background
`CanonicalOptions` exposes `post_return`, `callback`, and `sync` but they are currently unused. Canonical ABI requires these flags to control async vs sync paths and post-return cleanup.

### Scope
- Invoke `post_return` after successful lowerings when provided.
- Enforce `sync` by trapping when async behavior would occur while `sync == true`.
- Invoke `callback` when async continuations schedule additional work.
- Ensure option fields propagate through `InstanceContext::createLiftLowerContext` and task lifecycles.

### Acceptance Criteria
1. `post_return` is called exactly once per lowering when configured (verified via tests).
2. Async lowering attempts while `sync == true` trap with a descriptive error.
3. Registered callbacks fire for asynchronous continuations and can trigger host-side scheduling.
4. Documentation clarifies option usage and interaction with new runtime pieces.

---

## Issue: Expand docs and tests for canonical runtime features
- **Labels:** `documentation`, `testing`
- **Status:** Done

### Background
New runtime pieces require supporting documentation and tests. Currently, README lacks guidance and test coverage mirrors only existing functionality.

### Scope
- Update `README.md` (or add a new guide) summarizing the async runtime, resource management, and waitable APIs.
- Add doctest/ICU-backed unit tests covering the new behavior to `test/main.cpp` (or adjacent files).
- Optionally add a Python cross-check using `ref/component-model/design/mvp/canonical-abi/run_tests.py` for parity.

### Acceptance Criteria
1. Documentation explains how to configure `InstanceContext`, allocate options, and drive async flows.
2. New tests pass in CI and cover at least one example for each newly implemented feature.
3. The backlog of canonical ABI features is reflected as "Done" within this issue once merged.
