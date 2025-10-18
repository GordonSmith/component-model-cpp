/**
 * @file test_grammar.cpp
 * @brief Test suite for WIT grammar against official wit-bindgen test files
 *
 * This test validates that our ANTLR4 WIT grammar can successfully parse
 * all the WIT files in the wit-bindgen test suite at:
 * ref/wit-bindgen/tests/codegen
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

#include "antlr4-runtime.h"
#include "grammar/WitLexer.h"
#include "grammar/WitParser.h"

namespace fs = std::filesystem;

/**
 * Custom error listener to collect parsing errors
 */
class ErrorListener : public antlr4::BaseErrorListener
{
public:
    std::vector<std::string> errors;

    void syntaxError(
        antlr4::Recognizer *recognizer,
        antlr4::Token *offendingSymbol,
        size_t line,
        size_t charPositionInLine,
        const std::string &msg,
        std::exception_ptr e) override
    {

        std::ostringstream oss;
        oss << "line " << line << ":" << charPositionInLine << " " << msg;
        errors.push_back(oss.str());
    }

    bool hasErrors() const
    {
        return !errors.empty();
    }

    void printErrors(const std::string &filename) const
    {
        std::cerr << "Errors in " << filename << ":\n";
        for (const auto &error : errors)
        {
            std::cerr << "  " << error << "\n";
        }
    }
};

/**
 * Parse a single WIT file and return true if successful
 */
bool parseWitFile(const fs::path &filePath, bool verbose = false)
{
    try
    {
        // Read the file
        std::ifstream stream(filePath);
        if (!stream.is_open())
        {
            std::cerr << "Failed to open file: " << filePath << "\n";
            return false;
        }

        // Create ANTLR input stream
        antlr4::ANTLRInputStream input(stream);

        // Create lexer
        WitLexer lexer(&input);
        ErrorListener lexerErrorListener;
        lexer.removeErrorListeners();
        lexer.addErrorListener(&lexerErrorListener);

        // Create token stream
        antlr4::CommonTokenStream tokens(&lexer);

        // Create parser
        WitParser parser(&tokens);
        ErrorListener parserErrorListener;
        parser.removeErrorListeners();
        parser.addErrorListener(&parserErrorListener);

        // Parse the file
        antlr4::tree::ParseTree *tree = parser.witFile();

        // Check for errors
        if (lexerErrorListener.hasErrors() || parserErrorListener.hasErrors())
        {
            std::cerr << "Failed to parse: " << filePath.filename() << "\n";
            lexerErrorListener.printErrors(filePath.string());
            parserErrorListener.printErrors(filePath.string());
            return false;
        }

        if (verbose)
        {
            std::cout << "✓ Successfully parsed: " << filePath.filename() << "\n";
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception parsing " << filePath << ": " << e.what() << "\n";
        return false;
    }
}

/**
 * Recursively find all .wit files in a directory
 */
std::vector<fs::path> findWitFiles(const fs::path &directory)
{
    std::vector<fs::path> witFiles;

    if (!fs::exists(directory))
    {
        std::cerr << "Directory does not exist: " << directory << "\n";
        return witFiles;
    }

    for (const auto &entry : fs::recursive_directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wit")
        {
            witFiles.push_back(entry.path());
        }
    }

    // Sort for consistent ordering
    std::sort(witFiles.begin(), witFiles.end());

    return witFiles;
}

/**
 * Main test function
 */
int main(int argc, char *argv[])
{
    bool verbose = false;

    // Use compile-time defined path if available, otherwise use relative path
#ifdef WIT_TEST_DIR
    std::string testDir = WIT_TEST_DIR;
#else
    std::string testDir = "../ref/wit-bindgen/tests/codegen";
#endif

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "-d" || arg == "--directory")
        {
            if (i + 1 < argc)
            {
                testDir = argv[++i];
            }
        }
        else if (arg == "-h" || arg == "--help")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -v, --verbose       Print verbose output\n"
                      << "  -d, --directory DIR Specify test directory (default: " << testDir << ")\n"
                      << "  -h, --help          Show this help message\n";
            return 0;
        }
    }

    std::cout << "WIT Grammar Test Suite\n";
    std::cout << "======================\n";
    std::cout << "Test directory: " << testDir << "\n\n";

    // Find all WIT files
    auto witFiles = findWitFiles(testDir);

    if (witFiles.empty())
    {
        std::cerr << "No .wit files found in " << testDir << "\n";
        return 1;
    }

    std::cout << "Found " << witFiles.size() << " WIT files\n\n";

    // Parse all files
    int successCount = 0;
    int failureCount = 0;
    std::vector<fs::path> failedFiles;

    for (const auto &file : witFiles)
    {
        if (parseWitFile(file, verbose))
        {
            successCount++;
        }
        else
        {
            failureCount++;
            failedFiles.push_back(file);
        }
    }

    // Print summary
    std::cout << "\n======================\n";
    std::cout << "Test Results:\n";
    std::cout << "  Total files:  " << witFiles.size() << "\n";
    std::cout << "  Successful:   " << successCount << "\n";
    std::cout << "  Failed:       " << failureCount << "\n";

    if (failureCount > 0)
    {
        std::cout << "\nFailed files:\n";
        for (const auto &file : failedFiles)
        {
            std::cout << "  - " << file.filename() << "\n";
        }
        std::cout << "\n";
        return 1;
    }

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
