
#include "cmcpp.hpp"

#include <utility>
#include <iostream>

using namespace cmcpp;

void trap(const char *msg = "");

const char *encodingToICU(const Encoding encoding);

std::pair<void *, size_t> convert(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding);

class Heap
{
private:
    uint32_t last_alloc = 0;

public:
    std::vector<uint8_t> memory;

    Heap(size_t arg) : memory(arg), last_alloc(0)
    {
    }

    uint32_t realloc(uint32_t original_ptr, size_t original_size, uint32_t alignment, size_t new_size)
    {
        if (original_ptr != 0 && new_size < original_size)
        {
            return align_to(original_ptr, alignment);
        }

        uint32_t ret = align_to(last_alloc, alignment);
        last_alloc = static_cast<uint32_t>(ret + new_size);
        if (last_alloc > memory.size())
        {
            trap("oom");
        }
        std::memcpy(&memory[ret], &memory[original_ptr], original_size);
        return ret;
    }
};

inline std::unique_ptr<LiftLowerContext> createLiftLowerContext(Heap *heap, CanonicalOptions options);

inline std::unique_ptr<LiftLowerContext> createLiftLowerContext(Heap *heap, Encoding encoding)
{
    CanonicalOptions options;
    options.string_encoding = encoding;
    return createLiftLowerContext(heap, std::move(options));
}

inline std::unique_ptr<LiftLowerContext> createLiftLowerContext(Heap *heap, CanonicalOptions options)
{
    std::unique_ptr<cmcpp::InstanceContext> instanceContext = std::make_unique<cmcpp::InstanceContext>(trap, convert,
                                                                                                       [heap](int original_ptr, int original_size, int alignment, int new_size) -> int
                                                                                                       {
                                                                                                           return heap->realloc(original_ptr, original_size, alignment, new_size);
                                                                                                       });
    options.memory = heap->memory;
    return instanceContext->createLiftLowerContext(std::move(options));
}
