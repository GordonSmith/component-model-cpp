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
#include <unordered_map>
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

        void drop(const HostTrap &trap);

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
                                std::unique_lock<std::mutex> reclaim_lock(mu, std::try_to_lock);
                                if (!reclaim_lock.owns_lock())
                                {
                                    return;
                                }
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

            if (!sync)
            {
                return BLOCKED;
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

            if (!sync)
            {
                return BLOCKED;
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
            if (!sync)
            {
                return BLOCKED;
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
                return BLOCKED;
            }
            if (!sync)
            {
                return BLOCKED;
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
            return !opts_.sync || state_ == State::Resolved;
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

        auto entry = inst->table.get<ThreadEntry>(thread_index, trap);
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

        auto entry = inst->table.get<ThreadEntry>(thread_index, trap);
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

        auto entry = inst->table.get<ThreadEntry>(thread_index, trap);
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

        uint32_t index = inst->table.add(std::make_shared<ThreadEntry>(thread), trap);
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

    inline uint32_t canon_task_wait(bool /*cancellable*/,
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

        auto wset = inst->table.get<WaitableSet>(waitable_set_handle, trap);
        wset->begin_wait();
        if (!wset->has_pending_event())
        {
            wset->end_wait();
            write_event_fields(mem, event_ptr, 0, 0, trap);
            return BLOCKED;
        }
        auto event = wset->take_pending_event(trap);
        wset->end_wait();
        write_event_fields(mem, event_ptr, event.index, event.payload, trap);
        return static_cast<uint32_t>(event.code);
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
