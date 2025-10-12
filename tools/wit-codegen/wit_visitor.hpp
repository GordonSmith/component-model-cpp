#pragma once

#include <string>
#include <vector>
#include <set>
#include <antlr4-runtime.h>
#include "grammar/WitParser.h"
#include "grammar/WitBaseVisitor.h"
#include "types.hpp"
#include "utils.hpp"

// WIT Interface visitor that extracts functions, types (variants, enums, records, flags), resources, and use statements
// from WIT files using ANTLR grammar. Handles import/export items and provides helpers for parsing various WIT constructs.
class WitInterfaceVisitor : public WitBaseVisitor
{
private:
    std::vector<InterfaceInfo> &interfaces;
    InterfaceInfo *currentInterface = nullptr;
    std::string currentPackage;
    std::set<std::string> importedInterfaces;           // Track which interfaces are imported in the world
    std::set<std::string> exportedInterfaces;           // Track which interfaces are exported in the world
    std::vector<FunctionSignature> standaloneFunctions; // Standalone functions from world (not in interfaces)

    // Helper to get or create world-level types interface
    InterfaceInfo *getWorldLevelTypes();

    // Helper to parse funcType into a FunctionSignature
    void parseFuncType(WitParser::FuncTypeContext *funcType, FunctionSignature &func);

    // Helper to recursively parse variant cases
    void parseVariantCases(WitParser::VariantCasesContext *ctx, VariantDef &variant);

    // Helper to recursively parse enum cases
    void parseEnumCases(WitParser::EnumCasesContext *ctx, EnumDef &enumDef);

    // Helper to recursively parse flags fields
    void parseFlagsFields(WitParser::FlagsFieldsContext *ctx, FlagsDef &flagsDef);

    // Helper to recursively parse record fields
    void parseRecordFields(WitParser::RecordFieldsContext *ctx, RecordDef &record);

public:
    WitInterfaceVisitor(std::vector<InterfaceInfo> &ifaces);

    antlrcpp::Any visitPackageDecl(WitParser::PackageDeclContext *ctx) override;
    antlrcpp::Any visitImportItem(WitParser::ImportItemContext *ctx) override;
    antlrcpp::Any visitExportItem(WitParser::ExportItemContext *ctx) override;
    antlrcpp::Any visitInterfaceItem(WitParser::InterfaceItemContext *ctx) override;
    antlrcpp::Any visitFuncItem(WitParser::FuncItemContext *ctx) override;
    antlrcpp::Any visitVariantItems(WitParser::VariantItemsContext *ctx) override;
    antlrcpp::Any visitEnumItems(WitParser::EnumItemsContext *ctx) override;
    antlrcpp::Any visitFlagsItems(WitParser::FlagsItemsContext *ctx) override;
    antlrcpp::Any visitResourceItem(WitParser::ResourceItemContext *ctx) override;
    antlrcpp::Any visitRecordItem(WitParser::RecordItemContext *ctx) override;
    antlrcpp::Any visitTypeItem(WitParser::TypeItemContext *ctx) override;
    antlrcpp::Any visitUseItem(WitParser::UseItemContext *ctx) override;

    // Get the set of imported interfaces from the world
    const std::set<std::string> &getImportedInterfaces() const;

    // Get the set of exported interfaces from the world
    const std::set<std::string> &getExportedInterfaces() const;

    // Get standalone functions from the world
    const std::vector<FunctionSignature> &getStandaloneFunctions() const;

    // Get the package name from the WIT file
    const std::string &getPackageName() const;
};
