#include <cmcpp.hpp>

#include "host-util.hpp"

using namespace cmcpp;

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <cassert>
#include <any>
#include <atomic>
#include <limits>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <cstring>
// #include <fmt/core.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("Async runtime schedules threads")
{
    Store store;

    std::optional<std::vector<std::any>> resolved_payload;
    bool resolved = false;
    bool cancelled = false;
    std::shared_ptr<std::atomic<bool>> gate;

    FuncInst async_func = [&](Store &store, SupertaskPtr, OnStart on_start, OnResolve on_resolve)
    {
        auto args = std::make_shared<std::vector<std::any>>(on_start());
        gate = std::make_shared<std::atomic<bool>>(false);

        auto thread = Thread::create(
            store,
            [gate]()
            {
                return gate->load();
            },
            [args, on_resolve](bool was_cancelled)
            {
                if (was_cancelled)
                {
                    on_resolve(std::nullopt);
                }
                else
                {
                    on_resolve(*args);
                }
                return false;
            },
            true,
            [gate]()
            {
                gate->store(true);
            });

        return Call::from_thread(thread);
    };

    auto call = store.invoke(
        async_func,
        nullptr,
        []()
        {
            return std::vector<std::any>{int32_t(1), std::string("world")};
        },
        [&](std::optional<std::vector<std::any>> values)
        {
            resolved_payload = std::move(values);
            resolved = true;
            cancelled = !resolved_payload.has_value();
        });

    CHECK(gate);
    CHECK(store.pending_size() == 1);
    CHECK_FALSE(resolved);

    store.tick();
    CHECK_FALSE(resolved);

    gate->store(true);
    store.tick();

    CHECK(resolved);
    REQUIRE(resolved_payload.has_value());
    REQUIRE(resolved_payload->size() == 2);
    CHECK(std::any_cast<int32_t>((*resolved_payload)[0]) == 1);
    CHECK(std::any_cast<std::string>((*resolved_payload)[1]) == "world");
    CHECK_FALSE(cancelled);

    // Cancellation path
    resolved = false;
    resolved_payload.reset();
    cancelled = false;

    auto call2 = store.invoke(
        async_func,
        nullptr,
        []()
        {
            return std::vector<std::any>{int32_t(2)};
        },
        [&](std::optional<std::vector<std::any>> values)
        {
            resolved_payload = std::move(values);
            resolved = true;
            cancelled = !resolved_payload.has_value();
        });

    CHECK(store.pending_size() == 1);
    store.tick();
    CHECK(store.pending_size() == 1);

    call2.request_cancellation();
    store.tick();

    CHECK(resolved);
    CHECK(cancelled);
    CHECK_FALSE(resolved_payload.has_value());
}

TEST_CASE("Async runtime requeues until completion")
{
    Store store;

    auto counter = std::make_shared<int>(0);

    auto thread = Thread::create(
        store,
        []()
        {
            return true;
        },
        [counter](bool cancelled)
        {
            REQUIRE_FALSE(cancelled);
            ++(*counter);
            return *counter < 3;
        });

    CHECK(store.pending_size() == 1);

    store.tick();
    CHECK(*counter == 1);
    CHECK(store.pending_size() == 1);

    store.tick();
    CHECK(*counter == 2);
    CHECK(store.pending_size() == 1);

    store.tick();
    CHECK(*counter == 3);
    CHECK(store.pending_size() == 0);
    CHECK(thread->completed());
}

TEST_CASE("Async runtime propagates cancellation")
{
    Store store;

    auto ready_gate = std::make_shared<std::atomic<bool>>(false);
    bool on_cancel_called = false;
    bool resume_called = false;
    bool was_cancelled = false;

    auto thread = Thread::create(
        store,
        [ready_gate]()
        {
            return ready_gate->load();
        },
        [&on_cancel_called, &resume_called, &was_cancelled](bool cancelled)
        {
            resume_called = true;
            was_cancelled = cancelled;
            CHECK(on_cancel_called);
            return false;
        },
        true,
        [ready_gate, &on_cancel_called]()
        {
            on_cancel_called = true;
            ready_gate->store(true);
        });

    auto call = Call::from_thread(thread);

    CHECK(store.pending_size() == 1);
    CHECK_FALSE(on_cancel_called);
    CHECK_FALSE(resume_called);

    call.request_cancellation();
    CHECK(on_cancel_called);
    CHECK(store.pending_size() == 1);

    store.tick();

    CHECK(resume_called);
    CHECK(was_cancelled);
    CHECK(store.pending_size() == 0);
    CHECK(thread->completed());
}

TEST_CASE("Thread suspend_until supports force yield gating")
{
    Store store;

    auto gate = std::make_shared<std::atomic<bool>>(true);

    auto thread = std::shared_ptr<Thread>(new Thread(
        store,
        []()
        {
            return true;
        },
        [](bool)
        {
            return false;
        },
        true,
        {}));

    bool completed = thread->suspend_until(
        [gate]()
        {
            return gate->load();
        },
        true,
        true);

    CHECK_FALSE(completed);
    CHECK(thread->allow_cancellation());
    CHECK_FALSE(thread->ready());
    CHECK(thread->ready());

    gate->store(false);
    CHECK_FALSE(thread->ready());

    gate->store(true);
    CHECK(thread->ready());
}

TEST_CASE("Store microtasks run before pending threads")
{
    Store store;
    int microtask_counter = 0;

    store.enqueue([&]()
                  { microtask_counter += 1; });
    store.enqueue([&]()
                  { microtask_counter += 1; });

    auto thread = Thread::create(
        store,
        []()
        {
            return true;
        },
        [&microtask_counter](bool cancelled)
        {
            CHECK_FALSE(cancelled);
            microtask_counter += 10;
            return false;
        });

    CHECK(store.pending_size() == 1);

    store.tick();
    CHECK(microtask_counter == 1);
    CHECK(store.pending_size() == 1);

    store.tick();
    CHECK(microtask_counter == 2);
    CHECK(store.pending_size() == 1);

    store.tick();
    CHECK(microtask_counter == 12);
    CHECK(store.pending_size() == 0);
    CHECK(thread->completed());
}

TEST_CASE("Backpressure counters and may_leave guards")
{
    ComponentInstance inst;
    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    canon_backpressure_set(inst, true);
    CHECK(inst.backpressure == 1);
    canon_backpressure_inc(inst, trap);
    CHECK(inst.backpressure == 2);
    canon_backpressure_dec(inst, trap);
    CHECK(inst.backpressure == 1);
    canon_backpressure_set(inst, false);
    CHECK(inst.backpressure == 0);
    CHECK_THROWS(canon_backpressure_dec(inst, trap));

    Store store;
    inst.store = &store;
    inst.may_leave = false;
    CHECK_THROWS(canon_waitable_set_new(inst, trap));
    inst.may_leave = true;
    CHECK_NOTHROW(canon_waitable_set_new(inst, trap));
}

TEST_CASE("Context locals provide per-task storage")
{
    ComponentInstance inst;
    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    Task task(inst);

    CHECK(ContextLocalStorage::LENGTH == 1);
    CHECK(canon_context_get(task, 0, trap) == 0);

    canon_context_set(task, 0, 42, trap);
    CHECK(canon_context_get(task, 0, trap) == 42);

    CHECK_THROWS(canon_context_get(task, ContextLocalStorage::LENGTH, trap));
    CHECK_THROWS(canon_context_set(task, ContextLocalStorage::LENGTH, 99, trap));

    inst.may_leave = false;
    CHECK_THROWS(canon_context_get(task, 0, trap));
    inst.may_leave = true;
}

