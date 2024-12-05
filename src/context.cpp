#include "context.hpp"

#include <cstring>
#include <optional> // Include the necessary header file
#include <cassert>

namespace cmcpp
{
    class CanonicalOptionsImpl : public CanonicalOptions
    {
    private:
        const GuestRealloc &_realloc;
        const GuestPostReturn &_post_return;
        HostEncodeTo _encodeTo;
        HostDecodeFrom _decodeFrom;

    public:
        CanonicalOptionsImpl(const GuestMemory &memory, HostEncoding string_encoding, const GuestRealloc &realloc,
                             HostEncodeTo encodeTo,
                             HostDecodeFrom decodeFrom, const GuestPostReturn &post_return)
            : _realloc(realloc), _encodeTo(encodeTo), _decodeFrom(decodeFrom), _post_return(post_return)
        {
            this->memory = memory;
            this->string_encoding = string_encoding;
        }

        virtual int realloc(int ptr, int old_size, int align, int new_size)
        {
            return _realloc(ptr, old_size, align, new_size);
        }

        virtual std::pair<char8_t *, size_t> encodeTo(void *dest, const char *src, uint32_t byte_len, GuestEncoding encoding)
        {
            return _encodeTo(dest, src, byte_len, encoding);
        }

        virtual std::pair<char *, size_t> decodeFrom(const void *src, uint32_t byte_len, HostEncoding encoding)
        {
            // return {(char *)src, byte_len};
            return _decodeFrom(src, byte_len, encoding);
        }

        void post_return()
        {
            //  Optional
            if (_post_return)
            {
                _post_return();
            }
        }
    };

    CanonicalOptionsPtr createCanonicalOptions(const GuestMemory &memory, const GuestRealloc &realloc,
                                               HostEncodeTo encodeTo,
                                               HostDecodeFrom decodeFrom,
                                               HostEncoding encoding, const GuestPostReturn &post_return)
    {
        return std::make_shared<CanonicalOptionsImpl>(memory, encoding, realloc, encodeTo, decodeFrom, post_return);
    }

    class CallContextImpl : public LiftLowerContext
    {
    public:
        CallContextImpl(CanonicalOptionsPtr options) { opts = options; }
    };

    CallContextPtr createCallContext(const GuestMemory &memory, const GuestRealloc &realloc,
                                     HostEncodeTo encodeTo,
                                     HostDecodeFrom decodeFrom,
                                     HostEncoding encoding, const GuestPostReturn &post_return)
    {
        return std::make_shared<CallContextImpl>(
            createCanonicalOptions(memory, realloc, encodeTo, decodeFrom, encoding, post_return));
    }

}