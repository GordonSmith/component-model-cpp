#pragma once

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include "types.hpp"

// Code generator for C++ host functions
class CodeGenerator
{
public:
    static void generateHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename);

    // NOTE: This function generates stub implementations but is intentionally not called.
    // Host applications are responsible for providing their own implementations of the
    // declared functions. This function is kept for reference or optional stub generation.
    static void generateImplementation(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &headerName);

    static void generateWAMRBindings(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &headerFile, const std::string &wamrHeaderFile);

    static void generateWAMRHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &sampleHeaderFile);

private:
    // Helper to extract type names from a WIT type string
    static void extractTypeDependencies(const std::string &witType, const InterfaceInfo &iface, std::set<std::string> &deps);

    // Unified topological sort for all user-defined types (variants, records, type aliases)
    struct TypeDef
    {
        enum class Kind
        {
            Variant,
            Record,
            TypeAlias
        };
        Kind kind;
        std::string name;
        size_t originalIndex; // Track original position
    };

    static std::vector<TypeDef> sortTypesByDependencies(const InterfaceInfo &iface);

    // Legacy function kept for compatibility - now just filters unified sort
    static std::vector<VariantDef> sortVariantsByDependencies(const std::vector<VariantDef> &variants, const InterfaceInfo &iface);

    // Topologically sort records based on dependencies
    static std::vector<RecordDef> sortRecordsByDependencies(const std::vector<RecordDef> &records, const InterfaceInfo &iface);

    // Generate type definitions (variants, enums, records)
    static void generateTypeDefinitions(std::ofstream &out, const InterfaceInfo &iface);

    static void generateFunctionDeclaration(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface);

    static void generateGuestFunctionTypeAlias(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface);

    static void generateFunctionImplementation(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface);
};