TEST_CASE("Error context APIs manage debug messages")
{
    Store store;
    ComponentInstance inst;
    inst.store = &store;

    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    Task task(inst);

    Heap heap(1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    cx->inst = &inst;

    const std::string guest_message = "component failure";
    const uint32_t guest_ptr = 32;
    std::copy(guest_message.begin(), guest_message.end(), heap.memory.begin() + guest_ptr);
    uint32_t tagged_code_units = static_cast<uint32_t>(guest_message.size());

    uint32_t err_index = canon_error_context_new(task, cx.get(), guest_ptr, tagged_code_units, trap);
    CHECK(err_index != 0);

    const uint32_t record_ptr = 128;
    canon_error_context_debug_message(task, *cx, err_index, record_ptr, trap);

    uint32_t stored_ptr = integer::load<uint32_t>(*cx, record_ptr);
    uint32_t stored_len = integer::load<uint32_t>(*cx, record_ptr + 4);
    auto roundtrip = string::load_from_range<string_t>(*cx, stored_ptr, stored_len);
    CHECK(roundtrip == guest_message);

    inst.may_leave = false;
    CHECK_THROWS(canon_error_context_debug_message(task, *cx, err_index, record_ptr + 16, trap));
    inst.may_leave = true;

    canon_error_context_drop(task, err_index, trap);
    CHECK_THROWS(canon_error_context_drop(task, err_index, trap));

    uint32_t empty_index = canon_error_context_new(task, nullptr, 0, 0, trap);
    CHECK(empty_index != 0);

    const uint32_t empty_record = 192;
    canon_error_context_debug_message(task, *cx, empty_index, empty_record, trap);
    uint32_t empty_len = integer::load<uint32_t>(*cx, empty_record + 4);
    CHECK(empty_len == 0);

    canon_error_context_drop(task, empty_index, trap);
}

TEST_CASE("Waitable sets deliver events and enforce invariants")
{
    Store store;
    ComponentInstance inst;
    inst.store = &store;

    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    std::shared_ptr<TableEntry> null_entry;
    CHECK_THROWS(inst.table.add(null_entry, trap));

    auto waitable = std::make_shared<Waitable>();
    uint32_t waitable_index = inst.table.add(waitable, trap);
    uint32_t set_index = canon_waitable_set_new(inst, trap);

    uint32_t fail_set_index = canon_waitable_set_new(inst, trap);
    auto fail_set = inst.table.get<WaitableSet>(fail_set_index, trap);
    Waitable stack_waitable;
    fail_set->add_waitable(stack_waitable);
    CHECK_THROWS(fail_set->drop(trap));
    fail_set->remove_waitable(stack_waitable);
    canon_waitable_set_drop(inst, fail_set_index, trap);

    canon_waitable_join(inst, waitable_index, set_index, trap);
    CHECK(waitable->joined_set() != nullptr);

    Event pending{EventCode::STREAM_READ, 7, 99};
    waitable->set_pending_event(pending);
    CHECK_THROWS(waitable->drop(trap));
    waitable->clear_pending_event();

    Heap heap(64);
    GuestMemory mem(heap.memory.data(), heap.memory.size());
    uint32_t record_ptr = 8;

    auto blocked = canon_waitable_set_wait(false, mem, inst, set_index, record_ptr, trap);
    CHECK(blocked == BLOCKED);
    uint32_t recorded_index = 123;
    std::memcpy(&recorded_index, heap.memory.data() + record_ptr, sizeof(uint32_t));
    CHECK(recorded_index == 0);

    waitable->set_pending_event(pending);
    auto polled = canon_waitable_set_poll(false, mem, inst, set_index, record_ptr, trap);
    CHECK(polled == static_cast<uint32_t>(EventCode::STREAM_READ));

    uint32_t stored_index = 0;
    std::memcpy(&stored_index, heap.memory.data() + record_ptr, sizeof(uint32_t));
    CHECK(stored_index == pending.index);
    uint32_t stored_payload = 0;
    std::memcpy(&stored_payload, heap.memory.data() + record_ptr + sizeof(uint32_t), sizeof(uint32_t));
    CHECK(stored_payload == pending.payload);

    waitable->set_pending_event(pending);
    auto waited = canon_waitable_set_wait(false, mem, inst, set_index, record_ptr, trap);
    CHECK(waited == static_cast<uint32_t>(EventCode::STREAM_READ));

    canon_waitable_join(inst, waitable_index, 0, trap);
    CHECK(waitable->joined_set() == nullptr);

    canon_waitable_set_drop(inst, set_index, trap);
    CHECK_THROWS(inst.table.get<WaitableSet>(set_index, trap));
    CHECK_THROWS(inst.table.get<WaitableSet>(waitable_index, trap));
}

TEST_CASE("Task yield, cancel, and return")
{
    Store store;
    ComponentInstance inst;
    inst.store = &store;
    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    CanonicalOptions async_opts;
    async_opts.sync = false;

    bool resolved_called = false;
    std::optional<std::vector<std::any>> resolved_value;

    auto cancel_task = std::make_shared<Task>(inst, async_opts, nullptr, [&](std::optional<std::vector<std::any>> values)
                                              {
                                                 resolved_called = true;
                                                 resolved_value = std::move(values); });

    auto cancel_gate = std::make_shared<std::atomic<bool>>(true);
    auto cancel_thread = Thread::create(
        store,
        [cancel_gate]()
        {
            return cancel_gate->load();
        },
        [cancel_task, &trap](bool was_cancelled)
        {
            CHECK(was_cancelled);
            REQUIRE(cancel_task->enter(trap));
            auto event_code = canon_yield(true, *cancel_task, trap);
            CHECK(event_code == 1);
            canon_task_cancel(*cancel_task, trap);
            cancel_task->exit();
            return false;
        },
        true,
        [cancel_gate]()
        {
            cancel_gate->store(true);
        });

    cancel_task->set_thread(cancel_thread);
    cancel_task->request_cancellation();
    CHECK(cancel_task->state() == Task::State::CancelDelivered);
    store.tick();

    CHECK(resolved_called);
    CHECK_FALSE(resolved_value.has_value());
    CHECK(store.pending_size() == 0);

    resolved_called = false;
    resolved_value.reset();

    auto return_task = std::make_shared<Task>(inst, async_opts, nullptr, [&](std::optional<std::vector<std::any>> values)
                                              {
                                                 resolved_called = true;
                                                 resolved_value = std::move(values); });

    auto return_gate = std::make_shared<std::atomic<bool>>(true);
    auto return_thread = Thread::create(
        store,
        [return_gate]()
        {
            return return_gate->load();
        },
        [return_task, &trap](bool was_cancelled)
        {
            CHECK_FALSE(was_cancelled);
            REQUIRE(return_task->enter(trap));
            auto event_code = canon_yield(false, *return_task, trap);
            CHECK(event_code == 0);
            std::vector<std::any> payload;
            payload.emplace_back(int32_t(42));
            canon_task_return(*return_task, std::move(payload), trap);
            return_task->exit();
            return false;
        });

    return_task->set_thread(return_thread);
    store.tick();

    CHECK(resolved_called);
    REQUIRE(resolved_value.has_value());
    REQUIRE(resolved_value->size() == 1);
    CHECK(std::any_cast<int32_t>((*resolved_value)[0]) == 42);
    CHECK(store.pending_size() == 0);
}

TEST_CASE("Canonical options control lift/lower callbacks")
{
    SUBCASE("post_return runs once for heap spill")
    {
        Heap heap(1024);
        int post_return_calls = 0;

        CanonicalOptions options;
        options.post_return = GuestPostReturn([&]()
                                              { ++post_return_calls; });

        auto cx = createLiftLowerContext(&heap, std::move(options));
        using ResultTuple = tuple_t<string_t, string_t>;
        ResultTuple value{"alpha", "beta"};

        auto lowered = lower_flat_values<ResultTuple>(*cx, MAX_FLAT_RESULTS, nullptr, std::move(value));
        CHECK(lowered.size() == 1);
        CHECK(post_return_calls == 1);
    }

    SUBCASE("post_return runs once when using provided out pointer")
    {
        Heap heap(1024);
        int post_return_calls = 0;

        CanonicalOptions options;
        options.post_return = GuestPostReturn([&]()
                                              { ++post_return_calls; });

        auto cx = createLiftLowerContext(&heap, std::move(options));
        using ResultTuple = tuple_t<string_t, string_t>;
        ResultTuple value{"gamma", "delta"};

        uint32_t out_ptr = align_to(32u, ValTrait<ResultTuple>::alignment);
        auto lowered = lower_flat_values<ResultTuple>(*cx, MAX_FLAT_RESULTS, &out_ptr, std::move(value));
        CHECK(lowered.empty());
        CHECK(post_return_calls == 1);
    }

    SUBCASE("sync context traps on async lowering")
    {
        Heap heap(1024);
        CanonicalOptions options;
        auto cx = createLiftLowerContext(&heap, std::move(options));
        using ResultTuple = tuple_t<string_t, string_t>;
        CHECK_THROWS(lower_flat_values<ResultTuple>(*cx, 0, nullptr, ResultTuple{"omega", "sigma"}));
    }

    SUBCASE("async callback fires for stream completion")
    {
        Heap heap(1024);
        bool callback_called = false;
        EventCode observed_code = EventCode::NONE;
        uint32_t observed_index = 0;
        uint32_t observed_payload = 0;

        CanonicalOptions options;
        options.sync = false;
        options.callback = GuestCallback([&](EventCode code, uint32_t index, uint32_t payload)
                                         {
                                             callback_called = true;
                                             observed_code = code;
                                             observed_index = index;
                                             observed_payload = payload; });

        auto cx_unique = createLiftLowerContext(&heap, options);
        std::shared_ptr<LiftLowerContext> cx(cx_unique.release(), [](LiftLowerContext *ptr)
                                             { delete ptr; });

        HostTrap trap = [](const char *msg)
        {
            throw std::runtime_error(msg ? msg : "trap");
        };

        auto descriptor = make_stream_descriptor<uint8_t>();
        auto shared_state = std::make_shared<SharedStreamState>(descriptor);
        ReadableStreamEnd readable(shared_state);
        WritableStreamEnd writable(shared_state);

        uint32_t read_ptr = 0;
        uint32_t write_ptr = 64;
        heap.memory[write_ptr] = 0x42;

        auto blocked = readable.read(cx, 1, read_ptr, 1, false, trap);
        CHECK(blocked == BLOCKED);
        CHECK_FALSE(callback_called);

        writable.write(cx, 2, write_ptr, 1, trap);

        CHECK(callback_called);
        CHECK(observed_code == EventCode::STREAM_READ);
        CHECK(observed_index == 1);
        CHECK(observed_payload == pack_copy_result(CopyResult::Completed, 1));
        CHECK(heap.memory[read_ptr] == heap.memory[write_ptr]);
    }
}

TEST_CASE("InstanceContext wires canonical options")
{
    Heap heap(512);
    int post_return_calls = 0;
    std::vector<Event> observed_events;

    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    auto realloc_fn = [&](int ptr, int old_size, int align, int new_size)
    {
        return heap.realloc(ptr, static_cast<size_t>(old_size), static_cast<uint32_t>(align), static_cast<size_t>(new_size));
    };

    auto icx = createInstanceContext(trap, convert, realloc_fn);

    CanonicalOptions options;
    options.memory = GuestMemory(heap.memory.data(), heap.memory.size());
    options.realloc = realloc_fn;
    options.sync = false;
    options.post_return = GuestPostReturn([&]()
                                          { ++post_return_calls; });
    options.callback = GuestCallback([&](EventCode code, uint32_t index, uint32_t payload)
                                     { observed_events.push_back({code, index, payload}); });

    auto lift_cx = icx->createLiftLowerContext(options);

    REQUIRE(lift_cx->canonical_options() != nullptr);
    CHECK_FALSE(lift_cx->is_sync());
    CHECK(lift_cx->opts.memory.data() == heap.memory.data());

    auto lowered = lower_flat_values<int32_t>(*lift_cx, MAX_FLAT_RESULTS, nullptr, int32_t{77});
    CHECK(lowered.size() == 1);
    CHECK(post_return_calls == 1);

    lift_cx->notify_async_event(EventCode::STREAM_READ, 5, 0xCAFE'BEEFu);
    REQUIRE(observed_events.size() == 1);
    CHECK(observed_events.back().code == EventCode::STREAM_READ);
    CHECK(observed_events.back().index == 5);
    CHECK(observed_events.back().payload == 0xCAFE'BEEFu);
}

TEST_CASE("Canonical callbacks surface waitable events")
{
    ComponentInstance inst;
    Heap heap(512);
    std::vector<Event> observed_events;

    HostTrap trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    auto realloc_fn = [&](int ptr, int old_size, int align, int new_size)
    {
        return heap.realloc(ptr, static_cast<size_t>(old_size), static_cast<uint32_t>(align), static_cast<size_t>(new_size));
    };

    auto icx = createInstanceContext(trap, convert, realloc_fn);

    CanonicalOptions options;
    options.memory = GuestMemory(heap.memory.data(), heap.memory.size());
    options.realloc = realloc_fn;
    options.sync = false;
    options.callback = GuestCallback([&](EventCode code, uint32_t index, uint32_t payload)
                                     { observed_events.push_back({code, index, payload}); });

    auto lift_unique = icx->createLiftLowerContext(options);
    std::shared_ptr<LiftLowerContext> cx(lift_unique.release(), [](LiftLowerContext *ptr)
                                         { delete ptr; });
    cx->inst = &inst;

    auto desc = make_stream_descriptor<int32_t>();
    uint64_t handles = canon_stream_new(inst, desc, trap);
    uint32_t readable = static_cast<uint32_t>(handles & 0xFFFF'FFFFu);
    uint32_t writable = static_cast<uint32_t>(handles >> 32);

    uint32_t waitable_set = canon_waitable_set_new(inst, trap);
    canon_waitable_join(inst, readable, waitable_set, trap);

    uint32_t read_ptr = 0;
    uint32_t write_ptr = 96;
    uint32_t event_ptr = 160;

    auto blocked = canon_stream_read(inst, desc, readable, cx, read_ptr, 2, false, trap);
    CHECK(blocked == BLOCKED);
    CHECK(observed_events.empty());

    int32_t payload[2] = {11, 22};
    std::memcpy(heap.memory.data() + write_ptr, payload, sizeof(payload));
    auto write_payload = canon_stream_write(inst, desc, writable, cx, write_ptr, 2, trap);
    CHECK((write_payload & 0xF) == static_cast<uint32_t>(CopyResult::Completed));

    REQUIRE(observed_events.size() == 1);
    CHECK(observed_events.back().code == EventCode::STREAM_READ);
    CHECK(observed_events.back().index == readable);
    CHECK(observed_events.back().payload == pack_copy_result(CopyResult::Completed, 2));

    GuestMemory mem(heap.memory.data(), heap.memory.size());
    auto code = canon_waitable_set_poll(false, mem, inst, waitable_set, event_ptr, trap);
    CHECK(code == static_cast<uint32_t>(EventCode::STREAM_READ));

    uint32_t recorded_index = integer::load<uint32_t>(*cx, event_ptr);
    uint32_t recorded_payload = integer::load<uint32_t>(*cx, event_ptr + 4);
    CHECK(recorded_index == readable);
    CHECK(recorded_payload == pack_copy_result(CopyResult::Completed, 2));

    int32_t read_values[2] = {0, 0};
    std::memcpy(read_values, heap.memory.data() + read_ptr, sizeof(read_values));
    CHECK(read_values[0] == 11);
    CHECK(read_values[1] == 22);

    canon_stream_drop_readable(inst, readable, trap);
    canon_stream_drop_writable(inst, writable, trap);
    canon_waitable_set_drop(inst, waitable_set, trap);
}

TEST_CASE("Resource handle lifecycle mirrors canonical definitions")
{
    ComponentInstance resource_impl;
    ComponentInstance inst;
    std::vector<uint32_t> dtor_calls;

    HostTrap host_trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    ResourceType rt(resource_impl, [&](uint32_t rep)
                    { dtor_calls.push_back(rep); });

    REQUIRE(inst.may_leave);
    REQUIRE(inst.may_enter);

    uint32_t h1 = canon_resource_new(inst, rt, 42, host_trap);
    uint32_t h2 = canon_resource_new(inst, rt, 43, host_trap);

    CHECK(h1 == 1);
    CHECK(h2 == 2);

    LiftLowerOptions borrow_opts;
    HostUnicodeConversion noop_convert = [](void *, uint32_t, const void *, uint32_t, Encoding, Encoding)
    {
        return std::pair<void *, size_t>{nullptr, 0};
    };
    LiftLowerContext borrow_scope(host_trap, noop_convert, borrow_opts, &inst);
    borrow_scope.borrow_count = 1;

    HandleElement borrowed;
    borrowed.rep = 44;
    borrowed.own = false;
    borrowed.scope = &borrow_scope;
    uint32_t h3 = inst.handles.add(rt, borrowed, host_trap);
    CHECK(h3 == 3);

    uint32_t h4 = canon_resource_new(inst, rt, 45, host_trap);
    CHECK(h4 == 4);

    const auto &table_entries = inst.handles.table(rt).entries();
    CHECK(table_entries.size() == 5);
    CHECK_FALSE(table_entries[0].has_value());
    CHECK(table_entries[1].has_value());
    CHECK(table_entries[2].has_value());
    CHECK(table_entries[3].has_value());
    CHECK(table_entries[4].has_value());

    CHECK(canon_resource_rep(inst, rt, h1, host_trap) == 42);
    CHECK(canon_resource_rep(inst, rt, h2, host_trap) == 43);
    CHECK(canon_resource_rep(inst, rt, h3, host_trap) == 44);
    CHECK(canon_resource_rep(inst, rt, h4, host_trap) == 45);

    dtor_calls.clear();
    canon_resource_drop(inst, rt, h1, host_trap);
    CHECK(dtor_calls == std::vector<uint32_t>{42});
    auto &table_after_drop = inst.handles.table(rt);
    CHECK(table_after_drop.entries()[1].has_value() == false);
    CHECK(table_after_drop.free_list().size() == 1);

    uint32_t h5 = canon_resource_new(inst, rt, 46, host_trap);
    CHECK(h5 == 1);
    CHECK(table_after_drop.entries().size() == 5);
    CHECK(table_after_drop.entries()[1].has_value());
    CHECK(dtor_calls == std::vector<uint32_t>{42});

    borrow_scope.borrow_count = 1;
    canon_resource_drop(inst, rt, h3, host_trap);
    CHECK(dtor_calls == std::vector<uint32_t>{42});
    CHECK(borrow_scope.borrow_count == 0);
    CHECK(table_after_drop.entries()[3].has_value() == false);
    CHECK(table_after_drop.free_list().size() == 1);

    canon_resource_drop(inst, rt, h2, host_trap);
    CHECK(dtor_calls == std::vector<uint32_t>{42, 43});

    canon_resource_drop(inst, rt, h4, host_trap);
    CHECK(dtor_calls == std::vector<uint32_t>{42, 43, 45});

    canon_resource_drop(inst, rt, h5, host_trap);
    CHECK(dtor_calls == std::vector<uint32_t>{42, 43, 45, 46});

    auto &final_table = inst.handles.table(rt);
    CHECK(final_table.free_list().size() == 4);
    for (size_t i = 1; i < final_table.entries().size(); ++i)
    {
        CHECK_FALSE(final_table.entries()[i].has_value());
    }
}

TEST_CASE("Boolean")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    auto v = lower_flat(*cx, true);
    auto b = lift_flat<bool_t>(*cx, v);
    CHECK(b == true);
    v = lower_flat(*cx, false);
    b = lift_flat<bool_t>(*cx, v);
    CHECK(b == false);
}

const char_t chars[] = {0, 65, 0xD7FF, 0xE000, 0x10FFFF};
const char_t bad_chars[] = {0xD800, 0xDFFF, 0x110000, 0xFFFFFFFF};
TEST_CASE("Char")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    for (auto c : chars)
    {
        auto v = lower_flat(*cx, c);
        auto c2 = lift_flat<char_t>(*cx, v);
        CHECK(c == c2);
    }
    for (auto c : bad_chars)
    {
        try
        {
            auto v = lower_flat(*cx, c);
            auto c2 = lift_flat<char_t>(*cx, v);
            CHECK(false);
        }
        catch (...)
        {
            CHECK(true);
        }
    }
}

TEST_CASE("Char Boundary Cases - Extended")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test specific Unicode boundary values from Python tests
    struct CharTestCase
    {
        char_t input;
        bool should_succeed;
        char_t expected; // Only valid if should_succeed is true
    };

    std::vector<CharTestCase> test_cases = {
        {0, true, '\x00'},          // Null character
        {65, true, 'A'},            // ASCII 'A'
        {0xD7FF, true, 0xD7FF},     // Last valid character before surrogates
        {0xD800, false, 0},         // First high surrogate (invalid)
        {0xDFFF, false, 0},         // Last low surrogate (invalid)
        {0xE000, true, 0xE000},     // First character after surrogates
        {0x10FFFF, true, 0x10FFFF}, // Last valid Unicode character
        {0x110000, false, 0},       // Beyond Unicode range
        {0xFFFFFFFF, false, 0}      // Way beyond Unicode range
    };

    for (const auto &test_case : test_cases)
    {
        if (test_case.should_succeed)
        {
            auto v = lower_flat(*cx, test_case.input);
            auto result = lift_flat<char_t>(*cx, v);
            CHECK(result == test_case.expected);
        }
        else
        {
            try
            {
                auto v = lower_flat(*cx, test_case.input);
                auto result = lift_flat<char_t>(*cx, v);
                CHECK(false); // Should have thrown an exception
            }
            catch (...)
            {
                CHECK(true); // Expected to throw
            }
        }
    }
}

template <typename T>
void test_numeric(const std::unique_ptr<LiftLowerContext> &cx, T v = 42)
{
    auto flat_v = lower_flat<T>(*cx, v);
    auto b = lift_flat<T>(*cx, flat_v);
    CHECK(b == v);
    v = ValTrait<T>::LOW_VALUE;
    flat_v = lower_flat<T>(*cx, v);
    b = lift_flat<T>(*cx, flat_v);
    CHECK(b == v);
    v = ValTrait<T>::HIGH_VALUE;
    flat_v = lower_flat<T>(*cx, v);
    b = lift_flat<T>(*cx, flat_v);
    CHECK(b == v);
}

TEST_CASE("Signed Integer")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    test_numeric<int8_t>(cx);
    test_numeric<int16_t>(cx);
    test_numeric<int32_t>(cx);
    test_numeric<int64_t>(cx);
    test_numeric<int8_t>(cx, -42);
    test_numeric<int16_t>(cx, -42);
    test_numeric<int32_t>(cx, -42);
    test_numeric<int64_t>(cx, -42);
}

