#ifndef CMCPP_MONOSTATE_HPP
#define CMCPP_MONOSTATE_HPP

#include "context.hpp"
#include "traits.hpp"
#include "util.hpp"

namespace cmcpp
{
    //  Monostate (unit type for variant cases without payload)  ---------------
    template <typename T>
        requires std::is_same_v<T, std::monostate>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        // Monostate has no data to store (size = 0)
    }

    template <typename T>
        requires std::is_same_v<T, std::monostate>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        // Monostate has no data to load (size = 0)
        return T{};
    }

    template <typename T>
        requires std::is_same_v<T, std::monostate>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        // Monostate has no flat representation (empty vector)
        return {};
    }

    template <typename T>
        requires std::is_same_v<T, std::monostate>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        // Monostate has no data to lift
        return T{};
    }

    //  Result-specific monostates (for result<_, _> edge cases)  --------------
    template <typename T>
        requires std::is_same_v<T, result_ok_monostate> || std::is_same_v<T, result_err_monostate>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        // Result monostates have no data to store (size = 0)
    }

    template <typename T>
        requires std::is_same_v<T, result_ok_monostate> || std::is_same_v<T, result_err_monostate>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        // Result monostates have no data to load (size = 0)
        return T{};
    }

    template <typename T>
        requires std::is_same_v<T, result_ok_monostate> || std::is_same_v<T, result_err_monostate>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        // Result monostates have no flat representation (empty vector)
        return {};
    }

    template <typename T>
        requires std::is_same_v<T, result_ok_monostate> || std::is_same_v<T, result_err_monostate>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        // Result monostates have no data to lift
        return T{};
    }

    //  Result wrappers (for result<T, T> edge cases)  -------------------------
    template <typename T>
    inline void store(LiftLowerContext &cx, const result_ok_wrapper<T> &v, uint32_t ptr)
    {
        store(cx, v.value, ptr);
    }

    template <typename T>
    inline void store(LiftLowerContext &cx, const result_err_wrapper<T> &v, uint32_t ptr)
    {
        store(cx, v.value, ptr);
    }

    template <typename T>
    inline result_ok_wrapper<T> load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return result_ok_wrapper<T>{load<T>(cx, ptr)};
    }

    template <typename T>
    inline result_err_wrapper<T> load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return result_err_wrapper<T>{load<T>(cx, ptr)};
    }

    template <typename T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const result_ok_wrapper<T> &v)
    {
        return lower_flat(cx, v.value);
    }

    template <typename T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const result_err_wrapper<T> &v)
    {
        return lower_flat(cx, v.value);
    }

    template <typename T>
    inline result_ok_wrapper<T> lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return result_ok_wrapper<T>{lift_flat<T>(cx, vi)};
    }

    template <typename T>
    inline result_err_wrapper<T> lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return result_err_wrapper<T>{lift_flat<T>(cx, vi)};
    }
}

// std::tuple_size and std::tuple_element specializations for result wrappers
// These are needed for std::apply to work with result_ok_wrapper/result_err_wrapper
// The wrappers act as single-element tuples containing their wrapped value
namespace std
{
    template <typename T>
    struct tuple_size<cmcpp::result_ok_wrapper<T>> : std::integral_constant<std::size_t, std::tuple_size_v<T>>
    {
    };

    template <std::size_t I, typename T>
    struct tuple_element<I, cmcpp::result_ok_wrapper<T>>
    {
        using type = std::tuple_element_t<I, T>;
    };

    template <typename T>
    struct tuple_size<cmcpp::result_err_wrapper<T>> : std::integral_constant<std::size_t, std::tuple_size_v<T>>
    {
    };

    template <std::size_t I, typename T>
    struct tuple_element<I, cmcpp::result_err_wrapper<T>>
    {
        using type = std::tuple_element_t<I, T>;
    };
}

// std::get specializations for result wrappers to support structured bindings and std::apply
// Forward get<I> calls to the wrapped tuple
namespace std
{
    template <std::size_t I, typename T>
    constexpr decltype(auto) get(cmcpp::result_ok_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(const cmcpp::result_ok_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(cmcpp::result_ok_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(const cmcpp::result_ok_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(cmcpp::result_err_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(const cmcpp::result_err_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(cmcpp::result_err_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    template <std::size_t I, typename T>
    constexpr decltype(auto) get(const cmcpp::result_err_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }
}

#endif // CMCPP_MONOSTATE_HPP
