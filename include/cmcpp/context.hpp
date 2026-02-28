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
#include <thread>
#include <condition_variable>
#include <typeindex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>
#include <mutex>
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
    using ReclaimBuffer = std::function<void()>;

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

    enum class SuspendResult : uint32_t
    {
        NOT_CANCELLED = 0,
        CANCELLED = 1,
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

    // Minimal representation of a Component Model function type.
    // Mirrors Python's FuncType with the fields needed by Task.
    struct FuncType
    {
        bool async_ = false;
        // result descriptor intentionally opaque — used for identity checks
        // in canon_task_return validation.
        const void *result_id = nullptr;
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

    class ThreadEntry : public TableEntry
    {
    public:
        explicit ThreadEntry(std::shared_ptr<Thread> thread) : thread_(std::move(thread)) {}

        const std::shared_ptr<Thread> &thread() const
        {
            return thread_;
        }

    private:
        std::shared_ptr<Thread> thread_;
    };

    class Waitable : public TableEntry
    {
    public:
        Waitable() = default;

        void set_pending_event(const Event &event, ReclaimBuffer reclaim = {})
        {
            pending_event_ = event;
            pending_reclaim_ = std::move(reclaim);
        }

        bool has_pending_event() const
        {
            return pending_event_.has_value();
        }

        Event get_pending_event(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !pending_event_.has_value(), "waitable pending event missing");
            auto reclaim = std::move(pending_reclaim_);
            Event event = *pending_event_;
            pending_event_.reset();
            pending_reclaim_ = {};
            if (reclaim)
            {
                reclaim();
            }
            return event;
        }

        void clear_pending_event()
        {
            pending_event_.reset();
            pending_reclaim_ = {};
        }

        void join(WaitableSet *set, const HostTrap &trap);

        WaitableSet *joined_set() const
        {
            return wset_;
        }

        virtual void drop(const HostTrap &trap);

    private:
        std::optional<Event> pending_event_;
        ReclaimBuffer pending_reclaim_;
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
                trap_if(trap_cx, entries_.size() > ((1u << 28) - 1), "instance table overflow");
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
        IDLE = 1,
        SYNC_COPYING = 2,
        ASYNC_COPYING = 3,
        CANCELLING_COPY = 4,
        DONE = 5
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

    using OnCopy = std::function<void(ReclaimBuffer)>;
    using OnCopyDone = std::function<void(CopyResult)>;

    class BufferGuestImpl
    {
    public:
        static constexpr uint32_t MAX_LENGTH = (1u << 28) - 1;

        virtual ~BufferGuestImpl() = default;

        BufferGuestImpl(uint32_t elem_size, uint32_t alignment, std::shared_ptr<LiftLowerContext> cx, uint32_t ptr, uint32_t length, const HostTrap &trap)
            : elem_size_(elem_size), alignment_(alignment), cx_(std::move(cx)), ptr_(ptr), length_(length)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, length_ > MAX_LENGTH, "buffer length overflow");
            trap_if(trap_cx, !cx_, "lift/lower context required");
            if (length_ > 0)
            {
                ensure_memory_range(*cx_, ptr_, length_, alignment_, elem_size_);
            }
        }

        uint32_t remain() const
        {
            return length_ - progress_;
        }

        bool is_zero_length() const
        {
            return length_ == 0;
        }

        uint32_t progress() const
        {
            return progress_;
        }

    protected:
        uint32_t elem_size_ = 0;
        uint32_t alignment_ = 1;
        std::shared_ptr<LiftLowerContext> cx_;
        uint32_t ptr_ = 0;
        uint32_t length_ = 0;
        uint32_t progress_ = 0;
    };

    class ReadableBufferGuestImpl : public BufferGuestImpl
    {
    public:
        using BufferGuestImpl::BufferGuestImpl;

        std::vector<uint8_t> read(uint32_t n, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, n > remain(), "buffer read past end");
            std::vector<uint8_t> bytes(static_cast<std::size_t>(n) * elem_size_);
            if (n > 0)
            {
                uint32_t read_ptr = ptr_ + progress_ * elem_size_;
                ensure_memory_range(*cx_, read_ptr, n, alignment_, elem_size_);
                std::memcpy(bytes.data(), cx_->opts.memory.data() + read_ptr, bytes.size());
            }
            progress_ += n;
            return bytes;
        }
    };

    class WritableBufferGuestImpl : public BufferGuestImpl
    {
    public:
        using BufferGuestImpl::BufferGuestImpl;

        void write(const std::vector<uint8_t> &bytes, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, elem_size_ == 0, "invalid element size");
            trap_if(trap_cx, bytes.size() % elem_size_ != 0, "buffer write size mismatch");
            uint32_t n = static_cast<uint32_t>(bytes.size() / elem_size_);
            trap_if(trap_cx, n > remain(), "buffer write past end");
            if (n > 0)
            {
                uint32_t write_ptr = ptr_ + progress_ * elem_size_;
                ensure_memory_range(*cx_, write_ptr, n, alignment_, elem_size_);
                std::memcpy(cx_->opts.memory.data() + write_ptr, bytes.data(), bytes.size());
            }
            progress_ += n;
        }
    };

    struct SharedStreamState
    {
        explicit SharedStreamState(const StreamDescriptor &desc) : descriptor(desc) {}

        StreamDescriptor descriptor;
        bool dropped = false;

        std::shared_ptr<BufferGuestImpl> pending_buffer;
        OnCopy pending_on_copy;
        OnCopyDone pending_on_copy_done;

        std::mutex mu;
        std::condition_variable cv;

        void notify_all()
        {
            cv.notify_all();
        }

        template <typename Pred>
        void wait_until(Pred pred)
        {
            std::unique_lock<std::mutex> lock(mu);
            cv.wait(lock, std::move(pred));
        }

        void reset_pending()
        {
            pending_buffer.reset();
            pending_on_copy = {};
            pending_on_copy_done = {};
        }

        void set_pending(const std::shared_ptr<BufferGuestImpl> &buffer, OnCopy on_copy, OnCopyDone on_copy_done)
        {
            pending_buffer = buffer;
            pending_on_copy = std::move(on_copy);
            pending_on_copy_done = std::move(on_copy_done);
        }

        void reset_and_notify_pending(CopyResult result)
        {
            auto on_copy_done = std::move(pending_on_copy_done);
            reset_pending();
            if (on_copy_done)
            {
                on_copy_done(result);
            }
        }

        void cancel()
        {
            if (pending_buffer)
            {
                reset_and_notify_pending(CopyResult::Cancelled);
            }
        }

        void drop()
        {
            if (dropped)
            {
                return;
            }
            dropped = true;
            if (pending_buffer)
            {
                reset_and_notify_pending(CopyResult::Dropped);
            }
        }

        void read(const std::shared_ptr<WritableBufferGuestImpl> &dst, OnCopy on_copy, OnCopyDone on_copy_done, const HostTrap &trap)
        {
            std::scoped_lock<std::mutex> lock(mu);
            if (dropped)
            {
                on_copy_done(CopyResult::Dropped);
                return;
            }
            if (!pending_buffer)
            {
                set_pending(dst, std::move(on_copy), std::move(on_copy_done));
                return;
            }

            auto src = std::dynamic_pointer_cast<ReadableBufferGuestImpl>(pending_buffer);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !src, "stream pending buffer type mismatch");

            if (src->remain() > 0)
            {
                if (dst->remain() > 0)
                {
                    uint32_t n = std::min<uint32_t>(dst->remain(), src->remain());
                    dst->write(src->read(n, trap), trap);
                    if (pending_on_copy)
                    {
                        pending_on_copy([this]()
                                        {
                                            std::scoped_lock<std::mutex> reclaim_lock(mu);
                                            reset_pending(); });
                    }
                }
                on_copy_done(CopyResult::Completed);
                return;
            }

            reset_and_notify_pending(CopyResult::Completed);
            set_pending(dst, std::move(on_copy), std::move(on_copy_done));
        }

        void write(const std::shared_ptr<ReadableBufferGuestImpl> &src, OnCopy on_copy, OnCopyDone on_copy_done, const HostTrap &trap)
        {
            std::scoped_lock<std::mutex> lock(mu);
            if (dropped)
            {
                on_copy_done(CopyResult::Dropped);
                return;
            }
            if (!pending_buffer)
            {
                set_pending(src, std::move(on_copy), std::move(on_copy_done));
                return;
            }

            auto dst = std::dynamic_pointer_cast<WritableBufferGuestImpl>(pending_buffer);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !dst, "stream pending buffer type mismatch");

            if (dst->remain() > 0)
            {
                if (src->remain() > 0)
                {
                    uint32_t n = std::min<uint32_t>(src->remain(), dst->remain());
                    dst->write(src->read(n, trap), trap);
                    if (pending_on_copy)
                    {
                        pending_on_copy(
                            [this]()
                            {
                                std::scoped_lock<std::mutex> reclaim_lock(mu);
                                reset_pending();
                            });
                    }
                }
                on_copy_done(CopyResult::Completed);
                return;
            }

            if (src->is_zero_length() && dst->is_zero_length())
            {
                on_copy_done(CopyResult::Completed);
                return;
            }

            reset_and_notify_pending(CopyResult::Completed);
            set_pending(src, std::move(on_copy), std::move(on_copy_done));
        }
    };

    class CopyEnd : public Waitable
    {
    public:
        bool copying() const
        {
            switch (state_)
            {
            case CopyState::IDLE:
            case CopyState::DONE:
                return false;
            case CopyState::SYNC_COPYING:
            case CopyState::ASYNC_COPYING:
            case CopyState::CANCELLING_COPY:
                return true;
            }
            return false;
        }

    protected:
        CopyState state_ = CopyState::IDLE;
    };

    class ReadableStreamEnd final : public CopyEnd
    {
    public:
        explicit ReadableStreamEnd(std::shared_ptr<SharedStreamState> shared) : shared_(std::move(shared)) {}

        const StreamDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t read(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, uint32_t n, bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !shared_, "stream state missing");
            trap_if(trap_cx, state_ != CopyState::IDLE, "stream read not idle");

            auto buffer = std::make_shared<WritableBufferGuestImpl>(shared_->descriptor.element_size, shared_->descriptor.alignment, cx, ptr, n, trap);

            auto make_payload = [buffer](CopyResult result) -> uint32_t
            {
                return pack_copy_result(result, buffer->progress());
            };

            OnCopy on_copy = [this, handle_index, cx, make_payload](ReclaimBuffer reclaim)
            {
                bool notify = (state_ == CopyState::ASYNC_COPYING || state_ == CopyState::CANCELLING_COPY);
                uint32_t payload = make_payload(CopyResult::Completed);
                set_pending_event({EventCode::STREAM_READ, handle_index, payload}, std::move(reclaim));
                state_ = CopyState::IDLE;
                if (shared_)
                {
                    shared_->notify_all();
                }
                if (cx && notify)
                {
                    cx->notify_async_event(EventCode::STREAM_READ, handle_index, payload);
                }
            };

            OnCopyDone on_copy_done = [this, handle_index, cx, make_payload](CopyResult result)
            {
                bool notify = (state_ == CopyState::ASYNC_COPYING || state_ == CopyState::CANCELLING_COPY);
                uint32_t payload = make_payload(result);
                set_pending_event({EventCode::STREAM_READ, handle_index, payload});
                state_ = (result == CopyResult::Dropped) ? CopyState::DONE : CopyState::IDLE;
                if (shared_)
                {
                    shared_->notify_all();
                }
                if (cx && notify)
                {
                    cx->notify_async_event(EventCode::STREAM_READ, handle_index, payload);
                }
            };

            shared_->read(buffer, std::move(on_copy), std::move(on_copy_done), trap);

            if (!has_pending_event())
            {
                if (sync)
                {
                    state_ = CopyState::SYNC_COPYING;
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    state_ = CopyState::ASYNC_COPYING;
                    return BLOCKED;
                }
            }
            auto event = get_pending_event(trap);
            return event.payload;
        }

        uint32_t cancel(bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ != CopyState::ASYNC_COPYING, "stream cancel requires async copy");
            state_ = CopyState::CANCELLING_COPY;

            if (!has_pending_event() && shared_)
            {
                shared_->cancel();
            }

            if (!has_pending_event())
            {
                if (sync)
                {
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    return BLOCKED;
                }
            }

            auto event = get_pending_event(trap);
            return event.payload;
        }

        void drop(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, copying(), "cannot drop stream end while copying");
            if (shared_)
            {
                shared_->drop();
            }
            state_ = CopyState::DONE;
            Waitable::drop(trap);
        }

    private:
        std::shared_ptr<SharedStreamState> shared_;
    };

    class WritableStreamEnd final : public CopyEnd
    {
    public:
        explicit WritableStreamEnd(std::shared_ptr<SharedStreamState> shared) : shared_(std::move(shared)) {}

        const StreamDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t write(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, uint32_t n, bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !shared_, "stream state missing");
            trap_if(trap_cx, state_ != CopyState::IDLE, "stream write not idle");

            auto buffer = std::make_shared<ReadableBufferGuestImpl>(shared_->descriptor.element_size, shared_->descriptor.alignment, cx, ptr, n, trap);
            auto make_payload = [buffer](CopyResult result) -> uint32_t
            {
                return pack_copy_result(result, buffer->progress());
            };

            OnCopy on_copy = [this, handle_index, cx, make_payload](ReclaimBuffer reclaim)
            {
                bool notify = (state_ == CopyState::ASYNC_COPYING || state_ == CopyState::CANCELLING_COPY);
                uint32_t payload = make_payload(CopyResult::Completed);
                set_pending_event({EventCode::STREAM_WRITE, handle_index, payload}, std::move(reclaim));
                state_ = CopyState::IDLE;
                if (shared_)
                {
                    shared_->notify_all();
                }
                if (cx && notify)
                {
                    cx->notify_async_event(EventCode::STREAM_WRITE, handle_index, payload);
                }
            };

            OnCopyDone on_copy_done = [this, handle_index, cx, make_payload](CopyResult result)
            {
                bool notify = (state_ == CopyState::ASYNC_COPYING || state_ == CopyState::CANCELLING_COPY);
                uint32_t payload = make_payload(result);
                set_pending_event({EventCode::STREAM_WRITE, handle_index, payload});
                state_ = (result == CopyResult::Dropped) ? CopyState::DONE : CopyState::IDLE;
                if (shared_)
                {
                    shared_->notify_all();
                }
                if (cx && notify)
                {
                    cx->notify_async_event(EventCode::STREAM_WRITE, handle_index, payload);
                }
            };

            shared_->write(buffer, std::move(on_copy), std::move(on_copy_done), trap);

            if (!has_pending_event())
            {
                if (sync)
                {
                    state_ = CopyState::SYNC_COPYING;
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    state_ = CopyState::ASYNC_COPYING;
                    return BLOCKED;
                }
            }
            auto event = get_pending_event(trap);
            return event.payload;
        }

        uint32_t cancel(bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ != CopyState::ASYNC_COPYING, "stream cancel requires async copy");
            state_ = CopyState::CANCELLING_COPY;

            if (!has_pending_event() && shared_)
            {
                shared_->cancel();
            }

            if (!has_pending_event())
            {
                if (sync)
                {
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    return BLOCKED;
                }
            }

            auto event = get_pending_event(trap);
            return event.payload;
        }

        void drop(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, copying(), "cannot drop stream end while copying");
            if (shared_)
            {
                shared_->drop();
            }
            state_ = CopyState::DONE;
            Waitable::drop(trap);
        }

    private:
        std::shared_ptr<SharedStreamState> shared_;
    };

    struct SharedFutureState
    {
        explicit SharedFutureState(const FutureDescriptor &desc) : descriptor(desc) {}

        FutureDescriptor descriptor;
        bool dropped = false;

        std::shared_ptr<BufferGuestImpl> pending_buffer;
        OnCopyDone pending_on_copy_done;

        std::mutex mu;
        std::condition_variable cv;

        void notify_all()
        {
            cv.notify_all();
        }

        template <typename Pred>
        void wait_until(Pred pred)
        {
            std::unique_lock<std::mutex> lock(mu);
            cv.wait(lock, std::move(pred));
        }

        void reset_pending()
        {
            pending_buffer.reset();
            pending_on_copy_done = {};
        }

        void set_pending(const std::shared_ptr<BufferGuestImpl> &buffer, OnCopyDone on_copy_done)
        {
            pending_buffer = buffer;
            pending_on_copy_done = std::move(on_copy_done);
        }

        void reset_and_notify_pending(CopyResult result)
        {
            auto on_copy_done = std::move(pending_on_copy_done);
            reset_pending();
            if (on_copy_done)
            {
                on_copy_done(result);
            }
        }

        void cancel()
        {
            if (pending_buffer)
            {
                reset_and_notify_pending(CopyResult::Cancelled);
            }
        }

        void drop()
        {
            if (dropped)
            {
                return;
            }
            dropped = true;
            if (pending_buffer)
            {
                reset_and_notify_pending(CopyResult::Dropped);
            }
        }

        void read(const std::shared_ptr<WritableBufferGuestImpl> &dst, OnCopyDone on_copy_done, const HostTrap &trap)
        {
            std::scoped_lock<std::mutex> lock(mu);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, dst->remain() != 1, "future read length must be 1");

            if (dropped)
            {
                on_copy_done(CopyResult::Dropped);
                return;
            }

            if (!pending_buffer)
            {
                set_pending(dst, std::move(on_copy_done));
                return;
            }

            auto src = std::dynamic_pointer_cast<ReadableBufferGuestImpl>(pending_buffer);
            trap_if(trap_cx, !src, "future pending buffer type mismatch");
            dst->write(src->read(1, trap), trap);
            reset_and_notify_pending(CopyResult::Completed);
            on_copy_done(CopyResult::Completed);
        }

        void write(const std::shared_ptr<ReadableBufferGuestImpl> &src, OnCopyDone on_copy_done, const HostTrap &trap)
        {
            std::scoped_lock<std::mutex> lock(mu);
            if (dropped)
            {
                on_copy_done(CopyResult::Dropped);
                return;
            }

            if (!pending_buffer)
            {
                set_pending(src, std::move(on_copy_done));
                return;
            }

            auto dst = std::dynamic_pointer_cast<WritableBufferGuestImpl>(pending_buffer);
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !dst, "future pending buffer type mismatch");
            dst->write(src->read(1, trap), trap);
            reset_and_notify_pending(CopyResult::Completed);
            on_copy_done(CopyResult::Completed);
        }
    };

    class ReadableFutureEnd final : public CopyEnd
    {
    public:
        explicit ReadableFutureEnd(std::shared_ptr<SharedFutureState> shared) : shared_(std::move(shared)) {}

        const FutureDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t read(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !shared_, "future state missing");
            trap_if(trap_cx, state_ != CopyState::IDLE, "future read not idle");

            auto buffer = std::make_shared<WritableBufferGuestImpl>(shared_->descriptor.element_size, shared_->descriptor.alignment, cx, ptr, 1, trap);
            OnCopyDone on_copy_done = [this, handle_index, cx](CopyResult result)
            {
                bool notify = (state_ == CopyState::ASYNC_COPYING || state_ == CopyState::CANCELLING_COPY);
                set_pending_event({EventCode::FUTURE_READ, handle_index, static_cast<uint32_t>(result)});
                state_ = (result == CopyResult::Cancelled) ? CopyState::IDLE : CopyState::DONE;
                if (shared_)
                {
                    shared_->notify_all();
                }
                if (cx && notify)
                {
                    cx->notify_async_event(EventCode::FUTURE_READ, handle_index, static_cast<uint32_t>(result));
                }
            };

            shared_->read(buffer, std::move(on_copy_done), trap);

            if (!has_pending_event())
            {
                if (sync)
                {
                    state_ = CopyState::SYNC_COPYING;
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    state_ = CopyState::ASYNC_COPYING;
                    return BLOCKED;
                }
            }
            auto event = get_pending_event(trap);
            return event.payload;
        }

        uint32_t cancel(bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ != CopyState::ASYNC_COPYING, "future cancel requires async copy");
            state_ = CopyState::CANCELLING_COPY;
            if (!has_pending_event() && shared_)
            {
                shared_->cancel();
            }
            if (!has_pending_event())
            {
                if (sync)
                {
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    return BLOCKED;
                }
            }
            auto event = get_pending_event(trap);
            return event.payload;
        }

        void drop(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, copying(), "cannot drop future end while copying");
            if (shared_)
            {
                shared_->drop();
            }
            state_ = CopyState::DONE;
            Waitable::drop(trap);
        }

    private:
        std::shared_ptr<SharedFutureState> shared_;
    };

    class WritableFutureEnd final : public CopyEnd
    {
    public:
        explicit WritableFutureEnd(std::shared_ptr<SharedFutureState> shared) : shared_(std::move(shared)) {}

        const FutureDescriptor &descriptor() const
        {
            return shared_->descriptor;
        }

        uint32_t write(const std::shared_ptr<LiftLowerContext> &cx, uint32_t handle_index, uint32_t ptr, bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !shared_, "future state missing");
            trap_if(trap_cx, state_ != CopyState::IDLE, "future write not idle");

            auto buffer = std::make_shared<ReadableBufferGuestImpl>(shared_->descriptor.element_size, shared_->descriptor.alignment, cx, ptr, 1, trap);
            OnCopyDone on_copy_done = [this, handle_index, cx](CopyResult result)
            {
                bool notify = (state_ == CopyState::ASYNC_COPYING || state_ == CopyState::CANCELLING_COPY);
                set_pending_event({EventCode::FUTURE_WRITE, handle_index, static_cast<uint32_t>(result)});
                // For future.write, COMPLETED and DROPPED end the write.
                state_ = (result == CopyResult::Cancelled) ? CopyState::IDLE : CopyState::DONE;
                if (shared_)
                {
                    shared_->notify_all();
                }
                if (cx && notify)
                {
                    cx->notify_async_event(EventCode::FUTURE_WRITE, handle_index, static_cast<uint32_t>(result));
                }
            };

            shared_->write(buffer, std::move(on_copy_done), trap);

            if (!has_pending_event())
            {
                if (sync)
                {
                    state_ = CopyState::SYNC_COPYING;
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    state_ = CopyState::ASYNC_COPYING;
                    return BLOCKED;
                }
            }
            auto event = get_pending_event(trap);
            return event.payload;
        }

        uint32_t cancel(bool sync, const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ != CopyState::ASYNC_COPYING, "future cancel requires async copy");
            state_ = CopyState::CANCELLING_COPY;
            if (!has_pending_event() && shared_)
            {
                shared_->cancel();
            }
            if (!has_pending_event())
            {
                if (sync)
                {
                    shared_->wait_until([this]()
                                        { return has_pending_event(); });
                }
                else
                {
                    return BLOCKED;
                }
            }
            auto event = get_pending_event(trap);
            return event.payload;
        }

        void drop(const HostTrap &trap)
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, state_ != CopyState::DONE, "writable future end must be done before drop");
            if (shared_)
            {
                shared_->drop();
            }
            Waitable::drop(trap);
        }

    private:
        std::shared_ptr<SharedFutureState> shared_;
    };

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
        static constexpr uint32_t MAX_LENGTH = (1u << 28) - 1;

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
        ComponentInstance *parent = nullptr;
        bool may_leave = true;
        bool may_enter = true;
        bool exclusive = false;
        uint32_t backpressure = 0;
        uint32_t num_waiting_to_enter = 0;
        HandleTables handles;
        InstanceTable table;
        InstanceTable threads;

        std::unordered_set<const ComponentInstance *> reflexive_ancestors() const
        {
            std::unordered_set<const ComponentInstance *> s;
            const ComponentInstance *inst = this;
            while (inst != nullptr)
            {
                s.insert(inst);
                inst = inst->parent;
            }
            return s;
        }

        bool is_reflexive_ancestor_of(const ComponentInstance *other) const
        {
            while (other != nullptr)
            {
                if (this == other)
                {
                    return true;
                }
                other = other->parent;
            }
            return false;
        }
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

    inline bool call_might_be_recursive(const SupertaskPtr &caller, const ComponentInstance *callee_inst)
    {
        if (!caller || !callee_inst)
        {
            return false;
        }
        if (caller->instance == nullptr)
        {
            auto callee_ancestors = callee_inst->reflexive_ancestors();
            auto current = caller.get();
            while (current != nullptr)
            {
                if (current->instance)
                {
                    auto caller_ancestors = current->instance->reflexive_ancestors();
                    for (auto *a : caller_ancestors)
                    {
                        if (callee_ancestors.count(a))
                        {
                            return true;
                        }
                    }
                }
                current = current->parent.get();
            }
            return false;
        }
        else
        {
            return caller->instance->is_reflexive_ancestor_of(callee_inst) ||
                   callee_inst->is_reflexive_ancestor_of(caller->instance);
        }
    }

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
             OnResolve on_resolve = {},
             FuncType ft = {})
            : opts_(std::move(options)), inst_(&instance), supertask_(std::move(supertask)), on_resolve_(std::move(on_resolve)), ft_(ft)
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

            // Python: if not self.ft.async_: return True
            if (!ft_.async_)
            {
                return true;
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
            if (!ft_.async_)
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

        bool suspend_until(Thread::ReadyFn ready, bool cancellable, bool force_yield = false)
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
            bool completed = thread_->suspend_until(std::move(ready), cancellable, force_yield);
            if (!completed && cancellable && state_ == State::PendingCancel)
            {
                state_ = State::CancelDelivered;
            }
            return completed;
        }

        Event yield_until(Thread::ReadyFn ready, bool cancellable, bool force_yield = false)
        {
            if (!suspend_until(std::move(ready), cancellable, force_yield))
            {
                return {EventCode::TASK_CANCELLED, 0, 0};
            }
            return {EventCode::NONE, 0, 0};
        }

        Event wait_until(Thread::ReadyFn ready, bool cancellable, std::shared_ptr<WaitableSet> wset, const HostTrap &trap)
        {
            if (wset)
            {
                wset->begin_wait();
            }
            auto ready_and_has_event = [ready = std::move(ready), wset]() -> bool
            {
                return ready() && (!wset || wset->has_pending_event());
            };
            Event event;
            if (!suspend_until(ready_and_has_event, cancellable))
            {
                event = {EventCode::TASK_CANCELLED, 0, 0};
            }
            else if (wset)
            {
                event = wset->take_pending_event(trap);
            }
            else
            {
                event = {EventCode::NONE, 0, 0};
            }
            if (wset)
            {
                wset->end_wait();
            }
            return event;
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

        bool may_block() const
        {
            return ft_.async_ || state_ == State::Resolved;
        }

        const FuncType &func_type() const
        {
            return ft_;
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
        FuncType ft_;
    };

    inline void canon_task_return(Task &task, std::vector<std::any> result, const HostTrap &trap,
                                  const void *result_type_id = nullptr,
                                  const LiftOptions *caller_opts = nullptr)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, task.options().sync, "task.return requires async context");
        // Python: trap_if(result_type != task.ft.result)
        if (result_type_id != nullptr && task.func_type().result_id != nullptr)
        {
            trap_if(trap_cx, result_type_id != task.func_type().result_id, "task.return result type mismatch");
        }
        // Python: trap_if(not LiftOptions.equal(opts, task.opts))
        if (caller_opts != nullptr)
        {
            trap_if(trap_cx, !(*caller_opts == static_cast<const LiftOptions &>(task.options())), "task.return options mismatch");
        }
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

    inline uint32_t canon_thread_yield(bool cancellable, Task &task, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        if (!task.may_block())
        {
            return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
        }

        auto event = task.yield_until([]
                                      { return true; },
                                      cancellable);
        switch (event.code)
        {
        case EventCode::NONE:
            return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
        case EventCode::TASK_CANCELLED:
            return static_cast<uint32_t>(SuspendResult::CANCELLED);
        default:
            return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
        }
    }

    inline void canon_thread_resume_later(Task &task, uint32_t thread_index, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.resume-later missing component instance");
        ensure_may_leave(*inst, trap);

        auto entry = inst->threads.get<ThreadEntry>(thread_index, trap);
        auto other_thread = entry->thread();
        trap_if(trap_cx, !other_thread, "thread.resume-later null thread");
        trap_if(trap_cx, !other_thread->suspended(), "thread not suspended");
        other_thread->resume_later();
    }

    inline uint32_t canon_thread_yield_to(bool cancellable, Task &task, uint32_t thread_index, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.yield-to missing component instance");
        ensure_may_leave(*inst, trap);

        auto entry = inst->threads.get<ThreadEntry>(thread_index, trap);
        auto other_thread = entry->thread();
        trap_if(trap_cx, !other_thread, "thread.yield-to null thread");
        trap_if(trap_cx, !other_thread->suspended(), "thread not suspended");

        // Make the other thread runnable.
        other_thread->resume_later();

        if (!cancellable)
        {
            return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
        }
        if (task.state() == Task::State::CancelDelivered || task.state() == Task::State::PendingCancel)
        {
            return static_cast<uint32_t>(SuspendResult::CANCELLED);
        }
        return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
    }

    inline uint32_t canon_thread_switch_to(bool cancellable, Task &task, uint32_t thread_index, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.switch-to missing component instance");
        ensure_may_leave(*inst, trap);

        auto entry = inst->threads.get<ThreadEntry>(thread_index, trap);
        auto other_thread = entry->thread();
        trap_if(trap_cx, !other_thread, "thread.switch-to null thread");
        trap_if(trap_cx, !other_thread->suspended(), "thread not suspended");

        // Make the other thread runnable.
        other_thread->resume_later();

        // Approximate a switch by forcing this thread to yield for at least one tick.
        task.suspend_until([]
                           { return true; },
                           cancellable,
                           true);

        if (!cancellable)
        {
            return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
        }
        if (task.state() == Task::State::CancelDelivered || task.state() == Task::State::PendingCancel)
        {
            return static_cast<uint32_t>(SuspendResult::CANCELLED);
        }
        return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
    }

    inline uint32_t canon_thread_new_ref(bool /*shared*/, Task &task, std::function<void(uint32_t)> callee, uint32_t c, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.new-ref missing component instance");
        ensure_may_leave(*inst, trap);
        trap_if(trap_cx, inst->store == nullptr, "thread.new-ref missing store");
        trap_if(trap_cx, !callee, "thread.new-ref null callee");

        auto thread = Thread::create_suspended(
            *inst->store,
            [callee = std::move(callee), c](bool)
            {
                callee(c);
                return false;
            },
            true,
            {});
        trap_if(trap_cx, !thread || !thread->suspended(), "thread.new-ref failed to create suspended thread");

        uint32_t index = inst->threads.add(std::make_shared<ThreadEntry>(thread), trap);
        thread->set_index(index);
        return index;
    }

    inline uint32_t canon_thread_new_indirect(bool /*shared*/, Task &task, const std::vector<std::function<void(uint32_t)>> &table, uint32_t fi, uint32_t c, const HostTrap &trap)
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, fi >= table.size(), "thread.new-indirect out of bounds");
        auto callee = table[fi];
        trap_if(trap_cx, !callee, "thread.new-indirect null callee");
        return canon_thread_new_ref(false, task, std::move(callee), c, trap);
    }

    inline uint32_t canon_thread_spawn_ref(bool shared, Task &task, std::function<void(uint32_t)> callee, uint32_t c, const HostTrap &trap)
    {
        uint32_t index = canon_thread_new_ref(shared, task, std::move(callee), c, trap);
        canon_thread_resume_later(task, index, trap);
        return index;
    }

    inline uint32_t canon_thread_spawn_indirect(bool shared, Task &task, const std::vector<std::function<void(uint32_t)>> &table, uint32_t fi, uint32_t c, const HostTrap &trap)
    {
        uint32_t index = canon_thread_new_indirect(shared, task, table, fi, c, trap);
        canon_thread_resume_later(task, index, trap);
        return index;
    }

    inline uint32_t canon_thread_available_parallelism(bool shared, Task &task, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.available-parallelism missing component instance");
        ensure_may_leave(*inst, trap);

        if (!shared)
        {
            return 1;
        }
        auto hc = std::thread::hardware_concurrency();
        return hc == 0 ? 1u : hc;
    }

    inline uint32_t canon_thread_index(Task &task, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.index missing component instance");
        ensure_may_leave(*inst, trap);

        auto thread = task.thread();
        trap_if(trap_cx, !thread, "thread missing");
        auto index = thread->index();
        trap_if(trap_cx, !index.has_value(), "thread index missing");
        return *index;
    }

    inline uint32_t canon_thread_suspend(bool cancellable, Task &task, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "thread.suspend missing component instance");
        ensure_may_leave(*inst, trap);
        trap_if(trap_cx, !task.may_block(), "thread.suspend may not block");

        // Force a yield of this thread for at least one tick.
        task.suspend_until([]
                           { return true; },
                           cancellable,
                           true);

        if (cancellable && (task.state() == Task::State::CancelDelivered || task.state() == Task::State::PendingCancel))
        {
            return static_cast<uint32_t>(SuspendResult::CANCELLED);
        }
        return static_cast<uint32_t>(SuspendResult::NOT_CANCELLED);
    }

    inline uint32_t canon_task_wait(bool cancellable,
                                    GuestMemory mem,
                                    Task &task,
                                    uint32_t waitable_set_handle,
                                    uint32_t event_ptr,
                                    const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "task.wait missing component instance");
        ensure_may_leave(*inst, trap);
        trap_if(trap_cx, !task.may_block(), "task.wait may not block");

        auto wset = inst->table.get<WaitableSet>(waitable_set_handle, trap);
        auto event = task.wait_until(
            []()
            { return true; },
            cancellable,
            wset,
            trap);
        write_event_fields(mem, event_ptr, event.index, event.payload, trap);
        return static_cast<uint32_t>(event.code);
    }

    struct Subtask : Waitable
    {
        enum class State : uint32_t
        {
            STARTING = 0,
            STARTED = 1,
            RETURNED = 2,
            CANCELLED_BEFORE_STARTED = 3,
            CANCELLED_BEFORE_RETURNED = 4
        };

        State state = State::STARTING;
        Call callee;
        std::vector<HandleElement *> lenders;
        bool cancellation_requested = false;
        bool lenders_delivered = false;

        bool resolved() const
        {
            switch (state)
            {
            case State::STARTING:
            case State::STARTED:
                return false;
            case State::RETURNED:
            case State::CANCELLED_BEFORE_STARTED:
            case State::CANCELLED_BEFORE_RETURNED:
                return true;
            }
            return false;
        }

        void add_lender(HandleElement &lending_handle)
        {
            lending_handle.lend_count += 1;
            lenders.push_back(&lending_handle);
        }

        void deliver_resolve()
        {
            for (auto *h : lenders)
            {
                if (h && h->lend_count > 0)
                {
                    h->lend_count -= 1;
                }
            }
            lenders.clear();
            lenders_delivered = true;
        }

        bool resolve_delivered() const
        {
            return lenders_delivered;
        }

        void drop(const HostTrap &trap) override
        {
            auto trap_cx = make_trap_context(trap);
            trap_if(trap_cx, !resolve_delivered(), "subtask drop before resolve delivered");
            Waitable::drop(trap);
        }
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
        auto thread = task.thread();
        trap_if(trap_cx, !thread, "thread missing");
        return thread->context().get(index);
    }

    inline void canon_context_set(Task &task, uint32_t index, int32_t value, const HostTrap &trap)
    {
        if (auto *inst = task.component_instance())
        {
            ensure_may_leave(*inst, trap);
        }
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, index >= ContextLocalStorage::LENGTH, "context index out of bounds");
        auto thread = task.thread();
        trap_if(trap_cx, !thread, "thread missing");
        thread->context().set(index, value);
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

    inline uint32_t canon_subtask_cancel(bool async_, Task &task, uint32_t subtask_index, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "subtask.cancel missing component instance");
        ensure_may_leave(*inst, trap);
        trap_if(trap_cx, !task.may_block() && !async_, "subtask.cancel may not block");

        auto subtask = inst->table.get<Subtask>(subtask_index, trap);
        trap_if(trap_cx, subtask->resolve_delivered(), "subtask already resolved and delivered");
        trap_if(trap_cx, subtask->cancellation_requested, "subtask cancel already requested");

        if (subtask->resolved())
        {
            // Already resolved — just consume the pending event
            auto event = subtask->get_pending_event(trap);
            subtask->deliver_resolve();
            return static_cast<uint32_t>(subtask->state);
        }

        subtask->cancellation_requested = true;
        subtask->callee.request_cancellation();

        if (!subtask->resolved())
        {
            if (!async_)
            {
                // Sync: block until resolved
                task.suspend_until([subtask]()
                                   { return subtask->resolved(); },
                                   false);
            }
            else
            {
                return BLOCKED;
            }
        }

        auto event = subtask->get_pending_event(trap);
        subtask->deliver_resolve();
        return static_cast<uint32_t>(subtask->state);
    }

    inline void canon_subtask_drop(Task &task, uint32_t subtask_index, const HostTrap &trap)
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "subtask.drop missing component instance");
        ensure_may_leave(*inst, trap);

        auto subtask = inst->table.remove<Subtask>(subtask_index, trap);
        subtask->drop(trap);
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
        bool sync = cx->is_sync();
        return writable->write(cx, writable_index, ptr, n, sync, trap);
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
        bool sync = cx->is_sync();
        return writable->write(cx, writable_index, ptr, sync, trap);
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

    //  canon_lift / canon_lower  ---

    // Core wasm callee: receives a Thread and flat args, returns flat results.
    using CoreCallee = std::function<WasmValVector(std::shared_ptr<Thread>, WasmValVector)>;

    // Lift-side callback callee: receives a Thread and (event_code, p1, p2), returns packed result.
    using LiftCallback = std::function<WasmValVector(std::shared_ptr<Thread>, WasmValVector)>;

    enum class CallbackCode : uint32_t
    {
        EXIT = 0,
        YIELD = 1,
        WAIT = 2,
        MAX = 2,
    };

    inline std::pair<CallbackCode, uint32_t> unpack_callback_result(uint32_t packed)
    {
        uint32_t code = packed & 0xfu;
        uint32_t waitable_set_index = packed >> 4;
        return {static_cast<CallbackCode>(code), waitable_set_index};
    }

    // Wraps exceptions from core wasm calls as traps.
    inline WasmValVector call_and_trap_on_throw(const CoreCallee &callee, const std::shared_ptr<Thread> &thread, WasmValVector args)
    {
        try
        {
            return callee(thread, std::move(args));
        }
        catch (...)
        {
            throw; // re-raise as trap
        }
    }

    /// canon_lift: Implements the canonical lifting of a component-model call.
    ///
    /// Mirrors Python's canon_lift(opts, inst, ft, callee, caller, on_start, on_resolve).
    /// Creates a Task, wires up a Thread, lowers the args, calls the core callee,
    /// lifts the results, and handles sync/async/callback modes.
    ///
    /// @param opts       Canonical options (encoding, memory, realloc, async, callback, post_return)
    /// @param inst       The callee component instance
    /// @param ft         The function type being lifted
    /// @param callee     The core wasm function to call
    /// @param caller     The supertask representing the caller
    /// @param on_start   Callback to retrieve the high-level arguments
    /// @param on_resolve Callback to deliver the high-level results (or nullopt on cancel)
    /// @param trap       Host trap handler
    /// @param convert    Unicode conversion function (for LiftLowerContext)
    /// @return           A Call handle for the created task
    inline Call canon_lift(CanonicalOptions opts,
                           ComponentInstance &inst,
                           FuncType ft,
                           CoreCallee callee,
                           SupertaskPtr caller,
                           OnStart on_start,
                           OnResolve on_resolve,
                           const HostTrap &trap,
                           const HostUnicodeConversion &convert = {})
    {
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, call_might_be_recursive(caller, &inst), "canon_lift: recursive call");

        auto task = std::make_shared<Task>(inst, opts, std::move(caller), std::move(on_resolve), ft);

        // Build the ResumeFn that runs the lifted function body.
        auto thread_func = [task, opts, &inst, ft, callee = std::move(callee),
                            on_start = std::move(on_start), trap, convert](bool was_cancelled) mutable -> bool
        {
            if (was_cancelled)
            {
                return false;
            }

            if (!task->enter(trap))
            {
                return false;
            }

            // Register the thread in the instance's threads table.
            auto thread = task->thread();
            if (thread && !thread->index().has_value())
            {
                uint32_t idx = inst.threads.add(std::make_shared<ThreadEntry>(thread), trap);
                thread->set_index(idx);
            }

            LiftLowerOptions ll_opts(opts.string_encoding, opts.memory, opts.realloc);
            LiftLowerContext cx(trap, convert, ll_opts, &inst);
            cx.set_canonical_options(opts);

            auto args = on_start();

            // Sync path
            if (opts.sync)
            {
                WasmValVector flat_args;
                for (auto &arg : args)
                {
                    // The caller is expected to provide already-lowered flat args
                    // via on_start in the host embedding.  For full spec compliance
                    // this would invoke lower_flat_values, but that requires
                    // compile-time type information.  The host embedding is
                    // responsible for providing correct flat values.
                    if (arg.type() == typeid(int32_t))
                        flat_args.push_back(std::any_cast<int32_t>(arg));
                    else if (arg.type() == typeid(int64_t))
                        flat_args.push_back(std::any_cast<int64_t>(arg));
                    else if (arg.type() == typeid(float))
                        flat_args.push_back(std::any_cast<float>(arg));
                    else if (arg.type() == typeid(double))
                        flat_args.push_back(std::any_cast<double>(arg));
                }

                WasmValVector flat_results = call_and_trap_on_throw(callee, thread, std::move(flat_args));

                // Lift the flat results back to high-level values.
                // The host embedding is expected to interpret flat_results.
                std::vector<std::any> result;
                for (auto &fv : flat_results)
                {
                    result.push_back(fv);
                }
                task->return_result(std::move(result), trap);

                if (opts.post_return)
                {
                    inst.may_leave = false;
                    (*opts.post_return)();
                    inst.may_leave = true;
                }

                task->exit();
                return false;
            }

            // Async path without callback
            if (!opts.callback)
            {
                WasmValVector flat_args;
                for (auto &arg : args)
                {
                    if (arg.type() == typeid(int32_t))
                        flat_args.push_back(std::any_cast<int32_t>(arg));
                    else if (arg.type() == typeid(int64_t))
                        flat_args.push_back(std::any_cast<int64_t>(arg));
                    else if (arg.type() == typeid(float))
                        flat_args.push_back(std::any_cast<float>(arg));
                    else if (arg.type() == typeid(double))
                        flat_args.push_back(std::any_cast<double>(arg));
                }

                call_and_trap_on_throw(callee, thread, std::move(flat_args));
                task->exit();
                return false;
            }

            // Async path with callback
            WasmValVector flat_args;
            for (auto &arg : args)
            {
                if (arg.type() == typeid(int32_t))
                    flat_args.push_back(std::any_cast<int32_t>(arg));
                else if (arg.type() == typeid(int64_t))
                    flat_args.push_back(std::any_cast<int64_t>(arg));
                else if (arg.type() == typeid(float))
                    flat_args.push_back(std::any_cast<float>(arg));
                else if (arg.type() == typeid(double))
                    flat_args.push_back(std::any_cast<double>(arg));
            }

            WasmValVector packed_vec = call_and_trap_on_throw(callee, thread, std::move(flat_args));
            auto trap_cx2 = make_trap_context(trap);
            trap_if(trap_cx2, packed_vec.empty(), "canon_lift callback: callee returned no value");
            uint32_t packed = static_cast<uint32_t>(std::get<int32_t>(packed_vec[0]));
            auto [code, si] = unpack_callback_result(packed);
            trap_if(trap_cx2, static_cast<uint32_t>(code) > static_cast<uint32_t>(CallbackCode::MAX), "invalid callback code");

            while (code != CallbackCode::EXIT)
            {
                inst.exclusive = false;
                Event event;
                switch (code)
                {
                case CallbackCode::YIELD:
                    if (task->may_block())
                    {
                        event = task->yield_until([&inst]()
                                                  { return !inst.exclusive; },
                                                  true);
                    }
                    else
                    {
                        event = {EventCode::NONE, 0, 0};
                    }
                    break;
                case CallbackCode::WAIT:
                {
                    trap_if(trap_cx2, !task->may_block(), "callback WAIT: may not block");
                    auto wset = inst.table.get<WaitableSet>(si, trap);
                    event = task->wait_until([&inst]()
                                             { return !inst.exclusive; },
                                             true,
                                             wset,
                                             trap);
                    break;
                }
                default:
                    trap_if(trap_cx2, true, "invalid callback code");
                    break;
                }
                inst.exclusive = true;

                auto callback_fn = *opts.callback;
                callback_fn(event.code, event.index, event.payload);

                // The callback returns a packed result via the callback convention.
                // In the C++ model, the callback is a void function that updates
                // state; we re-enter the loop checking for EXIT.
                // For full fidelity, the callback would need to return a packed int.
                // We break out since the C++ callback model doesn't return values.
                code = CallbackCode::EXIT;
            }

            task->exit();
            return false;
        };

        auto thread = Thread::create(
            *inst.store,
            []()
            { return true; },
            std::move(thread_func),
            !opts.sync,
            [task]()
            {
                task->request_cancellation();
            });

        task->set_thread(thread);

        // Initial tick to start the thread
        inst.store->tick();

        return Call(
            [task]()
            {
                task->request_cancellation();
            },
            !opts.sync);
    }

    /// canon_lower: Implements the canonical lowering of a component-model call.
    ///
    /// Mirrors Python's canon_lower(opts, ft, callee, thread, flat_args).
    /// Creates a Subtask, lifts the flat args, calls the component function,
    /// lowers the results, and handles sync/async paths.
    ///
    /// @param opts       Canonical options
    /// @param ft         The function type being lowered
    /// @param callee     The component function to call (FuncInst)
    /// @param task       The current task
    /// @param flat_args  The flat wasm arguments
    /// @param trap       Host trap handler
    /// @param convert    Unicode conversion function
    /// @return           Flat wasm results
    inline WasmValVector canon_lower(CanonicalOptions opts,
                                     FuncType ft,
                                     FuncInst callee,
                                     Task &task,
                                     WasmValVector flat_args,
                                     const HostTrap &trap,
                                     const HostUnicodeConversion &convert = {})
    {
        auto *inst = task.component_instance();
        auto trap_cx = make_trap_context(trap);
        trap_if(trap_cx, inst == nullptr, "canon_lower: missing component instance");
        trap_if(trap_cx, !inst->may_leave, "canon_lower: may not leave");
        trap_if(trap_cx, !task.may_block() && ft.async_ && !opts.sync, "canon_lower: sync context cannot call async");

        auto subtask = std::make_shared<Subtask>();
        LiftLowerOptions ll_opts(opts.string_encoding, opts.memory, opts.realloc);
        auto cx = std::make_shared<LiftLowerContext>(trap, convert, ll_opts, inst);
        cx->set_canonical_options(opts);

        WasmValVector flat_results;
        auto flat_args_iter = std::make_shared<WasmValVector>(std::move(flat_args));

        auto on_start = [subtask, flat_args_iter]() -> std::vector<std::any>
        {
            subtask->state = Subtask::State::STARTED;
            // Convert flat args to std::any vector for the callee
            std::vector<std::any> args;
            for (auto &v : *flat_args_iter)
            {
                std::visit([&args](auto &&val)
                           { args.push_back(val); },
                           v);
            }
            return args;
        };

        auto on_resolve = [subtask, &flat_results, inst, &trap](std::optional<std::vector<std::any>> result)
        {
            if (!result.has_value())
            {
                // Cancelled
                if (subtask->state == Subtask::State::STARTED)
                {
                    subtask->state = Subtask::State::CANCELLED_BEFORE_RETURNED;
                }
                else
                {
                    subtask->state = Subtask::State::CANCELLED_BEFORE_STARTED;
                }
            }
            else
            {
                subtask->state = Subtask::State::RETURNED;
                // Store flat results from the resolved values
                for (auto &v : *result)
                {
                    if (v.type() == typeid(int32_t))
                        flat_results.push_back(std::any_cast<int32_t>(v));
                    else if (v.type() == typeid(int64_t))
                        flat_results.push_back(std::any_cast<int64_t>(v));
                    else if (v.type() == typeid(float))
                        flat_results.push_back(std::any_cast<float>(v));
                    else if (v.type() == typeid(double))
                        flat_results.push_back(std::any_cast<double>(v));
                }
            }
        };

        // Call the callee
        subtask->callee = inst->store->invoke(callee, task.thread() ? std::make_shared<Supertask>(Supertask{nullptr, task.thread(), inst}) : nullptr, std::move(on_start), std::move(on_resolve));

        // Sync path
        if (opts.sync)
        {
            if (!subtask->resolved())
            {
                // Block until resolved
                task.suspend_until([subtask]()
                                   { return subtask->resolved(); },
                                   false);
            }
            subtask->deliver_resolve();
            return flat_results;
        }

        // Async path
        if (subtask->resolved())
        {
            subtask->deliver_resolve();
            return {static_cast<int32_t>(subtask->state)};
        }
        else
        {
            uint32_t subtaski = inst->table.add(subtask, trap);

            // Set up progress notification
            auto on_progress = [subtask, subtaski, inst, &trap]()
            {
                auto subtask_event = [subtask, subtaski]() -> Event
                {
                    if (subtask->resolved())
                    {
                        subtask->deliver_resolve();
                    }
                    return {EventCode::SUBTASK, subtaski, static_cast<uint32_t>(subtask->state)};
                };
                subtask->set_pending_event(subtask_event());
            };

            uint32_t packed = static_cast<uint32_t>(subtask->state) | (subtaski << 4);
            return {static_cast<int32_t>(packed)};
        }
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
