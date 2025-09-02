#ifndef CMCPP_RESOURCE_HPP
#define CMCPP_RESOURCE_HPP

#include "context.hpp"
#include "util.hpp"
#include "integer.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <functional>
#include <stdexcept>
#include <any>

namespace cmcpp
{
    // Resource handle table for managing component instance resources
    class ResourceTable
    {
    public:
        static constexpr uint32_t MAX_LENGTH = (1u << 28) - 1;

        ResourceTable() : array_(1, std::nullopt) {} // Start with one empty entry

        template <typename T>
        std::optional<T> get(uint32_t i) const
        {
            if (i >= array_.size() || !array_[i].has_value())
            {
                return std::nullopt;
            }
            try
            {
                return std::any_cast<T>(array_[i].value());
            }
            catch (const std::bad_any_cast &)
            {
                return std::nullopt;
            }
        }

        template <typename T>
        uint32_t add(const T &handle)
        {
            if (!free_.empty())
            {
                uint32_t i = free_.back();
                free_.pop_back();
                array_[i] = handle;
                return i;
            }
            else
            {
                uint32_t i = array_.size();
                if (i > MAX_LENGTH)
                {
                    throw std::runtime_error("Resource table full");
                }
                array_.push_back(handle);
                return i;
            }
        }

        void remove(uint32_t i)
        {
            if (i < array_.size() && array_[i].has_value())
            {
                array_[i] = std::nullopt;
                free_.push_back(i);
            }
        }

        size_t size() const { return array_.size(); }

    private:
        std::vector<std::optional<std::any>> array_;
        std::vector<uint32_t> free_;
    };

    // Internal stored handle representation for a resource type
    template <typename Rep>
    struct StoredHandle
    {
        ResourceType<Rep> *rt;
        Rep rep;
        bool own; // true if owned, false if borrowed
    };

    // Per-type resource table accessor
    template <typename Rep>
    inline ResourceTable &resource_table()
    {
        static ResourceTable tbl;
        return tbl;
    }

    namespace resource
    {
        // Helper to create a new handle in the per-type table
        template <typename Rep>
        inline uint32_t make_handle(ResourceType<Rep> *rt, const Rep &rep, bool own)
        {
            StoredHandle<Rep> sh{rt, rep, own};
            return resource_table<Rep>().add(sh);
        }

        // Helper to get a stored handle by index (if present)
        template <typename Rep>
        inline std::optional<StoredHandle<Rep>> get_handle(uint32_t i)
        {
            return resource_table<Rep>().template get<StoredHandle<Rep>>(i);
        }

        // Helper to remove a handle from the table
        template <typename Rep>
        inline void remove_handle(uint32_t i)
        {
            resource_table<Rep>().remove(i);
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            int32_t handle = static_cast<int32_t>(make_handle<Rep>(v.rt, v.rep, /*own*/ true));
            integer::store(cx, handle, ptr, 4);
#else
            // For owned resources, store the representation as i32 (stub: address-based)
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            integer::store(cx, static_cast<int32_t>(addr & 0xFFFFFFFF), ptr, 4);
#endif
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            int32_t handle = static_cast<int32_t>(make_handle<Rep>(v.rt, v.rep, /*own*/ false));
            integer::store(cx, handle, ptr, 4);
#else
            // For borrowed resources, store the representation as i32 (stub: address-based)
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            integer::store(cx, static_cast<int32_t>(addr & 0xFFFFFFFF), ptr, 4);
#endif
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            int32_t handle = static_cast<int32_t>(make_handle<Rep>(v.rt, v.rep, /*own*/ true));
            return {handle};
#else
            // For owned resources, create a handle representing ownership transfer (stub: address-based)
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            return {static_cast<int32_t>(addr & 0xFFFFFFFF)};
#endif
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            int32_t handle = static_cast<int32_t>(make_handle<Rep>(v.rt, v.rep, /*own*/ false));
            return {handle};
#else
            // For borrowed resources, create a handle representing a borrow (stub: address-based)
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            return {static_cast<int32_t>(addr & 0xFFFFFFFF)};
#endif
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline T load(const LiftLowerContext &cx, uint32_t ptr)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            auto handle = integer::load<int32_t>(cx, ptr, 4);
            auto sh = get_handle<Rep>(handle);
            if (sh)
                return T(sh->rt, sh->rep);
            return T(nullptr, Rep{});
#else
            auto handle = integer::load<int32_t>(cx, ptr, 4);
            // Stub: return default resource
            return T(nullptr, typename T::rep_type{});
#endif
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline T load(const LiftLowerContext &cx, uint32_t ptr)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            auto handle = integer::load<int32_t>(cx, ptr, 4);
            auto sh = get_handle<Rep>(handle);
            if (sh)
                return T(sh->rt, sh->rep);
            return T(nullptr, Rep{});
#else
            auto handle = integer::load<int32_t>(cx, ptr, 4);
            // Stub: return default resource
            return T(nullptr, typename T::rep_type{});
#endif
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            auto handle = vi.next<int32_t>();
            auto sh = get_handle<Rep>(handle);
            if (sh)
            {
                remove_handle<Rep>(handle); // transfer
                return T(sh->rt, sh->rep);
            }
            return T(nullptr, Rep{});
#else
            auto handle = vi.next<int32_t>();
            // Stub: return default resource
            return T(nullptr, typename T::rep_type{});
#endif
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
#ifdef CMCPP_EXPERIMENTAL_RESOURCES
            using Rep = typename T::rep_type;
            auto handle = vi.next<int32_t>();
            auto sh = get_handle<Rep>(handle);
            if (sh)
                return T(sh->rt, sh->rep);
            return T(nullptr, Rep{});
#else
            auto handle = vi.next<int32_t>();
            // Stub: return default resource
            return T(nullptr, typename T::rep_type{});
#endif
        }

        // Canonical ABI resource functions (stubs for now)
        template <typename T>
        inline uint32_t canon_resource_new(ResourceType<T> *rt, uint32_t rep)
        {
            // Stub: just echo the representation as the handle
            (void)rt;
            return rep;
        }

        template <typename T>
        inline void canon_resource_drop(ResourceType<T> *rt, uint32_t handle, bool sync = true)
        {
            // Stub: call destructor with the handle cast to rep type
            (void)sync;
            if (rt && rt->dtor)
            {
                rt->dtor(static_cast<T>(handle));
            }
        }

        template <typename T>
        inline uint32_t canon_resource_rep(ResourceType<T> *rt, uint32_t handle)
        {
            // Stub: return the handle as representation
            (void)rt;
            return handle;
        }
    }

    // Forward declarations for resource types in the main namespace
    template <typename T>
        requires cmcpp::Own<T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        resource::store(cx, v, ptr);
    }

    template <typename T>
        requires cmcpp::Borrow<T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        resource::store(cx, v, ptr);
    }

    template <typename T>
        requires cmcpp::Own<T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return resource::lower_flat(cx, v);
    }

    template <typename T>
        requires cmcpp::Borrow<T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return resource::lower_flat(cx, v);
    }

    template <typename T>
        requires cmcpp::Own<T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return resource::load<T>(cx, ptr);
    }

    template <typename T>
        requires cmcpp::Borrow<T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return resource::load<T>(cx, ptr);
    }

    template <typename T>
        requires cmcpp::Own<T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return resource::lift_flat<T>(cx, vi);
    }

    template <typename T>
        requires cmcpp::Borrow<T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return resource::lift_flat<T>(cx, vi);
    }
}

#endif
