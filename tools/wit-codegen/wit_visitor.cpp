#include "wit_visitor.hpp"
#include "utils.hpp"
#include "types.hpp"

// Constructor
WitInterfaceVisitor::WitInterfaceVisitor(std::vector<InterfaceInfo> &ifaces) : interfaces(ifaces) {}

// Helper to get or create world-level types interface
InterfaceInfo *WitInterfaceVisitor::getWorldLevelTypes()
{
    // Find existing _world_types interface
    for (auto &iface : interfaces)
    {
        if (iface.name == "_world_types")
        {
            return &iface;
        }
    }
    // Create new one
    interfaces.emplace_back();
    InterfaceInfo *wlt = &interfaces.back();
    wlt->name = "_world_types";
    wlt->package_name = currentPackage;
    wlt->is_world_level = true;
    wlt->kind = InterfaceKind::Export;
    return wlt;
}

antlrcpp::Any WitInterfaceVisitor::visitPackageDecl(WitParser::PackageDeclContext *ctx)
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
antlrcpp::Any WitInterfaceVisitor::visitImportItem(WitParser::ImportItemContext *ctx)
{
    // Check for "import id : externType" pattern (function import or inline interface)
    if (ctx->id() && ctx->externType())
    {
        std::string importName = extract_identifier(ctx->id());

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
        }
        else if (ctx->externType()->interfaceItems().size() > 0)
        {
            // This is an inline interface import with a body
            importedInterfaces.insert(importName);

            // Create an InterfaceInfo for this inline interface
            interfaces.emplace_back();
            InterfaceInfo *savedInterface = currentInterface;
            currentInterface = &interfaces.back();
            currentInterface->name = importName;
            currentInterface->package_name = currentPackage;
            currentInterface->kind = InterfaceKind::Import;

            // Process the interface body
            for (auto *item : ctx->externType()->interfaceItems())
            {
                visit(item);
            }

            currentInterface = savedInterface;
            // Don't call visitChildren - we already processed the interface items
            return nullptr;
        }
        else
        {
            // This is an inline interface import without body (shouldn't happen)
            importedInterfaces.insert(importName);
        }
    }
    // Check for "import usePath ;" pattern (interface import)
    else if (ctx->usePath())
    {
        std::string interfaceName = ctx->usePath()->getText();
        importedInterfaces.insert(interfaceName);
    }
    return visitChildren(ctx);
}

// Visit export items in world to track which interfaces to generate
antlrcpp::Any WitInterfaceVisitor::visitExportItem(WitParser::ExportItemContext *ctx)
{
    // Check for "export id : externType" pattern (function export or inline interface)
    if (ctx->id() && ctx->externType())
    {
        std::string exportName = extract_identifier(ctx->id());

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
        }
        else if (ctx->externType()->interfaceItems().size() > 0)
        {
            // This is an inline interface export with a body
            exportedInterfaces.insert(exportName);

            // Create an InterfaceInfo for this inline interface
            interfaces.emplace_back();
            InterfaceInfo *savedInterface = currentInterface;
            currentInterface = &interfaces.back();
            currentInterface->name = exportName;
            currentInterface->package_name = currentPackage;
            currentInterface->kind = InterfaceKind::Export;

            // Process the interface body
            for (auto *item : ctx->externType()->interfaceItems())
            {
                visit(item);
            }

            currentInterface = savedInterface;
            // Don't call visitChildren - we already processed the interface items
            return nullptr;
        }
        else
        {
            // This is an inline interface export without body (shouldn't happen)
            exportedInterfaces.insert(exportName);
        }
    }
    // Check for "export usePath ;" pattern (interface export)
    else if (ctx->usePath())
    {
        std::string interfaceName = ctx->usePath()->getText();
        exportedInterfaces.insert(interfaceName);
    }
    return visitChildren(ctx);
}

// Helper to parse funcType into a FunctionSignature
void WitInterfaceVisitor::parseFuncType(WitParser::FuncTypeContext *funcType, FunctionSignature &func)
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
                param.name = extract_identifier(namedType->id());
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

antlrcpp::Any WitInterfaceVisitor::visitInterfaceItem(WitParser::InterfaceItemContext *ctx)
{
    // Save the previous interface context
    InterfaceInfo *savedInterface = currentInterface;

    if (ctx->id())
    {
        std::string interfaceName = extract_identifier(ctx->id());

        // Only add interface if it's imported (or if we haven't seen any world yet)
        interfaces.emplace_back();
        currentInterface = &interfaces.back();
        currentInterface->name = interfaceName;
        currentInterface->package_name = currentPackage;
    }

    auto result = visitChildren(ctx);

    // Restore the previous interface context
    currentInterface = savedInterface;

    return result;
}

