// generate_test_stubs.cpp
// Generate C++ stub files for all WIT files in the grammar test suite.
// Provides detailed output and better error handling than the bash version.
// C++ port of generate_test_stubs.py

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ANSI color codes
namespace Colors
{
    constexpr const char *RED = "\033[0;31m";
    constexpr const char *GREEN = "\033[0;32m";
    constexpr const char *YELLOW = "\033[1;33m";
    constexpr const char *BLUE = "\033[0;34m";
    constexpr const char *NC = "\033[0m";

    bool supports_color()
    {
#ifdef _WIN32
        return _isatty(_fileno(stdout));
#else
        return isatty(fileno(stdout));
#endif
    }

    std::string color(const std::string &text, const char *color_code)
    {
        if (supports_color())
        {
            return std::string(color_code) + text + NC;
        }
        return text;
    }
}

// Platform-specific symbols
#ifdef _WIN32
constexpr const char *CHECKMARK = "OK";
constexpr const char *CROSSMARK = "FAIL";
constexpr const char *SKIPMARK = "SKIP";
#else
constexpr const char *CHECKMARK = "✓";
constexpr const char *CROSSMARK = "✗";
constexpr const char *SKIPMARK = "⊘";
#endif

// Execute command and capture output
std::pair<int, std::string> exec_command(const std::string &cmd, int timeout_seconds = 30)
{
    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    // On Windows, cmd.exe requires extra quotes around the entire command
    // when the command itself starts with a quoted string
    std::string win_cmd = "\"" + cmd + "\" 2>&1";
    FILE *pipe = _popen(win_cmd.c_str(), "r");
#else
    FILE *pipe = popen((cmd + " 2>&1").c_str(), "r");
#endif

    if (!pipe)
    {
        return {-1, ""};
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }

#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif

    return {status, result};
}

// Find all .wit files in a directory
std::vector<fs::path> find_wit_files(const fs::path &directory)
{
    std::vector<fs::path> wit_files;

    for (const auto &entry : fs::recursive_directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wit")
        {
            wit_files.push_back(entry.path());
        }
    }

    std::sort(wit_files.begin(), wit_files.end());
    return wit_files;
}

