#ifndef CMCPP_CONTEXT_HPP
#define CMCPP_CONTEXT_HPP

#include "traits.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <future>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <limits>
#if __has_include(<span>)
#include <span>
#else
#include <string>
#include <sstream>
#endif

namespace cmcpp
{
    enum class EventCode : uint8_t;

    using HostTrap = std::function<void(const char *msg) noexcept(false)>;
    using GuestRealloc = std::function<int(int ptr, int old_size, int align, int new_size)>;
    using GuestMemory = std::span<uint8_t>;
    using GuestPostReturn = std::function<void()>;
    using GuestCallback = std::function<void(EventCode, uint32_t, uint32_t)>;
    using HostUnicodeConversion = std::function<std::pair<void *, size_t>(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding)>;

    // Canonical ABI Options ---
    class LiftOptions
    {
    public:
        Encoding string_encoding = Encoding::Utf8;
        GuestMemory memory;

        LiftOptions(const Encoding &string_encoding = Encoding::Utf8, const GuestMemory &memory = {})
            : string_encoding(string_encoding), memory(memory) {}

        bool operator==(const LiftOptions &rhs) const
        {
            return string_encoding == rhs.string_encoding && (memory.data() == rhs.memory.data());
        }
    };

    class LiftLowerOptions : public LiftOptions
    {
    public:
        GuestRealloc realloc;

        LiftLowerOptions(Encoding string_encoding = Encoding::Utf8, GuestMemory memory = {}, GuestRealloc realloc = {})
            : LiftOptions(string_encoding, memory), realloc(realloc) {}
    };

    struct CanonicalOptions : LiftLowerOptions
    {
        std::optional<GuestPostReturn> post_return;
        bool sync = true;
        std::optional<GuestCallback> callback;
        bool allways_task_return = false;
    };

    struct ComponentInstance;
    struct HandleElement;

    class LiftLowerContext
    {
    public:
        HostTrap trap;
        HostUnicodeConversion convert;

        LiftLowerOptions opts;
        ComponentInstance *inst = nullptr;
        std::vector<HandleElement *> lenders;
        uint32_t borrow_count = 0;

        LiftLowerContext(const HostTrap &host_trap, const HostUnicodeConversion &conversion, const LiftLowerOptions &options, ComponentInstance *instance = nullptr)
            : trap(host_trap), convert(conversion), opts(options), inst(instance) {}

        void set_canonical_options(CanonicalOptions options);
        CanonicalOptions *canonical_options();
        const CanonicalOptions *canonical_options() const;
        bool is_sync() const;
        void invoke_post_return() const;
        void notify_async_event(EventCode code, uint32_t index, uint32_t payload) const;

        void track_owning_lend(HandleElement &lending_handle);
        void exit_call();

    private:
        std::optional<CanonicalOptions> canonical_opts_;
    };

    inline void trap_if(const LiftLowerContext &cx, bool condition, const char *message = nullptr) noexcept(false)
    {
        if (!condition)
        {
            return;
        }

        const char *msg = message == nullptr ? "Unknown trap" : message;
        if (cx.trap)
        {
            cx.trap(msg);
            return;
        }
        throw std::runtime_error(msg);
    }

    inline void LiftLowerContext::set_canonical_options(CanonicalOptions options)
    {
        canonical_opts_ = std::move(options);
        opts = *canonical_opts_;
    }

    inline CanonicalOptions *LiftLowerContext::canonical_options()
    {
        return canonical_opts_ ? &*canonical_opts_ : nullptr;
    }

    inline const CanonicalOptions *LiftLowerContext::canonical_options() const
    {
        return canonical_opts_ ? &*canonical_opts_ : nullptr;
    }

    inline bool LiftLowerContext::is_sync() const
    {
        if (auto *canon = canonical_options())
        {
            return canon->sync;
        }
        return true;
    }

    inline void LiftLowerContext::invoke_post_return() const
    {
        if (auto *canon = canonical_options())
        {
            if (canon->post_return)
            {
                (*canon->post_return)();
            }
        }
    }

    inline void LiftLowerContext::notify_async_event(EventCode code, uint32_t index, uint32_t payload) const
    {
        if (auto *canon = canonical_options())
        {
            trap_if(*this, canon->sync, "async continuation requires async canonical options");
            if (canon->callback)
            {
                (*canon->callback)(code, index, payload);
            }
        }
    }

    inline LiftLowerContext make_trap_context(const HostTrap &trap)
    {
        HostUnicodeConversion convert{};
        LiftLowerOptions opts{};
        return LiftLowerContext(trap, convert, opts);
    }

    constexpr uint32_t BLOCKED = 0xFFFF'FFFFu;

    enum class EventCode : uint8_t
    {
        NONE = 0,
        SUBTASK = 1,
        STREAM_READ = 2,
        STREAM_WRITE = 3,
        FUTURE_READ = 4,
        FUTURE_WRITE = 5,
        TASK_CANCELLED = 6
    };

    struct Event
    {
        EventCode code = EventCode::NONE;
        uint32_t index = 0;
        uint32_t payload = 0;
    };

    struct TableEntry
    {
        virtual ~TableEntry() = default;
    };

    class WaitableSet;

    class Waitable : public TableEntry
    {
    public:
        Waitable() = default;

        void set_pending_event(const Event &event)
        {
            pending_event_ = event;
        }

        bool has_pending_event() const
        {
            return pending_event_.has_value();
        }

        Event get_pending_event(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !pending_event_.has_value(), "waitable pending event missing");
            Event event = *pending_event_;
            pending_event_.reset();
            return event;
        }

        void clear_pending_event()
        {
            pending_event_.reset();
        }

        void join(WaitableSet *set, const HostTrap &trap);

        WaitableSet *joined_set() const
        {
            return wset_;
        }

        void drop(const HostTrap &trap);

