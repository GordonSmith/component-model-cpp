#ifndef VAL3_HPP
#define VAL3_HPP

#include "val.hpp"

#include <type_traits>
#include <concepts>
#include <string>
#include <vector>
#include <tuple>
#include <iostream>
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/type_index.hpp>

template <typename T>
constexpr bool IsTuple = false;

template <typename... types>
constexpr bool IsTuple<std::tuple<types...>> = true;

namespace cmcpp3
{
    using float32_t = float;
    using float64_t = double;

    template <typename T>
    concept Numeric = std::is_arithmetic_v<T>;

    // template <Numeric T>
    // std::vector<cmcpp::WasmVal> print()
    // {
    //     std::cout << boost::typeindex::type_id<T>().pretty_name() << std::endl;
    //     return {};
    // }

    template <Numeric T>
    std::vector<cmcpp::WasmVal> print(const T &x)
    {
        std::cout << boost::typeindex::type_id<T>().pretty_name();
        return {};
    }

    std::vector<cmcpp::WasmVal> print(const char *x)
    {
        std::cout << boost::typeindex::type_id<char *>().pretty_name();
        return {};
    }

    std::vector<cmcpp::WasmVal> print(const std::string &x)
    {
        std::cout << "string";
        return {};
    }

    // template <typename T>
    // concept VectorType = requires(T t) {
    //     {
    //         t.size()
    //     } -> std::same_as<std::size_t>; // Requires a size() member function
    //     {
    //         t.push_back(std::declval<typename T::value_type>())
    //     };
    // };

    template <typename T>
    std::vector<cmcpp::WasmVal> print(const std::tuple<T> &x);

    template <typename T>
    std::vector<cmcpp::WasmVal> print(const std::string_view &label, const T &x);

    template <typename T>
    std::vector<cmcpp::WasmVal> print(const T &x);

    template <typename... T>
    std::vector<cmcpp::WasmVal> print(const std::tuple<T...> &x);

    template <typename T>
    std::vector<cmcpp::WasmVal> print(const std::vector<T> &x)
    {
        std::cout << "List<";
        print(T{});
        std::cout << ">";
        // for (const auto &v : x)
        // {
        //     print(v);
        // }
        return {};
    }

    template <typename T>
    std::vector<cmcpp::WasmVal> print(const std::string_view &label, const T &x)
    {
        std::cout << "Field<" << label << ": ";
        print(x);
        std::cout << ">";
        return {};
    }

    template <class Tuple, std::size_t N>
    struct TuplePrinter
    {
        static void tprint(const Tuple &t)
        {
            TuplePrinter<Tuple, N - 1>::tprint(t);
            std::cout << ", ";
            print(std::get<N - 1>(t));
        }
    };

    template <class Tuple>
    struct TuplePrinter<Tuple, 1>
    {
        static void tprint(const Tuple &t)
        {
            print(std::get<0>(t));
        }
    };

    template <typename... Args>
    std::vector<cmcpp::WasmVal> print(const std::tuple<Args...> &x)
    {
        std::cout << "Tuple<";
        TuplePrinter<decltype(x), sizeof...(Args)>::tprint(x);
        std::cout << ">";
        return {};
    }

    std::map<std::string, bool> dedup;

    template <typename T>
    std::vector<cmcpp::WasmVal> print(const T &x)
    {
        const std::string label = boost::typeindex::type_id<T>().pretty_name();
        std::cout << "Record<" << label << ">";

        if (!dedup[label])
        {
            dedup[label] = true;
            constexpr auto names = boost::pfr::names_as_array<T>();
            boost::pfr::for_each_field(x, [&names](const auto &field, std::size_t idx)
                                       {
                                        std::cout << std::endl;
                                        print(names[idx], field);
                            return; });
        }
        return {};
    }

    // template <std::size_t I = 0, typename... Tp>
    // inline typename std::enable_if<I == sizeof...(Tp), void>::type
    // printTuple(std::tuple<Tp...> &t)
    // {
    //     // Base case: no more elements to print
    // }

    // template <std::size_t I = 0, typename... Tp>
    //     inline typename std::enable_if < I<sizeof...(Tp), void>::type
    //                                      printTuple(std::tuple<Tp...> &t)
    // {
    //     std::cout << "Tuple" << std::endl; // Print the current element
    //     printTuple<I + 1, Tp...>(t);       // Recurse to the next element
    // }
}
#endif
