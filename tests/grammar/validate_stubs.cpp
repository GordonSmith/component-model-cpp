// validate_stubs.cpp
// Selectively compile generated WIT stubs to validate code generation.
// C++ port of validate_stubs.py

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
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
    constexpr const char *CYAN = "\033[0;36m";
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

// Test groups
std::map<std::string, std::vector<std::string>> get_test_groups()
{
    return {
        {"basic", {"floats", "integers", "strings", "char"}},
        {"collections", {"lists", "zero-size-tuple"}},
        {"composite", {"records", "variants", "enum-has-go-keyword", "flags"}},
        {"options", {"simple-option", "option-result", "result-empty"}},
        {"functions", {"multi-return", "conventions"}},
        {"resources", {"resources"}},
        {"async", {"streams", "futures"}}};
}

struct CompileResult
{
    bool success;
    std::string error_message;
};

// Execute a command and capture output
std::string exec_command(const std::string &cmd)
{
    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "r");
#else
    FILE *pipe = popen(cmd.c_str(), "r");
#endif

    if (!pipe)
    {
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
}

// Find vcpkg installed directory
fs::path find_vcpkg_installed(const fs::path &stub_dir, const fs::path &include_dir)
{
    // Try relative to stub_dir first (most reliable)
    std::vector<fs::path> candidates = {
        stub_dir.parent_path().parent_path() / "vcpkg_installed",
        include_dir.parent_path() / "build" / "vcpkg_installed",
        include_dir.parent_path() / "vcpkg_installed"};

    for (const auto &candidate : candidates)
    {
        if (fs::exists(candidate))
        {
            return candidate;
        }
    }

    return fs::path();
}

// Compile a single stub file
CompileResult compile_stub(const std::string &stub_name,
                           const fs::path &stub_dir,
                           const fs::path &include_dir,
                           bool verbose)
{
    fs::path stub_cpp = stub_dir / (stub_name + "_wamr.cpp");

    if (!fs::exists(stub_cpp))
    {
        return {false, "File not found: " + stub_cpp.filename().string()};
    }

    // Find vcpkg installed directory for WAMR headers
    fs::path vcpkg_installed = find_vcpkg_installed(stub_dir, include_dir);

    // Build compile command
    std::ostringstream cmd;
    cmd << "g++ -std=c++20 -c \"" << stub_cpp.string() << "\" "
        << "-I\"" << include_dir.string() << "\" "
        << "-I\"" << stub_dir.string() << "\" "
        << "-Wno-unused-parameter -Wno-unused-variable ";

    // Add WAMR includes if vcpkg_installed exists
    if (!vcpkg_installed.empty())
    {
        // Find the triplet include directory
        for (const auto &entry : fs::directory_iterator(vcpkg_installed))
        {
            if (entry.is_directory() && entry.path().filename().string()[0] != '.')
            {
                fs::path triplet_include = entry.path() / "include";
                if (fs::exists(triplet_include))
                {
                    cmd << "-I\"" << triplet_include.string() << "\" ";
                    if (verbose)
                    {
                        std::cout << "  Using WAMR includes: " << triplet_include << std::endl;
                    }
                    break;
                }
            }
        }
    }
    else if (verbose)
    {
        std::cout << "  Warning: vcpkg_installed not found" << std::endl;
    }

    // Output to temp directory
#ifdef _WIN32
    fs::path temp_obj = fs::temp_directory_path() / (stub_name + "_wamr.obj");
#else
    fs::path temp_obj = fs::path("/tmp") / (stub_name + "_wamr.o");
#endif

    cmd << "-o \"" << temp_obj.string() << "\" 2>&1";

    // Execute compilation
    std::string output = exec_command(cmd.str());

    if (output.empty() || output.find("error:") == std::string::npos)
    {
        return {true, ""};
    }

    // Extract error lines
    std::vector<std::string> errors;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        if (line.find("error:") != std::string::npos ||
            line.find("note:") != std::string::npos)
        {
            errors.push_back(line);
        }
    }

    // Limit to first 10 errors
    std::ostringstream error_msg;
    size_t count = 0;
    for (const auto &error : errors)
    {
        if (count >= 10)
        {
            error_msg << "\n... and " << (errors.size() - 10) << " more errors";
            break;
        }
        if (count > 0)
            error_msg << "\n";
        error_msg << error;
        ++count;
    }

    return {false, error_msg.str()};
}

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS] [stubs...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Selectively compile generated WIT stubs" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -g, --group <name>        Test a predefined group (all, basic, collections," << std::endl;
    std::cout << "                            composite, options, functions, resources, async)" << std::endl;
    std::cout << "  --exclude <group>         Exclude a group when using --group all" << std::endl;
    std::cout << "  -d, --stub-dir <path>     Directory containing generated stubs" << std::endl;
    std::cout << "                            (default: build/test/generated_stubs)" << std::endl;
    std::cout << "  -i, --include-dir <path>  Directory containing cmcpp headers" << std::endl;
    std::cout << "                            (default: include)" << std::endl;
    std::cout << "  -v, --verbose             Show compilation errors" << std::endl;
    std::cout << "  --list-groups             List available test groups and exit" << std::endl;
    std::cout << "  --list-stubs              List available stub files and exit" << std::endl;
    std::cout << "  --help                    Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  # Test only basic types" << std::endl;
    std::cout << "  " << program_name << " -g basic" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Test specific files" << std::endl;
    std::cout << "  " << program_name << " floats integers strings" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Test with verbose output" << std::endl;
    std::cout << "  " << program_name << " -v -g composite" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Test everything except async" << std::endl;
    std::cout << "  " << program_name << " -g all --exclude async" << std::endl;
}