    private:
        std::optional<Event> pending_event_;
        WaitableSet *wset_ = nullptr;
    };

    class WaitableSet : public TableEntry
    {
    public:
        void add_waitable(Waitable &waitable)
        {
            if (std::find(waitables_.begin(), waitables_.end(), &waitable) == waitables_.end())
            {
                waitables_.push_back(&waitable);
            }
        }

        void remove_waitable(Waitable &waitable)
        {
            auto it = std::find(waitables_.begin(), waitables_.end(), &waitable);
            if (it != waitables_.end())
            {
                waitables_.erase(it);
            }
        }

        bool has_pending_event() const
        {
            return std::any_of(waitables_.begin(), waitables_.end(), [](const Waitable *w)
                               { return w && w->has_pending_event(); });
        }

        Event take_pending_event(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, waitables_.empty(), "waitable set empty");
            for (auto *w : waitables_)
            {
                if (w != nullptr && w->has_pending_event())
                {
                    return w->get_pending_event(trap);
                }
            }
            trap_if(trap_cx, true, "waitable set missing event");
            return {};
        }

        void drop(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !waitables_.empty(), "waitable set not empty");
            trap_if(trap_cx, num_waiting_ != 0, "waitable set has waiters");
        }

        void begin_wait()
        {
            num_waiting_ += 1;
        }

        void end_wait()
        {
            if (num_waiting_ > 0)
            {
                num_waiting_ -= 1;
            }
        }

        uint32_t num_waiting() const
        {
            return num_waiting_;
        }

    private:
        std::vector<Waitable *> waitables_;
        uint32_t num_waiting_ = 0;
    };

    inline void Waitable::join(WaitableSet *set, const HostTrap &)
    {
        if (wset_ == set)
        {
            return;
        }
        if (wset_)
        {
            wset_->remove_waitable(*this);
        }
        wset_ = set;
        if (wset_)
        {
            wset_->add_waitable(*this);
        }
    }

    inline void Waitable::drop(const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, has_pending_event(), "waitable drop with pending event");
        if (wset_)
        {
            wset_->remove_waitable(*this);
            wset_ = nullptr;
        }
    }

    class InstanceTable
    {
    public:
        uint32_t add(const std::shared_ptr<TableEntry> &entry, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !entry, "null table entry");
            uint32_t index;
            if (!free_.empty())
            {
                index = free_.back();
                free_.pop_back();
                entries_[index] = entry;
            }
            else
            {
                trap_if(trap_cx, entries_.size() >= (1u << 30), "instance table overflow");
                entries_.push_back(entry);
                index = static_cast<uint32_t>(entries_.size() - 1);
            }
            return index;
        }

        std::shared_ptr<TableEntry> get_entry(uint32_t index, const HostTrap &trap) const
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, index == 0 || index >= entries_.size(), "table index out of bounds");
            auto entry = entries_[index];
            trap_if(trap_cx, !entry, "table slot empty");
            return entry;
        }

        std::shared_ptr<TableEntry> remove_entry(uint32_t index, const HostTrap &trap)
        {
            auto entry = get_entry(index, trap);
            entries_[index].reset();
            free_.push_back(index);
            return entry;
        }

        template <typename T>
        std::shared_ptr<T> get(uint32_t index, const HostTrap &trap) const
        {
            auto base = get_entry(index, trap);
            auto derived = std::dynamic_pointer_cast<T>(base);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !derived, "table entry type mismatch");
            return derived;
        }

        template <typename T>
        std::shared_ptr<T> remove(uint32_t index, const HostTrap &trap)
        {
            auto base = remove_entry(index, trap);
            auto derived = std::dynamic_pointer_cast<T>(base);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !derived, "table entry type mismatch");
            return derived;
        }

    private:
        std::vector<std::shared_ptr<TableEntry>> entries_{nullptr};
        std::vector<uint32_t> free_;
    };

    struct StreamDescriptor
    {
        uint32_t element_size = 0;
        uint32_t alignment = 1;
        std::type_index type = typeid(void);
    };

    template <typename T>
    StreamDescriptor make_stream_descriptor()
    {
        return StreamDescriptor{ValTrait<T>::size, ValTrait<T>::alignment, std::type_index(typeid(T))};
    }

    struct FutureDescriptor
    {
        uint32_t element_size = 0;
        uint32_t alignment = 1;
        std::type_index type = typeid(void);
    };

    template <typename T>
    FutureDescriptor make_future_descriptor()
    {
        return FutureDescriptor{ValTrait<T>::size, ValTrait<T>::alignment, std::type_index(typeid(T))};
    }

    inline uint8_t normalize_alignment(uint32_t alignment)
    {
        if (alignment == 0)
        {
            return 1;
        }
        return static_cast<uint8_t>(std::min<uint32_t>(alignment, 255));
    }

    inline void ensure_memory_range(const LiftLowerContext &cx, uint32_t ptr, uint32_t count, uint32_t alignment, uint32_t elem_size)
    {
        auto align_value = normalize_alignment(alignment);
        trap_if(cx, ptr != align_to(ptr, align_value), "misaligned memory access");
        uint64_t total_bytes = static_cast<uint64_t>(count) * elem_size;
        trap_if(cx, ptr + total_bytes > cx.opts.memory.size(), "memory overflow");
    }

    inline void write_event_fields(GuestMemory mem, uint32_t ptr, uint32_t p1, uint32_t p2, const HostTrap &trap)
    {
        if (ptr + 8 > mem.size())
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, true, "event write out of bounds");
        }
        std::memcpy(mem.data() + ptr, &p1, sizeof(uint32_t));
        std::memcpy(mem.data() + ptr + sizeof(uint32_t), &p2, sizeof(uint32_t));
    }

    enum class CopyResult : uint32_t
    {
        Completed = 0,
        Dropped = 1,
        Cancelled = 2
    };

    enum class CopyState : uint8_t
    {
        Idle = 0,
        Copying = 1,
        Done = 2
    };

    inline uint32_t pack_copy_result(CopyResult result, uint32_t progress)
    {
        return static_cast<uint32_t>(result) | (progress << 4);
    }

    inline void validate_descriptor(const StreamDescriptor &expected, const StreamDescriptor &actual, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, expected.element_size != actual.element_size, "stream descriptor size mismatch");
        trap_if(trap_cx, expected.alignment != actual.alignment, "stream descriptor alignment mismatch");
        trap_if(trap_cx, expected.type != actual.type, "stream descriptor type mismatch");
    }

    inline void validate_descriptor(const FutureDescriptor &expected, const FutureDescriptor &actual, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, expected.element_size != actual.element_size, "future descriptor size mismatch");
        trap_if(trap_cx, expected.alignment != actual.alignment, "future descriptor alignment mismatch");
        trap_if(trap_cx, expected.type != actual.type, "future descriptor type mismatch");
    }

    class ReadableStreamEnd;
    class WritableStreamEnd;

    struct SharedStreamState
    {
        explicit SharedStreamState(const StreamDescriptor &desc) : descriptor(desc) {}

        StreamDescriptor descriptor;
        std::deque<std::vector<uint8_t>> queue;
        bool readable_dropped = false;
        bool writable_dropped = false;

        struct PendingRead
        {
            std::shared_ptr<LiftLowerContext> cx;
            uint32_t ptr = 0;
            uint32_t requested = 0;
            uint32_t progress = 0;
            uint32_t handle_index = 0;
            ReadableStreamEnd *endpoint = nullptr;
        };

        std::optional<PendingRead> pending_read;
    };

    inline void copy_into_queue(const std::shared_ptr<LiftLowerContext> &cx, uint32_t ptr, uint32_t count, SharedStreamState &state, const HostTrap &trap)
    {
        if (count == 0)
        {
            return;
        }
        ensure_memory_range(*cx, ptr, count, state.descriptor.alignment, state.descriptor.element_size);
        auto *src = cx->opts.memory.data() + ptr;
        for (uint32_t i = 0; i < count; ++i)
        {
            std::vector<uint8_t> bytes(state.descriptor.element_size);
            std::memcpy(bytes.data(), src + i * state.descriptor.element_size, state.descriptor.element_size);
            state.queue.push_back(std::move(bytes));
        }
    }

    inline uint32_t copy_from_queue(const std::shared_ptr<LiftLowerContext> &cx,
                                    uint32_t ptr,
                                    uint32_t offset,
                                    uint32_t max_count,
                                    SharedStreamState &state,
                                    const HostTrap &trap)
    {
        if (max_count == 0)
        {
            return 0;
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, state.queue.size() > std::numeric_limits<uint32_t>::max(), "stream queue size overflow");
        uint32_t available = std::min<uint32_t>(max_count, static_cast<uint32_t>(state.queue.size()));
        if (available == 0)
        {
            return 0;
        }
        ensure_memory_range(*cx, ptr, offset + available, state.descriptor.alignment, state.descriptor.element_size);
        auto *dest = cx->opts.memory.data() + ptr + offset * state.descriptor.element_size;
        for (uint32_t i = 0; i < available; ++i)
        {
            const auto &bytes = state.queue.front();
            trap_if(trap_cx, bytes.size() != state.descriptor.element_size, "stream element size mismatch");
            std::memcpy(dest + i * state.descriptor.element_size, bytes.data(), state.descriptor.element_size);
            state.queue.pop_front();
        }
        return available;
    }

    inline void satisfy_pending_read(SharedStreamState &state, const HostTrap &trap);

    class ReadableStreamEnd : public Waitable
    {
    public:
        explicit ReadableStreamEnd(std::shared_ptr<SharedStreamState> shared) : shared_(std::move(shared)) {}

        const StreamDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t read(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, uint32_t n, bool sync, const HostTrap &trap);
        uint32_t cancel(bool sync, const HostTrap &trap);
        void drop(const HostTrap &trap);
        void complete_async(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, CopyResult result, uint32_t progress, const HostTrap &trap);

    private:
        std::shared_ptr<SharedStreamState> shared_;
        CopyState state_ = CopyState::Idle;
    };

    class WritableStreamEnd : public Waitable
    {
    public:
        explicit WritableStreamEnd(std::shared_ptr<SharedStreamState> shared) : shared_(std::move(shared)) {}

        const StreamDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t write(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, uint32_t n, const HostTrap &trap);
        uint32_t cancel(bool sync, const HostTrap &trap);
        void drop(const HostTrap &trap);

    private:
        std::shared_ptr<SharedStreamState> shared_;
        CopyState state_ = CopyState::Idle;
    };

    inline uint32_t ReadableStreamEnd::read(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, uint32_t n, bool sync, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !shared_, "stream state missing");
        trap_if(trap_cx, shared_->descriptor.element_size == 0, "invalid stream descriptor");
        trap_if(trap_cx, state_ != CopyState::Idle, "stream read busy");

        uint32_t consumed = copy_from_queue(cx, ptr, 0, n, *shared_, trap);
        if (consumed > 0 || n == 0)
        {
            set_pending_event({EventCode::STREAM_READ, handle_index, pack_copy_result(CopyResult::Completed, consumed)});
            auto event = get_pending_event(trap);
            state_ = CopyState::Idle;
            return event.payload;
        }

        if (shared_->writable_dropped)
        {
            set_pending_event({EventCode::STREAM_READ, handle_index, pack_copy_result(CopyResult::Dropped, 0)});
            auto event = get_pending_event(trap);
            state_ = CopyState::Done;
            return event.payload;
        }

        trap_if(trap_cx, sync, "sync stream read would block");
        shared_->pending_read = SharedStreamState::PendingRead{cx, ptr, n, 0, handle_index, this};
        state_ = CopyState::Copying;
        return BLOCKED;
    }

    inline uint32_t ReadableStreamEnd::cancel(bool sync, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, state_ != CopyState::Copying, "no pending stream read");
        trap_if(trap_cx, !shared_ || !shared_->pending_read, "no pending stream read");

        auto pending = std::move(*shared_->pending_read);
        shared_->pending_read.reset();
        auto payload = pack_copy_result(CopyResult::Cancelled, pending.progress);
        set_pending_event({EventCode::STREAM_READ, pending.handle_index, payload});
        state_ = CopyState::Done;

        if (sync)
        {
            auto event = get_pending_event(trap);
            return event.payload;
        }
        if (pending.cx)
        {
            pending.cx->notify_async_event(EventCode::STREAM_READ, pending.handle_index, payload);
        }
        return BLOCKED;
    }

    inline void ReadableStreamEnd::drop(const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, state_ == CopyState::Copying, "cannot drop pending stream read");
        trap_if(trap_cx, shared_ && shared_->pending_read.has_value(), "pending read must complete before drop");
        if (shared_)
        {
            shared_->readable_dropped = true;
        }
        state_ = CopyState::Done;
        Waitable::drop(trap);
    }

    inline void ReadableStreamEnd::complete_async(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, CopyResult result, uint32_t progress, const HostTrap &trap)
    {
        auto payload = pack_copy_result(result, progress);
        set_pending_event({EventCode::STREAM_READ, handle_index, payload});
        state_ = (result == CopyResult::Completed) ? CopyState::Idle : CopyState::Done;
        if (cx)
        {
            cx->notify_async_event(EventCode::STREAM_READ, handle_index, payload);
        }
    }

    inline void satisfy_pending_read(SharedStreamState &state, const HostTrap &trap)
    {
        if (!state.pending_read)
        {
            return;
        }
        auto &pending = *state.pending_read;
        uint32_t remaining = pending.requested - pending.progress;
        uint32_t consumed = copy_from_queue(pending.cx, pending.ptr, pending.progress, remaining, state, trap);
        pending.progress += consumed;
        if (pending.progress >= pending.requested)
        {
            pending.endpoint->complete_async(pending.cx, pending.handle_index, CopyResult::Completed, pending.progress, trap);
            state.pending_read.reset();
        }
    }

    inline uint32_t WritableStreamEnd::write(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, uint32_t n, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !shared_, "stream state missing");
        trap_if(trap_cx, shared_->descriptor.element_size == 0, "invalid stream descriptor");
        trap_if(trap_cx, state_ != CopyState::Idle, "stream write busy");

        copy_into_queue(cx, ptr, n, *shared_, trap);
        satisfy_pending_read(*shared_, trap);

        set_pending_event({EventCode::STREAM_WRITE, handle_index, pack_copy_result(CopyResult::Completed, n)});
        auto event = get_pending_event(trap);
        state_ = CopyState::Idle;
        return event.payload;
    }

    inline uint32_t WritableStreamEnd::cancel(bool, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, true, "no pending stream write");
        return BLOCKED;
    }

    inline void WritableStreamEnd::drop(const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, state_ == CopyState::Copying, "cannot drop pending stream write");
        if (shared_)
        {
            if (shared_->pending_read)
            {
                auto pending = std::move(*shared_->pending_read);
                shared_->pending_read.reset();
                pending.endpoint->complete_async(pending.cx, pending.handle_index, CopyResult::Dropped, pending.progress, trap);
            }
            shared_->writable_dropped = true;
        }
        state_ = CopyState::Done;
        Waitable::drop(trap);
    }

    class ReadableFutureEnd;
    class WritableFutureEnd;

    struct SharedFutureState
    {
        explicit SharedFutureState(const FutureDescriptor &desc) : descriptor(desc), value(desc.element_size) {}

        FutureDescriptor descriptor;
        bool readable_dropped = false;
        bool writable_dropped = false;
        bool value_ready = false;
        std::vector<uint8_t> value;

        struct PendingRead
        {
            std::shared_ptr<LiftLowerContext> cx;
            uint32_t ptr = 0;
            uint32_t handle_index = 0;
            ReadableFutureEnd *endpoint = nullptr;
        };

        std::optional<PendingRead> pending_read;
    };

    class ReadableFutureEnd : public Waitable
    {
    public:
        explicit ReadableFutureEnd(std::shared_ptr<SharedFutureState> shared) : shared_(std::move(shared)) {}

        const FutureDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t read(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, bool sync, const HostTrap &trap);
        uint32_t cancel(bool sync, const HostTrap &trap);
        void drop(const HostTrap &trap);
        void complete_async(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, CopyResult result, uint32_t progress, const HostTrap &trap);

    private:
        std::shared_ptr<SharedFutureState> shared_;
        CopyState state_ = CopyState::Idle;
    };

    class WritableFutureEnd : public Waitable
    {
    public:
        explicit WritableFutureEnd(std::shared_ptr<SharedFutureState> shared) : shared_(std::move(shared)) {}

        const FutureDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t write(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, const HostTrap &trap);
        uint32_t cancel(bool sync, const HostTrap &trap);
        void drop(const HostTrap &trap);

    private:
        std::shared_ptr<SharedFutureState> shared_;
        CopyState state_ = CopyState::Idle;
    };

    inline uint32_t ReadableFutureEnd::read(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, bool sync, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !shared_, "future state missing");
        trap_if(trap_cx, shared_->descriptor.element_size == 0, "invalid future descriptor");
        trap_if(trap_cx, state_ != CopyState::Idle, "future read busy");

        if (shared_->value_ready)
        {
            ensure_memory_range(*cx, ptr, 1, shared_->descriptor.alignment, shared_->descriptor.element_size);
            std::memcpy(cx->opts.memory.data() + ptr, shared_->value.data(), shared_->descriptor.element_size);
            set_pending_event({EventCode::FUTURE_READ, handle_index, pack_copy_result(CopyResult::Completed, 1)});
            auto event = get_pending_event(trap);
            state_ = CopyState::Idle;
            return event.payload;
        }

        if (shared_->writable_dropped)
        {
            set_pending_event({EventCode::FUTURE_READ, handle_index, pack_copy_result(CopyResult::Dropped, 0)});
            auto event = get_pending_event(trap);
            state_ = CopyState::Done;
            return event.payload;
        }

        trap_if(trap_cx, sync, "sync future read would block");
        shared_->pending_read = SharedFutureState::PendingRead{cx, ptr, handle_index, this};
        state_ = CopyState::Copying;
        return BLOCKED;
    }

    inline uint32_t ReadableFutureEnd::cancel(bool sync, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, state_ != CopyState::Copying, "no pending future read");
        trap_if(trap_cx, !shared_ || !shared_->pending_read, "no pending future read");

        auto pending = std::move(*shared_->pending_read);
        shared_->pending_read.reset();
        auto payload = pack_copy_result(CopyResult::Cancelled, 0);
        set_pending_event({EventCode::FUTURE_READ, pending.handle_index, payload});
        state_ = CopyState::Done;

        if (sync)
        {
            auto event = get_pending_event(trap);
            return event.payload;
        }
        if (pending.cx)
        {
            pending.cx->notify_async_event(EventCode::FUTURE_READ, pending.handle_index, payload);
        }
        return BLOCKED;
    }

    inline void ReadableFutureEnd::drop(const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, state_ == CopyState::Copying, "cannot drop pending future read");
        trap_if(trap_cx, shared_ && shared_->pending_read.has_value(), "pending future read must complete before drop");
        if (shared_)
        {
            shared_->readable_dropped = true;
        }
        state_ = CopyState::Done;
        Waitable::drop(trap);
    }

    inline void ReadableFutureEnd::complete_async(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, CopyResult result, uint32_t progress, const HostTrap &trap)
    {
        auto payload = pack_copy_result(result, progress);
        set_pending_event({EventCode::FUTURE_READ, handle_index, payload});
        state_ = (result == CopyResult::Completed) ? CopyState::Idle : CopyState::Done;
        if (cx)
        {
            cx->notify_async_event(EventCode::FUTURE_READ, handle_index, payload);
        }
    }

    inline uint32_t WritableFutureEnd::write(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !shared_, "future state missing");
        trap_if(trap_cx, shared_->descriptor.element_size == 0, "invalid future descriptor");
        trap_if(trap_cx, shared_->value_ready, "future already resolved");

        ensure_memory_range(*cx, ptr, 1, shared_->descriptor.alignment, shared_->descriptor.element_size);
        std::memcpy(shared_->value.data(), cx->opts.memory.data() + ptr, shared_->descriptor.element_size);
        shared_->value_ready = true;

        if (shared_->pending_read)
        {
            auto pending = std::move(*shared_->pending_read);
            shared_->pending_read.reset();
            ensure_memory_range(*pending.cx, pending.ptr, 1, shared_->descriptor.alignment, shared_->descriptor.element_size);
            std::memcpy(pending.cx->opts.memory.data() + pending.ptr, shared_->value.data(), shared_->descriptor.element_size);
            pending.endpoint->complete_async(pending.cx, pending.handle_index, CopyResult::Completed, 1, trap);
        }

        set_pending_event({EventCode::FUTURE_WRITE, handle_index, pack_copy_result(CopyResult::Completed, 1)});
        auto event = get_pending_event(trap);
        state_ = CopyState::Idle;
        return event.payload;
    }

    inline uint32_t WritableFutureEnd::cancel(bool, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, true, "no pending future write");
        return BLOCKED;
    }

    inline void WritableFutureEnd::drop(const HostTrap &trap)
    {
        if (shared_ && !shared_->value_ready)
        {
            if (shared_->pending_read)
            {
                auto pending = std::move(*shared_->pending_read);
                shared_->pending_read.reset();
                pending.endpoint->complete_async(pending.cx, pending.handle_index, CopyResult::Dropped, 0, trap);
            }
            shared_->writable_dropped = true;
        }
        state_ = CopyState::Done;
        Waitable::drop(trap);
    }

    struct ResourceType
    {
        ComponentInstance *impl = nullptr;
        std::function<void(uint32_t)> dtor;

        ResourceType() = default;

        explicit ResourceType(ComponentInstance &instance, std::function<void(uint32_t)> destructor = {})
            : impl(&instance), dtor(std::move(destructor)) {}
    };

    struct HandleElement
    {
        uint32_t rep = 0;
        bool own = false;
        LiftLowerContext *scope = nullptr;
        uint32_t lend_count = 0;
    };

    class HandleTable
    {
    public:
        static constexpr uint32_t MAX_LENGTH = 1u << 30;

        HandleElement &get(uint32_t index, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, index >= entries_.size(), "resource index out of bounds");
            auto &slot = entries_[index];
            trap_if(trap_cx, !slot.has_value(), "resource slot empty");
            return slot.value();
        }

        const HandleElement &get(uint32_t index, const HostTrap &trap) const
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, index >= entries_.size(), "resource index out of bounds");
            const auto &slot = entries_[index];
            trap_if(trap_cx, !slot.has_value(), "resource slot empty");
            return slot.value();
        }

        uint32_t add(const HandleElement &element, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            uint32_t index;
            if (!free_.empty())
            {
                index = free_.back();
                free_.pop_back();
                entries_[index] = element;
            }
            else
            {
                trap_if(trap_cx, entries_.size() >= MAX_LENGTH, "resource table overflow");
                entries_.push_back(element);
                index = static_cast<uint32_t>(entries_.size() - 1);
            }
            return index;
        }

        HandleElement remove(uint32_t index, const HostTrap &trap)
        {
            HandleElement element = get(index, trap);
            entries_[index].reset();
            free_.push_back(index);
            return element;
        }

        const std::vector<std::optional<HandleElement>> &entries() const
        {
            return entries_;
        }

        const std::vector<uint32_t> &free_list() const
        {
            return free_;
        }

    private:
        std::vector<std::optional<HandleElement>> entries_{std::nullopt};
        std::vector<uint32_t> free_;
    };

    class HandleTables
    {
    public:
        HandleElement &get(ResourceType &rt, uint32_t index, const HostTrap &trap)
        {
            return table(rt).get(index, trap);
        }

        const HandleElement &get(const ResourceType &rt, uint32_t index, const HostTrap &trap) const
        {
            auto it = tables_.find(&rt);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, it == tables_.end(), "resource table missing");
            return it->second.get(index, trap);
        }

        uint32_t add(ResourceType &rt, const HandleElement &element, const HostTrap &trap)
        {
            return table(rt).add(element, trap);
        }

        HandleElement remove(ResourceType &rt, uint32_t index, const HostTrap &trap)
        {
            return table(rt).remove(index, trap);
        }

        HandleTable &table(ResourceType &rt)
        {
            return tables_[&rt];
        }

    private:
        std::unordered_map<const ResourceType *, HandleTable> tables_;
    };

    struct ComponentInstance
    {
        Store *store = nullptr;
        bool may_leave = true;
        bool may_enter = true;
        bool exclusive = false;
        uint32_t backpressure = 0;
        uint32_t num_waiting_to_enter = 0;
        HandleTables handles;
        InstanceTable table;
    };

    inline void ensure_may_leave(ComponentInstance &inst, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !inst.may_leave, "component may not leave");
    }

    inline void canon_backpressure_set(ComponentInstance &inst, bool enabled)
    {
        inst.backpressure = enabled ? 1u : 0u;
    }

    inline void canon_backpressure_inc(ComponentInstance &inst, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst.backpressure >= 0x1'0000u, "backpressure overflow");
        inst.backpressure += 1;
    }

    inline void canon_backpressure_dec(ComponentInstance &inst, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst.backpressure == 0, "backpressure underflow");
        inst.backpressure -= 1;
    }

    class ContextLocalStorage
    {
    public:
        static constexpr uint32_t LENGTH = 1;

        ContextLocalStorage() = default;

        void set(uint32_t index, int32_t value)
        {
            storage_[index] = value;
        }

        int32_t get(uint32_t index) const
        {
            return storage_[index];
        }

    private:
        std::array<int32_t, LENGTH> storage_{};
    };

    class Task : public std::enable_shared_from_this<Task>
    {
    public:
        enum class State
        {
            Initial,
            PendingCancel,
            CancelDelivered,
            Resolved
        };

        Task(ComponentInstance &instance,
             CanonicalOptions options = {},
             SupertaskPtr supertask = {},
             OnResolve on_resolve = {})
            : opts_(std::move(options)), inst_(&instance), supertask_(std::move(supertask)), on_resolve_(std::move(on_resolve))
        {
        }

        void set_thread(const std::shared_ptr<Thread> &thread)
        {
            thread_ = thread;
            if (thread_)
            {
                thread_->set_allow_cancellation(!opts_.sync);
                thread_->set_in_event_loop(opts_.callback.has_value());
                if (inst_)
                {
                    auto super = std::make_shared<Supertask>();
                    super->instance = inst_;
                    super->thread = thread_;
                    super->parent = supertask_;
                    supertask_ = std::move(super);
                }
            }
        }

        std::shared_ptr<Thread> thread() const
        {
            return thread_;
        }

        void set_on_resolve(OnResolve on_resolve)
        {
            on_resolve_ = std::move(on_resolve);
        }

        bool enter(const HostTrap &trap)
        {
            auto *thread_ptr = thread_.get();
            auto *inst = inst_;
            if (!thread_ptr || !inst)
            {
                return false;
            }

            auto has_backpressure = [inst, this]() -> bool
            {
                return inst->backpressure > 0 || (needs_exclusive() && inst->exclusive);
            };

            if (has_backpressure() || inst->num_waiting_to_enter > 0)
            {
                inst->num_waiting_to_enter += 1;
                bool completed = thread_ptr->suspend_until([has_backpressure]()
                                                           { return !has_backpressure(); },
                                                           true);
                inst->num_waiting_to_enter -= 1;
                if (!completed)
                {
                    if (state_ == State::CancelDelivered)
                    {
                        cancel(trap);
                    }
                    return false;
                }
            }

            if (needs_exclusive())
            {
                inst->exclusive = true;
            }
            return true;
        }

        void exit()
        {
            if (!inst_)
            {
                return;
            }
            if (needs_exclusive())
            {
                inst_->exclusive = false;
            }
        }

        void request_cancellation()
        {
            if (state_ != State::Initial || !thread_)
            {
                return;
            }

            if (ready_for_cancellation())
            {
                state_ = State::CancelDelivered;
            }
            else
            {
                state_ = State::PendingCancel;
            }
            thread_->request_cancellation();
        }

        bool suspend_until(Thread::ReadyFn ready, bool cancellable)
        {
            if (cancellable && state_ == State::CancelDelivered)
            {
                return false;
            }
            if (cancellable && state_ == State::PendingCancel)
            {
                state_ = State::CancelDelivered;
                return false;
            }
            if (!thread_)
            {
                return false;
            }
            bool completed = thread_->suspend_until(std::move(ready), cancellable);
            if (!completed && cancellable && state_ == State::PendingCancel)
            {
                state_ = State::CancelDelivered;
            }
            return completed;
        }

        Event yield_until(Thread::ReadyFn ready, bool cancellable)
        {
            if (!suspend_until(std::move(ready), cancellable))
            {
                return {EventCode::TASK_CANCELLED, 0, 0};
            }
            return {EventCode::NONE, 0, 0};
        }

        void return_result(std::vector<std::any> result, const HostTrap &trap)
        {
            ensure_resolvable(trap);
            if (on_resolve_)
            {
                on_resolve_(std::optional<std::vector<std::any>>(std::move(result)));
            }
            state_ = State::Resolved;
        }

        void cancel(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ != State::CancelDelivered, "task cancellation not delivered");
            trap_if(trap_cx, num_borrows_ > 0, "task has outstanding borrows");
            if (on_resolve_)
            {
                on_resolve_(std::nullopt);
            }
            state_ = State::Resolved;
        }

        State state() const
        {
            return state_;
        }

        ContextLocalStorage &context()
        {
            return context_;
        }

        const ContextLocalStorage &context() const
        {
            return context_;
        }

        ComponentInstance *component_instance() const
        {
            return inst_;
        }

        const CanonicalOptions &options() const
        {
            return opts_;
        }

        void incr_borrows()
        {
            num_borrows_ += 1;
        }

        void decr_borrows()
        {
            if (num_borrows_ > 0)
            {
                num_borrows_ -= 1;
            }
        }

    private:
        bool needs_exclusive() const
        {
            return opts_.sync || opts_.callback.has_value();
        }

        void ensure_resolvable(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ == State::Resolved, "task already resolved");
            trap_if(trap_cx, num_borrows_ > 0, "task has outstanding borrows");
        }

        bool ready_for_cancellation() const
        {
            if (!thread_)
            {
                return false;
            }
            return thread_->cancellable() && !(thread_->in_event_loop() && inst_ && inst_->exclusive);
        }

        CanonicalOptions opts_;
        ComponentInstance *inst_ = nullptr;
        SupertaskPtr supertask_;
        OnResolve on_resolve_;
        uint32_t num_borrows_ = 0;
        std::shared_ptr<Thread> thread_;
        State state_ = State::Initial;
        ContextLocalStorage context_{};
    };

    inline void canon_task_return(Task &task, std::vector<std::any> result, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, task.options().sync, "task.return requires async context");
        task.return_result(std::move(result), trap);
    }

    inline void canon_task_cancel(Task &task, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, task.options().sync, "task.cancel requires async context");
        task.cancel(trap);
    }

    inline uint32_t canon_yield(bool cancellable, Task &task, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        if (task.state() == Task::State::CancelDelivered || task.state() == Task::State::PendingCancel)
        {
            return 1u;
        }
        auto event = task.yield_until([]
                                      { return true; },
                                      cancellable);
        return event.code == EventCode::TASK_CANCELLED ? 1u : 0u;
    }

    struct Subtask : Waitable
    {
    };

    struct Future
    {
    };

    inline void LiftLowerContext::track_owning_lend(HandleElement &lending_handle)
    {
        trap_if(*this, !lending_handle.own, "lender must own resource");
        lending_handle.lend_count += 1;
        lenders.push_back(&lending_handle);
    }

    inline void LiftLowerContext::exit_call()
    {
        trap_if(*this, borrow_count != 0, "borrow count mismatch on exit");
        for (auto *handle : lenders)
        {
            if (handle && handle->lend_count > 0)
            {
                handle->lend_count -= 1;
            }
        }
        lenders.clear();
    }

    inline uint32_t canon_resource_new(ComponentInstance &inst, ResourceType &rt, uint32_t rep, const HostTrap &trap)
    {
        HandleElement element;
        element.rep = rep;
        element.own = true;
        return inst.handles.add(rt, element, trap);
    }

    inline void canon_resource_drop(ComponentInstance &inst, ResourceType &rt, uint32_t index, const HostTrap &trap)
    {
        HandleElement element = inst.handles.remove(rt, index, trap);
        auto trap_cx = make_trap_context(trap);
        if (element.own)
        {
            trap_if(trap_cx, element.scope != nullptr, "own handle cannot have borrow scope");
            trap_if(trap_cx, element.lend_count != 0, "resource has outstanding lends");
            trap_if(trap_cx, rt.impl != nullptr && (&inst != rt.impl) && !rt.impl->may_enter, "resource impl may not enter");
            if (rt.dtor)
            {
                rt.dtor(element.rep);
            }
        }
        else
        {
            trap_if(trap_cx, element.scope == nullptr, "borrow scope missing");
            trap_if(trap_cx, element.scope->borrow_count == 0, "borrow scope underflow");
            element.scope->borrow_count -= 1;
        }
    }

    inline uint32_t canon_resource_rep(ComponentInstance &inst, ResourceType &rt, uint32_t index, const HostTrap &trap)
    {
        const HandleElement &element = inst.handles.get(rt, index, trap);
        return element.rep;
    }

    inline int32_t canon_context_get(Task &task, uint32_t index, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, index >= ContextLocalStorage::LENGTH, "context index out of bounds");
        return task.context().get(index);
    }

    inline void canon_context_set(Task &task, uint32_t index, int32_t value, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, index >= ContextLocalStorage::LENGTH, "context index out of bounds");
        task.context().set(index, value);
    }

    inline uint32_t canon_waitable_set_new(ComponentInstance &inst, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto wset = std::make_shared<WaitableSet>();
        return inst.table.add(wset, trap);
    }

    inline uint32_t canon_waitable_set_wait(bool /*cancellable*/, GuestMemory mem, ComponentInstance &inst, uint32_t set_index, uint32_t ptr, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto wset = inst.table.get<WaitableSet>(set_index, trap);
        wset->begin_wait();
        if (!wset->has_pending_event())
        {
            wset->end_wait();
            write_event_fields(mem, ptr, 0, 0, trap);
            return BLOCKED;
        }
        auto event = wset->take_pending_event(trap);
        wset->end_wait();
        write_event_fields(mem, ptr, event.index, event.payload, trap);
        return static_cast<uint32_t>(event.code);
    }

    inline uint32_t canon_waitable_set_poll(bool /*cancellable*/, GuestMemory mem, ComponentInstance &inst, uint32_t set_index, uint32_t ptr, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto wset = inst.table.get<WaitableSet>(set_index, trap);
        if (!wset->has_pending_event())
        {
            write_event_fields(mem, ptr, 0, 0, trap);
            return static_cast<uint32_t>(EventCode::NONE);
        }
        auto event = wset->take_pending_event(trap);
        write_event_fields(mem, ptr, event.index, event.payload, trap);
        return static_cast<uint32_t>(event.code);
    }

    inline void canon_waitable_set_drop(ComponentInstance &inst, uint32_t set_index, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto wset = inst.table.remove<WaitableSet>(set_index, trap);
        wset->drop(trap);
    }

    inline void canon_waitable_join(ComponentInstance &inst, uint32_t waitable_index, uint32_t set_index, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto waitable = inst.table.get<Waitable>(waitable_index, trap);
        if (set_index == 0)
        {
            waitable->join(nullptr, trap);
            return;
        }
        auto wset = inst.table.get<WaitableSet>(set_index, trap);
        waitable->join(wset.get(), trap);
    }

    inline uint64_t canon_stream_new(ComponentInstance &inst, const StreamDescriptor &descriptor, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, descriptor.element_size == 0, "stream descriptor invalid");
        auto shared = std::make_shared<SharedStreamState>(descriptor);
        auto readable = std::make_shared<ReadableStreamEnd>(shared);
        auto writable = std::make_shared<WritableStreamEnd>(shared);
        uint32_t readable_index = inst.table.add(readable, trap);
        uint32_t writable_index = inst.table.add(writable, trap);
        return (static_cast<uint64_t>(writable_index) << 32) | readable_index;
    }

    inline uint32_t canon_stream_read(ComponentInstance &inst,
                                      const StreamDescriptor &descriptor,
                                      uint32_t readable_index,
                                      const std::shared_ptr<LiftLowerContext> &cx,
                                      uint32_t ptr,
                                      uint32_t n,
                                      bool sync,
                                      const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !cx, "lift/lower context required");
        auto readable = inst.table.get<ReadableStreamEnd>(readable_index, trap);
        validate_descriptor(descriptor, readable->descriptor(), trap);
        return readable->read(cx, readable_index, ptr, n, sync, trap);
    }

    inline uint32_t canon_stream_write(ComponentInstance &inst,
                                       const StreamDescriptor &descriptor,
                                       uint32_t writable_index,
                                       const std::shared_ptr<LiftLowerContext> &cx,
                                       uint32_t ptr,
                                       uint32_t n,
                                       const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !cx, "lift/lower context required");
        auto writable = inst.table.get<WritableStreamEnd>(writable_index, trap);
        validate_descriptor(descriptor, writable->descriptor(), trap);
        return writable->write(cx, writable_index, ptr, n, trap);
    }

    inline uint32_t canon_stream_cancel_read(ComponentInstance &inst, uint32_t readable_index, bool sync, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto readable = inst.table.get<ReadableStreamEnd>(readable_index, trap);
        return readable->cancel(sync, trap);
    }

    inline uint32_t canon_stream_cancel_write(ComponentInstance &inst, uint32_t writable_index, bool sync, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto writable = inst.table.get<WritableStreamEnd>(writable_index, trap);
        return writable->cancel(sync, trap);
    }

    inline void canon_stream_drop_readable(ComponentInstance &inst, uint32_t readable_index, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto readable = inst.table.remove<ReadableStreamEnd>(readable_index, trap);
        readable->drop(trap);
    }

    inline void canon_stream_drop_writable(ComponentInstance &inst, uint32_t writable_index, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto writable = inst.table.remove<WritableStreamEnd>(writable_index, trap);
        writable->drop(trap);
    }

    inline uint64_t canon_future_new(ComponentInstance &inst, const FutureDescriptor &descriptor, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, descriptor.element_size == 0, "future descriptor invalid");
        auto shared = std::make_shared<SharedFutureState>(descriptor);
        auto readable = std::make_shared<ReadableFutureEnd>(shared);
        auto writable = std::make_shared<WritableFutureEnd>(shared);
        uint32_t readable_index = inst.table.add(readable, trap);
        uint32_t writable_index = inst.table.add(writable, trap);
        return (static_cast<uint64_t>(writable_index) << 32) | readable_index;
    }

    inline uint32_t canon_future_read(ComponentInstance &inst,
                                      const FutureDescriptor &descriptor,
                                      uint32_t readable_index,
                                      const std::shared_ptr<LiftLowerContext> &cx,
                                      uint32_t ptr,
                                      bool sync,
                                      const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !cx, "lift/lower context required");
        auto readable = inst.table.get<ReadableFutureEnd>(readable_index, trap);
        validate_descriptor(descriptor, readable->descriptor(), trap);
        return readable->read(cx, readable_index, ptr, sync, trap);
    }

    inline uint32_t canon_future_write(ComponentInstance &inst,
                                       const FutureDescriptor &descriptor,
                                       uint32_t writable_index,
                                       const std::shared_ptr<LiftLowerContext> &cx,
                                       uint32_t ptr,
                                       const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, !cx, "lift/lower context required");
        auto writable = inst.table.get<WritableFutureEnd>(writable_index, trap);
        validate_descriptor(descriptor, writable->descriptor(), trap);
        return writable->write(cx, writable_index, ptr, trap);
    }

    inline uint32_t canon_future_cancel_read(ComponentInstance &inst, uint32_t readable_index, bool sync, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto readable = inst.table.get<ReadableFutureEnd>(readable_index, trap);
        return readable->cancel(sync, trap);
    }

    inline uint32_t canon_future_cancel_write(ComponentInstance &inst, uint32_t writable_index, bool sync, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto writable = inst.table.get<WritableFutureEnd>(writable_index, trap);
        return writable->cancel(sync, trap);
    }

    inline void canon_future_drop_readable(ComponentInstance &inst, uint32_t readable_index, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto readable = inst.table.remove<ReadableFutureEnd>(readable_index, trap);
        readable->drop(trap);
    }

    inline void canon_future_drop_writable(ComponentInstance &inst, uint32_t writable_index, const HostTrap &trap)
    {
        ensure_may_leave(inst, trap);
        auto writable = inst.table.remove<WritableFutureEnd>(writable_index, trap);
        writable->drop(trap);
    }

    //  ----------------------------

    struct InstanceContext
    {
        HostTrap trap;
        HostUnicodeConversion convert;
        GuestRealloc realloc;

        InstanceContext() = default;

        InstanceContext(const HostTrap &trap_fn, HostUnicodeConversion convert_fn, const GuestRealloc &realloc_fn)
            : trap(trap_fn), convert(convert_fn), realloc(realloc_fn) {}

        std::unique_ptr<LiftLowerContext> createLiftLowerContext(const GuestMemory &memory,
                                                                 const Encoding &string_encoding = Encoding::Utf8,
                                                                 const std::optional<GuestPostReturn> &post_return = std::nullopt,
                                                                 bool sync = true,
                                                                 const std::optional<GuestCallback> &callback = std::nullopt)
        {
            CanonicalOptions options;
            options.string_encoding = string_encoding;
            options.memory = memory;
            options.realloc = realloc;
            options.post_return = post_return;
            options.sync = sync;
            options.callback = callback;
            return createLiftLowerContext(std::move(options));
        }

        std::unique_ptr<LiftLowerContext> createLiftLowerContext(CanonicalOptions options)
        {
            if (!options.realloc)
            {
                options.realloc = realloc;
            }
            LiftLowerOptions opts(options.string_encoding, options.memory, options.realloc);
            auto retVal = std::make_unique<LiftLowerContext>(trap, convert, opts);
            retVal->set_canonical_options(std::move(options));
            return retVal;
        }
    };

    inline std::unique_ptr<InstanceContext> createInstanceContext(const HostTrap &trap, HostUnicodeConversion convert, const GuestRealloc &realloc)
    {
        auto retVal = std::make_unique<InstanceContext>();
        retVal->trap = trap;
        retVal->convert = convert;
        retVal->realloc = realloc;
        return retVal;
    }
}

#endif
