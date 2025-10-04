#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <antlr4-runtime.h>
#include "grammar/WitLexer.h"
#include "grammar/WitParser.h"
#include "grammar/WitBaseVisitor.h"

using namespace antlr4;

// Data structures for code generation
struct Parameter
{
    std::string name;
    std::string type;
};

struct FunctionSignature
{
    std::string interface_name;
    std::string name;
    std::vector<Parameter> parameters;
    std::vector<std::string> results;
    bool is_import = false; // Track if this is an import (for standalone functions)
};

// Type definition structures
struct VariantCase
{
    std::string name;
    std::string type; // empty if no associated type
};

struct VariantDef
{
    std::string name;
    std::vector<VariantCase> cases;
};

struct EnumDef
{
    std::string name;
    std::vector<std::string> values;
};

struct RecordField
{
    std::string name;
    std::string type;
};

struct RecordDef
{
    std::string name;
    std::vector<RecordField> fields;
};

enum class InterfaceKind
{
    Import, // Host implements, guest calls
    Export  // Guest implements, host calls
};

struct InterfaceInfo
{
    std::string package_name;
    std::string name;
    InterfaceKind kind;
    std::vector<FunctionSignature> functions;
    std::vector<VariantDef> variants;
    std::vector<EnumDef> enums;
    std::vector<RecordDef> records;
    bool is_standalone_function = false; // True if this is a synthetic interface for a world-level function
};

// Type mapper from WIT types to cmcpp types
class TypeMapper
{
public:
    static std::string mapType(const std::string &witType, const InterfaceInfo *iface = nullptr)
    {
        // Remove whitespace
        std::string type = witType;
        type.erase(std::remove_if(type.begin(), type.end(), ::isspace), type.end());

        // Basic types
        if (type == "bool")
            return "cmcpp::bool_t";
        if (type == "u8")
            return "uint8_t";
        if (type == "u16")
            return "uint16_t";
        if (type == "u32")
            return "uint32_t";
        if (type == "u64")
            return "uint64_t";
        if (type == "s8")
            return "int8_t";
        if (type == "s16")
            return "int16_t";
        if (type == "s32")
            return "int32_t";
        if (type == "s64")
            return "int64_t";
        if (type == "f32")
            return "cmcpp::float32_t";
        if (type == "f64")
            return "cmcpp::float64_t";
        if (type == "char")
            return "cmcpp::char_t";
        if (type == "string")
            return "cmcpp::string_t";

        // Check if it's a locally defined type in the current interface
        if (iface)
        {
            // Check enums
            for (const auto &enumDef : iface->enums)
            {
                if (enumDef.name == type)
                    return enumDef.name; // TODO: sanitize when type definitions are implemented
            }
            // Check variants
            for (const auto &variant : iface->variants)
            {
                if (variant.name == type)
                    return variant.name; // TODO: sanitize when type definitions are implemented
            }
            // Check records
            for (const auto &record : iface->records)
            {
                if (record.name == type)
                    return record.name; // TODO: sanitize when type definitions are implemented
            }
        }

        // List types
        if (type.find("list<") == 0)
        {
            size_t start = type.find('<') + 1;
            size_t end = type.rfind('>');
            std::string innerType = type.substr(start, end - start);
            return "cmcpp::list_t<" + mapType(innerType, iface) + ">";
        }

        // Option types
        if (type.find("option<") == 0)
        {
            size_t start = type.find('<') + 1;
            size_t end = type.rfind('>');
            std::string innerType = type.substr(start, end - start);
            return "cmcpp::option_t<" + mapType(innerType, iface) + ">";
        }

        // Result types
        if (type.find("result<") == 0)
        {
            size_t start = type.find('<') + 1;
            size_t end = type.rfind('>');
            std::string innerTypes = type.substr(start, end - start);
            size_t comma = innerTypes.find(',');
            if (comma != std::string::npos)
            {
                std::string okType = mapType(innerTypes.substr(0, comma), iface);
                std::string errType = mapType(innerTypes.substr(comma + 1), iface);
                return "cmcpp::result_t<" + okType + ", " + errType + ">";
            }
            else
            {
                return "cmcpp::result_t<" + mapType(innerTypes, iface) + ">";
            }
        }

        // Tuple types
        if (type.find("tuple<") == 0)
        {
            size_t start = type.find('<') + 1;
            size_t end = type.rfind('>');
            std::string innerTypes = type.substr(start, end - start);
            std::stringstream ss(innerTypes);
            std::string item;
            std::vector<std::string> types;
            while (std::getline(ss, item, ','))
            {
                types.push_back(mapType(item, iface));
            }
            std::string result = "cmcpp::tuple_t<";
            for (size_t i = 0; i < types.size(); ++i)
            {
                if (i > 0)
                    result += ", ";
                result += types[i];
            }
            result += ">";
            return result;
        }

        // Unknown type - return as-is with warning
        std::cerr << "Warning: Unknown type '" << type << "', using as-is" << std::endl;
        return type;
    }
};