int main(int argc, char *argv[])
{
    // Default paths
    fs::path stub_dir = "build/test/generated_stubs";
    fs::path include_dir = "include";

    // Options
    bool verbose = false;
    bool list_groups = false;
    bool list_stubs = false;
    std::string group;
    std::string exclude_group;
    std::vector<std::string> explicit_stubs;

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
        else if (arg == "--list-groups")
        {
            list_groups = true;
        }
        else if (arg == "--list-stubs")
        {
            list_stubs = true;
        }
        else if (arg == "-g" || arg == "--group")
        {
            if (i + 1 < argc)
            {
                group = argv[++i];
            }
            else
            {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
        }
        else if (arg == "--exclude")
        {
            if (i + 1 < argc)
            {
                exclude_group = argv[++i];
            }
            else
            {
                std::cerr << "Error: --exclude requires an argument" << std::endl;
                return 1;
            }
        }
        else if (arg == "-d" || arg == "--stub-dir")
        {
            if (i + 1 < argc)
            {
                stub_dir = argv[++i];
            }
            else
            {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
        }
        else if (arg == "-i" || arg == "--include-dir")
        {
            if (i + 1 < argc)
            {
                include_dir = argv[++i];
            }
            else
            {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
        }
        else if (arg[0] != '-')
        {
            explicit_stubs.push_back(arg);
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    auto groups = get_test_groups();

    // List groups if requested
    if (list_groups)
    {
        std::cout << "Available test groups:" << std::endl;
        for (const auto &[group_name, stubs] : groups)
        {
            std::cout << "  " << Colors::color(group_name, Colors::CYAN);
            std::cout << std::string(20 - group_name.length(), ' ');
            for (size_t i = 0; i < stubs.size(); ++i)
            {
                if (i > 0)
                    std::cout << ", ";
                std::cout << stubs[i];
            }
            std::cout << std::endl;
        }
        return 0;
    }

    // Resolve paths relative to script directory (parent of current working dir)
    fs::path script_dir = fs::current_path();
    stub_dir = (script_dir / stub_dir).lexically_normal();
    include_dir = (script_dir / include_dir).lexically_normal();

    // List stubs if requested
    if (list_stubs)
    {
        std::cout << "Available stubs in " << stub_dir << ":" << std::endl;
        if (fs::exists(stub_dir))
        {
            std::vector<std::string> stubs;
            for (const auto &entry : fs::directory_iterator(stub_dir))
            {
                if (entry.path().extension() == ".cpp")
                {
                    std::string name = entry.path().stem().string();
                    if (name.ends_with("_wamr"))
                    {
                        name = name.substr(0, name.length() - 5);
                        stubs.push_back(name);
                    }
                }
            }
            std::sort(stubs.begin(), stubs.end());
            for (const auto &stub : stubs)
            {
                std::cout << "  " << stub << std::endl;
            }
        }
        else
        {
            std::cout << "  " << Colors::color("Directory not found!", Colors::RED) << std::endl;
        }
        return 0;
    }

    // Validation
    if (!fs::exists(stub_dir))
    {
        std::cerr << Colors::color("Error: Stub directory not found: " + stub_dir.string(), Colors::RED) << std::endl;
        std::cerr << "Run: cmake --build build --target generate-test-stubs" << std::endl;
        return 1;
    }

    if (!fs::exists(include_dir))
    {
        std::cerr << Colors::color("Error: Include directory not found: " + include_dir.string(), Colors::RED) << std::endl;
        return 1;
    }

    // Determine which stubs to test
    std::vector<std::string> stubs_to_test;

    if (!explicit_stubs.empty())
    {
        stubs_to_test = explicit_stubs;
    }
    else if (!group.empty())
    {
        if (group == "all")
        {
            for (const auto &[group_name, group_stubs] : groups)
            {
                if (exclude_group.empty() || group_name != exclude_group)
                {
                    stubs_to_test.insert(stubs_to_test.end(), group_stubs.begin(), group_stubs.end());
                }
            }
        }
        else if (groups.count(group))
        {
            stubs_to_test = groups[group];
        }
        else
        {
            std::cerr << Colors::color("Error: Unknown group: " + group, Colors::RED) << std::endl;
            return 1;
        }
    }
    else
    {
        // Default: test basic types
        stubs_to_test = groups["basic"];
        std::cout << Colors::color("No stubs specified, testing basic group by default", Colors::YELLOW) << std::endl;
        std::cout << Colors::color("Use --group or specify stub names explicitly\n", Colors::YELLOW) << std::endl;
    }

    if (stubs_to_test.empty())
    {
        std::cerr << Colors::color("Error: No stubs to test!", Colors::RED) << std::endl;
        return 1;
    }

    // Print header
    std::cout << Colors::color("WIT Stub Compilation Validation", Colors::CYAN) << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "Stub directory:    " << stub_dir << std::endl;
    std::cout << "Include directory: " << include_dir << std::endl;
    std::cout << "Testing " << stubs_to_test.size() << " stubs\n"
              << std::endl;

    // Test each stub
    size_t success_count = 0;
    size_t failure_count = 0;
    size_t not_found_count = 0;
    std::vector<std::pair<std::string, std::string>> failures;

    for (size_t i = 0; i < stubs_to_test.size(); ++i)
    {
        const auto &stub_name = stubs_to_test[i];

        std::cout << "[" << (i + 1) << "/" << stubs_to_test.size() << "] ";
        std::cout << stub_name;
        std::cout << std::string(30 - std::min(stub_name.length(), size_t(30)), ' ');
        std::cout << " ... " << std::flush;

        auto result = compile_stub(stub_name, stub_dir, include_dir, verbose);

        if (result.success)
        {
            std::cout << Colors::color("✓", Colors::GREEN) << std::endl;
            ++success_count;
        }
        else if (result.error_message.find("not found") != std::string::npos)
        {
            std::cout << Colors::color("⊘ (not generated)", Colors::YELLOW) << std::endl;
            ++not_found_count;
        }
        else
        {
            std::cout << Colors::color("✗", Colors::RED) << std::endl;
            ++failure_count;
            failures.push_back({stub_name, result.error_message});

            if (verbose)
            {
                std::cout << Colors::color("  Errors:", Colors::RED) << std::endl;
                std::istringstream stream(result.error_message);
                std::string line;
                while (std::getline(stream, line))
                {
                    if (!line.empty())
                    {
                        std::cout << "    " << line << std::endl;
                    }
                }
                std::cout << std::endl;
            }
        }
    }

    // Print summary
    std::cout << "\n"
              << std::string(60, '=') << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total tested:     " << stubs_to_test.size() << std::endl;
    std::cout << Colors::color("  Successful:       " + std::to_string(success_count), Colors::GREEN) << std::endl;
    std::cout << Colors::color("  Failed:           " + std::to_string(failure_count), Colors::RED) << std::endl;
    std::cout << Colors::color("  Not generated:    " + std::to_string(not_found_count), Colors::YELLOW) << std::endl;

    if (!failures.empty())
    {
        std::cout << "\n"
                  << Colors::color("Failed stubs:", Colors::RED) << std::endl;
        for (const auto &[stub_name, error] : failures)
        {
            std::cout << "  - " << stub_name << std::endl;
            if (!verbose && !error.empty())
            {
                // Show just first error line
                size_t newline_pos = error.find('\n');
                std::string first_error = (newline_pos != std::string::npos)
                                              ? error.substr(0, newline_pos)
                                              : error;
                std::cout << "    " << first_error << std::endl;
            }
        }

        if (!verbose)
        {
            std::cout << "\n"
                      << Colors::color("Run with -v to see full error output", Colors::YELLOW) << std::endl;
        }
    }

    // Success rate
    if (!stubs_to_test.empty())
    {
        double success_rate = (static_cast<double>(success_count) / stubs_to_test.size()) * 100.0;
        std::cout << "\nSuccess rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
    }

    return failure_count == 0 ? 0 : 1;
}
