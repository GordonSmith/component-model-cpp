// summarize_stub_compilation.cpp
// Summarize stub compilation results from build log.
// C++ port of summarize_stub_compilation.py

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Platform-agnostic symbols
constexpr const char *CHECK_MARK = "✓";
constexpr const char *CROSS_MARK = "✗";

struct CompilationResults
{
    std::set<std::string> successful;
    std::set<std::string> failed;
};

// Parse compilation log and extract results
CompilationResults parse_compilation_log(const fs::path &log_file)
{
    CompilationResults results;

    if (!fs::exists(log_file))
    {
        std::cerr << "Error: Log file not found: " << log_file << std::endl;
        return results;
    }

    std::ifstream file(log_file);
    if (!file)
    {
        std::cerr << "Error: Could not open log file: " << log_file << std::endl;
        return results;
    }

    // Read entire file into string
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Pattern for root CMakeLists.txt build (subdirectory-based build)
    // [XX%] Building CXX object SUBDIR/CMakeFiles/TARGET-stub.dir/FILE_wamr.cpp.o
    std::regex building_pattern(R"(\[\s*\d+%\] Building CXX object (.+)/CMakeFiles/(.+)-stub\.dir/(.+)_wamr\.cpp\.o)");

    // Pattern for successful target build (Unix make/Ninja)
    // [XX%] Built target TARGET-stub
    std::regex success_pattern(R"(\[\s*\d+%\] Built target (.+)-stub)");

    // Pattern for compilation errors (Unix make)
    // gmake[2]: *** [SUBDIR/CMakeFiles/TARGET-stub.dir/build.make:XX: ...] Error 1
    std::regex error_pattern(R"(gmake\[\d+\]: \*\*\* \[(.+)/CMakeFiles/(.+)-stub\.dir/)");

    // MSBuild patterns (Visual Studio generator on Windows)
    // TARGET-stub.vcxproj -> G:\...\TARGET-stub.lib
    std::regex msbuild_success_pattern(R"((.+)-stub\.vcxproj -> )");
    // TARGET-stub.vcxproj(NN,NN) : error C2XXX:
    std::regex msbuild_error_pattern(R"((.+)-stub\.vcxproj\(.*\) : error )");

    // Legacy pattern for monolithic build
    // [XX/YY] Building CXX object test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/NAME_wamr.cpp.o
    std::regex legacy_success_pattern(R"(\[\d+/\d+\] Building CXX object test/CMakeFiles/test-stubs-compiled\.dir/generated_stubs/(\w+)_wamr\.cpp\.o)");

    // Legacy pattern for failed compilation
    // FAILED: test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/NAME_wamr.cpp.o
    std::regex legacy_failed_pattern(R"(FAILED: test/CMakeFiles/test-stubs-compiled\.dir/generated_stubs/(\w+)_wamr\.cpp\.o)");

    std::smatch match;

    // Find all successful target builds (root CMakeLists.txt pattern)
    auto search_start = content.cbegin();
    while (std::regex_search(search_start, content.cend(), match, success_pattern))
    {
        results.successful.insert(match[1].str());
        search_start = match.suffix().first;
    }

    // Find all failed builds (root CMakeLists.txt pattern)
    search_start = content.cbegin();
    while (std::regex_search(search_start, content.cend(), match, error_pattern))
    {
        std::string target = match[2].str();
        results.failed.insert(target);
        results.successful.erase(target); // Remove from successful if it was there
        search_start = match.suffix().first;
    }

    // MSBuild: Find all successful target builds (Windows Visual Studio generator)
    search_start = content.cbegin();
    while (std::regex_search(search_start, content.cend(), match, msbuild_success_pattern))
    {
        results.successful.insert(match[1].str());
        search_start = match.suffix().first;
    }

    // MSBuild: Find all failed builds (Windows Visual Studio generator)
    search_start = content.cbegin();
    while (std::regex_search(search_start, content.cend(), match, msbuild_error_pattern))
    {
        std::string target = match[1].str();
        results.failed.insert(target);
        results.successful.erase(target); // Remove from successful if it was there
        search_start = match.suffix().first;
    }

    // Legacy: Find all successful compilations (monolithic build pattern)
    search_start = content.cbegin();
    while (std::regex_search(search_start, content.cend(), match, legacy_success_pattern))
    {
        results.successful.insert(match[1].str());
        search_start = match.suffix().first;
    }

    // Legacy: Find all failed compilations (monolithic build pattern)
    search_start = content.cbegin();
    while (std::regex_search(search_start, content.cend(), match, legacy_failed_pattern))
    {
        std::string stub_name = match[1].str();
        results.failed.insert(stub_name);
        results.successful.erase(stub_name); // Remove from successful if it was there
        search_start = match.suffix().first;
    }

    return results;
}

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " --log-file <path> [--stub-list <path>]" << std::endl;
    std::cout << std::endl;
    std::cout << "Summarize stub compilation results from build log" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --log-file <path>    Path to compilation log file (required)" << std::endl;
    std::cout << "  --stub-list <path>   Path to file listing expected stubs (optional)" << std::endl;
    std::cout << "  --help               Show this help message" << std::endl;
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    fs::path log_file;
    fs::path stub_list;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--log-file")
        {
            if (i + 1 < argc)
            {
                log_file = argv[++i];
            }
            else
            {
                std::cerr << "Error: --log-file requires an argument" << std::endl;
                return 1;
            }
        }
        else if (arg == "--stub-list")
        {
            if (i + 1 < argc)
            {
                stub_list = argv[++i];
            }
            else
            {
                std::cerr << "Error: --stub-list requires an argument" << std::endl;
                return 1;
            }
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if (log_file.empty())
    {
        std::cerr << "Error: --log-file is required" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Parse log file
    auto results = parse_compilation_log(log_file);

    // Convert sets to sorted vectors for consistent output
    std::vector<std::string> successful(results.successful.begin(), results.successful.end());
    std::vector<std::string> failed(results.failed.begin(), results.failed.end());
    std::sort(successful.begin(), successful.end());
    std::sort(failed.begin(), failed.end());

    size_t total = successful.size() + failed.size();

    // Print summary
    std::cout << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "Stub Compilation Summary" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << std::endl;
    std::cout << "Total stubs attempted: " << total << std::endl;
    std::cout << "Successful:            " << successful.size()
              << " (" << (total > 0 ? (100 * successful.size() / total) : 0) << "%)" << std::endl;
    std::cout << "Failed:                " << failed.size()
              << " (" << (total > 0 ? (100 * failed.size() / total) : 0) << "%)" << std::endl;
    std::cout << std::endl;

    if (!successful.empty())
    {
        std::cout << "Successfully compiled stubs:" << std::endl;
        for (const auto &stub : successful)
        {
            std::cout << "  " << CHECK_MARK << " " << stub << std::endl;
        }
        std::cout << std::endl;
    }

    if (!failed.empty())
    {
        std::cout << "Failed stubs:" << std::endl;
        for (const auto &stub : failed)
        {
            std::cout << "  " << CROSS_MARK << " " << stub << std::endl;
        }
        std::cout << std::endl;
        std::cout << "To see detailed errors for a specific stub, run:" << std::endl;
        std::cout << "  ninja test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/<stub-name>_wamr.cpp.o" << std::endl;
    }
    else
    {
        std::cout << "All stubs compiled successfully! " << CHECK_MARK << std::endl;
    }

    std::cout << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << std::endl;

    // Return non-zero if any failed
    return failed.empty() ? 0 : 1;
}