// WIT Interface visitor that extracts functions using ANTLR grammar
class WitInterfaceVisitor : public WitBaseVisitor
{
private:
    std::vector<InterfaceInfo> &interfaces;
    InterfaceInfo *currentInterface = nullptr;
    std::string currentPackage;
    std::set<std::string> importedInterfaces;           // Track which interfaces are imported in the world
    std::set<std::string> exportedInterfaces;           // Track which interfaces are exported in the world
    std::vector<FunctionSignature> standaloneFunctions; // Standalone functions from world (not in interfaces)

public:
    WitInterfaceVisitor(std::vector<InterfaceInfo> &ifaces) : interfaces(ifaces) {}

    antlrcpp::Any visitPackageDecl(WitParser::PackageDeclContext *ctx) override
    {
        // Package format: package ns:pkg/name@version
        // Extract just the package identifier (ns:pkg/name@version), not the "package" keyword
        if (ctx)
        {
            std::string fullText = ctx->getText();
            // Remove "package" prefix
            if (fullText.find("package") == 0)
            {
                fullText = fullText.substr(7); // Remove "package"
                // Trim leading whitespace
                size_t start = fullText.find_first_not_of(" \t\n\r");
                if (start != std::string::npos)
                {
                    fullText = fullText.substr(start);
                }
            }
            // Remove trailing semicolon if present
            if (!fullText.empty() && fullText.back() == ';')
            {
                fullText.pop_back();
            }
            currentPackage = fullText;
        }
        return visitChildren(ctx);
    }

    // Visit import items in world to track which interfaces to generate
    antlrcpp::Any visitImportItem(WitParser::ImportItemContext *ctx) override
    {
        // Check for "import id : externType" pattern (function import or inline interface)
        if (ctx->id() && ctx->externType())
        {
            std::string importName = ctx->id()->getText();

            // Check if externType is a funcType (standalone function)
            if (ctx->externType()->funcType())
            {
                // This is a standalone function import
                FunctionSignature func;
                func.name = importName;
                func.interface_name = ""; // Empty indicates standalone function
                func.is_import = true;    // Mark as import

                // Parse the function type
                parseFuncType(ctx->externType()->funcType(), func);

                // Mark as imported
                standaloneFunctions.push_back(func);
                std::cout << "  Found standalone import function: " << importName << std::endl;
            }
            else
            {
                // This is an inline interface import
                importedInterfaces.insert(importName);
                std::cout << "  Found import: " << importName << std::endl;
            }
        }
        // Check for "import usePath ;" pattern (interface import)
        else if (ctx->usePath())
        {
            std::string interfaceName = ctx->usePath()->getText();
            importedInterfaces.insert(interfaceName);
            std::cout << "  Found import: " << interfaceName << std::endl;
        }
        return visitChildren(ctx);
    }

    // Visit export items in world to track which interfaces to generate
    antlrcpp::Any visitExportItem(WitParser::ExportItemContext *ctx) override
    {
        // Check for "export id : externType" pattern (function export or inline interface)
        if (ctx->id() && ctx->externType())
        {
            std::string exportName = ctx->id()->getText();

            // Check if externType is a funcType (standalone function)
            if (ctx->externType()->funcType())
            {
                // This is a standalone function export
                FunctionSignature func;
                func.name = exportName;
                func.interface_name = ""; // Empty indicates standalone function
                func.is_import = false;   // Mark as export

                // Parse the function type
                parseFuncType(ctx->externType()->funcType(), func);

                // Mark as exported
                standaloneFunctions.push_back(func);
                std::cout << "  Found standalone export function: " << exportName << std::endl;
            }
            else
            {
                // This is an inline interface export
                exportedInterfaces.insert(exportName);
                std::cout << "  Found export: " << exportName << std::endl;
            }
        }
        // Check for "export usePath ;" pattern (interface export)
        else if (ctx->usePath())
        {
            std::string interfaceName = ctx->usePath()->getText();
            exportedInterfaces.insert(interfaceName);
            std::cout << "  Found export: " << interfaceName << std::endl;
        }
        return visitChildren(ctx);
    }

    // Helper to parse funcType into a FunctionSignature
    void parseFuncType(WitParser::FuncTypeContext *funcType, FunctionSignature &func)
    {
        if (!funcType)
            return;

        // Get parameters from paramList -> namedTypeList
        if (funcType->paramList() && funcType->paramList()->namedTypeList())
        {
            auto namedTypeList = funcType->paramList()->namedTypeList();
            for (auto namedType : namedTypeList->namedType())
            {
                if (namedType->id() && namedType->ty())
                {
                    Parameter param;
                    param.name = namedType->id()->getText();
                    param.type = namedType->ty()->getText();
                    func.parameters.push_back(param);
                }
            }
        }

        // Get results from resultList
        if (funcType->resultList() && funcType->resultList()->ty())
        {
            func.results.push_back(funcType->resultList()->ty()->getText());
        }
    }