// Generate CMakeLists.txt for a stub
void generate_cmake_file(const fs::path &output_prefix,
                         const std::vector<fs::path> &generated_files,
                         const std::string &project_name,
                         const std::string &unique_target_prefix)
{
    if (generated_files.empty())
        return;

    fs::path cmake_path = generated_files[0].parent_path() / "CMakeLists.txt";

    // Collect source and header files
    std::vector<std::string> sources, headers;
    for (const auto &f : generated_files)
    {
        if (f.extension() == ".cpp")
        {
            sources.push_back(f.filename().string());
        }
        else if (f.extension() == ".hpp")
        {
            headers.push_back(f.filename().string());
        }
    }

    if (sources.empty())
        return;

    std::string target_base = unique_target_prefix.empty() ? project_name
                                                           : unique_target_prefix + "-" + project_name;

    std::ostringstream cmake_content;
    cmake_content << R"(# Generated CMakeLists.txt for testing )" << project_name << R"( stub compilation
# NOTE: This CMakeLists.txt is generated for compilation testing purposes.
# Files ending in _wamr.cpp require the WAMR (WebAssembly Micro Runtime) headers.
# To build successfully, ensure WAMR is installed or available in your include path.

cmake_minimum_required(VERSION 3.10)
project()" << project_name
                  << R"(-stub-test)

# Note: When built as part of the root CMakeLists.txt, include paths are inherited.
# When built standalone, you need to set CMCPP_INCLUDE_DIR or install cmcpp package.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only search for cmcpp if not already configured (standalone build)
if(NOT DEFINED CMCPP_INCLUDE_DIR AND NOT TARGET cmcpp)
    find_package(cmcpp QUIET)
    if(NOT cmcpp_FOUND)
        # Use local include directory (for standalone testing without installation)
        # Calculate path based on directory depth
        set(CMCPP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include")
        if(NOT EXISTS "${CMCPP_INCLUDE_DIR}/cmcpp.hpp")
            message(FATAL_ERROR "cmcpp headers not found. Set CMCPP_INCLUDE_DIR or install cmcpp package.")
        endif()
        message(STATUS "Using local cmcpp from: ${CMCPP_INCLUDE_DIR}")
        include_directories(${CMCPP_INCLUDE_DIR})
    endif()
endif()

# Suppress narrowing conversion warnings for WebAssembly 32-bit ABI
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/wd4244")
    add_compile_options(
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4305  # truncation from 'type1' to 'type2'
        /wd4309  # truncation of constant value
    )
endif()

# Source files (may include _wamr.cpp which requires WAMR headers)
set(STUB_SOURCES
)";

    for (const auto &s : sources)
    {
        cmake_content << "    " << s << "\n";
    }

    cmake_content << R"()

# Create a static library from the generated stub
# NOTE: Compilation may fail if WAMR headers are not available
add_library()" << target_base
                  << R"(-stub STATIC
    ${STUB_SOURCES}
)

if(TARGET cmcpp::cmcpp)
    target_link_libraries()"
                  << target_base << R"(-stub PRIVATE cmcpp::cmcpp)
else()
    target_include_directories()"
                  << target_base << R"(-stub PRIVATE ${CMCPP_INCLUDE_DIR})
endif()

target_include_directories()"
                  << target_base << R"(-stub
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test executable is excluded from default build since it has no main()
# The static library compilation is sufficient for validation
# add_executable()"
                  << target_base
                  << R"(-stub-test EXCLUDE_FROM_ALL
#     )" << sources[0]
                  << R"(
# )
# 
# target_link_libraries()"
                  << target_base << R"(-stub-test
#     PRIVATE )" << target_base
                  << R"(-stub
# )
# 
# if(NOT TARGET cmcpp::cmcpp)
#     target_include_directories()"
                  << target_base << R"(-stub-test PRIVATE ${CMCPP_INCLUDE_DIR})
# endif()
)";

    std::ofstream file(cmake_path);
    file << cmake_content.str();
}

