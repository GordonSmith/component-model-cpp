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

    // Concept to match only result wrapper types
    template <typename T>
    concept IsResultWrapper = is_result_wrapper<T>::value;

    // Store and lower_flat functions for result_ok_wrapper and result_err_wrapper
    // These delegate to the wrapped type's functions
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
    inline WasmValVector lower_flat(LiftLowerContext &cx, const result_ok_wrapper<T> &v)
    {
        return lower_flat(cx, v.value);
    }

    template <typename T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const result_err_wrapper<T> &v)
    {
        return lower_flat(cx, v.value);
    }

    // Load and lift_flat for result wrappers - constrained to only match wrapper types
    // This prevents ambiguity with other load/lift_flat overloads
    template <IsResultWrapper T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        using inner_type = typename ValTrait<T>::inner_type;
        return T{load<inner_type>(cx, ptr)};
    }

    template <IsResultWrapper T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        using inner_type = typename ValTrait<T>::inner_type;
        return T{lift_flat<inner_type>(cx, vi)};
    }
}

// std::tuple_size and std::tuple_element specializations for result wrappers
// These are needed for std::apply to work with result_ok_wrapper/result_err_wrapper
// Wrappers for tuple types forward to the wrapped tuple
// Wrappers for non-tuple types act as single-element tuples
namespace std
{
    // Helper to detect if type is tuple-like
    template <typename T, typename = void>
    struct is_tuple_like : std::false_type
    {
    };

    template <typename T>
    struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type
    {
    };

    // For tuple-wrapped types, forward the size
    template <typename T>
    struct tuple_size<cmcpp::result_ok_wrapper<T>> : std::conditional_t<
                                                         is_tuple_like<T>::value,
                                                         std::integral_constant<std::size_t, std::tuple_size_v<T>>,
                                                         std::integral_constant<std::size_t, 1>>
    {
    };

    // For tuple-wrapped types, forward the element type; for non-tuple, return T
    template <std::size_t I, typename T>
    struct tuple_element<I, cmcpp::result_ok_wrapper<T>>
    {
        using type = std::conditional_t<is_tuple_like<T>::value, std::tuple_element_t<I, T>, T>;
    };

    template <typename T>
    struct tuple_size<cmcpp::result_err_wrapper<T>> : std::conditional_t<
                                                          is_tuple_like<T>::value,
                                                          std::integral_constant<std::size_t, std::tuple_size_v<T>>,
                                                          std::integral_constant<std::size_t, 1>>
    {
    };

    template <std::size_t I, typename T>
    struct tuple_element<I, cmcpp::result_err_wrapper<T>>
    {
        using type = std::conditional_t<is_tuple_like<T>::value, std::tuple_element_t<I, T>, T>;
    };
}

// std::get specializations for result wrappers to support structured bindings and std::apply
// For tuple-wrapped types, forward get<I> calls to the wrapped tuple
// For non-tuple types, return the value directly when I == 0
namespace std
{
    // result_ok_wrapper - tuple wrapped types
    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<T &>()))>
    get(cmcpp::result_ok_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<const T &>()))>
    get(const cmcpp::result_ok_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<T &&>()))>
    get(cmcpp::result_ok_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<const T &&>()))>
    get(const cmcpp::result_ok_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    // result_ok_wrapper - non-tuple wrapped types (I must be 0)
    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, T &>
    get(cmcpp::result_ok_wrapper<T> &wrapper) noexcept
    {
        return wrapper.value;
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, const T &>
    get(const cmcpp::result_ok_wrapper<T> &wrapper) noexcept
    {
        return wrapper.value;
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, T &&>
    get(cmcpp::result_ok_wrapper<T> &&wrapper) noexcept
    {
        return std::move(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, const T &&>
    get(const cmcpp::result_ok_wrapper<T> &&wrapper) noexcept
    {
        return std::move(wrapper.value);
    }

    // result_err_wrapper - tuple wrapped types
    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<T &>()))>
    get(cmcpp::result_err_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<const T &>()))>
    get(const cmcpp::result_err_wrapper<T> &wrapper) noexcept
    {
        return std::get<I>(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<T &&>()))>
    get(cmcpp::result_err_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<is_tuple_like<T>::value, decltype(std::get<I>(std::declval<const T &&>()))>
    get(const cmcpp::result_err_wrapper<T> &&wrapper) noexcept
    {
        return std::get<I>(std::move(wrapper.value));
    }

    // result_err_wrapper - non-tuple wrapped types (I must be 0)
    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, T &>
    get(cmcpp::result_err_wrapper<T> &wrapper) noexcept
    {
        return wrapper.value;
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, const T &>
    get(const cmcpp::result_err_wrapper<T> &wrapper) noexcept
    {
        return wrapper.value;
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, T &&>
    get(cmcpp::result_err_wrapper<T> &&wrapper) noexcept
    {
        return std::move(wrapper.value);
    }

    template <std::size_t I, typename T>
    constexpr std::enable_if_t<!is_tuple_like<T>::value && I == 0, const T &&>
    get(const cmcpp::result_err_wrapper<T> &&wrapper) noexcept
    {
        return std::move(wrapper.value);
    }
}

#endif // CMCPP_MONOSTATE_HPP