    antlrcpp::Any visitInterfaceItem(WitParser::InterfaceItemContext *ctx) override
    {
        if (ctx->id())
        {
            std::string interfaceName = ctx->id()->getText();

            // Only add interface if it's imported (or if we haven't seen any world yet)
            interfaces.emplace_back();
            currentInterface = &interfaces.back();
            currentInterface->name = interfaceName;
            currentInterface->package_name = currentPackage;
        }
        return visitChildren(ctx);
    }

    antlrcpp::Any visitFuncItem(WitParser::FuncItemContext *ctx) override
    {
        if (!currentInterface)
        {
            return nullptr;
        }

        FunctionSignature func;
        func.interface_name = currentInterface->name;

        // Get function name (id before ':')
        if (ctx->id())
        {
            func.name = ctx->id()->getText();
        }

        // Get function type (params and results)
        if (ctx->funcType())
        {
            parseFuncType(ctx->funcType(), func);
        }

        currentInterface->functions.push_back(func);
        return nullptr;
    }

    // Visit variant type definitions
    antlrcpp::Any visitVariantItems(WitParser::VariantItemsContext *ctx) override
    {
        if (!currentInterface || !ctx->id())
        {
            return visitChildren(ctx);
        }

        VariantDef variant;
        variant.name = ctx->id()->getText();

        // Parse variant cases recursively
        if (ctx->variantCases())
        {
            parseVariantCases(ctx->variantCases(), variant);
        }

        currentInterface->variants.push_back(variant);
        return visitChildren(ctx);
    }

    // Helper to recursively parse variant cases
    void parseVariantCases(WitParser::VariantCasesContext *ctx, VariantDef &variant)
    {
        if (!ctx)
            return;

        // Get the first variant case
        if (ctx->variantCase())
        {
            VariantCase vcase;
            auto variantCase = ctx->variantCase();

            // Get case name (always present)
            if (variantCase->id())
            {
                vcase.name = variantCase->id()->getText();

                // Check if the case has an associated type: id '(' ty ')'
                if (variantCase->ty())
                {
                    vcase.type = variantCase->ty()->getText();
                }

                variant.cases.push_back(vcase);
            }
        }

        // Recursively parse remaining cases
        if (ctx->variantCases())
        {
            parseVariantCases(ctx->variantCases(), variant);
        }
    }

    // Visit enum type definitions
    antlrcpp::Any visitEnumItems(WitParser::EnumItemsContext *ctx) override
    {
        if (!currentInterface || !ctx->id())
        {
            return visitChildren(ctx);
        }

        EnumDef enumDef;
        enumDef.name = ctx->id()->getText();

        // Parse enum values recursively
        if (ctx->enumCases())
        {
            parseEnumCases(ctx->enumCases(), enumDef);
        }

        currentInterface->enums.push_back(enumDef);
        return visitChildren(ctx);
    }

    // Helper to recursively parse enum cases
    void parseEnumCases(WitParser::EnumCasesContext *ctx, EnumDef &enumDef)
    {
        if (!ctx)
            return;

        // Get the first enum case
        if (ctx->id())
        {
            enumDef.values.push_back(ctx->id()->getText());
        }

        // Recursively parse remaining cases
        if (ctx->enumCases())
        {
            parseEnumCases(ctx->enumCases(), enumDef);
        }
    }

    // Get the set of imported interfaces from the world
    const std::set<std::string> &getImportedInterfaces() const
    {
        return importedInterfaces;
    }

    // Get the set of exported interfaces from the world
    const std::set<std::string> &getExportedInterfaces() const
    {
        return exportedInterfaces;
    }

    // Get standalone functions from the world
    const std::vector<FunctionSignature> &getStandaloneFunctions() const
    {
        return standaloneFunctions;
    }

    // Get the package name from the WIT file
    const std::string &getPackageName() const
    {
        return currentPackage;
    }
};

// Result of parsing a WIT file
struct ParseResult
{
    std::vector<InterfaceInfo> interfaces;
    std::string packageName;
};

// Parse WIT file using ANTLR grammar
class WitGrammarParser
{
public:
    static ParseResult parse(const std::string &filename)
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

            // Process standalone functions by creating synthetic interfaces for them
            if (!standaloneFunctions.empty())
            {
                std::cout << "Processing standalone functions..." << std::endl;
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

                    std::cout << "  " << func.name << " -> "
                              << (func.is_import ? "Import" : "Export")
                              << " (standalone function)" << std::endl;
                }
            }

            if (!importedInterfaces.empty() || !exportedInterfaces.empty())
            {
                std::cout << "Categorizing interfaces..." << std::endl;
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
                        std::cout << "  " << iface.name << " -> Import + Export (bidirectional)" << std::endl;

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
                        std::cout << "  " << iface.name << " -> Import (host implements)" << std::endl;
                        expandedInterfaces.push_back(iface);
                    }
                    else if (isExport)
                    {
                        iface.kind = InterfaceKind::Export;
                        std::cout << "  " << iface.name << " -> Export (guest implements)" << std::endl;
                        expandedInterfaces.push_back(iface);
                    }
                    else
                    {
                        // Default to export if not explicitly imported
                        iface.kind = InterfaceKind::Export;
                        std::cout << "  " << iface.name << " -> Export (default)" << std::endl;
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
};