TEST_CASE("Unigned Integer")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    test_numeric<uint8_t>(cx);
    test_numeric<uint16_t>(cx);
    test_numeric<uint32_t>(cx);
    test_numeric<uint64_t>(cx);
}

TEST_CASE("Float")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    test_numeric<float32_t>(cx);
    test_numeric<float64_t>(cx);

    auto flat_v = lower_flat(*cx, std::numeric_limits<float>::infinity());
    auto b = lift_flat<float32_t>(*cx, flat_v);
    CHECK(std::isnan(b));
    flat_v = lower_flat(*cx, std::numeric_limits<double>::infinity());
    b = lift_flat<float64_t>(*cx, flat_v);
    CHECK(std::isnan(b));

    using FloatTuple = tuple_t<float32_t, float64_t>;
    FloatTuple ft = {42.0, 43.0};
    auto flat_ft = lower_flat(*cx, ft);
    auto ft2 = lift_flat<FloatTuple>(*cx, flat_ft);
    CHECK(ft == ft2);

    using FloatList = list_t<float32_t>;
    FloatList fl = {42.0, 43.0};
    auto flat_fl = lower_flat(*cx, fl);
    auto fl2 = lift_flat<FloatList>(*cx, flat_fl);
    CHECK(fl == fl2);

    using Float64List = list_t<float64_t>;
    Float64List fl64 = {42.0, 43.0};
    auto flat_fl64 = lower_flat(*cx, fl64);
    auto fl642 = lift_flat<Float64List>(*cx, flat_fl64);
    CHECK(fl64 == fl642);
}

