#ifndef CMCPP_MAP_HPP
#define CMCPP_MAP_HPP

#include "context.hpp"
#include "list.hpp"
#include "util.hpp"

namespace cmcpp
{
    namespace map
    {
        template <typename T>
        using entry_type_t = typename ValTrait<T>::entry_type;

        template <typename T, std::enable_if_t<Map<T>, int> = 0>
        list_t<entry_type_t<T>> entries_from_map(const T &map_value)
        {
            list_t<entry_type_t<T>> entries;
            entries.reserve(map_value.size());
            for (const auto &[key, value] : map_value)
            {
                entries.emplace_back(key, value);
            }
            return entries;
        }

        template <typename T, std::enable_if_t<Map<T>, int> = 0>
        T map_from_entries(const list_t<entry_type_t<T>> &entries)
        {
            T map_value;
            for (const auto &entry : entries)
            {
                map_value[std::get<0>(entry)] = std::get<1>(entry);
            }
            return map_value;
        }

        template <typename T, std::enable_if_t<Map<T>, int> = 0>
        void store(LiftLowerContext &cx, const T &map_value, offset ptr)
        {
            list::store(cx, entries_from_map(map_value), ptr);
        }

        template <typename T, std::enable_if_t<Map<T>, int> = 0>
        WasmValVector lower_flat(LiftLowerContext &cx, const T &map_value)
        {
            return list::lower_flat(cx, entries_from_map(map_value));
        }

        template <typename T, std::enable_if_t<Map<T>, int> = 0>
        T load(const LiftLowerContext &cx, offset ptr)
        {
            auto entries = list::load<entry_type_t<T>>(cx, ptr);
            return map_from_entries<T>(entries);
        }

        template <typename T, std::enable_if_t<Map<T>, int> = 0>
        T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
            auto entries = list::lift_flat<entry_type_t<T>>(cx, vi);
            return map_from_entries<T>(entries);
        }
    }

    template <typename T, std::enable_if_t<Map<T>, int> = 0>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        map::store(cx, v, ptr);
    }

    template <typename T, std::enable_if_t<Map<T>, int> = 0>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return map::lower_flat(cx, v);
    }

    template <typename T, std::enable_if_t<Map<T>, int> = 0>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return map::load<T>(cx, ptr);
    }

    template <typename T, std::enable_if_t<Map<T>, int> = 0>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return map::lift_flat<T>(cx, vi);
    }
}

#endif