// Code generator for C++ host functions
class CodeGenerator
{
public:
    static void generateHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename)
    {
        std::ofstream out(filename);
        if (!out.is_open())
        {
            throw std::runtime_error("Cannot create header file: " + filename);
        }

        // Generate header guard
        std::string guard = "GENERATED_" + sanitize_identifier(filename) + "_HPP";
        std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);

        out << "#pragma once\n\n";
        out << "#ifndef " << guard << "\n";
        out << "#define " << guard << "\n\n";
        out << "#include <cmcpp.hpp>\n\n";
        out << "// Generated host function declarations from WIT\n";
        out << "// - 'host' namespace: Guest imports (host implements these)\n";
        out << "// - 'guest' namespace: Guest exports (guest implements these, host calls them)\n\n";

        // Separate interfaces by kind
        std::vector<const InterfaceInfo *> imports;
        std::vector<const InterfaceInfo *> exports;

        for (const auto &iface : interfaces)
        {
            if (iface.kind == InterfaceKind::Import)
            {
                imports.push_back(&iface);
            }
            else
            {
                exports.push_back(&iface);
            }
        }

        std::cout << "Generating header with " << imports.size() << " imports and " << exports.size() << " exports" << std::endl;
        for (const auto *iface : imports)
        {
            std::cout << "  Import: " << iface->name << std::endl;
        }
        for (const auto *iface : exports)
        {
            std::cout << "  Export: " << iface->name << std::endl;
        }

        // Generate host namespace (host implements)
        if (!imports.empty())
        {
            out << "namespace host {\n\n";

            for (const auto *iface : imports)
            {
                // Check if this is a standalone function (world-level function, not in an interface)
                if (iface->is_standalone_function)
                {
                    // Generate standalone function directly in host namespace without sub-namespace
                    out << "// Standalone function: " << iface->name << "\n";
                    if (!iface->package_name.empty())
                    {
                        out << "// Package: " << iface->package_name << "\n";
                    }

                    for (const auto &func : iface->functions)
                    {
                        generateFunctionDeclaration(out, func, iface);
                    }
                    out << "\n";
                }
                else
                {
                    // Regular interface - create namespace
                    out << "// Interface: " << iface->name << "\n";
                    if (!iface->package_name.empty())
                    {
                        out << "// Package: " << iface->package_name << "\n";
                    }
                    out << "namespace " << sanitize_identifier(iface->name) << " {\n\n";

                    // Generate type definitions first
                    generateTypeDefinitions(out, *iface);

                    // Then generate function declarations
                    for (const auto &func : iface->functions)
                    {
                        generateFunctionDeclaration(out, func, iface);
                    }

                    out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
                }
            }

            out << "} // namespace host\n\n";
        }

        // Generate guest namespace (guest implements, host calls)
        if (!exports.empty())
        {
            out << "namespace guest {\n\n";

            for (const auto *iface : exports)
            {
                // Check if this is a standalone function (world-level function, not in an interface)
                if (iface->is_standalone_function)
                {
                    // Generate standalone function directly in guest namespace without sub-namespace
                    out << "// Standalone function: " << iface->name << "\n";
                    if (!iface->package_name.empty())
                    {
                        out << "// Package: " << iface->package_name << "\n";
                    }

                    for (const auto &func : iface->functions)
                    {
                        generateGuestFunctionTypeAlias(out, func, iface);
                    }
                    out << "\n";
                }
                else
                {
                    // Regular interface - create namespace
                    out << "// Interface: " << iface->name << "\n";
                    if (!iface->package_name.empty())
                    {
                        out << "// Package: " << iface->package_name << "\n";
                    }
                    out << "namespace " << sanitize_identifier(iface->name) << " {\n\n";

                    // Generate type definitions first
                    generateTypeDefinitions(out, *iface);

                    // Then generate function type aliases for guest functions
                    for (const auto &func : iface->functions)
                    {
                        generateGuestFunctionTypeAlias(out, func, iface);
                    }

                    out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
                }
            }

            out << "} // namespace guest\n\n";
        }

        out << "#endif // " << guard << "\n";
    }

    // NOTE: This function generates stub implementations but is intentionally not called.
    // Host applications are responsible for providing their own implementations of the
    // declared functions. This function is kept for reference or optional stub generation.
    static void generateImplementation(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &headerName)
    {
        std::ofstream out(filename);
        if (!out.is_open())
        {
            throw std::runtime_error("Cannot create implementation file: " + filename);
        }

        // Extract just the filename from the header path for the include directive
        std::string headerBasename = headerName.substr(headerName.find_last_of("/\\") + 1);
        out << "#include \"" << headerBasename << "\"\n\n";
        out << "// Generated host function implementations (stubs)\n\n";

        out << "namespace host {\n\n";

        for (const auto &iface : interfaces)
        {
            out << "// Interface: " << iface.name << "\n";
            out << "namespace " << sanitize_identifier(iface.name) << " {\n\n";

            for (const auto &func : iface.functions)
            {
                generateFunctionImplementation(out, func, &iface);
            }

            out << "} // namespace " << sanitize_identifier(iface.name) << "\n\n";
        }

        out << "} // namespace host\n";
    }

    static void generateWAMRBindings(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &headerFile, const std::string &wamrHeaderFile)
    {
        std::ofstream out(filename);
        if (!out.is_open())
        {
            throw std::runtime_error("Cannot create bindings file: " + filename);
        }

        out << "#include \"" << wamrHeaderFile << "\"\n\n";
        out << "#include <stdexcept>\n";
        out << "#include <vector>\n\n";
        out << "// Generated WAMR bindings for package: " << packageName << "\n";
        out << "// These symbol arrays can be used with wasm_runtime_register_natives_raw()\n";
        out << "// NOTE: You must implement the functions declared in the imports namespace\n";
        out << "// (See " << headerFile << " for declarations, provide implementations in your host code)\n\n";

        out << "using namespace cmcpp;\n\n";

        // Group interfaces by kind (Import vs Export)
        std::vector<const InterfaceInfo *> importInterfaces;
        std::vector<const InterfaceInfo *> exportInterfaces;

        for (const auto &iface : interfaces)
        {
            if (iface.kind == InterfaceKind::Import)
            {
                importInterfaces.push_back(&iface);
            }
            else
            {
                exportInterfaces.push_back(&iface);
            }
        }

        out << "// WAMR Native Symbol arrays organized by interface\n";
        out << "// Register these with wasm_runtime_register_natives_raw(namespace, array, count)\n\n";

        // Generate symbol arrays for each IMPORT interface
        for (const auto *iface : importInterfaces)
        {
            std::string arrayName = sanitize_identifier(iface->name) + "_symbols";
            std::string moduleName = iface->is_standalone_function ? "$root" : (packageName + "/" + iface->name);

            out << "// Import interface: " << iface->name << "\n";
            out << "// Register with: wasm_runtime_register_natives_raw(\"" << moduleName << "\", " << arrayName << ", " << iface->functions.size() << ")\n";
            out << "NativeSymbol " << arrayName << "[] = {\n";

            for (const auto &func : iface->functions)
            {
                std::string funcName = sanitize_identifier(func.name);

                // For standalone functions, reference directly in host namespace
                // For interface functions, reference within their interface namespace
                if (iface->is_standalone_function)
                {
                    out << "    host_function(\"" << func.name << "\", host::" << funcName << "),\n";
                }
                else
                {
                    out << "    host_function(\"" << func.name << "\", host::" << sanitize_identifier(iface->name) << "::" << funcName << "),\n";
                }
            }

            out << "};\n\n";
        }

        // Add a helper function that returns all symbol arrays
        out << "// Get all import interfaces for registration\n";
        out << "// Usage:\n";
        out << "//   for (const auto& reg : get_import_registrations()) {\n";
        out << "//       wasm_runtime_register_natives_raw(reg.module_name, reg.symbols, reg.count);\n";
        out << "//   }\n";
        out << "std::vector<NativeRegistration> get_import_registrations() {\n";
        out << "    return {\n";

        for (const auto *iface : importInterfaces)
        {
            std::string arrayName = sanitize_identifier(iface->name) + "_symbols";
            std::string moduleName = iface->is_standalone_function ? "$root" : (packageName + "/" + iface->name);
            out << "        {\"" << moduleName << "\", " << arrayName << ", " << iface->functions.size() << "},\n";
        }

        out << "    };\n";
        out << "}\n\n";

        // Add helper functions for common WAMR operations
        out << "// Helper function to register all import interfaces at once\n";
        out << "// Returns the number of functions registered\n";
        out << "int register_all_imports() {\n";
        out << "    int count = 0;\n";
        out << "    for (const auto& reg : get_import_registrations()) {\n";
        out << "        if (!wasm_runtime_register_natives_raw(reg.module_name, reg.symbols, reg.count)) {\n";
        out << "            return -1;  // Registration failed\n";
        out << "        }\n";
        out << "        count += reg.count;\n";
        out << "    }\n";
        out << "    return count;\n";
        out << "}\n\n";

        out << "// Helper function to unregister all import interfaces\n";
        out << "void unregister_all_imports() {\n";
        out << "    for (const auto& reg : get_import_registrations()) {\n";
        out << "        wasm_runtime_unregister_natives(reg.module_name, reg.symbols);\n";
        out << "    }\n";
        out << "}\n\n";

        // WASM utilities namespace
        out << "// WASM file utilities\n";
        out << "namespace wasm_utils {\n\n";

        out << "const uint32_t DEFAULT_STACK_SIZE = 8192;\n";
        out << "const uint32_t DEFAULT_HEAP_SIZE = 8192;\n\n";

        out << "// Note: create_guest_realloc() and create_lift_lower_context() have been\n";
        out << "// moved to <wamr.hpp> in the cmcpp namespace and are available for use directly\n\n";

        out << "} // namespace wasm_utils\n";
    }

    static void generateWAMRHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &sampleHeaderFile)
    {
        std::ofstream out(filename);
        if (!out.is_open())
        {
            throw std::runtime_error("Cannot create WAMR header file: " + filename);
        }

        // Header guard
        std::string guard = "GENERATED_WAMR_BINDINGS_HPP";
        out << "#ifndef " << guard << "\n";
        out << "#define " << guard << "\n\n";

        out << "// Generated WAMR helper functions for package: " << packageName << "\n";
        out << "// This header provides utility functions for initializing and using WAMR with Component Model bindings\n\n";

        out << "#include <wamr.hpp>\n";
        out << "#include <cmcpp.hpp>\n";
        out << "#include \"" << sampleHeaderFile << "\"\n\n";
        out << "#include <span>\n";
        out << "#include <stdexcept>\n";
        out << "#include <vector>\n\n";

        // Forward declarations
        out << "// Forward declarations\n";
        out << "struct NativeSymbol;\n";
        out << "struct NativeRegistration {\n";
        out << "    const char* module_name;\n";
        out << "    NativeSymbol* symbols;\n";
        out << "    size_t count;\n";
        out << "};\n\n";

        // Function declarations
        out << "// Get all import interface registrations\n";
        out << "// Returns a vector of all import interfaces that need to be registered with WAMR\n";
        out << "std::vector<NativeRegistration> get_import_registrations();\n\n";

        out << "// Register all import interfaces at once\n";
        out << "// Returns the number of functions registered, or -1 on failure\n";
        out << "int register_all_imports();\n\n";

        out << "// Unregister all import interfaces\n";
        out << "void unregister_all_imports();\n\n";

        // WASM utilities namespace with constants
        out << "// WASM file utilities\n";
        out << "namespace wasm_utils {\n\n";

        out << "// Default WAMR runtime configuration\n";
        out << "extern const uint32_t DEFAULT_STACK_SIZE;\n";
        out << "extern const uint32_t DEFAULT_HEAP_SIZE;\n\n";

        out << "} // namespace wasm_utils\n\n";

        out << "// Note: Helper functions create_guest_realloc() and create_lift_lower_context()\n";
        out << "// are now available directly from <wamr.hpp> in the cmcpp namespace\n\n";

        out << "#endif // " << guard << "\n";
    }

