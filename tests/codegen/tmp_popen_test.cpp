#include <array>
#include <cstdio>
#include <iostream>
#include <string>

int main()
{
    std::string cmd = "\"G:/component-model-cpp/build/tools/wit-codegen/Debug/wit-codegen.exe\" \"G:/component-model-cpp/ref/wit-bindgen/tests/codegen/allow-unused.wit\" \"G:/component-model-cpp/build/test/generated_stubs/allow-unused/allow-unused\"";
    FILE *pipe = _popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe)
    {
        std::cerr << "popen failed\n";
        return 1;
    }
    std::array<char, 256> buffer{};
    std::string output;
    while (fgets(buffer.data(), buffer.size(), pipe))
    {
        output += buffer.data();
    }
    int status = _pclose(pipe);
    std::cout << "status=" << status << "\n";
    std::cout << output << std::endl;
    return 0;
}
