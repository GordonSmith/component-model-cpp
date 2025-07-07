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

    // Simple resource handle for now - can be expanded later
    template <typename T>
    struct SimpleResourceHandle
    {
        uint32_t rep;
        bool own;
        ResourceType<T> *rt;

        SimpleResourceHandle(uint32_t rep, bool own, ResourceType<T> *rt)
            : rep(rep), own(own), rt(rt) {}
    };

    namespace resource
    {
        template <typename T>
            requires cmcpp::Own<T>
        inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
        {
            // For owned resources, store the representation as i32
            // In this simplified implementation, we use a hash or address
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            integer::store(cx, static_cast<int32_t>(addr & 0xFFFFFFFF), ptr, 4);
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
        {
            // For borrowed resources, store the representation as i32
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            integer::store(cx, static_cast<int32_t>(addr & 0xFFFFFFFF), ptr, 4);
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
            // For owned resources, create a handle representing ownership transfer
            // In a full implementation, this would create a handle in the guest's resource table
            // For now, we use a simple hash/address approach
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            return {static_cast<int32_t>(addr & 0xFFFFFFFF)};
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
            // For borrowed resources, create a handle representing a borrow
            // In a full implementation, this would create a borrow handle with lifetime tracking
            uintptr_t addr = reinterpret_cast<uintptr_t>(&v.rep);
            return {static_cast<int32_t>(addr & 0xFFFFFFFF)};
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline T load(const LiftLowerContext &cx, uint32_t ptr)
        {
            auto handle = integer::load<int32_t>(cx, ptr, 4);
            // In a full implementation, this would look up the resource in tables
            // and transfer ownership from guest to host
            // For now, create a default-constructed resource with the handle as representation
            return T(nullptr, typename T::rep_type{});
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline T load(const LiftLowerContext &cx, uint32_t ptr)
        {
            auto handle = integer::load<int32_t>(cx, ptr, 4);
            // In a full implementation, this would look up the resource in tables
            // and create a borrow reference
            return T(nullptr, typename T::rep_type{});
        }

        template <typename T>
            requires cmcpp::Own<T>
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
            auto handle = vi.next<int32_t>();
            // In a full implementation, this would:
            // 1. Look up the handle in the guest resource table
            // 2. Transfer ownership from guest to host
            // 3. Remove the handle from guest table
            return T(nullptr, typename T::rep_type{});
        }

        template <typename T>
            requires cmcpp::Borrow<T>
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
            auto handle = vi.next<int32_t>();
            // In a full implementation, this would:
            // 1. Look up the handle in the guest resource table
            // 2. Create a borrow reference (no ownership transfer)
            // 3. Track the borrow for lifetime management
            return T(nullptr, typename T::rep_type{});
        }

        // Canonical ABI resource functions (stubs for now)
        template <typename T>
        inline uint32_t canon_resource_new(ResourceType<T> *rt, uint32_t rep)
        {
            // In a full implementation, this would create a new resource handle
            // and add it to the appropriate resource table
            return rep; // Simplified for now
        }

        template <typename T>
        inline void canon_resource_drop(ResourceType<T> *rt, uint32_t handle, bool sync = true)
        {
            // In a full implementation, this would:
            // 1. Remove the handle from the resource table
            // 2. If owned, call the destructor if present
            // 3. Handle cross-instance cleanup
            if (rt && rt->dtor)
            {
                rt->dtor(static_cast<T>(handle));
            }
        }

        template <typename T>
        inline uint32_t canon_resource_rep(ResourceType<T> *rt, uint32_t handle)
        {
            // In a full implementation, this would look up the handle
            // and return its representation
            return handle; // Simplified for now
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
