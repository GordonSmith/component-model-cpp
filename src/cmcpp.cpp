#include "cmcpp.hpp"

#include <cstring>
#include <optional> // Include the necessary header file

class CanonicalOptionsImpl : public CanonicalOptions
{
private:
    const GuestRealloc &_realloc;
    const GuestPostReturn &_post_return;

public:
    CanonicalOptionsImpl(const GuestMemory &memory, HostEncoding string_encoding, const GuestRealloc &realloc, const GuestPostReturn &post_return) : _realloc(realloc), _post_return(post_return)
    {
        this->memory = memory;
        this->string_encoding = string_encoding;
    }

    int realloc(int ptr, int old_size, int align, int new_size)
    {
        return _realloc(ptr, old_size, align, new_size);
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

CanonicalOptionsPtr createCanonicalOptions(const GuestMemory &memory, const GuestRealloc &realloc, HostEncoding encoding, const GuestPostReturn &post_return)
{
    return std::make_shared<CanonicalOptionsImpl>(memory, encoding, realloc, post_return);
}

class CallContextImpl : public CallContext
{
public:
    CallContextImpl(CanonicalOptionsPtr options)
    {
        opts = options;
    }
};

CallContextPtr createCallContext(const GuestMemory &memory, const GuestRealloc &realloc, HostEncoding encoding, const GuestPostReturn &post_return)
{
    return std::make_shared<CallContextImpl>(createCanonicalOptions(memory, realloc, encoding, post_return));
}
