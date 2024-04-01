#include "context.hpp"
#include "lower.hpp"
#include "lift.hpp"
#include "val.hpp"
#include "util.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

bool equal_modulo_string_encoding(auto s, auto t)
{
    // If both are null, they are equal
    if (s == nullptr && t == nullptr)
    {
        return true;
    }

    // If both are of type bool, int, float, or string, compare them directly
    if ((std::is_same<decltype(s), bool>::value || std::is_same<decltype(s), int>::value || std::is_same<decltype(s), float>::value || std::is_same<decltype(s), std::string>::value) &&
        (std::is_same<decltype(t), bool>::value || std::is_same<decltype(t), int>::value || std::is_same<decltype(t), float>::value || std::is_same<decltype(t), std::string>::value))
    {
        return s == t;
    }

    // If both are tuples, compare their size and each element
    if (typeid(s) == typeid(std::tuple<std::string>) && typeid(t) == typeid(std::tuple<std::string>))
    {
        if (std::tuple_size<decltype(s)>::value != std::tuple_size<decltype(t)>::value)
        {
            return false;
        }

        for (size_t i = 0; i < std::tuple_size<decltype(s)>::value; ++i)
        {
            if (std::get<i>(s) != std::get<i>(t))
            {
                return false;
            }
        }
        return true;
    }

    // If both are dictionaries, compare their values
    if (typeid(s) == typeid(std::map<std::string, decltype(s.begin()->second)>) && typeid(t) == typeid(std::map<std::string, decltype(t.begin()->second)>))
    {
        for (auto it_s = s.begin(), it_t = t.begin(); it_s != s.end() && it_t != t.end(); ++it_s, ++it_t)
        {
            if (!equal_modulo_string_encoding(it_s->second, it_t->second))
            {
                return false;
            }
        }
        return true;
    }

    // If both are lists, compare their elements
    if (typeid(s) == typeid(std::vector<decltype(s[0])>) && typeid(t) == typeid(std::vector<decltype(t[0])>))
    {
        for (size_t i = 0; i < s.size() && i < t.size(); ++i)
        {
            if (!equal_modulo_string_encoding(s[i], t[i]))
            {
                return false;
            }
        }
        return true;
    }

    // If the types are not supported, throw an exception
    throw std::runtime_error("Unsupported types for comparison");
}

#include <vector>
#include <iostream>

class Heap
{
private:
    std::vector<uint8_t> memory;
    size_t last_alloc;

    // Function to align the pointer to the given alignment
    size_t align_to(size_t ptr, size_t alignment)
    {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }

public:
    // Constructor that initializes the memory with the given size
    Heap(size_t arg) : memory(arg), last_alloc(0) {}

    // Function to reallocate memory
    size_t realloc(size_t original_ptr, size_t original_size, size_t alignment, size_t new_size)
    {
        if (original_ptr != 0 && new_size < original_size)
        {
            return align_to(original_ptr, alignment);
        }
        size_t ret = align_to(last_alloc, alignment);
        last_alloc = ret + new_size;
        if (last_alloc > memory.size())
        {
            std::cout << "oom: have " << memory.size() << " need " << last_alloc << std::endl;
            throw std::runtime_error("Out of memory");
        }
        std::copy(memory.begin() + original_ptr, memory.begin() + original_ptr + original_size, memory.begin() + ret);
        return ret;
    }
};

#include <tuple>
#include <string>

// Function to create a tuple with a string, its encoding, and its length
std::tuple<std::string, std::string, size_t> mk_str(const std::string &s)
{
    return std::make_tuple(s, "utf8", s.size());
}

#include <map>
#include <vector>
#include <string>

// Function to convert a vector to a map with indices as keys
template <typename T>
std::map<int, T> vectorToMap(const std::vector<T> &vec)
{
    std::map<int, T> result;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        result[i] = vec[i];
    }
    return result;
}

// Function to recursively convert nested vectors to maps
template <typename T>
std::map<int, T> mk_tup_rec(const T &x)
{
    if constexpr (std::is_same<T, std::vector<T>>::value)
    {
        std::map<int, T> result;
        for (size_t i = 0; i < x.size(); ++i)
        {
            result[i] = mk_tup_rec(x[i]);
        }
        return result;
    }
    else
    {
        return x;
    }
}

// Function to convert a list of arguments into a map
template <typename... Args>
std::map<int, std::map<int, Args...>> mk_tup(Args... args)
{
    std::vector<std::map<int, Args...>> vec = {mk_tup_rec(args)...};
    return vectorToMap(vec);
}

// Function to create a CallContext
// Takes a memory (as a vector of bytes), an encoding (as a string), a realloc function, and a post_return function as arguments
// Returns a CallContext with the given options and a new ComponentInstance
cmcpp::CallContextPtr mk_cx(std::vector<uint8_t> memory = std::vector<uint8_t>(), cmcpp::HostEncoding encoding = cmcpp::HostEncoding::Utf8, std::function<size_t(size_t, size_t, size_t, size_t)> realloc = nullptr, std::function<void()> post_return = nullptr)
{
    return cmcpp::createCallContext({memory.data(), memory.size()}, realloc, cmcpp::encodeTo, encoding, post_return);
}

