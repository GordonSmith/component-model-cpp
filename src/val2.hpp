#include <string>
#include <vector>
#include <iostream>
#include <typeinfo>

template <typename T>
struct TypeNameTrait
{
    static std::vector<std::string> name()
    {
        throw std::runtime_error("Unsupported Type");
    }
};

template <>
struct TypeNameTrait<bool>
{
    static std::vector<std::string> name()
    {
        return {"Bool"};
    }
};

template <>
struct TypeNameTrait<int8_t>
{
    static std::vector<std::string> name()
    {
        return {"S8"};
    }
};

template <>
struct TypeNameTrait<uint8_t>
{
    static std::vector<std::string> name()
    {
        return {"U8"};
    }
};

template <>
struct TypeNameTrait<int16_t>
{
    static std::vector<std::string> name()
    {
        return {"S16"};
    }
};

template <>
struct TypeNameTrait<uint16_t>
{
    static std::vector<std::string> name()
    {
        return {"U16"};
    }
};

template <>
struct TypeNameTrait<int32_t>
{
    static std::vector<std::string> name()
    {
        return {"S32"};
    }
};

template <>
struct TypeNameTrait<uint32_t>
{
    static std::vector<std::string> name()
    {
        return {"U32"};
    }
};

template <>
struct TypeNameTrait<int64_t>
{
    static std::vector<std::string> name()
    {
        return {"S64"};
    }
};

template <>
struct TypeNameTrait<uint64_t>
{
    static std::vector<std::string> name()
    {
        return {"U64"};
    }
};

template <>
struct TypeNameTrait<float>
{
    static std::vector<std::string> name()
    {
        return {"Float32"};
    }
};

template <>
struct TypeNameTrait<double>
{
    static std::vector<std::string> name()
    {
        return {"Float64"};
    }
};

template <>
struct TypeNameTrait<char>
{
    static std::vector<std::string> name()
    {
        return {"Char"};
    }
};

template <>
struct TypeNameTrait<std::string>
{
    static std::vector<std::string> name()
    {
        return {"String"};
    }
};

template <typename LT>
struct TypeNameTrait<std::vector<LT>>
{
    static std::vector<std::string> name()
    {
        std::vector<std::string> name = {"List"};
        std::vector<std::string> tname = TypeNameTrait<LT>::name();
        name.insert(name.end(), tname.begin(), tname.end());
        return name;
    }
};

template <typename T>
struct Val;

template <typename T>
struct Val
{
    T value;

    Val(const T &val) : value(val) {}

    std::string kind() const
    {
        return TypeNameTrait<T>::name()[0];
    }
    std::vector<std::string> types() const
    {
        return TypeNameTrait<T>::name();
    }
};

void test()
{
    Val Bool(false);
    std::cout << "Bool:  " << Bool.kind() << std::endl;
    Val<int8_t> S8(5);
    std::cout << "S8:  " << S8.kind() << std::endl;
    Val<uint8_t> U8(5);
    std::cout << "U8:  " << U8.kind() << std::endl;
    Val<int16_t> S16(5);
    std::cout << "S16:  " << S16.kind() << std::endl;
    Val<uint16_t> U16(5);
    std::cout << "U16:  " << U16.kind() << std::endl;
    Val<int32_t> S32(5);
    std::cout << "S32:  " << S32.kind() << std::endl;
    Val<uint32_t> U32(5);
    std::cout << "U32:  " << U32.kind() << std::endl;
    Val<int64_t> S64(5);
    std::cout << "S64:  " << S64.kind() << std::endl;
    Val<uint64_t> U64(5);
    std::cout << "U64:  " << U64.kind() << std::endl;
    Val<float> Float32(5.0);
    std::cout << "Float32:  " << Float32.kind() << std::endl;
    Val<double> Float64(5.0);
    std::cout << "Float64:  " << Float64.kind() << std::endl;
    Val<char> Char('a');
    std::cout << "Char:  " << Char.kind() << std::endl;
    Val<std::string> String("hello");
    std::cout << "String:  " << String.kind() << std::endl;
    Val<std::vector<int>> List({1, 2, 3});
    std::cout << "List:  " << List.kind() << List.types()[1] << std::endl;
}
