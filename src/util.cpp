#include "util.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <limits>
#include <codecvt>

namespace cmcpp
{

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
        switch (v.kind())
        {
        case ValType::Tuple:
        {
            RecordPtr r = std::make_shared<Record>();
            for (const auto &v2 : v.tuple()->vs)
            {
                Field f("", despecialize(v).kind());
                f.v = v2;
                r->fields.push_back(f);
            }
            return r;
        }
        case ValType::Enum:
        {
            std::vector<Case> cases;
            for (const auto &label : v.enum_()->labels)
            {
                cases.push_back(Case(label));
            }
            VariantPtr v = std::make_shared<Variant>(cases);
            return v;
        }
        case ValType::Option:
            return std::make_shared<Variant>(
                std::vector<Case>{Case("None"), Case("Some", v.option()->v)});
        case ValType::Result:
            return std::make_shared<Variant>(
                std::vector<Case>{Case("Ok", v.result()->ok), Case("Error", v.result()->error)});
        default:
            return v;
        }
    }

    ValType discriminant_type(const std::vector<Case> &cases)
    {
        size_t n = cases.size();

        assert(0 < n && n < std::numeric_limits<unsigned int>::max());
        int match = std::ceil(std::log2(n) / 8);
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

    int alignment_record(const std::vector<Field> &fields);
    int alignment_variant(const std::vector<Case> &cases);

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
        case ValType::Float32:
        case ValType::Char:
            return 4;
        case ValType::S64:
        case ValType::U64:
        case ValType::Float64:
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

    int alignment(Val _v)
    {
        Val v = despecialize(_v);
        switch (v.kind())
        {
        case ValType::Record:
            return alignment_record(_v.record()->fields);
        case ValType::Variant:
            return alignment_variant(_v.variant()->cases);
        // case ValType::Flags:
        //     return alignment_flags(_v.flags()->labels);
        default:
            return alignment(v);
        }
    }

    int alignment_record(const std::vector<Field> &fields)
    {
        int a = 1;
        for (const auto &f : fields)
        {
            a = std::max(a, alignment(f.v.value()));
        }
        return a;
    }

    int max_case_alignment(const std::vector<Case> &cases)
    {
        int a = 1;
        for (const auto &c : cases)
        {
            if (c.v.has_value()) // Check if c.t exists
            {
                a = std::max(a, alignment(c.v.value()));
            }
        }
        return a;
    }

    int alignment_variant(const std::vector<Case> &cases)
    {
        return std::max(alignment(discriminant_type(cases)), max_case_alignment(cases));
    }

    uint32_t align_to(uint32_t ptr, uint32_t alignment);

    int size_record(const std::vector<Field> &fields);
    int size_variant(const std::vector<Case> &cases);
    int size_flags(const std::vector<std::string> &labels);

    int size(ValType t)
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
        case ValType::Float32:
        case ValType::Char:
            return 4;
        case ValType::S64:
        case ValType::U64:
        case ValType::Float64:
            return 8;
        case ValType::String:
        case ValType::List:
            return 8;
            // case ValType::Own:
            // case ValType::Borrow:
            //     return 4;
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    int size(const Val &v)
    {
        ValType kind = despecialize(v).kind();
        switch (kind)
        {
        case ValType::Record:
            return size_record(v.record()->fields);
        case ValType::Variant:
            return size_variant(v.variant()->cases);
        // case ValType::Flags:
        //     return size_flags(v.flags()->labels);
        default:
            return size(kind);
        }
        throw std::runtime_error("Invalid type");
    }

    int size_record(const std::vector<Field> &fields)
    {
        int s = 0;
        for (const auto &f : fields)
        {
            s = align_to(s, alignment(f.v.value()));
            s += size(f.v.value());
        }
        assert(s > 0);
        return align_to(s, alignment_record(fields));
    }

    uint32_t align_to(uint32_t ptr, uint32_t alignment)
    {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }

    int size_variant(const std::vector<Case> &cases)
    {
        int s = size(discriminant_type(cases));
        s = align_to(s, max_case_alignment(cases));
        int cs = 0;
        for (const auto &c : cases)
        {
            if (c.v.has_value())
            {
                cs = std::max(cs, size(c.v.value()));
            }
        }
        s += cs;
        return align_to(s, alignment_variant(cases));
    }

    int num_i32_flags(const std::vector<std::string> &labels);

    int size_flags(const std::vector<std::string> &labels)
    {
        int n = labels.size();
        assert(n > 0);
        if (n <= 8)
            return 1;
        if (n <= 16)
            return 2;
        return 4 * num_i32_flags(labels);
    }

    int num_i32_flags(const std::vector<std::string> &labels)
    {
        return std::ceil(static_cast<double>(labels.size()) / 32);
    }

    bool isAligned(uint32_t ptr, uint32_t alignment) { return (ptr & (alignment - 1)) == 0; }

    bool convert_int_to_bool(uint8_t i) { return i > 0; }

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

    float core_f32_reinterpret_i32(int32_t i);
    float decode_i32_as_float(int32_t i) { return canonicalize_nan32(core_f32_reinterpret_i32(i)); }

    double core_f64_reinterpret_i64(int64_t i);
    double decode_i64_as_float(int64_t i) { return canonicalize_nan64(core_f64_reinterpret_i64(i)); }

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

    char convert_i32_to_char(int32_t i)
    {
        assert(i < 0x110000);
        assert(!(0xD800 <= i && i <= 0xDFFF));
        return static_cast<char>(i);
    }

    using HostStringTuple = std::tuple<const char * /*s*/, std::string /*encoding*/, size_t /*byte length*/>;

    std::string case_label_with_refinements(const Case &c, const std::vector<Case> &)
    {
        std::string label = c.label;
        Case currentCase = c;

        while (currentCase.refines.has_value())
        {
            // TODO:  currentCase = cases[find_case(currentCase.refines.value(), cases)];
            label += '|' + currentCase.label;
        }

        return label;
    }

    int find_case(const std::string &label, const std::vector<Case> &cases)
    {
        auto it = std::find_if(cases.begin(), cases.end(),
                               [&label](const Case &c)
                               { return c.label == label; });

        if (it != cases.end())
        {
            return std::distance(cases.begin(), it);
        }

        return -1;
    }

    std::map<std::string, bool> unpack_flags_from_int(int i, const std::vector<std::string> &labels)
    {
        std::map<std::string, bool> record;
        for (const auto &l : labels)
        {
            record[l] = bool(i & 1);
            i >>= 1;
        }
        return record;
    }

    uint32_t char_to_i32(char c) { return static_cast<uint32_t>(c); }

    template <typename T>
    T random_nan_bits(int bits, int quiet_bits)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(1 << quiet_bits, (1 << bits) - 1);
        return distrib(gen);
    }

    float32_t core_f32_reinterpret_i32(int32_t i);
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

    float64_t core_f64_reinterpret_i64(int64_t i);
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

    std::pair<int, Val> match_case(VariantPtr v)
    {
        assert(v->cases.size() == 1);
        auto key = v->cases[0].label;
        auto value = v->cases[0].v.value();
        for (auto label : split(key, '|'))
        {
            auto case_index = find_case(label, v->cases);
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

    std::pair<char8_t *, uint32_t> decode(void *src, uint32_t byte_len, GuestEncoding encoding)
    {
        switch (encoding)
        {
        case GuestEncoding::Utf8:
        case GuestEncoding::Latin1:
            return {reinterpret_cast<char8_t *>(src), byte_len};
        case GuestEncoding::Utf16le:
            assert(false);
            break;
        default:
            throw std::runtime_error("Invalid encoding");
        }
    }

    std::pair<void *, size_t> encode(const char8_t *src, uint32_t byte_len, GuestEncoding encoding)
    {
        switch (encoding)
        {
        case GuestEncoding::Utf8:
        case GuestEncoding::Latin1:
            return {const_cast<char8_t *>(src), byte_len};
        case GuestEncoding::Utf16le:
            assert(false);
            break;
        default:
            throw std::runtime_error("Invalid encoding");
        }
    }

    std::vector<char8_t> utf16ToSingleByte(const std::wstring &utf16String)
    {
        auto result = std::vector<char8_t>();

        for (size_t i = 0; i < utf16String.size(); ++i)
        {
            wchar_t utf16Char = utf16String[i];

            char8_t lowByte = static_cast<char8_t>(utf16Char & 0xFF);
            char8_t highByte = static_cast<char8_t>((utf16Char >> 8) & 0xFF);

            result.push_back(lowByte);

            // Note: Ignoring the high byte for now (may need additional logic)

            // For debugging, you can print the original UTF-16 character
            // std::wcout << "Original UTF-16 char: " << utf16Char << std::endl;
        }

        return std::vector<char8_t>(result);
    }

    std::vector<uint8_t> singleByteToUtf16(const std::string &singleByteString)
    {
        std::vector<uint8_t> result;

        for (char singleByteChar : singleByteString)
        {
            uint8_t lowByte = static_cast<uint8_t>(singleByteChar);
            uint8_t highByte = 0;
            result.push_back(lowByte);
            result.push_back(highByte);

            // Note: Ignoring the high byte for now (may need additional logic)

            // For debugging, you can print the original single-byte character
            // std::cout << "Original single-byte char: " << singleByteChar << std::endl;
        }

        return result;
    }

}
