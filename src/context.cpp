#include "context.hpp"

#include <cstring>
#include <optional> // Include the necessary header file

namespace cmcpp
{

    class CanonicalOptionsImpl : public CanonicalOptions
    {
    private:
        const GuestRealloc &_realloc;
        const GuestPostReturn &_post_return;
        const HostEncodeTo &_encodeTo;

    public:
        CanonicalOptionsImpl(const GuestMemory &memory, HostEncoding string_encoding,
                             const GuestRealloc &realloc, const HostEncodeTo &encodeTo, const GuestPostReturn &post_return)
            : _realloc(realloc), _encodeTo(encodeTo), _post_return(post_return)
        {
            this->memory = memory;
            this->string_encoding = string_encoding;
        }

        virtual int realloc(int ptr, int old_size, int align, int new_size)
        {
            return _realloc(ptr, old_size, align, new_size);
        }

        virtual size_t encodeTo(void *dest, const char *src, uint32_t byte_len, GuestEncoding encoding)
        {
            return _encodeTo(dest, src, byte_len, encoding);
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
                                               const HostEncodeTo &encodeTo,
                                               HostEncoding encoding,
                                               const GuestPostReturn &post_return)
    {
        return std::make_shared<CanonicalOptionsImpl>(memory, encoding, realloc, encodeTo, post_return);
    }

    class CallContextImpl : public CallContext
    {
    public:
        CallContextImpl(CanonicalOptionsPtr options) { opts = options; }
    };

    CallContextPtr createCallContext(const GuestMemory &memory, const GuestRealloc &realloc, const HostEncodeTo &encodeTo,
                                     HostEncoding encoding, const GuestPostReturn &post_return)
    {
        return std::make_shared<CallContextImpl>(
            createCanonicalOptions(memory, realloc, encodeTo, encoding, post_return));
    }

}