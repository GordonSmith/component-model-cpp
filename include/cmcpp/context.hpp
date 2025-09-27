#ifndef CMCPP_CONTEXT_HPP
#define CMCPP_CONTEXT_HPP

#include "traits.hpp"

#include <future>
#if __has_include(<span>)
#include <span>
#else
#include <string>
#include <sstream>
#endif

namespace cmcpp
{
    using HostTrap = std::function<void(const char *msg) noexcept(false)>;
    using GuestRealloc = std::function<int(int ptr, int old_size, int align, int new_size)>;
    using GuestMemory = std::span<uint8_t>;
    using GuestPostReturn = std::function<void()>;
    using GuestCallback = std::function<void()>;
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

    // Runtime State ---
    struct ResourceHandle
    {
    };

    struct Waitable
    {
    };

    struct WaitableSet
    {
    };

    struct ErrorContext
    {
    };

    struct ComponentInstance
    {
        std::vector<ResourceHandle> resources;
        std::vector<Waitable> waitables;
        std::vector<WaitableSet> waitable_sets;
        std::vector<ErrorContext> error_contexts;
        bool may_leave = true;
        bool backpressure = false;
        bool calling_sync_export = false;
        bool calling_sync_import = false;
        // std::vector<std::tuple<Task<R, Args...>, Future>> pending_tasks;
        bool starting_pending_tasks = false;
    };

    class ContextLocalStorage
    {
    public:
        static constexpr int LENGTH = 2;
        int array[LENGTH] = {0, 0};

        ContextLocalStorage() = default;

        void set(int i, int v)
        {
            array[i] = v;
        }

        int get(int i)
        {
            return array[i];
        }
    };

    template <Field R, Field... Args>
    class Task
    {
    public:
        using function_type = func_t<R(Args...)>;

        CanonicalOptions opts;
        ComponentInstance inst;
        function_type ft;
        std::optional<Task> supertask;
        std::optional<std::function<void()>> on_return;
        std::function<std::future<void>(std::future<void>)> on_block;
        int num_borrows = 0;
        ContextLocalStorage context();

        Task(CanonicalOptions &opts, ComponentInstance &inst, function_type &ft, std::optional<Task> &supertask = std::nullopt, std::optional<std::function<void()>> &on_return = std::nullopt, std::function<std::future<void>(std::future<void>)> &on_block = std::nullopt)
            : opts(opts), inst(inst), ft(ft), supertask(supertask), on_return(on_return), on_block(on_block) {}
    };

    struct Subtask : Waitable
    {
    };

    struct Future
    {
    };

    //  Lifting and Lowering Context  ---
    // template <Field R, Field... Args>
    class LiftLowerContext
    {
    public:
        HostTrap trap;
        HostUnicodeConversion convert;

        LiftLowerOptions opts;
        // ComponentInstance inst;
        // std::optional<std::variant<Task<R, Args...>, Subtask>> borrow_scope;

        LiftLowerContext(const HostTrap &trap, const HostUnicodeConversion &convert, const LiftLowerOptions &opts)
            : trap(trap), convert(convert), opts(opts) {}
    };

    //  ----------------------------

    struct InstanceContext
    {
        HostTrap trap;
        HostUnicodeConversion convert;
        GuestRealloc realloc;

        std::unique_ptr<LiftLowerContext> createLiftLowerContext(const GuestMemory &memory, const Encoding &string_encoding = Encoding::Utf8, const std::optional<GuestPostReturn> &post_return = std::nullopt)
        {
            LiftLowerOptions opts(string_encoding, memory, realloc);
            auto retVal = std::make_unique<LiftLowerContext>(trap, convert, opts);
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