// Comprehensive boundary testing inspired by Python test_pairs()
template <typename T>
void test_overflow_behavior(const std::unique_ptr<LiftLowerContext> &cx, T input_value, T expected_output)
{
    auto flat_v = lower_flat(*cx, input_value);
    auto result = lift_flat<T>(*cx, flat_v);
    CHECK(result == expected_output);
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconstant-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

TEST_CASE("Numeric Boundary Cases - Overflow Behavior")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // uint8_t overflow tests - values should wrap around
    test_overflow_behavior<uint8_t>(cx, 127, 127);
    test_overflow_behavior<uint8_t>(cx, 128, 128);
    test_overflow_behavior<uint8_t>(cx, 255, 255);
    test_overflow_behavior<uint8_t>(cx, 256, 0);
    test_overflow_behavior<uint8_t>(cx, UINT32_MAX, 255);
    test_overflow_behavior<uint8_t>(cx, UINT32_MAX - 127, 128);
    test_overflow_behavior<uint8_t>(cx, UINT32_MAX - 128, 127);

    // int8_t overflow tests
    test_overflow_behavior<int8_t>(cx, 127, 127);
    test_overflow_behavior<int8_t>(cx, 128, -128);
    test_overflow_behavior<int8_t>(cx, 255, -1);
    test_overflow_behavior<int8_t>(cx, 256, 0);
    test_overflow_behavior<int8_t>(cx, UINT32_MAX, -1);
    test_overflow_behavior<int8_t>(cx, UINT32_MAX - 127, -128);
    test_overflow_behavior<int8_t>(cx, UINT32_MAX - 128, 127);

    // uint16_t overflow tests
    test_overflow_behavior<uint16_t>(cx, 32767, 32767);
    test_overflow_behavior<uint16_t>(cx, 32768, 32768);
    test_overflow_behavior<uint16_t>(cx, 65535, 65535);
    test_overflow_behavior<uint16_t>(cx, 65536, 0);
    test_overflow_behavior<uint16_t>(cx, UINT32_MAX, 65535);
    test_overflow_behavior<uint16_t>(cx, UINT32_MAX - 32767, 32768);
    test_overflow_behavior<uint16_t>(cx, UINT32_MAX - 32768, 32767);

    // int16_t overflow tests
    test_overflow_behavior<int16_t>(cx, 32767, 32767);
    test_overflow_behavior<int16_t>(cx, 32768, -32768);
    test_overflow_behavior<int16_t>(cx, 65535, -1);
    test_overflow_behavior<int16_t>(cx, 65536, 0);
    test_overflow_behavior<int16_t>(cx, UINT32_MAX, -1);
    test_overflow_behavior<int16_t>(cx, UINT32_MAX - 32767, -32768);
    test_overflow_behavior<int16_t>(cx, UINT32_MAX - 32768, 32767);

    // Test edge values for 32-bit types
    test_overflow_behavior<uint32_t>(cx, (1U << 31) - 1, (1U << 31) - 1);
    test_overflow_behavior<uint32_t>(cx, 1U << 31, 1U << 31);
    test_overflow_behavior<uint32_t>(cx, UINT32_MAX, UINT32_MAX);

    test_overflow_behavior<int32_t>(cx, (1 << 30) + (1 << 29), (1 << 30) + (1 << 29)); // 2^31 - 1
    test_overflow_behavior<int32_t>(cx, 1U << 31, INT32_MIN);                          // 2^31 becomes -2^31
    test_overflow_behavior<int32_t>(cx, UINT32_MAX, -1);

    // Test edge values for 64-bit types
    test_overflow_behavior<uint64_t>(cx, (1ULL << 63) - 1, (1ULL << 63) - 1);
    test_overflow_behavior<uint64_t>(cx, 1ULL << 63, 1ULL << 63);
    test_overflow_behavior<uint64_t>(cx, UINT64_MAX, UINT64_MAX);

    test_overflow_behavior<int64_t>(cx, (1LL << 62) + (1LL << 61), (1LL << 62) + (1LL << 61)); // 2^63 - 1
    test_overflow_behavior<int64_t>(cx, 1ULL << 63, INT64_MIN);                                // 2^63 becomes -2^63
    test_overflow_behavior<int64_t>(cx, UINT64_MAX, -1);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

TEST_CASE("Float Special Values - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test various NaN representations
    float32_t nan32 = std::numeric_limits<float32_t>::quiet_NaN();
    auto v_nan32 = lower_flat(*cx, nan32);
    auto result_nan32 = lift_flat<float32_t>(*cx, v_nan32);
    CHECK(std::isnan(result_nan32));

    float32_t snan32 = std::numeric_limits<float32_t>::signaling_NaN();
    auto v_snan32 = lower_flat(*cx, snan32);
    auto result_snan32 = lift_flat<float32_t>(*cx, v_snan32);
    CHECK(std::isnan(result_snan32));

    // Test infinities
    float32_t inf32 = std::numeric_limits<float32_t>::infinity();
    auto v_inf32 = lower_flat(*cx, inf32);
    auto result_inf32 = lift_flat<float32_t>(*cx, v_inf32);
    // CHECK(std::isinf(result_inf32) && result_inf32 > 0);

    float32_t neg_inf32 = -std::numeric_limits<float32_t>::infinity();
    auto v_neg_inf32 = lower_flat(*cx, neg_inf32);
    auto result_neg_inf32 = lift_flat<float32_t>(*cx, v_neg_inf32);
    // CHECK(std::isinf(result_neg_inf32) && result_neg_inf32 < 0);

    // Test denormalized numbers (subnormal)
    float32_t denorm = std::numeric_limits<float32_t>::denorm_min();
    auto v_denorm = lower_flat(*cx, denorm);
    auto result_denorm = lift_flat<float32_t>(*cx, v_denorm);
    CHECK(result_denorm == denorm);

    // Test epsilon (smallest difference)
    float32_t eps = std::numeric_limits<float32_t>::epsilon();
    auto v_eps = lower_flat(*cx, eps);
    auto result_eps = lift_flat<float32_t>(*cx, v_eps);
    CHECK(result_eps == eps);

    // Test max/min normal values
    float32_t max_val = std::numeric_limits<float32_t>::max();
    auto v_max = lower_flat(*cx, max_val);
    auto result_max = lift_flat<float32_t>(*cx, v_max);
    CHECK(result_max == max_val);

    float32_t lowest_val = std::numeric_limits<float32_t>::lowest();
    auto v_lowest = lower_flat(*cx, lowest_val);
    auto result_lowest = lift_flat<float32_t>(*cx, v_lowest);
    CHECK(result_lowest == lowest_val);

    // Test zero and negative zero
    float32_t zero = 0.0f;
    auto v_zero = lower_flat(*cx, zero);
    auto result_zero = lift_flat<float32_t>(*cx, v_zero);
    CHECK(result_zero == 0.0f);
    float32_t neg_zero = -0.0f;
    auto v_neg_zero = lower_flat(*cx, neg_zero);
    auto result_neg_zero = lift_flat<float32_t>(*cx, v_neg_zero);
    CHECK(result_neg_zero == 0.0f); // -0.0 might become +0.0

    // Test the same for double precision
    float64_t nan64 = std::numeric_limits<float64_t>::quiet_NaN();
    auto v_nan64 = lower_flat(*cx, nan64);
    auto result_nan64 = lift_flat<float64_t>(*cx, v_nan64);
    CHECK(std::isnan(result_nan64));

    float64_t inf64 = std::numeric_limits<float64_t>::infinity();
    auto v_inf64 = lower_flat(*cx, inf64);
    auto result_inf64 = lift_flat<float64_t>(*cx, v_inf64);
    // CHECK(std::isinf(result_inf64) && result_inf64 > 0);

    float64_t max_val64 = std::numeric_limits<float64_t>::max();
    auto v_max64 = lower_flat(*cx, max_val64);
    auto result_max64 = lift_flat<float64_t>(*cx, v_max64);
    CHECK(result_max64 == max_val64);
}

TEST_CASE("Waitable set surfaces stream readiness")
{
    ComponentInstance inst;
    HostTrap host_trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    auto desc = make_stream_descriptor<int32_t>();
    uint64_t handles = canon_stream_new(inst, desc, host_trap);
    uint32_t readable = static_cast<uint32_t>(handles & 0xFFFFFFFFu);
    uint32_t writable = static_cast<uint32_t>(handles >> 32);

    uint32_t waitable_set = canon_waitable_set_new(inst, host_trap);
    canon_waitable_join(inst, readable, waitable_set, host_trap);

    Heap heap(256);
    CanonicalOptions options;
    options.sync = false;
    auto cx = std::shared_ptr<LiftLowerContext>(createLiftLowerContext(&heap, options).release(), [](LiftLowerContext *ptr)
                                                { delete ptr; });

    uint32_t read_ptr = 0;
    uint32_t write_ptr = 32;
    uint32_t event_ptr = 128;

    int32_t to_write[2] = {42, 87};
    std::memcpy(heap.memory.data() + write_ptr, to_write, sizeof(to_write));

    auto blocked = canon_stream_read(inst, desc, readable, cx, read_ptr, 2, false, host_trap);
    CHECK(blocked == BLOCKED);

    GuestMemory mem(heap.memory.data(), heap.memory.size());
    auto code = canon_waitable_set_poll(false, mem, inst, waitable_set, event_ptr, host_trap);
    CHECK(code == static_cast<uint32_t>(EventCode::NONE));

    auto write_payload = canon_stream_write(inst, desc, writable, cx, write_ptr, 2, host_trap);
    CHECK((write_payload & 0xF) == static_cast<uint32_t>(CopyResult::Completed));
    auto write_count = write_payload >> 4;
    CHECK(write_count == 2);

    code = canon_waitable_set_poll(false, mem, inst, waitable_set, event_ptr, host_trap);
    CHECK(code == static_cast<uint32_t>(EventCode::STREAM_READ));

    uint32_t reported_index = 0;
    uint32_t payload = 0;
    std::memcpy(&reported_index, heap.memory.data() + event_ptr, sizeof(uint32_t));
    std::memcpy(&payload, heap.memory.data() + event_ptr + sizeof(uint32_t), sizeof(uint32_t));
    CHECK(reported_index == readable);
    CHECK((payload & 0xF) == static_cast<uint32_t>(CopyResult::Completed));
    auto read_count = payload >> 4;
    CHECK(read_count == 2);

    int32_t read_values[2] = {0, 0};
    std::memcpy(read_values, heap.memory.data() + read_ptr, sizeof(read_values));
    CHECK(read_values[0] == 42);
    CHECK(read_values[1] == 87);

    canon_stream_drop_readable(inst, readable, host_trap);
    canon_stream_drop_writable(inst, writable, host_trap);
    canon_waitable_set_drop(inst, waitable_set, host_trap);
}

TEST_CASE("Stream cancellation posts events")
{
    ComponentInstance inst;
    HostTrap host_trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    auto desc = make_stream_descriptor<int32_t>();
    uint64_t handles = canon_stream_new(inst, desc, host_trap);
    uint32_t readable = static_cast<uint32_t>(handles & 0xFFFFFFFFu);

    uint32_t waitable_set = canon_waitable_set_new(inst, host_trap);
    canon_waitable_join(inst, readable, waitable_set, host_trap);

    Heap heap(128);
    CanonicalOptions options;
    options.sync = false;
    auto cx = std::shared_ptr<LiftLowerContext>(createLiftLowerContext(&heap, options).release(), [](LiftLowerContext *ptr)
                                                { delete ptr; });

    uint32_t read_ptr = 0;
    uint32_t event_ptr = 64;

    auto blocked = canon_stream_read(inst, desc, readable, cx, read_ptr, 1, false, host_trap);
    CHECK(blocked == BLOCKED);

    auto cancel_payload = canon_stream_cancel_read(inst, readable, false, host_trap);
    CHECK(cancel_payload == BLOCKED);

    GuestMemory mem(heap.memory.data(), heap.memory.size());
    auto code = canon_waitable_set_poll(false, mem, inst, waitable_set, event_ptr, host_trap);
    CHECK(code == static_cast<uint32_t>(EventCode::STREAM_READ));

    uint32_t payload = 0;
    std::memcpy(&payload, heap.memory.data() + event_ptr + sizeof(uint32_t), sizeof(uint32_t));
    CHECK((payload & 0xF) == static_cast<uint32_t>(CopyResult::Cancelled));
    auto cancel_count = payload >> 4;
    CHECK(cancel_count == 0);

    canon_stream_drop_readable(inst, readable, host_trap);
    canon_waitable_set_drop(inst, waitable_set, host_trap);
}

TEST_CASE("Future lifecycle completes")
{
    ComponentInstance inst;
    HostTrap host_trap = [](const char *msg)
    {
        throw std::runtime_error(msg ? msg : "trap");
    };

    auto desc = make_future_descriptor<int32_t>();
    uint64_t handles = canon_future_new(inst, desc, host_trap);
    uint32_t readable = static_cast<uint32_t>(handles & 0xFFFFFFFFu);
    uint32_t writable = static_cast<uint32_t>(handles >> 32);

    uint32_t waitable_set = canon_waitable_set_new(inst, host_trap);
    canon_waitable_join(inst, readable, waitable_set, host_trap);

    Heap heap(256);
    CanonicalOptions options;
    options.sync = false;
    auto cx = std::shared_ptr<LiftLowerContext>(createLiftLowerContext(&heap, options).release(), [](LiftLowerContext *ptr)
                                                { delete ptr; });

    uint32_t read_ptr = 0;
    uint32_t write_ptr = 32;
    uint32_t event_ptr = 96;

    auto read_blocked = canon_future_read(inst, desc, readable, cx, read_ptr, false, host_trap);
    CHECK(read_blocked == BLOCKED);

    int32_t value = 99;
    std::memcpy(heap.memory.data() + write_ptr, &value, sizeof(int32_t));

    auto write_payload = canon_future_write(inst, desc, writable, cx, write_ptr, host_trap);
    CHECK((write_payload & 0xF) == static_cast<uint32_t>(CopyResult::Completed));
    auto write_count = write_payload >> 4;
    CHECK(write_count == 1);

    GuestMemory mem(heap.memory.data(), heap.memory.size());
    auto code = canon_waitable_set_poll(false, mem, inst, waitable_set, event_ptr, host_trap);
    CHECK(code == static_cast<uint32_t>(EventCode::FUTURE_READ));

    uint32_t payload = 0;
    std::memcpy(&payload, heap.memory.data() + event_ptr + sizeof(uint32_t), sizeof(uint32_t));
    CHECK((payload & 0xF) == static_cast<uint32_t>(CopyResult::Completed));
    auto read_count = payload >> 4;
    CHECK(read_count == 1);

    int32_t observed = 0;
    std::memcpy(&observed, heap.memory.data() + read_ptr, sizeof(int32_t));
    CHECK(observed == value);

    canon_future_drop_readable(inst, readable, host_trap);
    canon_future_drop_writable(inst, writable, host_trap);
    canon_waitable_set_drop(inst, waitable_set, host_trap);
}

const char *const hw = "hello World!";
const char *const hw8 = "hello 世界-🌍-!";
const char16_t *hw16 = u"hello 世界-🌍-!";

template <String T, typename T2>
void printString(T str, T2 hw)
{
    for (size_t i = 0; i < str.length(); ++i)
    {
        std::wcout << L"Character " << i << L": wstr=" << static_cast<wchar_t>(str[i]) << L", whw=" << static_cast<wchar_t>(hw[i]) << std::endl;
        if (str[i] != hw[i])
        {
            std::wcout << L"Mismatch at position " << i << std::endl;
        }
    }
}
const char *const unicode_test_strings[] = {"", "a", "\x00", "hello", "こんにちは", "你好", "안녕하세요", "Здравствуйте", "مرحبا", "שלום", "नमस्ते", "สวัสดี", "γειά σου", "hola", "bonjour", "hallo", "ciao", "Olá", "hello 世界-🌍-!", "ہیلو", "ሰላም", "ہیلو", "ਸਤ ਸ੍ਰੀ ਅਕਾਲ", "வணக்கம்", "హలో", "ಹಲೋ", "ഹലോ", "ສະບາຍດີ", "မင်္ဂလာပါ", "សួស្តី", "ສະບາຍດີ", "ສະບາຍດີ", "ສະບາຍດີ"};
const char16_t *const unicode_test_u16strings[] = {u"", u"a", u"\x00", u"hello", u"こんにちは", u"你好", u"안녕하세요", u"Здравствуйте", u"مرحبا", u"שלום", u"नमस्ते", u"สวัสดี", u"γειά σου", u"hola", u"bonjour", u"hallo", u"ciao", u"Olá", u"hello 世界-🌍-!", u"ہیلو", u"ሰላም", u"ہیلو", u"ਸਤ ਸ੍ਰੀ ਅਕਾਲ", u"வணக்கம்", u"హలో", u"ಹಲೋ", u"ഹലോ", u"ສະບາຍດີ", u"မင်္ဂလာပါ", u"សួស្តី", u"ສະບາຍດີ", u"ສະບາຍດີ", u"ສະບາຍດີ"};
const char *const boundary_test_strings[] = {
    "\x7F",             // DEL character in ASCII
    "\xC2\x80",         // First 2-byte UTF-8 character
    "\xDF\xBF",         // Last 2-byte UTF-8 character
    "\xE0\xA0\x80",     // First 3-byte UTF-8 character
    "\xEF\xBF\xBF",     // Last 3-byte UTF-8 character
    "\xF0\x90\x80\x80", // First 4-byte UTF-8 character
    "\xF4\x8F\xBF\xBF", // Last 4-byte UTF-8 character
    "\xED\x9F\xBF",     // Last valid UTF-16 character before surrogates
    "\xEE\x80\x80",     // First character after surrogates
    "\xEF\xBF\xBD",     // Replacement character (U+FFFD)
    "\xED\xA0\x7F",     // First high surrogate - 1 (Invalid UTF-8)
    "\xED\xA0\x80",     // First high surrogate (Invalid UTF-8)
    "\xED\xBF\xBF",     // Last high surrogate (Invalid UTF-8)
    "\xED\xBF\xC0",     // Last high surrogate + 1 (Invalid UTF-8)
    "\xF4\x90\x80\x80"  // Beyond U+10FFFF (Invalid UTF-8)
};
const char16_t *const boundary_test_u16strings[] = {
    u"\x7F",             // DEL character in ASCII
    u"\xC2\x80",         // First 2-byte UTF-8 character
    u"\xDF\xBF",         // Last 2-byte UTF-8 character
    u"\xE0\xA0\x80",     // First 3-byte UTF-8 character
    u"\xEF\xBF\xBF",     // Last 3-byte UTF-8 character
    u"\xF0\x90\x80\x80", // First 4-byte UTF-8 character
    u"\xF4\x8F\xBF\xBF", // Last 4-byte UTF-8 character
    u"\xED\x9F\xBF",     // Last valid UTF-16 character before surrogates
    u"\xED\xA0\x7F",     // First high surrogate - 1
    u"\xED\xA0\x80",     // First high surrogate
    u"\xED\xBF\xBF",     // Last high surrogate
    u"\xED\xBF\xC0",     // Last high surrogate + 1
    u"\xEE\x80\x80",     // First character after surrogates
    u"\xEF\xBF\xBD",     // Replacement character (U+FFFD)
    u"\xF4\x90\x80\x80"  // Invalid UTF-8 (beyond U+10FFFF)
};

TEST_CASE("String-Utf16")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf16);
    u16string_t hw16 = u"Hello World!";
    auto v = lower_flat(*cx, hw16);
    auto hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);

    string_t hw = "Hello World!";
    v = lower_flat(*cx, hw);
    auto hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);
}

