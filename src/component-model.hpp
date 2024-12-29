#ifndef COMPONENT_MODEL_HPP
#define COMPONENT_MODEL_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <variant>

namespace cmcpp
{
    enum class WasmTypeKind
    {
        i32,
        i64,
        f32,
        f64
    };

    enum class ComponentModelTypeKind
    {
        _bool,
        u8,
        u16,
        u32,
        u64,
        s8,
        s16,
        s32,
        s64,
        float32,
        float64,
        _char,
        string,
        list,
        record,
        tuple,
        variant,
        _enum,
        flags,
        option,
        result,
        resource,
        resourceHandle,
        borrow,
        own
    };

    enum class AlignmentT
    {
        byte = 1,
        halfWord = 2,
        word = 4,
        doubleWord = 8
    };

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using s8 = int8_t;
    using s16 = int16_t;
    using s32 = int32_t;
    using s64 = int64_t;
    using float32 = float;
    using float64 = double;
    // using char = char;

    using ptr = uint32_t;
    using size = uint32_t;
    using offset = uint32_t;

    using DataView = std::span<uint8_t>;

    class ReadonlyMemoryRange;
    class MemoryRange;
    class Memory
    {
    protected:
    public:
        std::vector<uint8_t> buffer;
        virtual MemoryRange alloc(AlignmentT align, size size) = 0;
        virtual MemoryRange realloc(MemoryRange &range, AlignmentT align, size newSize) = 0;
        virtual MemoryRange preAllocated(ptr ptr, size size) = 0;
        virtual ReadonlyMemoryRange readonly(ptr ptr, size size) = 0;
        virtual void free(MemoryRange &range) = 0;
    };

    class BaseMemoryRange
    {
    protected:
        Memory &_memory;

    public:
        const ptr _ptr;
        const size _size;
        const AlignmentT _alignment;

        BaseMemoryRange(Memory &memory, ptr ptr, size size);
        u8 getUint8(offset offset) const;
        s8 getInt8(offset offset) const;
        u16 getUint16(offset offset) const;
        s16 getInt16(offset offset) const;
        u32 getUint32(offset offset) const;
        s32 getInt32(offset offset) const;
        u64 getUint64(offset offset) const;
        s64 getInt64(offset offset) const;
        float32 getFloat32(offset offset) const;
        float64 getFloat64(offset offset) const;
        ptr getPtr(offset offset) const;

        void copyBytes(offset offset, size length, MemoryRange &into, size into_offset);

    protected:
        std::span<uint8_t> view() const;
    };

    class ReadonlyMemoryRange : public BaseMemoryRange
    {
    public:
        ReadonlyMemoryRange(Memory &memory, ptr ptr, size size);

        ReadonlyMemoryRange &range(offset offset, size size);
    };

    using Uint8Array = std::vector<u8>;
    class MemoryRange : public BaseMemoryRange
    {
    public:
        const bool isAllocated;

        MemoryRange(Memory &memory, ptr ptr, size size, bool isPreallocated = false);
        void free();
        MemoryRange &range(offset offset, size size);
        void setUint8(offset offset, u8 value);
        void setInt8(offset offset, s8 value);
        void setUint16(offset offset, u16 value);
        void setInt16(offset offset, s16 value);
        void setUint32(offset offset, u32 value);
        void setInt32(offset offset, s32 value);
        void setUint64(offset offset, u64 value);
        void setInt64(offset offset, s64 value);
        void setFloat32(offset offset, float32 value);
        void setFloat64(offset offset, float64 value);
        void setPtr(offset offset, ptr value);
        std::span<uint8_t> getUint8View(offset offset, size length);

    private:
        template <typename T>
        std::span<T> getArrayView(offset byteOffset, std::optional<size> length)
        {
            auto start = (T *)view().data() + byteOffset / sizeof(T);
            auto count = length.value_or((_size - byteOffset) / sizeof(T));
            return std::span<T>(start, count);
        }
    };

    using i32 = int32_t;
    using i64 = int64_t;
    using f32 = float;
    using f64 = double;
    using WasmType = std::variant<i32, i64, f32, f64>;
    using WasmTypeVector = std::vector<WasmType>;
    using FlatValue = std::variant<i32, i64, f32, f64>;
    using FlatValues = std::vector<FlatValue>;
    using FlatValuesIter = FlatValues::const_iterator;

    namespace _bool
    {
        const ComponentModelTypeKind kind = ComponentModelTypeKind::_bool;
        const cmcpp::size size = 1;
        const AlignmentT alignment = AlignmentT::byte;
        const std::array<WasmTypeKind, 1> flatTypes = {WasmTypeKind::i32};

        bool load(const ReadonlyMemoryRange &memory, offset offset);
        bool liftFlat(const Memory &memory, const FlatValuesIter &values);
        void lowerFlat(WasmTypeVector &result, const Memory &_memory, bool value);
    }

    namespace Alignment
    {
        // size_t align(ptr : ptr, Alignment alignment)
        // {
        //     return Math.ceil(ptr / alignment) * alignment;
        // }
        AlignmentT getAlignment(ptr ptr)
        {
            if (ptr % (int)AlignmentT::doubleWord == 0)
            {
                return AlignmentT::doubleWord;
            }
            if (ptr % (int)AlignmentT::word == 0)
            {
                return AlignmentT::word;
            }
            if (ptr % (int)AlignmentT::halfWord == 0)
            {
                return AlignmentT::halfWord;
            }
            return AlignmentT::byte;
        }
    }

    enum class FlatTypeKind
    {
        i32,
        i64,
        f32,
        f64
    };

    constexpr const char *const toString(FlatTypeKind type)
    {
        switch (type)
        {
        case FlatTypeKind::i32:
            return "i32";
        case FlatTypeKind::i64:
            return "i64";
        case FlatTypeKind::f32:
            return "f32";
        case FlatTypeKind::f64:
            return "f64";
        default:
            return "Unknown";
        }
    }

    template <typename F>
    class FlatType
    {
    public:
        static_assert(std::is_same_v<F, i32> ||
                          std::is_same_v<F, i64> ||
                          std::is_same_v<F, f32> ||
                          std::is_same_v<F, f64>,
                      "F must be one of i32, i64, f32, or f64");

        const FlatTypeKind kind;
        const cmcpp::size size;
        const AlignmentT alignment;
        F load(const ReadonlyMemoryRange &memory, offset offset) = 0;
        void store(MemoryRange &memory, offset offset, F value) = 0;
        void copy(MemoryRange &dest, offset dest_offset, const ReadonlyMemoryRange &src, offset src_offset) = 0;
    };
}

#endif
