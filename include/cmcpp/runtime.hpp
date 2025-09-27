#ifndef CMCPP_RUNTIME_HPP
#define CMCPP_RUNTIME_HPP

#include <algorithm>
#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace cmcpp
{
    class Store;

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

        Thread(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable, CancelFn on_cancel);
        bool ready() const;
        void resume();
        void request_cancellation();
        bool cancellable() const;
        bool cancelled() const;
        bool completed() const;

    private:
        enum class State
        {
            Pending,
            Running,
            Completed
        };

        void set_pending(bool pending_again, const std::shared_ptr<Thread> &self);

        Store *store_;
        ReadyFn ready_;
        ResumeFn resume_;
        CancelFn on_cancel_;
        bool cancellable_;
        bool cancelled_;
        mutable std::mutex mutex_;
        State state_;
    };

    class Call
    {
    public:
        using CancelRequest = std::function<void()>;

        Call() = default;
        explicit Call(CancelRequest cancel) : request_cancellation_(std::move(cancel)) {}

        void request_cancellation() const
        {
            if (request_cancellation_)
            {
                request_cancellation_();
            }
        }

        static Call from_thread(const std::shared_ptr<Thread> &thread)
        {
            if (!thread)
            {
                return Call();
            }
            std::weak_ptr<Thread> weak = thread;
            return Call([weak]()
                        {
                if (auto locked = weak.lock())
                {
                    locked->request_cancellation();
                } });
        }

    private:
        CancelRequest request_cancellation_;
    };

    struct Supertask
    {
        SupertaskPtr parent;
    };

    using FuncInst = std::function<Call(Store &, SupertaskPtr, OnStart, OnResolve)>;

    class Store
    {
    public:
        Call invoke(const FuncInst &func, SupertaskPtr caller, OnStart on_start, OnResolve on_resolve);
        void tick();
        void schedule(const std::shared_ptr<Thread> &thread);
        std::size_t pending_size() const;

    private:
        friend class Thread;
        mutable std::mutex mutex_;
        std::vector<std::shared_ptr<Thread>> pending_;
    };

    inline std::shared_ptr<Thread> Thread::create(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable, CancelFn on_cancel)
    {
        auto thread = std::shared_ptr<Thread>(new Thread(store, std::move(ready), std::move(resume), cancellable, std::move(on_cancel)));
        store.schedule(thread);
        return thread;
    }

    inline Thread::Thread(Store &store, ReadyFn ready, ResumeFn resume, bool cancellable, CancelFn on_cancel)
        : store_(&store), ready_(std::move(ready)), resume_(std::move(resume)), on_cancel_(std::move(on_cancel)), cancellable_(cancellable), cancelled_(false), state_(State::Pending)
    {
    }

    inline bool Thread::ready() const
    {
        std::lock_guard lock(mutex_);
        if (state_ != State::Pending)
        {
            return false;
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
            if (state_ != State::Pending)
            {
                return;
            }
            state_ = State::Running;
            resume = resume_;
            was_cancelled = cancelled_;
        }

        bool keep_pending = false;
        if (resume)
        {
            keep_pending = resume(was_cancelled);
        }

        set_pending(keep_pending, self);
    }

    inline void Thread::request_cancellation()
    {
        CancelFn cancel;
        {
            std::lock_guard lock(mutex_);
            if (cancelled_ || !cancellable_)
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

    inline void Thread::set_pending(bool pending_again, const std::shared_ptr<Thread> &self)
    {
        {
            std::lock_guard lock(mutex_);
            state_ = pending_again ? State::Pending : State::Completed;
        }
        if (pending_again)
        {
            store_->schedule(self);
        }
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
        std::shared_ptr<Thread> selected;
        {
            std::lock_guard lock(mutex_);
            auto it = std::find_if(pending_.begin(), pending_.end(), [](const std::shared_ptr<Thread> &thread)
                                   { return thread && thread->ready(); });
            if (it == pending_.end())
            {
                return;
            }
            selected = *it;
            pending_.erase(it);
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
}

#endif
