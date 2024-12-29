#include "component-model.hpp"
#include <stdexcept>

namespace cmcpp
{
    BaseMemoryRange::BaseMemoryRange(Memory &memory, ptr ptr, size size) : _memory(memory), _ptr(ptr), _size(size), _alignment(Alignment::getAlignment(ptr))
    {
    }

    std::span<uint8_t> BaseMemoryRange::view() const
    {
        return std::span<uint8_t>(_memory.buffer.data() + _ptr, _size);
    }

    u8 BaseMemoryRange::getUint8(offset offset) const
    {
        return view()[offset];
    }

    s8 BaseMemoryRange::getInt8(offset offset) const
    {
        return getUint8(offset);
    }

    u16 BaseMemoryRange::getUint16(offset offset) const
    {
        return (view()[offset] << 8) |
               view()[offset + 1];
    }

    s16 BaseMemoryRange::getInt16(offset offset) const
    {
        return getUint16(offset);
    }

    u32 BaseMemoryRange::getUint32(offset offset) const
    {
        return (view()[offset] << 24) |
               (view()[offset + 1] << 16) |
               (view()[offset + 2] << 8) |
               view()[offset + 3];
    }

    s32 BaseMemoryRange::getInt32(offset offset) const
    {
        return getUint32(offset);
    }

    u64 BaseMemoryRange::getUint64(offset offset) const
    {
        return (static_cast<uint64_t>(view()[offset]) << 56) |
               (static_cast<uint64_t>(view()[offset + 1]) << 48) |
               (static_cast<uint64_t>(view()[offset + 2]) << 40) |
               (static_cast<uint64_t>(view()[offset + 3]) << 32) |
               (static_cast<uint64_t>(view()[offset + 4]) << 24) |
               (static_cast<uint64_t>(view()[offset + 5]) << 16) |
               (static_cast<uint64_t>(view()[offset + 6]) << 8) |
               static_cast<uint64_t>(view()[offset + 7]);
    }

    s64 BaseMemoryRange::getInt64(offset offset) const
    {
        return getUint64(offset);
    }

    float32 BaseMemoryRange::getFloat32(offset offset) const
    {
        uint32_t i = getUint32(offset);
        return *reinterpret_cast<float *>(&i);
    }

    float64 BaseMemoryRange::getFloat64(offset offset) const
    {
        uint64_t i = getUint64(offset);
        return *reinterpret_cast<double *>(&i);
    }

    void BaseMemoryRange::copyBytes(offset offset, size length, MemoryRange &into, size into_offset)
    {
        if (offset + length > _size)
        {
            throw std::runtime_error("Memory access is out of bounds");
        }
        auto target = into.getUint8View(into_offset, length);
    }

    MemoryRange::MemoryRange(Memory &memory, ptr ptr, size size, bool isPreallocated) : BaseMemoryRange(memory, ptr, size), isAllocated(!isPreallocated)
    {
    }

    void MemoryRange::free()
    {
        _memory.free(*this);
    }

    void MemoryRange::setUint8(offset offset, u8 value)
    {
        view()[offset] = value;
    }

    void MemoryRange::setInt8(offset offset, s8 value)
    {
        view()[offset] = value;
    }

    void MemoryRange::setUint16(offset offset, u16 value)
    {
        view()[offset] = value >> 8;
        view()[offset + 1] = value & 0xff;
    }

    void MemoryRange::setInt16(offset offset, s16 value)
    {
        setUint16(offset, value);
    }

    void MemoryRange::setUint32(offset offset, u32 value)
    {
        view()[offset] = value >> 24;
        view()[offset + 1] = (value >> 16) & 0xff;
        view()[offset + 2] = (value >> 8) & 0xff;
        view()[offset + 3] = value & 0xff;
    }

    void MemoryRange::setInt32(offset offset, s32 value)
    {
        setUint32(offset, value);
    }

    void MemoryRange::setUint64(offset offset, u64 value)
    {
        view()[offset] = value >> 56;
        view()[offset + 1] = (value >> 48) & 0xff;
        view()[offset + 2] = (value >> 40) & 0xff;
        view()[offset + 3] = (value >> 32) & 0xff;
        view()[offset + 4] = (value >> 24) & 0xff;
        view()[offset + 5] = (value >> 16) & 0xff;
        view()[offset + 6] = (value >> 8) & 0xff;
        view()[offset + 7] = value & 0xff;
    }

    void MemoryRange::setInt64(offset offset, s64 value)
    {
        setUint64(offset, value);
    }

    void MemoryRange::setFloat32(offset offset, float32 value)
    {
        setUint32(offset, *reinterpret_cast<uint32_t *>(&value));
    }

    void MemoryRange::setFloat64(offset offset, float64 value)
    {
        setUint64(offset, *reinterpret_cast<uint64_t *>(&value));
    }

    std::span<uint8_t> MemoryRange::getUint8View(offset offset, size length)
    {
        return getArrayView<u8>(offset, length);
    }

    namespace _bool
    {
        bool load(const ReadonlyMemoryRange &memory, offset offset)
        {
            return memory.getUint8(offset) != 0;
        }

        bool liftFlat(const Memory &memory, FlatValuesIter &values)
        {
            auto value = std::get<i32>(*values++);
            if (value < 0)
            {
                throw std::runtime_error("Invalid value for bool");
            }
            return value != 0;
        }

        MemoryRange alloc(Memory &memory)
        {
            return memory.alloc(alignment, size);
        }

        void store(MemoryRange &memory, offset offset, bool value)
        {
            memory.setUint8(offset, value ? 1 : 0);
        }

        void lowerFlat(WasmTypeVector &result, const Memory &memory, bool value)
        {
            result.push_back(value ? (i32)1 : (i32)0);
        }

        void copy(MemoryRange &dest, offset dest_offset, ReadonlyMemoryRange &src, offset src_offset)
        {
            src.copyBytes(src_offset, size, dest, dest_offset);
        }

        void copyFlat(WasmTypeVector &result, const Memory &dest, FlatValuesIter &values, Memory &src)
        {
            result.push_back(*values++);
        }
    }
}