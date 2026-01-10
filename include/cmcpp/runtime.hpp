#ifndef CMCPP_RUNTIME_HPP
#define CMCPP_RUNTIME_HPP

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace cmcpp
{
    class Store;
    struct ComponentInstance;

    class ContextLocalStorage
    {
    public:
        // Number of int32_t "slots" available for context-local data.
        //
        // Slot 0: reserved for cmcpp runtime/internal bookkeeping associated
        //         with the current execution context.
        // Slot 1: additional per-context storage, intended for extensibility
        //         (e.g., user-defined or future runtime data).
        static constexpr uint32_t LENGTH = 2;

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

    struct Supertask;
    using SupertaskPtr = std::shared_ptr<Supertask>;

    using OnStart = std::function<std::vector<std::any>()>;
    using OnResolve = std::function<void(std::optional<std::vector<std::any>>)>;

    class Thread : public std::enable_shared_from_this<Thread>
    {
    public:
        using ReadyFn = std::function<bool()>;
        using ResumeFn = std::function<bool(bool)>;
        using CancelFn = std::function<void()>;

        static std::shared_ptr<Thread> create(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable = false, CancelFn on_cancel = {});
        static std::shared_ptr<Thread> create_suspended(Store &store, ResumeFn resume, bool cancellable = false, CancelFn on_cancel = {});

        Thread(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable, CancelFn on_cancel);

        bool ready() const;
        void resume();
        void request_cancellation();
        bool cancellable() const;
        bool cancelled() const;
        bool completed() const;

        void set_index(uint32_t index);
        std::optional<uint32_t> index() const;

        bool suspended() const;
        void resume_later();

        bool suspend_until(ReadyFn ready, bool cancellable, bool force_yield = false);
        void set_ready(ReadyFn ready);
        void set_allow_cancellation(bool allow);
        bool allow_cancellation() const;
        void set_in_event_loop(bool value);
        bool in_event_loop() const;

        ContextLocalStorage &context()
        {
            return context_;
        }

        const ContextLocalStorage &context() const
        {
            return context_;
        }

    private:
        enum class State
        {
            Suspended,
            Pending,
            Running,
            Completed
        };

        void set_pending(bool pending_again, const std::shared_ptr<Thread> &self);

        Store *store_;
        ReadyFn ready_;
        ResumeFn resume_;
        CancelFn on_cancel_;
        bool allow_cancellation_;
        bool cancellable_;
        bool cancelled_;
        bool in_event_loop_;
        ContextLocalStorage context_{};
        std::optional<uint32_t> index_;
        mutable std::mutex mutex_;
        State state_;
        std::atomic<bool> reschedule_requested_{false};
    };

    class Call
    {
    public:
        using CancelRequest = std::function<void()>;

        Call() = default;
        explicit Call(CancelRequest cancel_req, bool cancellable = true)
            : request_cancellation_(std::move(cancel_req)), cancellable_(cancellable) {}

        void request_cancellation() const
        {
            if (!cancellable_)
            {
                return;
            }
            if (request_cancellation_)
            {
                request_cancellation_();
            }
        }

        bool cancellable() const
        {
            return cancellable_;
        }

        static Call from_thread(const std::shared_ptr<Thread> &thread);

    private:
        CancelRequest request_cancellation_;
        bool cancellable_ = false;
    };

    struct Supertask
    {
        SupertaskPtr parent;
        std::weak_ptr<Thread> thread;
        ComponentInstance *instance = nullptr;
    };

    using FuncInst = std::function<Call(Store &, SupertaskPtr, OnStart, OnResolve)>;

    class Store
    {
    public:
        Call invoke(const FuncInst &func, SupertaskPtr caller, OnStart on_start, OnResolve on_resolve);
        void tick();
        void schedule(const std::shared_ptr<Thread> &thread);
        std::size_t pending_size() const;
        void enqueue(std::function<void()> microtask);

    private:
        friend class Thread;

        mutable std::mutex mutex_;
        std::vector<std::shared_ptr<Thread>> pending_;
        std::deque<std::function<void()>> microtasks_;
    };

    inline std::shared_ptr<Thread> Thread::create(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable, CancelFn on_cancel)
    {
        auto thread = std::shared_ptr<Thread>(new Thread(store, std::move(ready), std::move(resume), cancellable, std::move(on_cancel)));
        store.schedule(thread);
        return thread;
    }

    inline std::shared_ptr<Thread> Thread::create_suspended(Store &store, ResumeFn resume, bool cancellable, CancelFn on_cancel)
    {
        auto thread = std::shared_ptr<Thread>(new Thread(store, nullptr, std::move(resume), cancellable, std::move(on_cancel)));
        {
            std::lock_guard lock(thread->mutex_);
            thread->state_ = State::Suspended;
        }
        return thread;
    }

    inline Thread::Thread(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable, CancelFn on_cancel)
        : store_(&store),
          ready_(std::move(ready)),
          resume_(std::move(resume)),
          on_cancel_(std::move(on_cancel)),
          allow_cancellation_(cancellable),
          cancellable_(cancellable),
          cancelled_(false),
          in_event_loop_(false),
          state_(State::Pending)
    {
    }

    inline bool Thread::ready() const
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::Pending)
        {
            return false;
        }
        if (cancelled_ && cancellable_)
        {
            return true;
        }
        if (!ready_)
        {
            return true;
        }
        return ready_();
    }

    inline void Thread::resume()
    {
        auto self = shared_from_this();
        ResumeFn resume;
        bool was_cancelled = false;

        {
            std::lock_guard lock(mutex_);
            if (state_ != State::Pending && state_ != State::Suspended)
            {
                return;
            }
            state_ = State::Running;
            resume = resume_;
            was_cancelled = cancelled_;
            cancelled_ = false;
        }

        bool keep_pending = false;
        if (resume)
        {
            keep_pending = resume(was_cancelled);
        }

        bool requested = reschedule_requested_.exchange(false, std::memory_order_relaxed);
        set_pending(keep_pending || requested, self);
    }

    inline void Thread::request_cancellation()
    {
        CancelFn cancel;
        {
            std::lock_guard lock(mutex_);
            if (!allow_cancellation_ || cancelled_)
            {
                return;
            }
            cancelled_ = true;
            cancel = on_cancel_;
        }

        if (cancel)
        {
            cancel();
        }
    }

    inline bool Thread::cancellable() const
    {
        std::lock_guard lock(mutex_);
        return cancellable_;
    }

    inline bool Thread::cancelled() const
    {
        std::lock_guard lock(mutex_);
        return cancelled_;
    }

    inline bool Thread::completed() const
    {
        std::lock_guard lock(mutex_);
        return state_ == State::Completed;
    }

    inline void Thread::set_index(uint32_t index)
    {
        std::lock_guard lock(mutex_);
        index_ = index;
    }

    inline std::optional<uint32_t> Thread::index() const
    {
        std::lock_guard lock(mutex_);
        return index_;
    }

    inline bool Thread::suspended() const
    {
        std::lock_guard lock(mutex_);
        return state_ == State::Suspended;
    }

    inline void Thread::resume_later()
    {
        auto self = shared_from_this();
        {
            std::lock_guard lock(mutex_);
            if (state_ != State::Suspended)
            {
                return;
            }
            ready_ = []()
            {
                return true;
            };
            cancellable_ = false;
            cancelled_ = false;
            state_ = State::Pending;
        }
        store_->schedule(self);
    }

    inline bool Thread::suspend_until(ReadyFn ready, bool cancellable, bool force_yield)
    {
        bool ready_now = false;
        if (ready && !force_yield)
        {
            ready_now = ready();
        }

        if (ready_now)
        {
            return true;
        }

        auto gate = std::make_shared<std::atomic<bool>>(false);
        ReadyFn wrapped = [ready = std::move(ready), gate, force_yield]() mutable -> bool
        {
            if (force_yield && !gate->exchange(true, std::memory_order_relaxed))
            {
                return false;
            }
            if (!ready)
            {
                return true;
            }
            return ready();
        };

        {
            std::lock_guard lock(mutex_);
            ready_ = std::move(wrapped);
            cancellable_ = allow_cancellation_ && cancellable;
        }

        reschedule_requested_.store(true, std::memory_order_relaxed);
        return false;
    }

    inline void Thread::set_ready(ReadyFn ready)
    {
        std::lock_guard lock(mutex_);
        ready_ = std::move(ready);
    }

    inline void Thread::set_allow_cancellation(bool allow)
    {
        std::lock_guard lock(mutex_);
        allow_cancellation_ = allow;
        if (!allow)
        {
            cancellable_ = false;
        }
    }

    inline bool Thread::allow_cancellation() const
    {
        std::lock_guard lock(mutex_);
        return allow_cancellation_;
    }

    inline void Thread::set_in_event_loop(bool value)
    {
        std::lock_guard lock(mutex_);
        in_event_loop_ = value;
    }

    inline bool Thread::in_event_loop() const
    {
        std::lock_guard lock(mutex_);
        return in_event_loop_;
    }

    inline void Thread::set_pending(bool pending_again, const std::shared_ptr<Thread> &self)
    {
        {
            std::lock_guard lock(mutex_);
            state_ = pending_again ? State::Pending : State::Completed;
            if (!pending_again)
            {
                ready_ = nullptr;
                cancellable_ = false;
            }
        }

        if (pending_again)
        {
            store_->schedule(self);
        }
    }

    inline Call Call::from_thread(const std::shared_ptr<Thread> &thread)
    {
        if (!thread)
        {
            return Call();
        }
        std::weak_ptr<Thread> weak = thread;
        return Call(
            [weak]()
            {
                if (auto locked = weak.lock())
                {
                    locked->request_cancellation();
                }
            },
            thread->allow_cancellation());
    }

    inline Call Store::invoke(const FuncInst &func, SupertaskPtr caller, OnStart on_start, OnResolve on_resolve)
    {
        if (!func)
        {
            return Call();
        }
        return func(*this, std::move(caller), std::move(on_start), std::move(on_resolve));
    }

    inline void Store::tick()
    {
        std::function<void()> microtask;
        std::shared_ptr<Thread> selected;

        {
            std::lock_guard lock(mutex_);
            if (!microtasks_.empty())
            {
                microtask = std::move(microtasks_.front());
                microtasks_.pop_front();
            }
            else
            {
                auto it = std::find_if(pending_.begin(), pending_.end(), [](const std::shared_ptr<Thread> &thread)
                                       { return thread && thread->ready(); });
                if (it == pending_.end())
                {
                    return;
                }
                selected = *it;
                pending_.erase(it);
            }
        }

        if (microtask)
        {
            microtask();
            return;
        }

        if (selected)
        {
            selected->resume();
        }
    }

    inline void Store::schedule(const std::shared_ptr<Thread> &thread)
    {
        if (!thread)
        {
            return;
        }
        std::lock_guard lock(mutex_);
        pending_.push_back(thread);
    }

    inline std::size_t Store::pending_size() const
    {
        std::lock_guard lock(mutex_);
        return pending_.size();
    }

    inline void Store::enqueue(std::function<void()> microtask)
    {
        if (!microtask)
        {
            return;
        }
        std::lock_guard lock(mutex_);
        microtasks_.push_back(std::move(microtask));
    }
}

#endif
