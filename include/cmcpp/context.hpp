#ifndef CMCPP_CONTEXT_HPP
#define CMCPP_CONTEXT_HPP

#include "traits.hpp"

#include <future>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>
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

        void track_owning_lend(HandleElement &lending_handle);
        void exit_call();
    };

    inline void trap_if(const LiftLowerContext &cx, bool condition, const char *message = nullptr) noexcept(false)
    {
        if (condition)
        {
            const char *msg = message == nullptr ? "Unknown trap" : message;
            if (cx.trap)
            {
                cx.trap(msg);
                return;
            }
            throw std::runtime_error(msg);
        }
    }

    inline LiftLowerContext make_trap_context(const HostTrap &trap)
    {
        HostUnicodeConversion convert{};
        LiftLowerOptions opts{};
        return LiftLowerContext(trap, convert, opts);
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
        std::vector<std::optional<HandleElement>> entries_{};
        std::vector<uint32_t> free_;
        HandleTable() { entries_.push_back(std::nullopt); }
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
        bool may_leave = true;
        bool may_enter = true;
        HandleTables handles;
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