// Read template file and replace placeholders
std::string read_template_file(const fs::path &template_path)
{
    std::ifstream file(template_path);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open template file: " + template_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Replace placeholders in template
std::string replace_placeholder(const std::string &content,
                                const std::string &placeholder,
                                const std::string &value)
{
    std::string result = content;
    size_t pos = result.find(placeholder);
    if (pos != std::string::npos)
    {
        result.replace(pos, placeholder.length(), value);
    }
    return result;
}

// Generate root CMakeLists.txt
void generate_root_cmake_file(const fs::path &output_dir,
                              const std::vector<fs::path> &stub_dirs,
                              const fs::path &template_base_dir)
{
    fs::path root_cmake_path = output_dir / "CMakeLists.txt";

    // Find template file
    fs::path template_path = template_base_dir / "cmake_templates" / "root_cmake_template.cmake.in";

    if (!fs::exists(template_path))
    {
        std::cerr << Colors::color("Warning: Template file not found at " + template_path.string(),
                                   Colors::YELLOW)
                  << std::endl;
        std::cerr << "Expected location: " << template_path << std::endl;
        throw std::runtime_error("Template file not found");
    }

    std::string cmake_content = read_template_file(template_path);

    // Generate subdirectory list
    std::ostringstream subdirs;
    auto sorted_dirs = stub_dirs;
    std::sort(sorted_dirs.begin(), sorted_dirs.end());

    for (const auto &stub_dir : sorted_dirs)
    {
        try
        {
            fs::path rel_path = fs::relative(stub_dir, output_dir);
            std::string rel_path_str = rel_path.generic_string();
            subdirs << "add_subdirectory(\"" << rel_path_str << "\")\n";
        }
        catch (...)
        {
            // Skip if not relative
        }
    }

    // Replace placeholders
    cmake_content = replace_placeholder(cmake_content, "@STUB_SUBDIRECTORIES@", subdirs.str());
    cmake_content = replace_placeholder(cmake_content, "@STUB_COUNT@", std::to_string(stub_dirs.size()));

    std::ofstream file(root_cmake_path);
    file << cmake_content;
}

// Generate stub for a single WIT file
std::pair<bool, std::string> generate_stub(const fs::path &wit_file,
                                           const fs::path &output_prefix,
                                           const fs::path &codegen_tool,
                                           bool verbose,
                                           bool generate_cmake,
                                           const std::string &unique_target_prefix)
{
    std::ostringstream cmd;
#ifdef _WIN32
    // On Windows, use forward slashes for better cmd.exe compatibility
    cmd << "\"" << codegen_tool.generic_string() << "\" "
        << "\"" << wit_file.generic_string() << "\" "
        << "\"" << output_prefix.generic_string() << "\"";
#else
    cmd << "\"" << codegen_tool.string() << "\" "
        << "\"" << wit_file.string() << "\" "
        << "\"" << output_prefix.string() << "\"";
#endif

    auto [status, output] = exec_command(cmd.str(), 30);

    if (status != 0)
    {
        std::string error_msg;
        if (output.empty())
        {
            error_msg = "Process exited with code " + std::to_string(status);
            // Common Windows exit codes
            if (status == -1073741819 || status == 3221225477) // 0xC0000005 = Access violation
            {
                error_msg += " (segmentation fault/access violation)";
            }
        }
        else
        {
            error_msg = output;
        }
        return {false, error_msg};
    }

    // Check generated files
    std::vector<fs::path> possible_files = {
        fs::path(output_prefix.string() + ".hpp"),
        fs::path(output_prefix.string() + "_wamr.hpp"),
        fs::path(output_prefix.string() + "_wamr.cpp")};

    std::vector<fs::path> files_exist;
    for (const auto &f : possible_files)
    {
        if (fs::exists(f))
        {
            files_exist.push_back(f);
        }
    }

    if (files_exist.empty())
    {
        return {false, "No output files generated"};
    }

    // Generate CMakeLists.txt
    if (generate_cmake && !files_exist.empty())
    {
        std::string project_name = output_prefix.filename().string();
        generate_cmake_file(output_prefix, files_exist, project_name, unique_target_prefix);
    }

    if (verbose)
    {
        std::ostringstream msg;
        msg << "Generated: ";
        for (size_t i = 0; i < files_exist.size(); ++i)
        {
            if (i > 0)
                msg << ", ";
            msg << files_exist[i].filename().string();
        }
        return {true, msg.str()};
    }

    return {true, ""};
}

// Work item for processing
struct WorkItem
{
    size_t index;
    fs::path wit_file;
    fs::path test_dir;
    fs::path output_dir;
    fs::path codegen_tool;
    bool verbose;
    bool generate_cmake;
};

struct WorkResult
{
    size_t index;
    fs::path rel_path;
    bool success;
    std::string message;
};

// Process a single WIT file
WorkResult process_wit_file(const WorkItem &work)
{
    fs::path rel_path = fs::relative(work.wit_file, work.test_dir);
    std::string stub_name = rel_path.stem().string();

    // Create subdirectory for each stub
    fs::path stub_dir = work.output_dir / rel_path.parent_path() / stub_name;
    fs::create_directories(stub_dir);

    fs::path output_prefix = stub_dir / stub_name;

    // Create unique target prefix
    std::string unique_prefix = (rel_path.parent_path() / stub_name).generic_string();
    std::replace(unique_prefix.begin(), unique_prefix.end(), '/', '-');
    std::replace(unique_prefix.begin(), unique_prefix.end(), '\\', '-');

    auto [success, message] = generate_stub(work.wit_file, output_prefix,
                                            work.codegen_tool, work.verbose,
                                            work.generate_cmake, unique_prefix);

    WorkResult result;
    result.index = work.index;
    result.rel_path = rel_path;
    result.success = success;
    result.message = message;

    return result;
}

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Generate C++ stub files for WIT test suite" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -d, --test-dir <path>      Directory containing WIT test files" << std::endl;
    std::cout << "                             (default: ../ref/wit-bindgen/tests/codegen)" << std::endl;
    std::cout << "  -o, --output-dir <path>    Output directory for generated stubs" << std::endl;
    std::cout << "                             (default: generated_stubs)" << std::endl;
    std::cout << "  -c, --codegen <path>       Path to wit-codegen tool" << std::endl;
    std::cout << "                             (default: ../build/tools/wit-codegen/wit-codegen)" << std::endl;
    std::cout << "  -v, --verbose              Print verbose output" << std::endl;
    std::cout << "  -f, --filter <pattern>     Only process files matching this pattern" << std::endl;
    std::cout << "  --no-cmake                 Skip CMakeLists.txt generation" << std::endl;
    std::cout << "  -j, --jobs <N>             Number of parallel jobs (default: CPU count)" << std::endl;
    std::cout << "  --help                     Show this help message" << std::endl;
}

