#include "wit_parser.hpp"
#include "wit_visitor.hpp"
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

        // Categorize interfaces as imports or exports
        auto importedInterfaces = visitor.getImportedInterfaces();
        auto exportedInterfaces = visitor.getExportedInterfaces();
        auto standaloneFunctions = visitor.getStandaloneFunctions();

        // Track world information
        result.worldImports = importedInterfaces;
        result.worldExports = exportedInterfaces;
        result.hasWorld = !importedInterfaces.empty() || !exportedInterfaces.empty() || !standaloneFunctions.empty();

        // Process standalone functions by creating synthetic interfaces for them
        if (!standaloneFunctions.empty())
        {
            for (const auto &func : standaloneFunctions)
            {
                // Create a synthetic interface for this standalone function
                InterfaceInfo syntheticInterface;
                syntheticInterface.name = func.name; // Use function name as interface name
                syntheticInterface.package_name = result.packageName;
                syntheticInterface.kind = func.is_import ? InterfaceKind::Import : InterfaceKind::Export;
                syntheticInterface.is_standalone_function = true; // Mark as standalone

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
