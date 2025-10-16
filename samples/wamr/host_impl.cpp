#include "generated/sample.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

// Host implementations of the generated interface functions
// These are the actual implementations that the host provides for guest functions

namespace host
{

    namespace booleans
    {

        cmcpp::bool_t and_(cmcpp::bool_t a, cmcpp::bool_t b)
        {
            std::cout << "[HOST] and(" << (a ? "true" : "false") << ", " << (b ? "true" : "false") << ") = " << (a && b ? "true" : "false") << std::endl;
            return a && b;
        }

    } // namespace booleans

    namespace floats
    {

        cmcpp::float64_t add(cmcpp::float64_t a, cmcpp::float64_t b)
        {
            std::cout << "[HOST] add(" << a << ", " << b << ") = " << (a + b) << std::endl;
            return a + b;
        }

    } // namespace floats

    namespace strings
    {

        cmcpp::string_t reverse(cmcpp::string_t a)
        {
            std::string result = a;
            std::reverse(result.begin(), result.end());
            std::cout << "[HOST] reverse(\"" << a << "\") = \"" << result << "\"" << std::endl;
            return result;
        }

        uint32_t lots(cmcpp::string_t p1, cmcpp::string_t p2, cmcpp::string_t p3, cmcpp::string_t p4,
                      cmcpp::string_t p5, cmcpp::string_t p6, cmcpp::string_t p7, cmcpp::string_t p8,
                      cmcpp::string_t p9, cmcpp::string_t p10, cmcpp::string_t p11, cmcpp::string_t p12,
                      cmcpp::string_t p13, cmcpp::string_t p14, cmcpp::string_t p15, cmcpp::string_t p16,
                      cmcpp::string_t p17)
        {
            size_t total_length = p1.length() + p2.length() + p3.length() + p4.length() + p5.length() +
                                  p6.length() + p7.length() + p8.length() + p9.length() + p10.length() +
                                  p11.length() + p12.length() + p13.length() + p14.length() + p15.length() +
                                  p16.length() + p17.length();
            std::cout << "[HOST] lots(...17 strings...) = " << total_length << std::endl;
            if (total_length > std::numeric_limits<uint32_t>::max())
            {
                throw std::overflow_error("lots string length exceeds 32-bit range");
            }
            return static_cast<uint32_t>(total_length);
        }

    } // namespace strings

    namespace lists
    {
        cmcpp::list_t<cmcpp::string_t> filter_bool(cmcpp::list_t<host::lists::v> a)
        {
            std::cout << "[HOST] filter_bool([" << a.size() << " variants])" << std::endl;
            cmcpp::list_t<cmcpp::string_t> result;
            for (const auto &variant : a)
            {
                if (std::holds_alternative<cmcpp::string_t>(variant))
                {
                    result.push_back(std::get<cmcpp::string_t>(variant));
                }
            }
            return result;
        }

    }

    namespace logging
    {

        void log_bool(cmcpp::bool_t a, cmcpp::string_t s)
        {
            std::cout << "[HOST LOG] " << s << ": " << (a ? "true" : "false") << std::endl;
        }

        void log_u32(uint32_t a, cmcpp::string_t s)
        {
            std::cout << "[HOST LOG] " << s << ": " << a << std::endl;
        }

        void log_u64(uint64_t a, cmcpp::string_t s)
        {
            std::cout << "[HOST LOG] " << s << ": " << a << std::endl;
        }

        void log_f32(cmcpp::float32_t a, cmcpp::string_t s)
        {
            std::cout << "[HOST LOG] " << s << ": " << a << std::endl;
        }

        void log_f64(cmcpp::float64_t a, cmcpp::string_t s)
        {
            std::cout << "[HOST LOG] " << s << ": " << a << std::endl;
        }

        void log_str(cmcpp::string_t a, cmcpp::string_t s)
        {
            std::cout << "[HOST LOG] " << s << ": \"" << a << "\"" << std::endl;
        }

    } // namespace logging

    // Standalone function implementations (not in a sub-namespace)
    void void_func()
    {
        std::cout << "[HOST] void_func() called" << std::endl;
    }

} // namespace host
