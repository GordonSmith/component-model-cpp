// validate_all_wit_bindgen.cpp
// Validate all WIT files from wit-bindgen test suite
// Generates C++ stubs and tests compilation
// C++ port of validate_all_wit_bindgen.py

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
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

// Colors
namespace Colors
{
    constexpr const char *GREEN = "\033[92m";
    constexpr const char *RED = "\033[91m";
    constexpr const char *YELLOW = "\033[93m";
    constexpr const char *RESET = "\033[0m";
}

// Paths (will be initialized in main)
struct Paths
{
    fs::path repo_root;
    fs::path wit_bindgen_dir;
    fs::path build_dir;
    fs::path generated_dir;
    fs::path BUILD_WIT_CODEGEN;
    fs::path include_dir;
};

// Result types
enum class ResultType
{
    SUCCESS,
    EMPTY_STUB,
    COMPILE_FAILED,
    GEN_FAILED
};

struct ProcessResult
{
    fs::path rel_path;
    ResultType result;
    std::string error_message;
};

// Execute a command and capture output
std::pair<int, std::string> exec_command(const std::string &cmd, int timeout_seconds = 30)
{
    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    FILE *pipe = _popen((cmd + " 2>&1").c_str(), "r");
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
std::vector<fs::path> find_all_wit_files(const fs::path &directory)
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

// Generate unique stub name from WIT file path
std::string get_stub_name(const fs::path &wit_path, const fs::path &base_dir)
{
    fs::path rel_path = fs::relative(wit_path, base_dir);
    std::string name = rel_path.string();

    // Replace path separators and remove .wit extension
    std::replace(name.begin(), name.end(), '/', '_');
    std::replace(name.begin(), name.end(), '\\', '_');

    if (name.ends_with(".wit"))
    {
        name = name.substr(0, name.length() - 4);
    }

    return name;
}

// Generate C++ stub for a WIT file
std::pair<bool, std::string> generate_stub(const fs::path &wit_path,
                                           const std::string &stub_name,
                                           const Paths &paths)
{
    fs::path output_base = paths.generated_dir / stub_name;

    std::ostringstream cmd;
    cmd << "\"" << paths.BUILD_WIT_CODEGEN.string() << "\" "
        << "\"" << wit_path.string() << "\" "
        << "\"" << output_base.string() << "\"";

    auto [status, output] = exec_command(cmd.str(), 10);

    if (status == 0)
    {
        // Check if the generated file exists (wit-codegen adds .hpp)
        fs::path generated_file = fs::path(output_base.string() + ".hpp");
        if (fs::exists(generated_file))
        {
            return {true, ""};
        }
        else
        {
            return {false, "Generation succeeded but file not found"};
        }
    }
    else
    {
        std::string error = output.substr(0, std::min(output.length(), size_t(200)));
        return {false, "Generation failed: " + error};
    }
}

// Check if generated stub only contains empty namespaces
bool check_if_empty_stub(const std::string &stub_name, const Paths &paths)
{
    fs::path stub_file = paths.generated_dir / (stub_name + ".hpp");

    if (!fs::exists(stub_file))
    {
        return false;
    }

    std::ifstream file(stub_file);
    if (!file)
    {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Check for the marker comment that indicates empty interfaces
    return content.find("This WIT file contains no concrete interface definitions") != std::string::npos &&
           content.find("namespace host {}") != std::string::npos &&
           content.find("namespace guest {}") != std::string::npos;
}

// Compile a generated stub
std::pair<bool, std::string> compile_stub(const std::string &stub_name, const Paths &paths)
{
    fs::path stub_file = paths.generated_dir / (stub_name + ".hpp");

    std::ostringstream cmd;
    cmd << "g++ -std=c++20 -fsyntax-only "
        << "-I\"" << paths.include_dir.string() << "\" "
        << "-c \"" << stub_file.string() << "\"";

    auto [status, output] = exec_command(cmd.str(), 30);

    if (status == 0)
    {
        return {true, ""};
    }
    else
    {
        // Extract first error line
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.find(": error:") != std::string::npos)
            {
                return {false, line.substr(0, std::min(line.length(), size_t(200)))};
            }
        }
        return {false, output.substr(0, std::min(output.length(), size_t(200)))};
    }
}

