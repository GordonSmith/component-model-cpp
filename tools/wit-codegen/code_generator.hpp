#pragma once

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include "types.hpp"

// Forward declarations
class PackageRegistry;

// Code generator for C++ host functions
class CodeGenerator
{
public:
    static void generateHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const PackageRegistry *registry = nullptr, const std::set<std::string> *external_deps = nullptr, const std::set<std::string> *world_imports = nullptr, const std::set<std::string> *world_exports = nullptr);

    // NOTE: This function generates stub implementations but is intentionally not called.
    // Host applications are responsible for providing their own implementations of the
    // declared functions. This function is kept for reference or optional stub generation.
    static void generateImplementation(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &headerName);

    static void generateWAMRBindings(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &headerFile, const std::string &wamrHeaderFile, const PackageRegistry *registry = nullptr, const std::set<std::string> *world_imports = nullptr, const std::set<std::string> *world_exports = nullptr);

    static void generateWAMRHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &sampleHeaderFile, const PackageRegistry *registry = nullptr, const std::set<std::string> *world_imports = nullptr, const std::set<std::string> *world_exports = nullptr);

private:
    // Generate guest function wrappers for exports
    static void generateGuestFunctionWrappers(std::ofstream &out, const std::vector<InterfaceInfo> &interfaces, const std::string &packageName);
    // Helper to extract type names from a WIT type string
    static void extractTypeDependencies(const std::string &witType, const InterfaceInfo &iface, std::set<std::string> &deps);

    // NEW: Generate external package stub declarations
    static void generateExternalPackageStubs(std::ofstream &out, const std::vector<InterfaceInfo> &interfaces, const PackageRegistry *registry, const std::set<std::string> *external_deps);

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

    static bool functionNameConflictsWithType(const InterfaceInfo *iface, const std::string &sanitizedName);
    static std::string getSanitizedFunctionName(const FunctionSignature &func, const InterfaceInfo *iface);

    static void generateFunctionDeclaration(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface);

    static void generateGuestFunctionTypeAlias(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface);

    static void generateFunctionImplementation(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface);
};
