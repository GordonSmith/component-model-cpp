#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>

// Local modular headers
#include "wit_parser.hpp"
#include "code_generator.hpp"

void print_help(const char *program_name)
{
    std::cout << "wit-codegen - WebAssembly Interface Types (WIT) Code Generator\n\n";
    std::cout << "USAGE:\n";
    std::cout << "  " << program_name << " <wit-file> [output-prefix]\n";
    std::cout << "  " << program_name << " --help\n";
    std::cout << "  " << program_name << " -h\n\n";
    std::cout << "ARGUMENTS:\n";
    std::cout << "  <wit-file>       Path to the WIT file to parse\n";
    std::cout << "  [output-prefix]  Optional output file prefix (default: derived from package name)\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "  -h, --help       Show this help message and exit\n\n";
    std::cout << "DESCRIPTION:\n";
    std::cout << "  Generates C++ host function bindings from WebAssembly Interface Types (WIT)\n";
    std::cout << "  files. The tool parses WIT syntax and generates type-safe C++ code for\n";
    std::cout << "  interfacing with WebAssembly components.\n\n";
    std::cout << "GENERATED FILES:\n";
    std::cout << "  <prefix>.hpp          - C++ header with type definitions and declarations\n";
    std::cout << "  <prefix>_wamr.hpp     - WAMR runtime integration header\n";
    std::cout << "  <prefix>_wamr.cpp     - WAMR binding implementation with NativeSymbol arrays\n\n";
    std::cout << "FEATURES:\n";
    std::cout << "  - Supports all Component Model types (primitives, strings, lists, records,\n";
    std::cout << "    variants, enums, options, results, flags)\n";
    std::cout << "  - Generates bidirectional bindings (imports and exports)\n";
    std::cout << "  - Type-safe C++ wrappers using cmcpp canonical ABI\n";
    std::cout << "  - WAMR native function registration helpers\n";
    std::cout << "  - Automatic memory management for complex types\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "  # Generate bindings using package-derived prefix\n";
    std::cout << "  " << program_name << " example.wit\n\n";
    std::cout << "  # Generate bindings with custom prefix\n";
    std::cout << "  " << program_name << " example.wit my_bindings\n\n";
    std::cout << "  # Show help message\n";
    std::cout << "  " << program_name << " --help\n\n";
    std::cout << "For more information, see: https://github.com/GordonSmith/component-model-cpp\n";
}

int main(int argc, char *argv[])
{
    // Check for help flag
    if (argc >= 2)
    {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h")
        {
            print_help(argv[0]);
            return 0;
        }
    }

    // Check for required arguments
    if (argc < 2)
    {
        std::cerr << "Error: Missing required argument <wit-file>\n\n";
        print_help(argv[0]);
        return 1;
    }

    std::string witFile = argv[1];
    std::string outputPrefix = argc >= 3 ? argv[2] : "";

    try
    {
        // Parse WIT file using ANTLR grammar
        auto parseResult = WitGrammarParser::parse(witFile);

        if (parseResult.interfaces.empty())
        {
            // Check if this is a world-only file (has world imports/exports but no interfaces)
            if (parseResult.hasWorld)
            {
                // World-only files that reference external interfaces are not currently supported
                return 0; // Success - nothing to generate
            }
            else
            {
                // Empty world with no interfaces, imports, or exports
                return 0; // Success - nothing to generate
            }
        }

        // If no output prefix provided, derive it from the package name
        if (outputPrefix.empty())
        {
            if (!parseResult.packageName.empty())
            {
                // Package format: "package example:sample" or just "example:sample"
                // Extract just the package part and sanitize it
                std::string pkgName = parseResult.packageName;

                // Remove "package" keyword if present
                size_t pkgPos = pkgName.find("package");
                if (pkgPos != std::string::npos)
                {
                    pkgName = pkgName.substr(pkgPos + 7); // Skip "package"
                }

                // Remove semicolon and trim
                pkgName.erase(std::remove(pkgName.begin(), pkgName.end(), ';'), pkgName.end());
                pkgName.erase(std::remove_if(pkgName.begin(), pkgName.end(), ::isspace), pkgName.end());

                // Extract the name part after the colon (e.g., "sample" from "example:sample")
                size_t colonPos = pkgName.find(':');
                if (colonPos != std::string::npos && colonPos + 1 < pkgName.size())
                {
                    outputPrefix = pkgName.substr(colonPos + 1);
                }
                else
                {
                    outputPrefix = pkgName;
                }

                // Remove version if present (e.g., "@1.0.0")
                size_t atPos = outputPrefix.find('@');
                if (atPos != std::string::npos)
                {
                    outputPrefix = outputPrefix.substr(0, atPos);
                }
            }

            if (outputPrefix.empty())
            {
                outputPrefix = "generated";
            }
        }

        // Generate files
        std::string headerFile = outputPrefix + ".hpp";
        std::string implFile = outputPrefix + ".cpp";
        std::string wamrBindingsFile = outputPrefix + "_wamr.cpp";
        std::string wamrHeaderFile = outputPrefix + "_wamr.hpp";

        // Extract just the filename for includes (not the full path)
        std::filesystem::path headerPath(headerFile);
        std::string headerFilename = headerPath.filename().string();
        std::filesystem::path wamrHeaderPath(wamrHeaderFile);
        std::string wamrHeaderFilename = wamrHeaderPath.filename().string();

        CodeGenerator::generateHeader(parseResult.interfaces, headerFile);
        CodeGenerator::generateWAMRHeader(parseResult.interfaces, wamrHeaderFile, parseResult.packageName, headerFilename);
        CodeGenerator::generateWAMRBindings(parseResult.interfaces, wamrBindingsFile, parseResult.packageName, headerFilename, wamrHeaderFilename);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
