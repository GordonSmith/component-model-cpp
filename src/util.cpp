#include "util.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <limits>
#include <locale>
#include <codecvt>

#include <fmt/core.h>

namespace cmcpp
{

    void trap_if(const LiftLowerContext &cx, bool condition, const char *message)
    {
        if (condition)
        {
            cx.opts->trap(message);
        }
    }

    ValType despecialize(const ValType t)
    {
        switch (t)
        {
        case ValType::Tuple:
            return ValType::Record;
        case ValType::Enum:
            return ValType::Variant;
        case ValType::Option:
            return ValType::Variant;
        case ValType::Result:
            return ValType::Variant;
        }
        return t;
    }

    Val despecialize(const Val &v)
    {
        switch (valType(v))
        {
        case ValType::Tuple:
        {
            auto r = std::make_shared<record_t>();
            auto vars = std::get<tuple_ptr>(v)->vs;
            for (size_t i = 0; i < vars.size(); ++i)
            {
                auto var = vars[i];
                auto f = std::make_shared<field_t>(fmt::format("{}", i), despecialize(var));
                r->fields.push_back(f);
            }
            return r;
        }
        case ValType::Enum:
        {
            std::vector<case_ptr> cases;
            for (auto label : std::get<enum_ptr>(v)->labels)
            {
                auto c = std::make_shared<case_t>(label);
                cases.push_back(c);
            }
            return std::make_shared<variant_t>(cases);
        }
        case ValType::Option:
            return std::make_shared<variant_t>(std::vector<case_ptr>({std::make_shared<case_t>("None"), std::make_shared<case_t>("Some", std::get<option_ptr>(v)->v)}));
        case ValType::Result:
            return std::make_shared<variant_t>(std::vector<case_ptr>({std::make_shared<case_t>("Ok", std::get<result_ptr>(v)->ok), std::make_shared<case_t>("Error", std::get<result_ptr>(v)->error)}));
        default:
            return v;
        }
    }

    bool convert_int_to_bool(int32_t i)
    {
        return i != 0;
    }

    wchar_t convert_i32_to_char(const LiftLowerContext &, int32_t i)
    {
        // assert(i >= 0);
        // assert(i < 0x110000);
        // assert(!(0xD800 <= i && i <= 0xDFFF));
        return static_cast<wchar_t>(i);
    }

    ValType discriminant_type(const std::vector<case_ptr> &cases)
    {
        size_t n = cases.size();

        assert(0 < n && n < std::numeric_limits<unsigned int>::max());
        int match = std::ceil<int>(std::log2(n) / 8);
        switch (match)
        {
        case 0:
            return ValType::U8;
        case 1:
            return ValType::U8;
        case 2:
            return ValType::U16;
        case 3:
            return ValType::U32;
        default:
            throw std::runtime_error("Invalid match value");
        }
    }

    uint32_t align_to(uint32_t ptr, uint32_t alignment)
    {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }

    bool isAligned(uint32_t ptr, uint32_t alignment) { return (ptr & (alignment - 1)) == 0; }

    bool convert_int_to_bool(uint8_t i)
    {
        return i > 0;
    }

    int alignment_record(const std::vector<field_ptr> &fields)
    {
        int a = 1;
        for (auto f : fields)
        {
            a = std::max(a, alignment(f->v));
        }
        return a;
    }

    int max_case_alignment(const std::vector<case_ptr> &cases)
    {
        int a = 1;
        for (auto c : cases)
        {
            if (c->v.has_value())
            {
                a = std::max(a, alignment(c->v.value()));
            }
        }
        return a;
    }

    int alignment_variant(const std::vector<case_ptr> &cases)
    {
        return std::max(alignment(discriminant_type(cases)), max_case_alignment(cases));
    }

    int alignment_flags(const std::vector<std::string> &labels)
    {
        int n = labels.size();
        if (n <= 8)
            return 1;
        if (n <= 16)
            return 2;
        return 4;
    }