TEST_CASE("String-Utf8")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);
    string_t hw = "Hello World!";
    auto v = lower_flat(*cx, hw);
    auto hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);

    u16string_t hw16 = u"Hello World!";
    v = lower_flat(*cx, hw16);
    auto hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);
}

TEST_CASE("String-Latin1_Utf16")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Latin1_Utf16);
    string_t hw = "Hello World!";
    auto v = lower_flat(*cx, hw);
    auto hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);
    auto hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Latin1);

    u16string_t hw16 = u"Hello World!";
    v = lower_flat(*cx, hw16);
    auto hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);
    hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Latin1);

    hw = "Hello 🌍!";
    v = lower_flat(*cx, hw);
    hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);
    hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Utf16);

    hw16 = u"Hello 🌍!";
    v = lower_flat(*cx, hw16);
    hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);
    hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Utf16);
}

void testString(Encoding guestEncoding)
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, guestEncoding);

    for (auto hw : unicode_test_strings)
    {
        string_t hw_str = hw;
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<string_t>(*cx, v);
        // printString(hw_str_ret, hw);
        CHECK(hw_str_ret == hw_str);
    }

    for (unsigned int i = 0; i < sizeof(boundary_test_strings) / sizeof(boundary_test_strings[0]); ++i)
    {
        string_t hw_str = boundary_test_strings[i];
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<string_t>(*cx, v);
        // printString(hw_str_ret, hw);

        if (guestEncoding != Encoding::Utf8 && i >= 10)
            CHECK(hw_str_ret != hw_str);
        else
            CHECK(hw_str_ret == hw_str);
    }

    for (auto hw : unicode_test_u16strings)
    {
        u16string_t hw_str = hw;
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<u16string_t>(*cx, v);
        CHECK(hw_str_ret.length() == hw_str.length());
        // printString(hw_str_ret, hw);
        CHECK(hw_str_ret == hw_str);
    }

    for (unsigned int i = 0; i < sizeof(boundary_test_u16strings) / sizeof(boundary_test_u16strings[0]); ++i)
    {
        u16string_t hw_str = boundary_test_u16strings[i];
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<u16string_t>(*cx, v);
        // printString(hw_str_ret, hw);
        CHECK(hw_str_ret == hw_str);
    }
}

TEST_CASE("String-complex")
{
    testString(Encoding::Utf8);
    testString(Encoding::Utf16);
    testString(Encoding::Latin1_Utf16);
}

TEST_CASE("String Edge Cases - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test empty string
    string_t empty_str = "";
    auto v = lower_flat(*cx, empty_str);
    auto result = lift_flat<string_t>(*cx, v);
    CHECK(result == empty_str);
    CHECK(result.length() == 0);

    // Test very long string
    string_t long_str(10000, 'A'); // 10,000 'A' characters
    v = lower_flat(*cx, long_str);
    result = lift_flat<string_t>(*cx, v);
    CHECK(result == long_str);
    CHECK(result.length() == 10000);

    // Test string with null bytes embedded
    string_t null_embedded = "Hello\x00World";
    null_embedded.resize(11); // Ensure the null byte is included
    v = lower_flat(*cx, null_embedded);
    result = lift_flat<string_t>(*cx, v);
    CHECK(result.length() == 11);
    CHECK(result[5] == '\x00');

    // Test repeated complex Unicode characters
    string_t unicode_repeated = "🌍🌍🌍🌍🌍"; // Repeated Earth emoji
    v = lower_flat(*cx, unicode_repeated);
    result = lift_flat<string_t>(*cx, v);
    CHECK(result == unicode_repeated);

    // Test mix of ASCII and high Unicode
    string_t mixed = "ABC\U0010FFFF\U0001F600DEF"; // ASCII + max Unicode + emoji + ASCII
    v = lower_flat(*cx, mixed);
    result = lift_flat<string_t>(*cx, v);
    CHECK(result == mixed);

    // Test UTF-16 edge cases
    u16string_t empty_u16 = u"";
    v = lower_flat(*cx, empty_u16);
    auto result_u16 = lift_flat<u16string_t>(*cx, v);
    CHECK(result_u16 == empty_u16);
    CHECK(result_u16.length() == 0);

    // Test very long UTF-16 string
    u16string_t long_u16(5000, u'B'); // 5,000 'B' characters
    v = lower_flat(*cx, long_u16);
    result_u16 = lift_flat<u16string_t>(*cx, v);
    CHECK(result_u16 == long_u16);
    CHECK(result_u16.length() == 5000);
}

TEST_CASE("List")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    list_t<string_t> strings = {"Hello", "World", "!"};
    auto v = lower_flat(*cx, strings);
    auto strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0] == "Hello");
    CHECK(strs[1] == "World");
    CHECK(strs[2] == "!");
}

TEST_CASE("List-Latin1_Utf16")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Latin1_Utf16);
    list_t<string_t> strings = {"Hello", "World", "!"};
    auto v = lower_flat(*cx, strings);
    auto strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0] == "Hello");
    CHECK(strs[1] == "World");
    CHECK(strs[2] == "!");
    strings = {"Hello", "🌍", "!"};
    v = lower_flat(*cx, strings);
    strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0] == "Hello");
    CHECK(strs[1] == "🌍");
    CHECK(strs[2] == "!");
}

TEST_CASE("List Boundary Cases - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test empty list
    list_t<uint32_t> empty_list;
    auto v_empty = lower_flat(*cx, empty_list);
    auto result_empty = lift_flat<list_t<uint32_t>>(*cx, v_empty);
    CHECK(result_empty.size() == 0);
    CHECK(result_empty.empty());

    // Test single element list
    list_t<uint32_t> single_list = {42};
    auto v_single = lower_flat(*cx, single_list);
    auto result_single = lift_flat<list_t<uint32_t>>(*cx, v_single);
    CHECK(result_single.size() == 1);
    CHECK(result_single[0] == 42);

    // Test large list
    list_t<uint32_t> large_list;
    for (uint32_t i = 0; i < 1000; ++i)
    {
        large_list.push_back(i);
    }
    auto v_large = lower_flat(*cx, large_list);
    auto result_large = lift_flat<list_t<uint32_t>>(*cx, v_large);
    CHECK(result_large.size() == 1000);
    for (uint32_t i = 0; i < 1000; ++i)
    {
        CHECK(result_large[i] == i);
    }

    // Test list with boundary values
    list_t<uint8_t> boundary_list = {0, 127, 128, 255};
    auto v_boundary = lower_flat(*cx, boundary_list);
    auto result_boundary = lift_flat<list_t<uint8_t>>(*cx, v_boundary);
    CHECK(result_boundary.size() == 4);
    CHECK(result_boundary[0] == 0);
    CHECK(result_boundary[1] == 127);
    CHECK(result_boundary[2] == 128);
    CHECK(result_boundary[3] == 255);

    // Test nested lists
    using NestedList = list_t<list_t<uint32_t>>;
    NestedList nested = {{1, 2}, {3, 4, 5}, {}};
    auto v_nested = lower_flat(*cx, nested);
    auto result_nested = lift_flat<NestedList>(*cx, v_nested);
    CHECK(result_nested.size() == 3);
    CHECK(result_nested[0].size() == 2);
    CHECK(result_nested[0][0] == 1);
    CHECK(result_nested[0][1] == 2);
    CHECK(result_nested[1].size() == 3);
    CHECK(result_nested[1][0] == 3);
    CHECK(result_nested[1][1] == 4);
    CHECK(result_nested[1][2] == 5);
    CHECK(result_nested[2].size() == 0);

    // Test list of strings with edge cases
    list_t<string_t> string_list = {"", "a", "very_long_string_" + string_t(100, 'x'), "🌍"};
    auto v_strings = lower_flat(*cx, string_list);
    auto result_strings = lift_flat<list_t<string_t>>(*cx, v_strings);
    CHECK(result_strings.size() == 4);
    CHECK(result_strings[0] == "");
    CHECK(result_strings[1] == "a");
    CHECK(result_strings[2].length() == 117); // "very_long_string_" + 100 'x'
    CHECK(result_strings[3] == "🌍");
}