// Process a single WIT file - generate and compile
ProcessResult process_wit_file(const fs::path &wit_path, const Paths &paths)
{
    std::string stub_name = get_stub_name(wit_path, paths.wit_bindgen_dir);
    fs::path rel_path = fs::relative(wit_path, paths.wit_bindgen_dir);

    // Generate stub
    auto [gen_ok, gen_error] = generate_stub(wit_path, stub_name, paths);

    if (gen_ok)
    {
        // Check if this generated an empty stub
        if (check_if_empty_stub(stub_name, paths))
        {
            return {rel_path, ResultType::EMPTY_STUB,
                    "Generated empty stub (references external packages only)"};
        }

        // Compile stub
        auto [compile_ok, compile_error] = compile_stub(stub_name, paths);

        if (compile_ok)
        {
            return {rel_path, ResultType::SUCCESS, ""};
        }
        else
        {
            return {rel_path, ResultType::COMPILE_FAILED, compile_error};
        }
    }
    else
    {
        return {rel_path, ResultType::GEN_FAILED, gen_error};
    }
}

int main()
{
    std::cout << "WIT-Bindgen Test Suite Validation" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Initialize paths
    Paths paths;
    paths.repo_root = fs::current_path();
    paths.wit_bindgen_dir = paths.repo_root / "ref" / "wit-bindgen" / "tests" / "codegen";
    paths.build_dir = paths.repo_root / "build";
    paths.generated_dir = paths.build_dir / "test" / "generated_wit_bindgen";
    paths.BUILD_WIT_CODEGEN = paths.build_dir / "tools" / "wit-codegen" / "wit-codegen";
    paths.include_dir = paths.repo_root / "include";

#ifdef _WIN32
    paths.BUILD_WIT_CODEGEN.replace_extension(".exe");
#endif

    // Ensure build directory exists
    if (!fs::exists(paths.BUILD_WIT_CODEGEN))
    {
        std::cerr << Colors::RED << "Error: wit-codegen not found at "
                  << paths.BUILD_WIT_CODEGEN << Colors::RESET << std::endl;
        std::cerr << "Please build the project first" << std::endl;
        return 1;
    }

    // Create output directory
    fs::create_directories(paths.generated_dir);

    // Find all WIT files
    std::vector<fs::path> wit_files = find_all_wit_files(paths.wit_bindgen_dir);
    size_t num_workers = std::min(std::thread::hardware_concurrency(),
                                  static_cast<unsigned int>(wit_files.size()));

    std::cout << "Found " << wit_files.size() << " WIT files in wit-bindgen test suite" << std::endl;
    std::cout << "Using " << num_workers << " parallel workers\n"
              << std::endl;

    // Statistics
    struct Stats
    {
        size_t total = 0;
        size_t gen_success = 0;
        size_t gen_failed = 0;
        size_t compile_success = 0;
        size_t compile_failed = 0;
        size_t empty_stubs = 0;
    } stats;

    stats.total = wit_files.size();

    std::vector<std::pair<fs::path, std::string>> failed_generation;
    std::vector<std::pair<fs::path, std::string>> failed_compilation;
    std::vector<std::pair<fs::path, std::string>> empty_stub_files;

    std::mutex output_mutex;
    std::vector<std::future<ProcessResult>> futures;

    // Process files in parallel
    for (const auto &wit_path : wit_files)
    {
        futures.push_back(std::async(std::launch::async, process_wit_file, wit_path, paths));
    }

    // Collect results as they complete
    size_t completed = 0;
    for (auto &future : futures)
    {
        ProcessResult result = future.get();

        std::lock_guard<std::mutex> lock(output_mutex);
        ++completed;

        const char *status = "";
        switch (result.result)
        {
        case ResultType::SUCCESS:
            ++stats.gen_success;
            ++stats.compile_success;
            status = Colors::GREEN;
            std::cout << Colors::GREEN << "✓" << Colors::RESET;
            break;

        case ResultType::EMPTY_STUB:
            ++stats.gen_success;
            ++stats.empty_stubs;
            status = Colors::YELLOW;
            empty_stub_files.push_back({result.rel_path, result.error_message});
            std::cout << Colors::YELLOW << "⊘" << Colors::RESET;
            break;

        case ResultType::COMPILE_FAILED:
            ++stats.gen_success;
            ++stats.compile_failed;
            status = Colors::RED;
            failed_compilation.push_back({result.rel_path, result.error_message});
            std::cout << Colors::RED << "✗" << Colors::RESET;
            break;

        case ResultType::GEN_FAILED:
            ++stats.gen_failed;
            status = Colors::YELLOW;
            failed_generation.push_back({result.rel_path, result.error_message});
            std::cout << Colors::YELLOW << "⚠" << Colors::RESET;
            break;
        }

        std::cout << " [" << completed << "/" << stats.total << "] "
                  << result.rel_path.string();

        if (completed % 3 == 0)
        {
            std::cout << std::endl;
        }
        else
        {
            std::cout << "  ";
        }

        std::cout.flush();
    }

    std::cout << "\n\n"
              << std::string(60, '=') << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total WIT files:        " << stats.total << std::endl;
    std::cout << "  Generation successful:  " << stats.gen_success
              << " (" << (stats.gen_success * 100 / stats.total) << "%)" << std::endl;
    std::cout << "  Generation failed:      " << stats.gen_failed << std::endl;
    std::cout << "  Empty stubs (no types): " << stats.empty_stubs << std::endl;
    std::cout << "  Compilation successful: " << stats.compile_success
              << " (" << (stats.compile_success * 100 / stats.total) << "%)" << std::endl;
    std::cout << "  Compilation failed:     " << stats.compile_failed << std::endl;

    if (!empty_stub_files.empty())
    {
        std::cout << "\n"
                  << Colors::YELLOW << "Empty stubs (" << empty_stub_files.size()
                  << " files):" << Colors::RESET << std::endl;
        std::cout << "These files only reference external packages and contain no concrete definitions:"
                  << std::endl;
        for (size_t i = 0; i < std::min(empty_stub_files.size(), size_t(10)); ++i)
        {
            std::cout << "  - " << empty_stub_files[i].first.string() << std::endl;
        }
        if (empty_stub_files.size() > 10)
        {
            std::cout << "  ... and " << (empty_stub_files.size() - 10) << " more" << std::endl;
        }
    }

    if (!failed_generation.empty())
    {
        std::cout << "\n"
                  << Colors::YELLOW << "Failed generation (" << failed_generation.size()
                  << " files):" << Colors::RESET << std::endl;
        for (size_t i = 0; i < std::min(failed_generation.size(), size_t(10)); ++i)
        {
            std::cout << "  - " << failed_generation[i].first.string() << std::endl;
            if (!failed_generation[i].second.empty())
            {
                std::cout << "    " << failed_generation[i].second << std::endl;
            }
        }
        if (failed_generation.size() > 10)
        {
            std::cout << "  ... and " << (failed_generation.size() - 10) << " more" << std::endl;
        }
    }

    if (!failed_compilation.empty())
    {
        std::cout << "\n"
                  << Colors::RED << "Failed compilation (" << failed_compilation.size()
                  << " files):" << Colors::RESET << std::endl;
        for (size_t i = 0; i < std::min(failed_compilation.size(), size_t(10)); ++i)
        {
            std::cout << "  - " << failed_compilation[i].first.string() << std::endl;
            if (!failed_compilation[i].second.empty())
            {
                std::cout << "    " << failed_compilation[i].second << std::endl;
            }
        }
        if (failed_compilation.size() > 10)
        {
            std::cout << "  ... and " << (failed_compilation.size() - 10) << " more" << std::endl;
        }
    }

    double success_rate = (static_cast<double>(stats.compile_success) / stats.total) * 100.0;
    const char *color = (success_rate > 80) ? Colors::GREEN : (success_rate > 50) ? Colors::YELLOW
                                                                                  : Colors::RED;

    std::cout << "\n"
              << color << "Overall success rate: "
              << static_cast<int>(success_rate) << "%" << Colors::RESET << std::endl;

    return (stats.compile_failed == 0) ? 0 : 1;
}