    int alignment(ValType t)
    {
        switch (t)
        {
        case ValType::Bool:
        case ValType::S8:
        case ValType::U8:
            return 1;
        case ValType::S16:
        case ValType::U16:
            return 2;
        case ValType::S32:
        case ValType::U32:
        case ValType::F32:
        case ValType::Char:
            return 4;
        case ValType::S64:
        case ValType::U64:
        case ValType::F64:
            return 8;
        case ValType::String:
        case ValType::List:
            return 4;
            // case ValType::Own:
            // case ValType::Borrow:
            //     return 4;
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    int alignment(const Val &_t)
    {
        auto t = despecialize(_t);
        switch (valType(t))
        {
        case ValType::Record:
            return alignment_record(std::get<record_ptr>(t)->fields);
        case ValType::Variant:
            return alignment_variant(std::get<variant_ptr>(t)->cases);
        case ValType::Flags:
            return alignment_flags(std::get<flags_ptr>(t)->labels);
        default:
            return alignment(valType(t));
        }
    }

    uint8_t elem_size(ValType t)
    {
        switch (despecialize(t))
        {
        case ValType::Bool:
        case ValType::S8:
        case ValType::U8:
            return 1;
        case ValType::S16:
        case ValType::U16:
            return 2;
        case ValType::S32:
        case ValType::U32:
        case ValType::F32:
        case ValType::Char:
            return 4;
        case ValType::S64:
        case ValType::U64:
        case ValType::F64:
            return 8;
        case ValType::String:
        case ValType::List:
            return 8;
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    uint8_t elem_size_record(const std::vector<field_ptr> &fields)
    {
        int s = 0;
        for (auto f : fields)
        {
            s = align_to(s, alignment(f->v));
            s += elem_size(f->v);
        }
        assert(s > 0);
        return align_to(s, alignment_record(fields));
    }

    uint8_t elem_size_variant(const std::vector<case_ptr> &cases)
    {
        int s = elem_size(discriminant_type(cases));
        s = align_to(s, max_case_alignment(cases));
        uint8_t cs = 0;
        for (auto c : cases)
        {
            if (c->v.has_value())
            {
                cs = std::max(cs, elem_size(c->v.value()));
            }
        }
        s += cs;
        return align_to(s, alignment_variant(cases));
    }

    int num_i32_flags(const std::vector<std::string> &labels)
    {
        return std::ceil(static_cast<double>(labels.size()) / 32);
    }

    int elem_size_flags(const std::vector<std::string> &labels)
    {
        int n = labels.size();
        assert(n > 0);
        if (n <= 8)
            return 1;
        if (n <= 16)
            return 2;
        return 4 * num_i32_flags(labels);
    }

    uint8_t elem_size(const Val &_t)
    {
        auto t = despecialize(_t);
        ValType kind = valType(t);
        switch (kind)
        {
        case ValType::Record:
            return elem_size_record(std::get<record_ptr>(t)->fields);
        case ValType::Variant:
            return elem_size_variant(std::get<variant_ptr>(t)->cases);
        case ValType::Flags:
            return elem_size_flags(std::get<flags_ptr>(t)->labels);
        default:
            return elem_size(kind);
        }
        throw std::runtime_error("Invalid type");
    }

    float32_t canonicalize_nan32(float32_t f)
    {
        if (std::isnan(f))
        {
            f = CANONICAL_FLOAT32_NAN;
            assert(std::isnan(f));
        }
        return f;
    }

    float64_t canonicalize_nan64(float64_t f)
    {
        if (std::isnan(f))
        {
            f = CANONICAL_FLOAT64_NAN;
            assert(std::isnan(f));
        }
        return f;
    }

    float32_t core_f32_reinterpret_i32(int32_t i);
    float32_t decode_i32_as_float(int32_t i)
    {
        return canonicalize_nan32(core_f32_reinterpret_i32(i));
    }

    float64_t core_f64_reinterpret_i64(int64_t i);
    float64_t decode_i64_as_float(int64_t i)
    {
        return canonicalize_nan64(core_f64_reinterpret_i64(i));
    }

    float32_t core_f32_reinterpret_i32(int32_t i)
    {
        float f;
        std::memcpy(&f, &i, sizeof f);
        return f;
    }

    float64_t core_f64_reinterpret_i64(int64_t i)
    {
        double d;
        std::memcpy(&d, &i, sizeof d);
        return d;
    }

    int find_case(const std::string &label, const std::vector<case_ptr> &cases)
    {
        auto it = std::find_if(cases.begin(), cases.end(),
                               [&label](auto c)
                               { return c->label == label; });

        if (it != cases.end())
        {
            return std::distance(cases.begin(), it);
        }

        return -1;
    }

    int32_t char_to_i32(wchar_t c) { return static_cast<uint32_t>(c); }

    template <typename T>
    T random_nan_bits(int bits, int quiet_bits)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(1 << quiet_bits, (1 << bits) - 1);
        return distrib(gen);
    }

    float32_t maybe_scramble_nan32(float32_t f)
    {
        if (std::isnan(f))
        {
            if (DETERMINISTIC_PROFILE)
            {
                f = CANONICAL_FLOAT32_NAN;
            }
            else
            {
                f = core_f32_reinterpret_i32(random_nan_bits<int32_t>(32, 8));
            }
            assert(std::isnan(f));
        }
        return f;
    }

    float64_t maybe_scramble_nan64(float64_t f)
    {
        if (std::isnan(f))
        {
            if (DETERMINISTIC_PROFILE)
            {
                f = CANONICAL_FLOAT32_NAN;
            }
            else
            {
                f = core_f64_reinterpret_i64(random_nan_bits<int64_t>(64, 11));
            }
            assert(std::isnan(f));
        }
        return f;
    }

    uint32_t encode_float_as_i32(float32_t f)
    {
        return std::bit_cast<uint32_t>(maybe_scramble_nan32(f));
    }

    uint64_t encode_float_as_i64(float64_t f)
    {
        return std::bit_cast<uint64_t>(maybe_scramble_nan64(f));
    }

    std::vector<std::string> split(const std::string &input, char delimiter)
    {
        std::vector<std::string> result;
        std::string token;
        std::istringstream stream(input);

        while (std::getline(stream, token, delimiter))
        {
            result.push_back(token);
        }

        return result;
    }

    std::pair<int, std::optional<Val>> match_case(const variant_ptr &v, const std::vector<case_ptr> &cases)
    {
        assert(v->cases.size() == 1);
        auto key = v->cases[0]->label;
        auto value = v->cases[0]->v;
        for (auto label : split(key, '|'))
        {
            auto case_index = find_case(label, cases);
            if (case_index != -1)
            {
                return {case_index, value};
            }
        }
        throw std::runtime_error("Case not found");
    }

    std::string join(const std::string &a, const std::string &b)
    {
        if (a == b)
            return a;
        if ((a == "i32" && b == "f32") || (a == "f32" && b == "i32"))
            return "i32";
        return "i64";
    }

    // std::pair<char *, uint32_t> decodeXXX(void *src, uint32_t byte_len, Encoding encoding)
    // {
    //     switch (encoding)
    //     {
    //     case Encoding::Utf8:
    //     case Encoding::Latin1:
    //         return {reinterpret_cast<char *>(src), byte_len};
    //     case Encoding::Latin1_Utf16:
    //         assert(false);
    //         break;
    //     default:
    //         throw std::runtime_error("Invalid encoding");
    //     }
    // }

    // std::u32string encode(const char *src, uint32_t code_units, Encoding encoding)
    // {
    //     std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    //     switch (encoding)
    //     {
    //     case Encoding::Utf8:
    //     case Encoding::Latin1:
    //     case Encoding::Utf16le:
    //         return conv.from_bytes(reinterpret_cast<const char *>(src), reinterpret_cast<const char *>(src + byte_len));
    //     default:
    //         throw std::runtime_error("Invalid encoding");
    //     }
    // }

    // size_t encodeTo(void *dest, const char *src, uint32_t byte_len, Encoding encoding)
    // {
    //     switch (encoding)
    //     {
    //     case Encoding::Utf8:
    //     case Encoding::Latin1:
    //         std::memcpy(dest, src, byte_len);
    //         return byte_len;
    //     case Encoding::Utf16le:
    //         assert(false);
    //         break;
    //     default:
    //         throw std::runtime_error("Invalid encoding");
    //     }
    // }

    CoreValueIter::CoreValueIter(const std::vector<WasmVal> &values) : values(values), i(_i)
    {
    }

    CoreValueIter::CoreValueIter(const std::vector<WasmVal> &values, std::size_t &i) : values(values), i(i)
    {
    }

    int32_t CoreValueIter::next(int32_t _) const
    {
        return std::get<int32_t>(values[i++]);
    }

    int64_t CoreValueIter::next(int64_t _) const
    {
        return std::get<int64_t>(values[i++]);
    }

    float32_t CoreValueIter::next(float32_t _) const
    {
        return std::get<float32_t>(values[i++]);
    }

    float64_t CoreValueIter::next(float64_t _) const
    {
        return std::get<float64_t>(values[i++]);
    }

    void CoreValueIter::skip() const
    {
        ++i;
    }
}