int main(int argc, char *argv[])
{
    // Default options
    fs::path test_dir = "../ref/wit-bindgen/tests/codegen";
    fs::path output_dir = "generated_stubs";
    fs::path codegen_tool = "../build/tools/wit-codegen/wit-codegen";
    bool verbose = false;
    bool no_cmake = false;
    std::string filter;
    int jobs = 0;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "--no-cmake")
        {
            no_cmake = true;
        }
        else if (arg == "-d" || arg == "--test-dir")
        {
            if (i + 1 < argc)
                test_dir = argv[++i];
        }
        else if (arg == "-o" || arg == "--output-dir")
        {
            if (i + 1 < argc)
                output_dir = argv[++i];
        }
        else if (arg == "-c" || arg == "--codegen")
        {
            if (i + 1 < argc)
                codegen_tool = argv[++i];
        }
        else if (arg == "-f" || arg == "--filter")
        {
            if (i + 1 < argc)
                filter = argv[++i];
        }
        else if (arg == "-j" || arg == "--jobs")
        {
            if (i + 1 < argc)
                jobs = std::stoi(argv[++i]);
        }
    }

    // Resolve paths
    // Working directory is CMAKE_CURRENT_SOURCE_DIR (tests/codegen)
    fs::path script_dir = fs::current_path();
    test_dir = (script_dir / test_dir).lexically_normal();
    output_dir = (script_dir / output_dir).lexically_normal();
    codegen_tool = (script_dir / codegen_tool).lexically_normal();

    // Template is in current source directory (tests/codegen/cmake_templates/)
    fs::path template_base_dir = script_dir;

#ifdef _WIN32
    if (!codegen_tool.has_extension())
    {
        codegen_tool.replace_extension(".exe");
    }