const char *const ValTypeNames[] = {
    "Unknown",
    "Bool",
    "S8",
    "U8",
    "S16",
    "U16",
    "S32",
    "U32",
    "S64",
    "U64",
    "Float32",
    "Float64",
    "Char",
    "String",
    "List",
    "Field",
    "Record",
    "Tuple",
    "Case",
    "Variant",
    "Enum",
    "Option",
    "Result",
    "Flags",
    "Own",
    "Borrow"};

std::string test_name(cmcpp::ValType t)
{
    return std::string("test(") + ValTypeNames[(uint8_t)t] + /*"," + to_string(vals_to_lift) + "," + v.to_string() +*/ "):";
};

// Function to test the component model
void test(cmcpp::Val t, std::vector<cmcpp::Val> vals_to_lift, cmcpp::Val v,
          cmcpp::CallContextPtr cx = mk_cx(),
          std::string dst_encoding = "",
          cmcpp::ValType lower_t = cmcpp::ValType::Unknown,
          cmcpp::Val lower_v = nullptr)
{

    // Create a vector of Values. Each Value is constructed with a pair of elements from
    // the flattened type (t) and the vals_to_lift vectors. This is equivalent to the Python
    // list comprehension used in the original code.
    std::vector<cmcpp::WasmVal> vi;
    // for (auto it = std::make_pair(cmcpp::flatten_type(t).begin(), vals_to_lift.begin());
    //      it.first != cmcpp::flatten_type(t).end() && it.second != vals_to_lift.end();
    //      ++it.first, ++it.second)
    // {
    //     vi.push_back(cmcpp::WasmVal(*it.first, *it.second));
    // }
}

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/type_index.hpp>

template <class T>
auto my_io(const T &value);

namespace detail
{
    // Helpers to print individual values
    void print_each(std::ostream &out, std::string s)
    {
        std::cout << boost::typeindex::type_id<std::string>().pretty_name() << std::endl;
        out << s;
    }
    void print_each(std::ostream &out, std::uint8_t v)
    {
        std::cout << boost::typeindex::type_id<std::uint8_t>().pretty_name() << std::endl;
        out << static_cast<unsigned>(v);
    }
    void print_each(std::ostream &out, std::int8_t v)
    {
        std::cout << boost::typeindex::type_id<std::int8_t>().pretty_name() << std::endl;
        out << static_cast<int>(v);
    }
    void print_each(std::ostream &out, int v)
    {
        std::cout << boost::typeindex::type_id<int>().pretty_name() << std::endl;
        out << static_cast<int>(v);
    }

    template <class T>
    void print_each(std::ostream &out, const T &v)
    {
        std::cout << boost::typeindex::type_id<T>().pretty_name() << std::endl;
        const auto &x = v;
        out << ' ' << my_io(x);
    }

    template <class T>
    void print_each(std::ostream &out, std::vector<T> v)
    {
        std::cout << boost::typeindex::type_id<T>().pretty_name() << std::endl;
        out << "[";
        for (const auto &x : v)
        {
            out << "{";
            out << ' ' << my_io(x);
            out << "}, ";
        }
        out << "]";
    }

    template <class T>
    struct io_reference
    {
        typedef T type;
        const T &value;
    };

    // Output each field of io_reference::value
    template <class T>
    std::ostream &operator<<(std::ostream &out, io_reference<T> &&x)
    {
        std::cout << boost::typeindex::type_id<T>().pretty_name() << std::endl;
        const char *sep = "";
        constexpr auto names = boost::pfr::names_as_array<T>();

        boost::pfr::for_each_field(x.value, [&](const auto &v, std::size_t idx)
                                   {
            out << std::exchange(sep, ", ") << names[idx] << ": ";
            detail::print_each(out, v); });
        return out;
    }
}

// Definition:
template <class T>
auto my_io(const T &value)
{
    return detail::io_reference<T>{value};
}

template <typename T>
void lowerxxx(T v)
{
    constexpr auto names = boost::pfr::names_as_array<T>();
    boost::pfr::for_each_field(v, [&names](const auto &field, std::size_t idx)
                               { std::cout << idx << ": " << names[idx] << " = " << field << '\n'; 
                               return; });
}

struct Person
{
    std::string lname;
    std::string rname;
    int age;
    std::vector<Person> children;
};

struct World
{
    Person p;
};

TEST_CASE("component-model-cpp")
{
    // std::cout << "component-model-cpp" << std::endl;
    // Person p{"John", "Doe", 42, {{"Jane", "Doe", 41, {{"Jane", "Doe", 41, {}}, {"Jim", "Doe", 40, {}}}}, {"Jim", "Doe", 40, {}}}};
    // World w{p};
    // std::cout << my_io(w) << std::endl;
}