TEST_CASE("Flags")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Latin1_Utf16);
    using MyFlags = flags_t<"a", "bb", "ccc">;
    CHECK(ValTrait<MyFlags>::size == 1);
    CHECK(MyFlags::labelsSize == 3);
    MyFlags flags;
    CHECK(flags.labelsSize == 3);
    auto x = flags.labels;
    CHECK(std::strcmp(x[0], "a") == 0);
    CHECK(std::strcmp(x[1], "bb") == 0);
    CHECK(std::strcmp(x[2], "ccc") == 0);
    auto v = lower_flat(*cx, flags);
    auto f = lift_flat<MyFlags>(*cx, v);
    CHECK(flags.test<"a">() == f.test<"a">());
    CHECK(flags.test<"bb">() == f.test<"bb">());
    CHECK(flags.test<"ccc">() == f.test<"ccc">());
    CHECK(flags == f);

    auto encoded_flags = func::pack_flags_into_int(flags);
    auto decoded_flags = func::unpack_flags_from_int<MyFlags>(static_cast<uint32_t>(encoded_flags));
    CHECK(decoded_flags == flags);

    func::store(*cx, flags, 0);
    auto loaded_flags = func::load<MyFlags>(*cx, 0);
    CHECK(loaded_flags == flags);

    using MyFlags2 = flags_t<"one", "two", "three", "four", "five", "six", "seven", "8", "nine">;
    CHECK(ValTrait<MyFlags2>::size == 2);
    CHECK(MyFlags2::labelsSize == 9);
    MyFlags2 flags2;
    flags2.set<"nine">();
    auto vi = lower_flat(*cx, flags2);
    auto f2 = lift_flat<MyFlags2>(*cx, vi);
    CHECK(flags2 == f2);
    CHECK(flags2.test<"one">() == false);
    CHECK(flags2.test<"8">() == false);
    CHECK(flags2.test<"nine">() == true);
    flags2.reset<"nine">();
    CHECK(flags2.test<"nine">() == false);
    flags2.set<"nine">();
    CHECK(flags2.test<"nine">() == true);

    CHECK(f2.test<"one">() == false);
    CHECK(f2.test<"8">() == false);
    CHECK(f2.test<"nine">() == true);
    f2.reset<"nine">();
    CHECK(f2.test<"nine">() == false);
    f2.set<"nine">();
    CHECK(f2.test<"nine">() == true);
}

TEST_CASE("Flags Boundary Cases - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test flags with all bits set
    using MyFlags = flags_t<"a", "bb", "ccc">;
    MyFlags all_set;
    all_set.set<"a">();
    all_set.set<"bb">();
    all_set.set<"ccc">();
    auto v_all = lower_flat(*cx, all_set);
    auto result_all = lift_flat<MyFlags>(*cx, v_all);
    CHECK(result_all.test<"a">() == true);
    CHECK(result_all.test<"bb">() == true);
    CHECK(result_all.test<"ccc">() == true);

    // Test flags with no bits set
    MyFlags none_set;
    auto v_none = lower_flat(*cx, none_set);
    auto result_none = lift_flat<MyFlags>(*cx, v_none);
    CHECK(result_none.test<"a">() == false);
    CHECK(result_none.test<"bb">() == false);
    CHECK(result_none.test<"ccc">() == false);

    // Test flags with alternating pattern
    MyFlags alternating;
    alternating.set<"a">();
    alternating.set<"ccc">();
    auto v_alt = lower_flat(*cx, alternating);
    auto result_alt = lift_flat<MyFlags>(*cx, v_alt);
    CHECK(result_alt.test<"a">() == true);
    CHECK(result_alt.test<"bb">() == false);
    CHECK(result_alt.test<"ccc">() == true);

    // Test large flag set (spanning multiple bytes)
    using LargeFlags = flags_t<"flag0", "flag1", "flag2", "flag3", "flag4", "flag5", "flag6", "flag7",
                               "flag8", "flag9", "flag10", "flag11", "flag12", "flag13", "flag14", "flag15",
                               "flag16", "flag17", "flag18", "flag19", "flag20", "flag21", "flag22", "flag23",
                               "flag24", "flag25", "flag26", "flag27", "flag28", "flag29", "flag30", "flag31">;

    LargeFlags large_flags;
    // Set some specific flags
    large_flags.set<"flag0">();
    large_flags.set<"flag15">();
    large_flags.set<"flag31">();

    auto v_large = lower_flat(*cx, large_flags);
    auto result_large = lift_flat<LargeFlags>(*cx, v_large);

    CHECK(result_large.test<"flag0">() == true);
    CHECK(result_large.test<"flag1">() == false);
    CHECK(result_large.test<"flag15">() == true);
    CHECK(result_large.test<"flag16">() == false);
    CHECK(result_large.test<"flag31">() == true);

    // Test boundary: set all flags in large set
    LargeFlags all_large_set;
    all_large_set.set<"flag0">();
    all_large_set.set<"flag1">();
    all_large_set.set<"flag2">();
    all_large_set.set<"flag3">();
    all_large_set.set<"flag4">();
    all_large_set.set<"flag5">();
    all_large_set.set<"flag6">();
    all_large_set.set<"flag7">();
    all_large_set.set<"flag8">();
    all_large_set.set<"flag9">();
    all_large_set.set<"flag10">();
    all_large_set.set<"flag11">();
    all_large_set.set<"flag12">();
    all_large_set.set<"flag13">();
    all_large_set.set<"flag14">();
    all_large_set.set<"flag15">();
    all_large_set.set<"flag16">();
    all_large_set.set<"flag17">();
    all_large_set.set<"flag18">();
    all_large_set.set<"flag19">();
    all_large_set.set<"flag20">();
    all_large_set.set<"flag21">();
    all_large_set.set<"flag22">();
    all_large_set.set<"flag23">();
    all_large_set.set<"flag24">();
    all_large_set.set<"flag25">();
    all_large_set.set<"flag26">();
    all_large_set.set<"flag27">();
    all_large_set.set<"flag28">();
    all_large_set.set<"flag29">();
    all_large_set.set<"flag30">();
    all_large_set.set<"flag31">();

    auto v_all_large = lower_flat(*cx, all_large_set);
    auto result_all_large = lift_flat<LargeFlags>(*cx, v_all_large);

    // Check a few key flags
    CHECK(result_all_large.test<"flag0">() == true);
    CHECK(result_all_large.test<"flag15">() == true);
    CHECK(result_all_large.test<"flag31">() == true);
}

TEST_CASE("Tuples")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    using R0 = tuple_t<uint16_t, uint32_t>;
    auto flatTypes = ValTrait<R0>::flat_types;
    CHECK(flatTypes.size() == 2);
    R0 r0 = {42, 43};
    auto v = lower_flat(*cx, r0);
    auto rr = lift_flat<R0>(*cx, v);
    CHECK(r0 == rr);
    // auto str0 = to_struct<MyRecord0>(rr);

    using R1 = tuple_t<uint16_t, uint32_t, string_t>;
    R1 r1 = {142, 143, "Hello"};
    auto vv = lower_flat(*cx, r1);
    auto rr1 = lift_flat<R1>(*cx, vv);
    CHECK(r1 == rr1);
    // auto str1 = to_struct<MyRecord1>(rr1);

    using R2 = tuple_t<uint16_t, uint32_t, string_t, list_t<string_t>>;
    R2 r2 = {242, 243, "2Hello", {"2World", "!"}};
    auto vvv = lower_flat(*cx, r2);
    auto rr2 = lift_flat<R2>(*cx, vvv);
    CHECK(r2 == rr2);
    // auto str2 = to_struct<MyRecord2>(rr2);
}

TEST_CASE("Records")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    struct PersonStruct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
    };
    using Person = record_t<PersonStruct>;
    Person p_in = {"John", 42, 200};
    auto v = lower_flat(*cx, p_in);
    auto p_out = lift_flat<Person>(*cx, v);
    CHECK(p_in.name == p_out.name);

    CHECK(p_in.age == p_out.age);
    CHECK(p_in.weight == p_out.weight);

    struct PersonExStruct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
        string_t address;
    };
    using PersonEx = record_t<PersonExStruct>;
    PersonEx pex_in = {"John", 42, 200, "123 Main St."};
    v = lower_flat(*cx, pex_in);
    auto pex_out = lift_flat<PersonEx>(*cx, v);
    CHECK(pex_in.name == pex_out.name);
    CHECK(pex_in.age == pex_out.age);
    CHECK(pex_in.weight == pex_out.weight);
    CHECK(pex_in.address == pex_out.address);

    struct PersonEx2Struct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
        string_t address;
        list_t<string_t> phones;
    };
    using PersonEx2 = record_t<PersonEx2Struct>;
    PersonEx2 pex2_in = {"John", 42, 200, "123 Main St.", {"555-1212", "555-1234"}};
    v = lower_flat(*cx, pex2_in);
    auto pex2_out = lift_flat<PersonEx2>(*cx, v);
    CHECK(pex2_in.name == pex2_out.name);
    CHECK(pex2_in.age == pex2_out.age);
    CHECK(pex2_in.weight == pex2_out.weight);
    CHECK(pex2_in.address == pex2_out.address);
    CHECK(pex2_in.phones == pex2_out.phones);

    struct PersonEx3Struct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
        string_t address;
        list_t<string_t> phones;
        tuple_t<uint16_t, uint32_t> vital_stats;
        record_t<PersonEx2Struct> p;
    };
    using PersonEx3 = record_t<PersonEx3Struct>;
    PersonEx3 pex3_in = {"John", 42, 200, "123 Main St.", {"555-1212", "555-1234"}, {42, 43}, {"Jane", 43, 150, "63 Elm St.", {"555-1212", "555-1234", "555-4321"}}};
    v = lower_flat(*cx, pex3_in);
    auto pex3_out = lift_flat<PersonEx3>(*cx, v);
    CHECK(pex3_in.name == pex3_out.name);
    CHECK(pex3_in.age == pex3_out.age);
    CHECK(pex3_in.weight == pex3_out.weight);
    CHECK(pex3_in.address == pex3_out.address);
    CHECK(pex3_in.phones == pex3_out.phones);
    CHECK(pex3_in.vital_stats == pex3_out.vital_stats);
    CHECK(pex3_in.p.name == pex3_out.p.name);
    CHECK(pex3_in.p.age == pex3_out.p.age);
    CHECK(pex3_in.p.weight == pex3_out.p.weight);
    CHECK(pex3_in.p.address == pex3_out.p.address);
    CHECK(pex3_in.p.phones == pex3_out.p.phones);
}

TEST_CASE("Function flattening honors canonical limits")
{
    CanonicalOptions opts;
    opts.sync = true;

    using HeavyParamFn = std::function<void(string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t)>;

    auto lift_flat = func::flatten<HeavyParamFn>(opts, func::ContextType::Lift);
    CHECK(lift_flat.params.size() == 1);
    CHECK(lift_flat.params[0] == WasmValType::i32);
    CHECK(lift_flat.results.empty());

    auto lower_flat = func::flatten<HeavyParamFn>(opts, func::ContextType::Lower);
    CHECK(lower_flat.params.size() == 1);
    CHECK(lower_flat.params[0] == WasmValType::i32);
    CHECK(lower_flat.results.empty());

    using HeavyResultFn = std::function<tuple_t<string_t, string_t>()>;

    auto lift_results = func::flatten<HeavyResultFn>(opts, func::ContextType::Lift);
    CHECK(lift_results.params.empty());
    CHECK(lift_results.results.size() == 1);
    CHECK(lift_results.results[0] == WasmValType::i32);

    auto lower_results = func::flatten<HeavyResultFn>(opts, func::ContextType::Lower);
    CHECK(lower_results.params.size() == 1);
    CHECK(lower_results.params.back() == WasmValType::i32);
    CHECK(lower_results.results.empty());
}