private:
    // Generate type definitions (variants, enums, records)
    static void generateTypeDefinitions(std::ofstream &out, const InterfaceInfo &iface)
    {
        // Generate enums
        for (const auto &enumDef : iface.enums)
        {
            out << "enum class " << sanitize_identifier(enumDef.name) << " {\n";
            for (size_t i = 0; i < enumDef.values.size(); ++i)
            {
                out << "    " << sanitize_identifier(enumDef.values[i]);
                if (i < enumDef.values.size() - 1)
                    out << ",";
                out << "\n";
            }
            out << "};\n\n";
        }

        // Generate variants
        for (const auto &variant : iface.variants)
        {
            out << "using " << sanitize_identifier(variant.name) << " = cmcpp::variant_t<";
            for (size_t i = 0; i < variant.cases.size(); ++i)
            {
                if (i > 0)
                    out << ", ";
                if (variant.cases[i].type.empty())
                {
                    // Case with no payload - use monostate or unit type
                    out << "cmcpp::monostate";
                }
                else
                {
                    out << TypeMapper::mapType(variant.cases[i].type, &iface);
                }
            }
            out << ">;\n\n";
        }

        // Generate records
        for (const auto &record : iface.records)
        {
            out << "struct " << sanitize_identifier(record.name) << " {\n";
            for (const auto &field : record.fields)
            {
                out << "    " << TypeMapper::mapType(field.type, &iface) << " " << sanitize_identifier(field.name) << ";\n";
            }
            out << "};\n\n";
        }
    }

    static void generateFunctionDeclaration(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
    {
        // Determine return type
        std::string return_type = "void";
        if (func.results.size() == 1)
        {
            return_type = TypeMapper::mapType(func.results[0], iface);
        }
        else if (func.results.size() > 1)
        {
            // Multiple results become a tuple
            return_type = "cmcpp::tuple_t<";
            for (size_t i = 0; i < func.results.size(); ++i)
            {
                if (i > 0)
                    return_type += ", ";
                return_type += TypeMapper::mapType(func.results[i], iface);
            }
            return_type += ">";
        }

        // Generate host function declaration
        out << return_type << " " << sanitize_identifier(func.name) << "(";

        // Parameters
        for (size_t i = 0; i < func.parameters.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << TypeMapper::mapType(func.parameters[i].type, iface) << " " << sanitize_identifier(func.parameters[i].name);
        }

        out << ");\n\n";
    }

    static void generateGuestFunctionTypeAlias(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
    {
        // Check if function uses unknown local types (variant/enum/record)
        // Now that we parse these types, we can check if they're actually defined
        auto has_unknown_type = [iface](const std::string &type_str)
        {
            if (!iface)
                return false;

            // Check if the type string contains single lowercase letters that might be local types
            std::string trimmed = type_str;
            trimmed.erase(std::remove_if(trimmed.begin(), trimmed.end(), ::isspace), trimmed.end());

            // Extract potential local type names (single lowercase letters)
            std::vector<std::string> potential_types;

            // Simple heuristic: look for single letter types
            if (trimmed.length() == 1 && std::islower(trimmed[0]))
            {
                potential_types.push_back(trimmed);
            }
            // Check for single letter types inside angle brackets (generic types like list<v>)
            for (size_t i = 0; i + 2 < trimmed.length(); ++i)
            {
                if (trimmed[i] == '<' && std::islower(trimmed[i + 1]) && trimmed[i + 2] == '>')
                {
                    potential_types.push_back(std::string(1, trimmed[i + 1]));
                }
            }

            // Check if any potential local type is NOT defined in the interface
            for (const auto &potential_type : potential_types)
            {
                bool found = false;

                // Check in enums
                for (const auto &enumDef : iface->enums)
                {
                    if (enumDef.name == potential_type)
                    {
                        found = true;
                        break;
                    }
                }

                // Check in variants
                if (!found)
                {
                    for (const auto &variant : iface->variants)
                    {
                        if (variant.name == potential_type)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // Check in records
                if (!found)
                {
                    for (const auto &record : iface->records)
                    {
                        if (record.name == potential_type)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // If not found in any defined types, it's unknown
                if (!found)
                {
                    return true;
                }
            }

            return false;
        };

        bool skip = false;
        for (const auto &param : func.parameters)
        {
            if (has_unknown_type(param.type))
            {
                skip = true;
                break;
            }
        }
        for (const auto &result : func.results)
        {
            if (has_unknown_type(result))
            {
                skip = true;
                break;
            }
        }

        if (skip)
        {
            out << "// TODO: " << func.name << " - Type definitions for local types (variant/enum/record) not yet parsed\n";
            out << "// Will be available in a future update\n\n";
            return;
        }

        // Determine return type
        std::string return_type = "void";
        if (func.results.size() == 1)
        {
            return_type = TypeMapper::mapType(func.results[0], iface);
        }
        else if (func.results.size() > 1)
        {
            // Multiple results become a tuple
            return_type = "cmcpp::tuple_t<";
            for (size_t i = 0; i < func.results.size(); ++i)
            {
                if (i > 0)
                    return_type += ", ";
                return_type += TypeMapper::mapType(func.results[i], iface);
            }
            return_type += ">";
        }

        // Generate only the function signature typedef for guest functions
        // Note: No need to sanitize the function name here since _t suffix prevents keyword conflicts
        std::string type_alias_name = func.name;
        std::replace(type_alias_name.begin(), type_alias_name.end(), '-', '_');
        std::replace(type_alias_name.begin(), type_alias_name.end(), '.', '_');

        out << "// Guest function signature for use with guest_function<" << type_alias_name << "_t>()\n";
        out << "using " << type_alias_name << "_t = " << return_type << "(";

        // Parameters for function signature
        for (size_t i = 0; i < func.parameters.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << TypeMapper::mapType(func.parameters[i].type, iface);
        }

        out << ");\n\n";
    }

    static void generateFunctionImplementation(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
    {
        // Determine return type
        std::string return_type = "void";
        if (func.results.size() == 1)
        {
            return_type = TypeMapper::mapType(func.results[0], iface);
        }
        else if (func.results.size() > 1)
        {
            return_type = "cmcpp::tuple_t<";
            for (size_t i = 0; i < func.results.size(); ++i)
            {
                if (i > 0)
                    return_type += ", ";
                return_type += TypeMapper::mapType(func.results[i], iface);
            }
            return_type += ">";
        }

        out << return_type << " " << sanitize_identifier(func.name) << "(";

        // Parameters
        for (size_t i = 0; i < func.parameters.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << TypeMapper::mapType(func.parameters[i].type, iface) << " " << sanitize_identifier(func.parameters[i].name);
        }

        out << ") {\n";
        out << "    // TODO: Implement " << func.name << "\n";

        if (return_type != "void")
        {
            out << "    return {}; // Return default value\n";
        }

        out << "}\n\n";
    }

    static std::string sanitize_identifier(const std::string &name)
    {
        std::string result = name;
        std::replace(result.begin(), result.end(), '-', '_');
        std::replace(result.begin(), result.end(), '.', '_');
        std::replace(result.begin(), result.end(), ':', '_');
        std::replace(result.begin(), result.end(), '/', '_');

        // Handle C++ keywords by appending underscore
        static const std::set<std::string> keywords = {
            "and", "or", "not", "xor", "bool", "char", "int", "float", "double",
            "void", "return", "if", "else", "while", "for", "do", "switch",
            "case", "default", "break", "continue", "namespace", "class", "struct",
            "enum", "union", "typedef", "using", "public", "private", "protected",
            "virtual", "override", "final", "const", "static", "extern", "inline"};

        if (keywords.find(result) != keywords.end())
        {
            result += "_";
        }

        return result;
    }
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "wit-codegen - WebAssembly Interface Types (WIT) Code Generator\n\n";
        std::cerr << "USAGE:\n";
        std::cerr << "  " << argv[0] << " <wit-file> [output-prefix]\n\n";
        std::cerr << "ARGUMENTS:\n";
        std::cerr << "  <wit-file>       Path to the WIT file to parse\n";
        std::cerr << "  [output-prefix]  Optional output file prefix (default: derived from package name)\n\n";
        std::cerr << "DESCRIPTION:\n";
        std::cerr << "  Generates C++ host function bindings from WebAssembly Interface Types (WIT)\n";
        std::cerr << "  files. The tool parses WIT syntax and generates type-safe C++ code for\n";
        std::cerr << "  interfacing with WebAssembly components.\n\n";
        std::cerr << "GENERATED FILES:\n";
        std::cerr << "  <prefix>.hpp          - C++ header with type definitions and declarations\n";
        std::cerr << "  <prefix>_wamr.hpp     - WAMR runtime integration header\n";
        std::cerr << "  <prefix>_wamr.cpp     - WAMR binding implementation with NativeSymbol arrays\n\n";
        std::cerr << "FEATURES:\n";
        std::cerr << "  - Supports all Component Model types (primitives, strings, lists, records,\n";
        std::cerr << "    variants, enums, options, results, flags)\n";
        std::cerr << "  - Generates bidirectional bindings (imports and exports)\n";
        std::cerr << "  - Type-safe C++ wrappers using cmcpp canonical ABI\n";
        std::cerr << "  - WAMR native function registration helpers\n";
        std::cerr << "  - Automatic memory management for complex types\n\n";
        std::cerr << "EXAMPLES:\n";
        std::cerr << "  # Generate bindings using package-derived prefix\n";
        std::cerr << "  " << argv[0] << " example.wit\n\n";
        std::cerr << "  # Generate bindings with custom prefix\n";
        std::cerr << "  " << argv[0] << " example.wit my_bindings\n\n";
        std::cerr << "For more information, see: https://github.com/GordonSmith/component-model-cpp\n";
        return 1;
    }

    std::string witFile = argv[1];
    std::string outputPrefix = argc >= 3 ? argv[2] : "";

    try
    {
        std::cout << "Parsing WIT file with ANTLR grammar: " << witFile << std::endl;

        // Parse WIT file using ANTLR grammar
        auto parseResult = WitGrammarParser::parse(witFile);

        if (parseResult.interfaces.empty())
        {
            std::cerr << "Warning: No interfaces found in " << witFile << std::endl;
            return 1;
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

                std::cout << "Using package-derived output prefix: " << outputPrefix << std::endl;
            }

            if (outputPrefix.empty())
            {
                outputPrefix = "generated";
                std::cout << "No package name found, using default prefix: " << outputPrefix << std::endl;
            }
        }

        std::cout << "Found " << parseResult.interfaces.size() << " interface(s)" << std::endl;
        for (const auto &iface : parseResult.interfaces)
        {
            std::cout << "  - " << iface.name << " (" << iface.functions.size() << " functions)" << std::endl;
        }

        // Generate files
        std::string headerFile = outputPrefix + ".hpp";
        std::string implFile = outputPrefix + ".cpp";
        std::string wamrBindingsFile = outputPrefix + "_wamr.cpp";
        std::string wamrHeaderFile = outputPrefix + "_wamr.hpp";

        std::cout << "\nGenerating files:" << std::endl;
        std::cout << "  " << headerFile << std::endl;
        CodeGenerator::generateHeader(parseResult.interfaces, headerFile);

        std::cout << "  " << wamrHeaderFile << std::endl;
        CodeGenerator::generateWAMRHeader(parseResult.interfaces, wamrHeaderFile, parseResult.packageName, headerFile);

        std::cout << "  " << wamrBindingsFile << std::endl;
        CodeGenerator::generateWAMRBindings(parseResult.interfaces, wamrBindingsFile, parseResult.packageName, headerFile, wamrHeaderFile);

        std::cout << "\nCode generation complete!" << std::endl;
        std::cout << "Note: Host function implementations should be provided by the host application." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