#endif

    // Validation
    if (!fs::exists(codegen_tool))
    {
        std::cerr << Colors::color("Error: wit-codegen not found at " + codegen_tool.string(),
                                   Colors::RED)
                  << std::endl;
        std::cerr << "Please build it first with: cmake --build build --target wit-codegen" << std::endl;
        return 1;
    }

    if (!fs::exists(test_dir))
    {
        std::cerr << Colors::color("Error: Test directory not found at " + test_dir.string(),
                                   Colors::RED)
                  << std::endl;
        std::cerr << "Please ensure the wit-bindgen submodule is initialized" << std::endl;
        return 1;
    }

    // Find WIT files
    std::vector<fs::path> wit_files = find_wit_files(test_dir);

    // Apply filter
    if (!filter.empty())
    {
        std::vector<fs::path> filtered;
        for (const auto &f : wit_files)
        {
            if (f.string().find(filter) != std::string::npos)
            {
                filtered.push_back(f);
            }
        }
        wit_files = filtered;
    }

    if (wit_files.empty())
    {
        std::cerr << Colors::color("No .wit files found in " + test_dir.string(), Colors::RED) << std::endl;
        return 1;
    }

    // Create output directory
    fs::create_directories(output_dir);

    // Determine number of parallel jobs
    size_t max_workers = jobs > 0 ? jobs : std::thread::hardware_concurrency();

    // Print header
    std::cout << "WIT Grammar Test Stub Generator" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Test directory:   " << test_dir << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    std::cout << "Code generator:   " << codegen_tool << std::endl;
    std::cout << "Parallel jobs:    " << max_workers << std::endl;
    if (!filter.empty())
    {
        std::cout << "Filter:           " << filter << std::endl;
    }
    std::cout << "\nFound " << wit_files.size() << " WIT files\n"
              << std::endl;

    // Prepare work items
    std::vector<WorkItem> work_items;
    for (size_t i = 0; i < wit_files.size(); ++i)
    {
        WorkItem item;
        item.index = i + 1;
        item.wit_file = wit_files[i];
        item.test_dir = test_dir;
        item.output_dir = output_dir;
        item.codegen_tool = codegen_tool;
        item.verbose = verbose;
        item.generate_cmake = !no_cmake;
        work_items.push_back(item);
    }

    // Process files in parallel
    size_t success_count = 0;
    size_t failure_count = 0;
    size_t skipped_count = 0;
    std::vector<std::pair<std::string, std::string>> failures;
    std::vector<fs::path> generated_stub_dirs;
    std::mutex print_lock;

    std::vector<std::future<WorkResult>> futures;
    for (const auto &item : work_items)
    {
        futures.push_back(std::async(std::launch::async, process_wit_file, item));
    }

    for (auto &future : futures)
    {
        WorkResult result = future.get();

        std::lock_guard<std::mutex> lock(print_lock);

        // Track generated stub directories
        if (result.success)
        {
            fs::path stub_name = result.rel_path.stem();
            fs::path stub_dir = output_dir / result.rel_path.parent_path() / stub_name;
            generated_stub_dirs.push_back(stub_dir);
        }

        std::cout << "[" << result.index << "/" << wit_files.size() << "] ";
        std::cout << "Processing: " << result.rel_path.string() << " ... ";

        if (result.success)
        {
            std::cout << Colors::color(CHECKMARK, Colors::GREEN) << std::endl;
            if (verbose && !result.message.empty())
            {
                std::cout << "       " << result.message << std::endl;
            }
            ++success_count;
        }
        else if (result.message.find("No output files generated") != std::string::npos)
        {
            std::cout << Colors::color(std::string(SKIPMARK) + " (no output)", Colors::YELLOW) << std::endl;
            ++skipped_count;
        }
        else
        {
            std::cout << Colors::color(CROSSMARK, Colors::RED) << std::endl;
            if (verbose)
            {
                std::cout << "       Error: " << result.message << std::endl;
            }
            ++failure_count;
            failures.push_back({result.rel_path.string(), result.message});
        }
    }

    // Print summary
    std::cout << "\n"
              << std::string(50, '=') << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total files:      " << wit_files.size() << std::endl;
    std::cout << Colors::color("  Successful:       " + std::to_string(success_count), Colors::GREEN) << std::endl;
    std::cout << Colors::color("  Skipped:          " + std::to_string(skipped_count), Colors::YELLOW) << std::endl;
    std::cout << Colors::color("  Failed:           " + std::to_string(failure_count), Colors::RED) << std::endl;

    if (!failures.empty())
    {
        std::cout << "\n"
                  << Colors::color("Failed files:", Colors::RED) << std::endl;
        for (const auto &[file, error] : failures)
        {
            std::cout << "  - " << file << std::endl;
            std::cout << "    Error: " << error << std::endl;
        }
    }

    // Generate root CMakeLists.txt
    if (success_count > 0 && !no_cmake)
    {
        std::cout << "\nGenerating root CMakeLists.txt..." << std::endl;
        try
        {
            generate_root_cmake_file(output_dir, generated_stub_dirs, template_base_dir);
            std::cout << Colors::color(std::string(CHECKMARK) + " Created " +
                                           (output_dir / "CMakeLists.txt").string(),
                                       Colors::GREEN)
                      << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << Colors::color("Error generating root CMakeLists.txt: " + std::string(e.what()),
                                       Colors::RED)
                      << std::endl;
            return 1;
        }
    }

    std::cout << "\n"
              << Colors::color(std::string(CHECKMARK) + " Stub generation complete!",
                               Colors::GREEN)
              << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    if (!no_cmake)
    {
        std::cout << "CMakeLists.txt files generated for each stub (use --no-cmake to disable)" << std::endl;
    }

    return (success_count > 0) ? 0 : (failure_count > 0 ? 1 : 0);
}