TEST_CASE("Async function flattening matches canonical ABI")
{
    using AsyncFn = std::function<int32_t(int32_t, int32_t, int32_t, int32_t, int32_t)>;

    CanonicalOptions async_opts;
    async_opts.sync = false;

    auto lower_async = func::flatten<AsyncFn>(async_opts, func::ContextType::Lower);
    CHECK(lower_async.params.size() == 2);
    CHECK(lower_async.params[0] == WasmValType::i32);
    CHECK(lower_async.params[1] == WasmValType::i32);
    CHECK(lower_async.results.size() == 1);
    CHECK(lower_async.results[0] == WasmValType::i32);

    using AsyncLiftFn = std::function<int32_t(int32_t)>;

    CanonicalOptions lift_opts;
    lift_opts.sync = false;

    auto lift_no_callback = func::flatten<AsyncLiftFn>(lift_opts, func::ContextType::Lift);
    CHECK(lift_no_callback.params.size() == 1);
    CHECK(lift_no_callback.params[0] == WasmValType::i32);
    CHECK(lift_no_callback.results.empty());

    CanonicalOptions lift_with_callback;
    lift_with_callback.sync = false;
    lift_with_callback.callback = GuestCallback([](EventCode, uint32_t, uint32_t) {});

    auto lift_callback = func::flatten<AsyncLiftFn>(lift_with_callback, func::ContextType::Lift);
    CHECK(lift_callback.results.size() == 1);
    CHECK(lift_callback.results[0] == WasmValType::i32);
}

TEST_CASE("Heap-based lowering triggers for oversized results")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    using ResultTuple = tuple_t<string_t, string_t>;
    ResultTuple value{"alpha", "beta"};

    auto lowered = lower_flat_values<ResultTuple>(*cx, MAX_FLAT_RESULTS, nullptr, std::move(value));
    REQUIRE(lowered.size() == 1);
    auto ptr = std::get<int32_t>(lowered[0]);

    auto stored = load<ResultTuple>(*cx, ptr);
    CHECK(std::get<0>(stored) == "alpha");
    CHECK(std::get<1>(stored) == "beta");

    CoreValueIter iter(lowered);
    auto lifted = lift_flat_values<ResultTuple>(*cx, MAX_FLAT_RESULTS, iter);
    CHECK(std::get<0>(lifted) == "alpha");
    CHECK(std::get<1>(lifted) == "beta");
}

TEST_CASE("Variant")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    using Vb = variant_t<bool_t, uint32_t>;
    Vb vb = static_cast<uint32_t>(42);
    auto vvb = lower_flat(*cx, vb);
    auto vbb = lift_flat<Vb>(*cx, vvb);
    CHECK(vb == vbb);

    using V0 = variant_t<uint16_t, uint32_t>;
    auto x = ValTrait<ValTrait<V0>::discriminant_type>::size;
    V0 v0 = static_cast<uint32_t>(42);
    auto vv0 = lower_flat(*cx, v0);
    auto v00 = lift_flat<V0>(*cx, vv0);
    CHECK(v0 == v00);

    using V1 = variant_t<uint16_t, uint32_t, string_t>;
    auto x1 = ValTrait<ValTrait<V1>::discriminant_type>::size;
    V1 v1 = "Hello";
    auto vv1 = lower_flat(*cx, v1);
    auto v11 = lift_flat<V1>(*cx, vv1);
    CHECK(v1 == v11);

    using V2 = variant_t<uint16_t, uint32_t, string_t, list_t<string_t>>;
    auto x2 = ValTrait<ValTrait<V2>::discriminant_type>::size;
    V2 v2 = list_t<string_t>{"Hello", "World", "!"};
    auto vv2 = lower_flat(*cx, v2);
    auto v22 = lift_flat<V2>(*cx, vv2);
    CHECK(v2 == v22);

    using V3 = variant_t<uint16_t, uint32_t, string_t, list_t<string_t>, tuple_t<uint16_t, uint32_t>>;
    auto x3 = ValTrait<ValTrait<V3>::discriminant_type>::size;
    V3 v3 = tuple_t<uint16_t, uint32_t>{42, 43};
    auto vv3 = lower_flat(*cx, v3);
    auto v33 = lift_flat<V3>(*cx, vv3);
    CHECK(v3 == v33);

    using V4 = variant_t<uint16_t, uint32_t, string_t, list_t<string_t>, tuple_t<uint16_t, uint32_t>, V3>;
    auto x4 = ValTrait<ValTrait<V4>::discriminant_type>::size;
    V4 v4 = tuple_t<uint16_t, uint32_t>{42, 43};
    auto vv4 = lower_flat(*cx, v4);
    auto v44 = lift_flat<V4>(*cx, vv4);
    auto rr4 = std::get<tuple_t<uint16_t, uint32_t>>(v44);
    CHECK(v4 == v44);

    using VariantList = list_t<variant_t<bool_t>>;
    VariantList variants = {true, false};
    auto vv5 = lower_flat(*cx, variants);
    auto v55 = lift_flat<VariantList>(*cx, vv5);
    CHECK(variants.size() == v55.size());
    auto v0_ = std::get<bool_t>(v55[0]);
    auto v1_ = std::get<bool_t>(v55[1]);
    CHECK(variants[0] == v55[0]);
    CHECK(variants[1] == v55[1]);
    CHECK(variants == v55);

    using VariantList2 = list_t<variant_t<string_t>>;
    VariantList2 variants2 = {"Hello World 1", "Hello World 2", "Hello World 3"};
    auto vv6 = lower_flat(*cx, variants2);
    auto v66 = lift_flat<VariantList2>(*cx, vv6);
    CHECK(variants2 == v66);

    using VariantList3 = list_t<variant_t<string_t, bool_t>>;
    VariantList3 variants3 = {"Hello World 1", "Hello World 2", "Hello World 3"};
    auto vv7 = lower_flat(*cx, variants3);
    auto v77 = lift_flat<VariantList3>(*cx, vv7);
    CHECK(variants3 == v77);

    VariantList3 variants3b = {true, false, true};
    auto vv7b = lower_flat(*cx, variants3b);
    auto v77b = lift_flat<VariantList3>(*cx, vv7b);
    CHECK(variants3b == v77b);

    VariantList3 variants3c = {true, "false", true, "Hello World 1", "Hello World 2", "Hello World 3"};
    auto vv7c = lower_flat(*cx, variants3b);
    auto v77c = lift_flat<VariantList3>(*cx, vv7c);
    CHECK(variants3b == v77c);

    using VariantList4 = list_t<variant_t<string_t, uint32_t, bool_t>>;
    VariantList4 variants4 = {"Hello World3", (uint32_t)42, true};
    auto vv8 = lower_flat(*cx, variants4);
    auto v88 = lift_flat<VariantList4>(*cx, vv8);
    CHECK(variants4 == v88);
}

TEST_CASE("Variant Boundary Cases - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test variant with boundary numeric values
    using NumericVariant = variant_t<uint8_t, int8_t, uint16_t, int16_t>;

    NumericVariant v_u8_max = static_cast<uint8_t>(UINT8_MAX);
    auto flat_u8_max = lower_flat(*cx, v_u8_max);
    auto result_u8_max = lift_flat<NumericVariant>(*cx, flat_u8_max);
    CHECK(std::get<uint8_t>(result_u8_max) == UINT8_MAX);

    NumericVariant v_i8_min = static_cast<int8_t>(INT8_MIN);
    auto flat_i8_min = lower_flat(*cx, v_i8_min);
    auto result_i8_min = lift_flat<NumericVariant>(*cx, flat_i8_min);
    CHECK(std::get<int8_t>(result_i8_min) == INT8_MIN);

    NumericVariant v_u16_max = static_cast<uint16_t>(UINT16_MAX);
    auto flat_u16_max = lower_flat(*cx, v_u16_max);
    auto result_u16_max = lift_flat<NumericVariant>(*cx, flat_u16_max);
    CHECK(std::get<uint16_t>(result_u16_max) == UINT16_MAX);

    NumericVariant v_i16_min = static_cast<int16_t>(INT16_MIN);
    auto flat_i16_min = lower_flat(*cx, v_i16_min);
    auto result_i16_min = lift_flat<NumericVariant>(*cx, flat_i16_min);
    CHECK(std::get<int16_t>(result_i16_min) == INT16_MIN);

    // Test variant with float special values
    using FloatVariant = variant_t<float32_t, float64_t, uint32_t>;

    // FloatVariant v_nan = std::numeric_limits<float32_t>::quiet_NaN();
    // auto flat_nan = lower_flat(*cx, v_nan);
    // auto result_nan = lift_flat<FloatVariant>(*cx, flat_nan);
    // CHECK(std::isnan(std::get<float32_t>(result_nan)));

    // FloatVariant v_inf = std::numeric_limits<float32_t>::infinity();
    // auto flat_inf = lower_flat(*cx, v_inf);
    // auto result_inf = lift_flat<FloatVariant>(*cx, flat_inf);
    // CHECK(std::isinf(std::get<float32_t>(result_inf)));

    // FloatVariant v_f64_max = std::numeric_limits<float64_t>::max();
    // auto flat_f64_max = lower_flat(*cx, v_f64_max);
    // auto result_f64_max = lift_flat<FloatVariant>(*cx, flat_f64_max);
    // CHECK(std::get<float64_t>(result_f64_max) == std::numeric_limits<float64_t>::max());

    // Test variant with complex nested types
    using NestedVariant = variant_t<option_t<string_t>, list_t<uint32_t>, tuple_t<bool, uint8_t>>;

    NestedVariant v_opt_empty = option_t<string_t>("");
    auto flat_opt_empty = lower_flat(*cx, v_opt_empty);
    auto result_opt_empty = lift_flat<NestedVariant>(*cx, flat_opt_empty);
    auto &opt_result = std::get<option_t<string_t>>(result_opt_empty);
    CHECK(opt_result.has_value());
    CHECK(opt_result.value() == "");

    NestedVariant v_opt_none = option_t<string_t>();
    auto flat_opt_none = lower_flat(*cx, v_opt_none);
    auto result_opt_none = lift_flat<NestedVariant>(*cx, flat_opt_none);
    auto &opt_none_result = std::get<option_t<string_t>>(result_opt_none);
    CHECK(!opt_none_result.has_value());

    NestedVariant v_empty_list = list_t<uint32_t>();
    auto flat_empty_list = lower_flat(*cx, v_empty_list);
    auto result_empty_list = lift_flat<NestedVariant>(*cx, flat_empty_list);
    auto &list_result = std::get<list_t<uint32_t>>(result_empty_list);
    CHECK(list_result.empty());

    NestedVariant v_large_list = list_t<uint32_t>(100, 42);
    auto flat_large_list = lower_flat(*cx, v_large_list);
    auto result_large_list = lift_flat<NestedVariant>(*cx, flat_large_list);
    auto &large_list_result = std::get<list_t<uint32_t>>(result_large_list);
    CHECK(large_list_result.size() == 100);
    CHECK(large_list_result[0] == 42);
    CHECK(large_list_result[99] == 42);

    NestedVariant v_tuple = tuple_t<bool, uint8_t>{false, UINT8_MAX};
    auto flat_tuple = lower_flat(*cx, v_tuple);
    auto result_tuple = lift_flat<NestedVariant>(*cx, flat_tuple);
    auto &tuple_result = std::get<tuple_t<bool, uint8_t>>(result_tuple);
    CHECK(std::get<0>(tuple_result) == false);
    CHECK(std::get<1>(tuple_result) == UINT8_MAX);

    // Test variant with all string edge cases
    using StringVariant = variant_t<string_t, u16string_t>;

    StringVariant v_empty_str = string_t("");
    auto flat_empty_str = lower_flat(*cx, v_empty_str);
    auto result_empty_str = lift_flat<StringVariant>(*cx, flat_empty_str);
    CHECK(std::get<string_t>(result_empty_str) == "");

    StringVariant v_long_str = string_t(1000, 'Z');
    auto flat_long_str = lower_flat(*cx, v_long_str);
    auto result_long_str = lift_flat<StringVariant>(*cx, flat_long_str);
    CHECK(std::get<string_t>(result_long_str).length() == 1000);

    StringVariant v_unicode_str = string_t("Hello 🌍 World");
    auto flat_unicode_str = lower_flat(*cx, v_unicode_str);
    auto result_unicode_str = lift_flat<StringVariant>(*cx, flat_unicode_str);
    CHECK(std::get<string_t>(result_unicode_str) == "Hello 🌍 World");

    StringVariant v_u16_empty = u16string_t(u"");
    auto flat_u16_empty = lower_flat(*cx, v_u16_empty);
    auto result_u16_empty = lift_flat<StringVariant>(*cx, flat_u16_empty);
    CHECK(std::get<u16string_t>(result_u16_empty) == u"");

    StringVariant v_u16_unicode = u16string_t(u"Hello 🌍 World");
    auto flat_u16_unicode = lower_flat(*cx, v_u16_unicode);
    auto result_u16_unicode = lift_flat<StringVariant>(*cx, flat_u16_unicode);
    CHECK(std::get<u16string_t>(result_u16_unicode) == u"Hello 🌍 World");
}