antlrcpp::Any WitInterfaceVisitor::visitFuncItem(WitParser::FuncItemContext *ctx)
{
    if (!currentInterface)
    {
        return nullptr;
    }

    FunctionSignature func;
    func.interface_name = currentInterface->name;
    func.resource_name = currentResource; // Set resource name if we're in a resource

    // Get function name (id before ':')
    if (ctx->id())
    {
        func.name = extract_identifier(ctx->id());
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
antlrcpp::Any WitInterfaceVisitor::visitVariantItems(WitParser::VariantItemsContext *ctx)
{
    if (!ctx->id())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    VariantDef variant;
    variant.name = extract_identifier(ctx->id());

    // Parse variant cases recursively
    if (ctx->variantCases())
    {
        parseVariantCases(ctx->variantCases(), variant);
    }

    targetInterface->variants.push_back(variant);
    return visitChildren(ctx);
}

// Helper to recursively parse variant cases
void WitInterfaceVisitor::parseVariantCases(WitParser::VariantCasesContext *ctx, VariantDef &variant)
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
            vcase.name = extract_identifier(variantCase->id());

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
antlrcpp::Any WitInterfaceVisitor::visitEnumItems(WitParser::EnumItemsContext *ctx)
{
    if (!ctx->id())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    EnumDef enumDef;
    enumDef.name = extract_identifier(ctx->id());

    // Parse enum values recursively
    if (ctx->enumCases())
    {
        parseEnumCases(ctx->enumCases(), enumDef);
    }

    targetInterface->enums.push_back(enumDef);
    return visitChildren(ctx);
}

// Helper to recursively parse enum cases
void WitInterfaceVisitor::parseEnumCases(WitParser::EnumCasesContext *ctx, EnumDef &enumDef)
{
    if (!ctx)
        return;

    // Get the first enum case
    if (ctx->id())
    {
        enumDef.values.push_back(extract_identifier(ctx->id()));
    }

    // Recursively parse remaining cases
    if (ctx->enumCases())
    {
        parseEnumCases(ctx->enumCases(), enumDef);
    }
}

// Visit flags type definitions
antlrcpp::Any WitInterfaceVisitor::visitFlagsItems(WitParser::FlagsItemsContext *ctx)
{
    if (!ctx->id())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    FlagsDef flagsDef;
    flagsDef.name = extract_identifier(ctx->id());

    // Parse flags fields recursively
    if (ctx->flagsFields())
    {
        parseFlagsFields(ctx->flagsFields(), flagsDef);
    }

    targetInterface->flags.push_back(flagsDef);
    return visitChildren(ctx);
}

// Helper to recursively parse flags fields
void WitInterfaceVisitor::parseFlagsFields(WitParser::FlagsFieldsContext *ctx, FlagsDef &flagsDef)
{
    if (!ctx)
        return;

    // Get the first flag field
    if (ctx->id())
    {
        flagsDef.flags.push_back(extract_identifier(ctx->id()));
    }

    // Recursively parse remaining fields
    if (ctx->flagsFields())
    {
        parseFlagsFields(ctx->flagsFields(), flagsDef);
    }
}

// Visit resource type definitions
antlrcpp::Any WitInterfaceVisitor::visitResourceItem(WitParser::ResourceItemContext *ctx)
{
    if (!ctx->id())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    ResourceDef resourceDef;
    resourceDef.name = extract_identifier(ctx->id());

    targetInterface->resources.push_back(resourceDef);

    // Track current resource and visit children (methods)
    std::string previousResource = currentResource;
    currentResource = resourceDef.name;
    visitChildren(ctx);
    currentResource = previousResource;

    return nullptr;
}

// Visit record type definitions
antlrcpp::Any WitInterfaceVisitor::visitRecordItem(WitParser::RecordItemContext *ctx)
{
    if (!ctx->id())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    RecordDef record;
    record.name = extract_identifier(ctx->id());

    // Parse record fields recursively
    if (ctx->recordFields())
    {
        parseRecordFields(ctx->recordFields(), record);
    }

    targetInterface->records.push_back(record);
    return visitChildren(ctx);
}

// Helper to recursively parse record fields
void WitInterfaceVisitor::parseRecordFields(WitParser::RecordFieldsContext *ctx, RecordDef &record)
{
    if (!ctx)
        return;

    // Get the first record field
    if (ctx->recordField())
    {
        auto field = ctx->recordField();
        if (field->id() && field->ty())
        {
            RecordField rfield;
            rfield.name = extract_identifier(field->id());
            rfield.type = field->ty()->getText();
            record.fields.push_back(rfield);
        }
    }

    // Recursively parse remaining fields
    if (ctx->recordFields())
    {
        parseRecordFields(ctx->recordFields(), record);
    }
}

// Visit type alias definitions
antlrcpp::Any WitInterfaceVisitor::visitTypeItem(WitParser::TypeItemContext *ctx)
{
    if (!ctx->id() || !ctx->ty())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    TypeAliasDef typeAlias;
    typeAlias.name = extract_identifier(ctx->id());
    typeAlias.target_type = ctx->ty()->getText();

    targetInterface->type_aliases.push_back(typeAlias);
    return visitChildren(ctx);
}

// Visit use statements
antlrcpp::Any WitInterfaceVisitor::visitUseItem(WitParser::UseItemContext *ctx)
{
    if (!ctx->usePath() || !ctx->useNamesList())
    {
        return visitChildren(ctx);
    }

    // If not in an interface, create/use world-level types interface
    InterfaceInfo *targetInterface = currentInterface;
    if (!targetInterface)
    {
        targetInterface = getWorldLevelTypes();
    }

    UseStatement useStmt;

    // Extract the source interface name from usePath (e.g., "poll" from "wasi:poll/poll.{pollable}")
    std::string pathText = ctx->usePath()->getText();

    // Check if this is a cross-package reference (contains ':')
    size_t colonPos = pathText.find(':');
    if (colonPos != std::string::npos)
    {
        // Cross-package: "wasi:poll/poll" or "my:dep/a@0.1.0" -> extract package with version
        size_t slashPos = pathText.find('/', colonPos);
        if (slashPos != std::string::npos)
        {
            // Format: "namespace:package/interface@version"
            std::string interfacePart = pathText.substr(slashPos + 1);

            // Check if version is present after interface name
            size_t atPos = interfacePart.find('@');
            if (atPos != std::string::npos)
            {
                // Version present: "a@0.1.0"
                useStmt.source_interface = interfacePart.substr(0, atPos);
                std::string version = interfacePart.substr(atPos);               // "@0.1.0"
                useStmt.source_package = pathText.substr(0, slashPos) + version; // "my:dep@0.1.0"
            }
            else
            {
                // No version: "a"
                useStmt.source_interface = interfacePart;
                useStmt.source_package = pathText.substr(0, slashPos); // "my:dep"
            }
        }
        else
        {
            // No slash, just package:interface (e.g., "foo:bar")
            useStmt.source_package = pathText.substr(0, colonPos);
            std::string interfacePart = pathText.substr(colonPos + 1);

            // Strip version if present
            size_t atPos = interfacePart.find('@');
            if (atPos != std::string::npos)
            {
                useStmt.source_interface = interfacePart.substr(0, atPos);
                // Add version to package
                useStmt.source_package += interfacePart.substr(atPos);
            }
            else
            {
                useStmt.source_interface = interfacePart;
            }
        }
    }
    else
    {
        // Same-package reference: just "f" or "types-interface"
        useStmt.source_interface = pathText;
    }

    // Extract imported type names
    for (auto nameItem : ctx->useNamesList()->useNamesItem())
    {
        if (nameItem->id().size() > 0)
        {
            std::string originalName = extract_identifier(nameItem->id(0));
            useStmt.imported_types.push_back(originalName);

            // Check for "as new_name" renaming syntax
            if (nameItem->id().size() > 1)
            {
                std::string renamedName = extract_identifier(nameItem->id(1));
                useStmt.type_renames[originalName] = renamedName;
            }
        }
    }

    targetInterface->use_statements.push_back(useStmt);
    return visitChildren(ctx);
}

// Get the set of imported interfaces from the world
const std::set<std::string> &WitInterfaceVisitor::getImportedInterfaces() const
{
    return importedInterfaces;
}

// Get the set of exported interfaces from the world
const std::set<std::string> &WitInterfaceVisitor::getExportedInterfaces() const
{
    return exportedInterfaces;
}

// Get standalone functions from the world
const std::vector<FunctionSignature> &WitInterfaceVisitor::getStandaloneFunctions() const
{
    return standaloneFunctions;
}

// Get the package name from the WIT file
const std::string &WitInterfaceVisitor::getPackageName() const
{
    return currentPackage;
}
