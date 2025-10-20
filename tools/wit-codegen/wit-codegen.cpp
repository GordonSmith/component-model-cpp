#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>

// Local modular headers
#include "wit_parser.hpp"
#include "code_generator.hpp"
#include "package_registry.hpp"
#include "dependency_resolver.hpp"
#include "type_mapper.hpp"

void print_help(const char *program_name)
{
    std::cout << "wit-codegen - WebAssembly Interface Types (WIT) Code Generator\n\n";
    std::cout << "USAGE:\n";
    std::cout << "  " << program_name << " <wit-file-or-dir> [output-prefix]\n";
    std::cout << "  " << program_name << " --help\n";
    std::cout << "  " << program_name << " -h\n\n";
    std::cout << "ARGUMENTS:\n";
    std::cout << "  <wit-file-or-dir> Path to WIT file or directory with WIT package\n";
    std::cout << "  [output-prefix]   Optional output file prefix (default: derived from package name)\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "  -h, --help        Show this help message and exit\n\n";
    std::cout << "DESCRIPTION:\n";
    std::cout << "  Generates C++ host function bindings from WebAssembly Interface Types (WIT)\n";
    std::cout << "  files. The tool parses WIT syntax and generates type-safe C++ code for\n";
    std::cout << "  interfacing with WebAssembly components.\n\n";
    std::cout << "  Supports multi-file WIT packages with deps/ folder dependencies.\n\n";
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
    std::cout << "  - Automatic memory management for complex types\n";
    std::cout << "  - Multi-file package support with deps/ folder resolution\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "  # Generate bindings from single WIT file\n";
    std::cout << "  " << program_name << " example.wit\n\n";
    std::cout << "  # Generate bindings from WIT package directory\n";
    std::cout << "  " << program_name << " wit/\n\n";
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
        PackageRegistry registry;
        ParseResult parseResult;

        // Detect input mode: directory with deps/ or single file
        bool isDirectory = std::filesystem::is_directory(witFile);

        if (isDirectory)
        {
            // Multi-file mode: process directory with potential deps/
            std::cout << "Processing WIT package directory: " << witFile << "\n";

            DependencyResolver resolver;

            // 1. Discover dependencies from deps/ folder
            auto dep_files = resolver.discover_dependencies(witFile);
            std::cout << "Found " << dep_files.size() << " dependency files\n";

            // 2. Sort dependencies by load order
            if (!dep_files.empty())
            {
                dep_files = resolver.sort_by_dependencies(dep_files);

                // 3. Load all dependencies first
                for (const auto &dep_file : dep_files)
                {
                    std::cout << "Loading dependency: \"" << dep_file.generic_string() << "\"\n";
                    if (!registry.load_package(dep_file))
                    {
                        throw std::runtime_error("Failed to load dependency: " + dep_file.string());
                    }
                }
            }

            // 4. Find and load root WIT file(s) from main directory
            auto root_file = resolver.find_root_wit_file(witFile);
            if (root_file.empty())
            {
                std::cerr << "Error: No root WIT file found in directory: " << witFile << "\n";
                return 1;
            }

            std::cout << "Loading root file: " << root_file << "\n";
            parseResult = WitGrammarParser::parse(root_file.string());

            // Add root package to registry
            if (!parseResult.packageName.empty() && parseResult.package_id)
            {
                auto root_package = std::make_unique<WitPackage>();
                root_package->id = *parseResult.package_id;
                root_package->source_path = root_file;
                for (const auto &iface : parseResult.interfaces)
                {
                    root_package->add_interface(iface);
                }
                // Note: We don't actually need to add it to registry for code gen,
                // but it's good for consistency
            }
        }
        else
        {
            // Single-file mode (original behavior)
            std::cout << "Processing single WIT file: " << witFile << "\n";

            // Check if this file has a deps/ folder in its parent directory
            std::filesystem::path file_path(witFile);
            auto parent_dir = file_path.parent_path();
            auto deps_dir = parent_dir / "deps";

            if (std::filesystem::exists(deps_dir) && std::filesystem::is_directory(deps_dir))
            {
                // This single file has dependencies - load them
                std::cout << "Found deps/ folder, loading dependencies...\n";

                DependencyResolver resolver;
                // Pass parent_dir to discover_dependencies, not the file path
                auto dep_files = resolver.discover_dependencies(parent_dir);

                if (!dep_files.empty())
                {
                    std::cout << "Found " << dep_files.size() << " dependency files\n";
                    dep_files = resolver.sort_by_dependencies(dep_files);
                    for (const auto &dep_file : dep_files)
                    {
                        std::cout << "Loading dependency: \"" << dep_file.generic_string() << "\"\n";
                        if (!registry.load_package(dep_file))
                        {
                            throw std::runtime_error("Failed to load dependency: " + dep_file.string());
                        }
                    }
                }
            }

            parseResult = WitGrammarParser::parse(witFile);
        }

        // Set up TypeMapper with registry for external type resolution
        if (registry.get_packages().size() > 0)
        {
            TypeMapper::setPackageRegistry(&registry);
            std::cout << "Loaded " << registry.get_packages().size() << " packages into registry\n";
        }

        // Note: We now generate files even if interfaces are empty, to provide
        // consistent output for world-only files that reference external packages.
        // The generated files will contain minimal stub code with empty namespaces.

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

        // Generate code with registry for external package support
        PackageRegistry *registryPtr = (registry.get_packages().size() > 0) ? &registry : nullptr;
        const std::set<std::string> *external_deps_ptr = parseResult.external_dependencies.empty() ? nullptr : &parseResult.external_dependencies;
        const std::set<std::string> *world_imports_ptr = parseResult.worldImports.empty() ? nullptr : &parseResult.worldImports;
        const std::set<std::string> *world_exports_ptr = parseResult.worldExports.empty() ? nullptr : &parseResult.worldExports;

        CodeGenerator::generateHeader(parseResult.interfaces, headerFile, registryPtr, external_deps_ptr, world_imports_ptr, world_exports_ptr, witFile);
        CodeGenerator::generateWAMRHeader(parseResult.interfaces, wamrHeaderFile, parseResult.packageName, headerFilename, registryPtr, world_imports_ptr, world_exports_ptr);
        CodeGenerator::generateWAMRBindings(parseResult.interfaces, wamrBindingsFile, parseResult.packageName, headerFilename, wamrHeaderFilename, registryPtr, world_imports_ptr, world_exports_ptr);

        std::cout << "Generated files:\n";
        std::cout << "  " << headerFile << "\n";
        std::cout << "  " << wamrHeaderFile << "\n";
        std::cout << "  " << wamrBindingsFile << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
