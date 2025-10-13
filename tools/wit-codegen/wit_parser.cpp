#include "wit_parser.hpp"
#include "wit_visitor.hpp"
#include "package_registry.hpp"
#include <fstream>
#include <stdexcept>
#include <antlr4-runtime.h>
#include "grammar/WitLexer.h"
#include "grammar/WitParser.h"

using namespace antlr4;

ParseResult WitGrammarParser::parse(const std::string &filename)
{
    ParseResult result;

    try
    {
        std::ifstream stream(filename);
        if (!stream.is_open())
        {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        ANTLRInputStream input(stream);
        WitLexer lexer(&input);
        CommonTokenStream tokens(&lexer);
        WitParser parser(&tokens);

        // Parse the file
        WitParser::WitFileContext *tree = parser.witFile();

        // Visit the parse tree to extract interfaces and functions
        WitInterfaceVisitor visitor(result.interfaces);
        visitor.visit(tree);

        // Get the package name
        result.packageName = visitor.getPackageName();

        // Parse package ID from package name
        if (!result.packageName.empty())
        {
            result.package_id = PackageId::parse(result.packageName);
        }

        // Categorize interfaces as imports or exports
        auto importedInterfaces = visitor.getImportedInterfaces();
        auto exportedInterfaces = visitor.getExportedInterfaces();
        auto standaloneFunctions = visitor.getStandaloneFunctions();

        // Track world information
        result.worldImports = importedInterfaces;
        result.worldExports = exportedInterfaces;
        result.hasWorld = !importedInterfaces.empty() || !exportedInterfaces.empty() || !standaloneFunctions.empty();

        // Collect external dependencies from use statements in interfaces
        for (const auto &iface : result.interfaces)
        {
            for (const auto &use_stmt : iface.use_statements)
            {
                if (!use_stmt.source_package.empty())
                {
                    result.external_dependencies.insert(use_stmt.source_package);
                }
            }
        }

        // Collect external dependencies from world imports/exports
        // Format is "namespace:package/interface@version" or "namespace:package/interface"
        // We need to extract "namespace:package@version"
        for (const auto &import : importedInterfaces)
        {
            // Check if this is an external package reference (contains : and /)
            if (import.find(':') != std::string::npos && import.find('/') != std::string::npos)
            {
                // Extract package part with version
                // Format: "my:dep/a@0.1.0" -> extract "my:dep@0.1.0"
                size_t slash_pos = import.find('/');
                std::string before_slash = import.substr(0, slash_pos);
                std::string after_slash = import.substr(slash_pos + 1);

                // Check if version is in the part after /
                size_t at_pos = after_slash.find('@');
                if (at_pos != std::string::npos)
                {
                    // Version is after interface name: "my:dep/a@0.1.0"
                    std::string version = after_slash.substr(at_pos);            // "@0.1.0"
                    result.external_dependencies.insert(before_slash + version); // "my:dep@0.1.0"
                }
                else
                {
                    // No version, just use the package part
                    result.external_dependencies.insert(before_slash);
                }
            }
        }
        for (const auto &export_name : exportedInterfaces)
        {
            if (export_name.find(':') != std::string::npos && export_name.find('/') != std::string::npos)
            {
                size_t slash_pos = export_name.find('/');
                std::string before_slash = export_name.substr(0, slash_pos);
                std::string after_slash = export_name.substr(slash_pos + 1);

                size_t at_pos = after_slash.find('@');
                if (at_pos != std::string::npos)
                {
                    std::string version = after_slash.substr(at_pos);
                    result.external_dependencies.insert(before_slash + version);
                }
                else
                {
                    result.external_dependencies.insert(before_slash);
                }
            }
        }

        // Process standalone functions by creating synthetic interfaces for them
        if (!standaloneFunctions.empty())
        {
            // Find world-level types interface to copy use statements from
            // Copy world-level use statements by value so we don't keep dangling pointers
            std::vector<UseStatement> world_use_statements;
            for (const auto &iface : result.interfaces)
            {
                if (iface.is_world_level && iface.name == "_world_types")
                {
                    world_use_statements = iface.use_statements;
                    break;
                }
            }

            for (const auto &func : standaloneFunctions)
            {
                // Create a synthetic interface for this standalone function
                InterfaceInfo syntheticInterface;
                syntheticInterface.name = func.name; // Use function name as interface name
                syntheticInterface.package_name = result.packageName;
                syntheticInterface.kind = func.is_import ? InterfaceKind::Import : InterfaceKind::Export;
                syntheticInterface.is_standalone_function = true; // Mark as standalone

                // Copy world-level use statements so the function can reference world-level types
                if (!world_use_statements.empty())
                {
                    syntheticInterface.use_statements = world_use_statements;
                }

                // Add the function to this interface
                FunctionSignature funcCopy = func;
                funcCopy.interface_name = func.name; // Set interface name to match
                syntheticInterface.functions.push_back(funcCopy);

                result.interfaces.push_back(syntheticInterface);
            }
        }

        if (!importedInterfaces.empty() || !exportedInterfaces.empty())
        {
            std::vector<InterfaceInfo> expandedInterfaces;

            for (auto &iface : result.interfaces)
            {
                // Skip standalone functions - they're already categorized
                if (iface.is_standalone_function)
                {
                    expandedInterfaces.push_back(iface);
                    continue;
                }

                bool isImport = importedInterfaces.find(iface.name) != importedInterfaces.end();
                bool isExport = exportedInterfaces.find(iface.name) != exportedInterfaces.end();

                if (isImport && isExport)
                {
                    // Interface is both imported and exported - create two copies
                    InterfaceInfo importCopy = iface;
                    importCopy.kind = InterfaceKind::Import;
                    expandedInterfaces.push_back(importCopy);

                    InterfaceInfo exportCopy = iface;
                    exportCopy.kind = InterfaceKind::Export;
                    expandedInterfaces.push_back(exportCopy);
                }
                else if (isImport)
                {
                    iface.kind = InterfaceKind::Import;
                    expandedInterfaces.push_back(iface);
                }
                else if (isExport)
                {
                    iface.kind = InterfaceKind::Export;
                    expandedInterfaces.push_back(iface);
                }
                else
                {
                    // Default to export if not explicitly imported
                    iface.kind = InterfaceKind::Export;
                    expandedInterfaces.push_back(iface);
                }
            }

            result.interfaces = expandedInterfaces;
        }
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Parse error: " + std::string(e.what()));
    }

    return result;
}