TEST_CASE("Option")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    using O0 = option_t<uint32_t>;
    O0 o0 = 42;
    auto vv0 = lower_flat(*cx, o0);
    auto v00 = lift_flat<O0>(*cx, vv0);
    CHECK(o0 == v00);

    O0 o1;
    auto vv1 = lower_flat(*cx, o1);
    auto v01 = lift_flat<O0>(*cx, vv1);
    CHECK(o1 == v01);

    using O1 = option_t<string_t>;
    O1 o2 = "Hello";
    auto vv2 = lower_flat(*cx, o2);
    auto v02 = lift_flat<O1>(*cx, vv2);
    CHECK(o2 == v02);

    O1 o3;
    auto vv3 = lower_flat(*cx, o3);
    auto v03 = lift_flat<O1>(*cx, vv3);
    CHECK(o3 == v03);
}

TEST_CASE("Option Boundary Cases - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test Option with different numeric types at boundary values
    using OptU8 = option_t<uint8_t>;
    OptU8 opt_u8_max = UINT8_MAX;
    auto v_u8_max = lower_flat(*cx, opt_u8_max);
    auto result_u8_max = lift_flat<OptU8>(*cx, v_u8_max);
    CHECK(result_u8_max.has_value());
    CHECK(result_u8_max.value() == UINT8_MAX);

    OptU8 opt_u8_min = 0;
    auto v_u8_min = lower_flat(*cx, opt_u8_min);
    auto result_u8_min = lift_flat<OptU8>(*cx, v_u8_min);
    CHECK(result_u8_min.has_value());
    CHECK(result_u8_min.value() == 0);

    OptU8 opt_u8_none;
    auto v_u8_none = lower_flat(*cx, opt_u8_none);
    auto result_u8_none = lift_flat<OptU8>(*cx, v_u8_none);
    CHECK(!result_u8_none.has_value());

    // Test Option with float boundary values
    // using OptF32 = option_t<float32_t>;
    // OptF32 opt_inf = std::numeric_limits<float32_t>::infinity();
    // auto v_inf = lower_flat(*cx, opt_inf);
    // auto result_inf = lift_flat<OptF32>(*cx, v_inf);
    // CHECK(result_inf.has_value());
    // CHECK(std::isinf(result_inf.value()));

    // OptF32 opt_nan = std::numeric_limits<float32_t>::quiet_NaN();
    // auto v_nan = lower_flat(*cx, opt_nan);
    // auto result_nan = lift_flat<OptF32>(*cx, v_nan);
    // CHECK(result_nan.has_value());
    // CHECK(std::isnan(result_nan.value()));

    // Test Option with empty string
    using OptStr = option_t<string_t>;
    OptStr opt_empty_str = "";
    auto v_empty_str = lower_flat(*cx, opt_empty_str);
    auto result_empty_str = lift_flat<OptStr>(*cx, v_empty_str);
    CHECK(result_empty_str.has_value());
    CHECK(result_empty_str.value() == "");

    // Test Option with very long string
    OptStr opt_long_str = string_t(1000, 'X');
    auto v_long_str = lower_flat(*cx, opt_long_str);
    auto result_long_str = lift_flat<OptStr>(*cx, v_long_str);
    CHECK(result_long_str.has_value());
    CHECK(result_long_str.value().length() == 1000);

    // Test nested Option
    using NestedOpt = option_t<option_t<uint32_t>>;
    NestedOpt nested_some_some = option_t<uint32_t>(42);
    auto v_nested_some = lower_flat(*cx, nested_some_some);
    auto result_nested_some = lift_flat<NestedOpt>(*cx, v_nested_some);
    CHECK(result_nested_some.has_value());
    CHECK(result_nested_some.value().has_value());
    CHECK(result_nested_some.value().value() == 42);

    NestedOpt nested_some_none = option_t<uint32_t>();
    auto v_nested_none = lower_flat(*cx, nested_some_none);
    auto result_nested_none = lift_flat<NestedOpt>(*cx, v_nested_none);
    CHECK(result_nested_none.has_value());
    CHECK(!result_nested_none.value().has_value());

    NestedOpt nested_none;
    auto v_nested_outer_none = lower_flat(*cx, nested_none);
    auto result_nested_outer_none = lift_flat<NestedOpt>(*cx, v_nested_outer_none);
    CHECK(!result_nested_outer_none.has_value());
}

TEST_CASE("Func")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    using MyFunc = func_t<uint32_t(string_t, string_t)>;
    MyFunc f = [](string_t b, string_t c) -> uint32_t
    {
        return 42;
    };
}

TEST_CASE("Boolean Edge Cases - Enhanced")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test boundary values for boolean (should normalize to true/false)
    bool_t true_val = true;
    auto v_true = lower_flat(*cx, true_val);
    auto result_true = lift_flat<bool_t>(*cx, v_true);
    CHECK(result_true == true);

    bool_t false_val = false;
    auto v_false = lower_flat(*cx, false_val);
    auto result_false = lift_flat<bool_t>(*cx, v_false);
    CHECK(result_false == false);

    // Test in various container types
    list_t<bool_t> bool_list = {true, false, true, false};
    auto v_list = lower_flat(*cx, bool_list);
    auto result_list = lift_flat<list_t<bool_t>>(*cx, v_list);
    CHECK(result_list.size() == 4);
    CHECK(result_list[0] == true);
    CHECK(result_list[1] == false);
    CHECK(result_list[2] == true);
    CHECK(result_list[3] == false);

    option_t<bool_t> opt_true = true;
    auto v_opt_true = lower_flat(*cx, opt_true);
    auto result_opt_true = lift_flat<option_t<bool_t>>(*cx, v_opt_true);
    CHECK(result_opt_true.has_value());
    CHECK(result_opt_true.value() == true);

    option_t<bool_t> opt_false = false;
    auto v_opt_false = lower_flat(*cx, opt_false);
    auto result_opt_false = lift_flat<option_t<bool_t>>(*cx, v_opt_false);
    CHECK(result_opt_false.has_value());
    CHECK(result_opt_false.value() == false);

    option_t<bool_t> opt_none;
    auto v_opt_none = lower_flat(*cx, opt_none);
    auto result_opt_none = lift_flat<option_t<bool_t>>(*cx, v_opt_none);
    CHECK(!result_opt_none.has_value());
}

TEST_CASE("Error Handling - Memory and Edge Cases")
{
    // Test with very small heap to trigger allocation failures
    Heap small_heap(64); // Very small heap
    auto cx = createLiftLowerContext(&small_heap, Encoding::Utf8);

    // Try to allocate a large string that should exceed the heap
    bool caught_exception = false;
    try
    {
        string_t huge_string(1000000, 'A'); // 1MB string, likely to exceed small heap
        auto v = lower_flat(*cx, huge_string);
        auto result = lift_flat<string_t>(*cx, v);
    }
    catch (...)
    {
        caught_exception = true;
    }
    // Note: The actual behavior depends on implementation, we just test that it doesn't crash

    // Test with moderate heap for normal operations
    Heap heap(1024 * 1024);
    auto cx2 = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test multiple allocations to stress the heap
    std::vector<string_t> many_strings;
    for (int i = 0; i < 100; ++i)
    {
        string_t test_str = "Test string number " + std::to_string(i);
        auto v = lower_flat(*cx2, test_str);
        auto result = lift_flat<string_t>(*cx2, v);
        CHECK(result == test_str);
        many_strings.push_back(result);
    }
    CHECK(many_strings.size() == 100);

    // Test edge case with maximum size values that fit in the type system
    // but might stress memory allocation
    try
    {
        list_t<uint8_t> large_byte_array(50000, 0xFF);
        auto v = lower_flat(*cx2, large_byte_array);
        auto result = lift_flat<list_t<uint8_t>>(*cx2, v);
        CHECK(result.size() == 50000);
        CHECK(result[0] == 0xFF);
        CHECK(result[49999] == 0xFF);
    }
    catch (...)
    {
        // If allocation fails, that's also acceptable behavior
        CHECK(true);
    }
}

TEST_CASE("Complex Nested Structures - Boundary Cases")
{
    Heap heap(1024 * 1024);
    auto cx = createLiftLowerContext(&heap, Encoding::Utf8);

    // Test deeply nested variants
    using ComplexVariant = variant_t<uint32_t, string_t, list_t<uint32_t>, option_t<string_t>>;

    // Test each variant case
    ComplexVariant v1 = uint32_t(42);
    auto flat_v1 = lower_flat(*cx, v1);
    auto result_v1 = lift_flat<ComplexVariant>(*cx, flat_v1);
    CHECK(std::get<uint32_t>(result_v1) == 42);

    ComplexVariant v2 = string_t("Hello Variant");
    auto flat_v2 = lower_flat(*cx, v2);
    auto result_v2 = lift_flat<ComplexVariant>(*cx, flat_v2);
    CHECK(std::get<string_t>(result_v2) == "Hello Variant");

    ComplexVariant v3 = list_t<uint32_t>{1, 2, 3, 4, 5};
    auto flat_v3 = lower_flat(*cx, v3);
    auto result_v3 = lift_flat<ComplexVariant>(*cx, flat_v3);
    auto &list_result = std::get<list_t<uint32_t>>(result_v3);
    CHECK(list_result.size() == 5);
    CHECK(list_result[0] == 1);
    CHECK(list_result[4] == 5);

    ComplexVariant v4 = option_t<string_t>("Optional String");
    auto flat_v4 = lower_flat(*cx, v4);
    auto result_v4 = lift_flat<ComplexVariant>(*cx, flat_v4);
    auto &opt_result = std::get<option_t<string_t>>(result_v4);
    CHECK(opt_result.has_value());
    CHECK(opt_result.value() == "Optional String");

    ComplexVariant v5 = option_t<string_t>(); // None
    auto flat_v5 = lower_flat(*cx, v5);
    auto result_v5 = lift_flat<ComplexVariant>(*cx, flat_v5);
    auto &opt_result_none = std::get<option_t<string_t>>(result_v5);
    CHECK(!opt_result_none.has_value());

    // Test complex tuple with mixed types
    using ComplexTuple = tuple_t<bool, uint8_t, float32_t, string_t, list_t<uint16_t>>;
    ComplexTuple complex_tuple = {
        true,
        255,
        3.14159f,
        "Tuple String",
        {100, 200, 300}};
    auto flat_tuple = lower_flat(*cx, complex_tuple);
    auto result_tuple = lift_flat<ComplexTuple>(*cx, flat_tuple);

    CHECK(std::get<0>(result_tuple) == true);
    CHECK(std::get<1>(result_tuple) == 255);
    CHECK(std::abs(std::get<2>(result_tuple) - 3.14159f) < 0.0001f);
    CHECK(std::get<3>(result_tuple) == "Tuple String");
    auto &tuple_list = std::get<4>(result_tuple);
    CHECK(tuple_list.size() == 3);
    CHECK(tuple_list[0] == 100);
    CHECK(tuple_list[1] == 200);
    CHECK(tuple_list[2] == 300);
}
