#pragma once

#include <string>
#include <vector>
#include <map>

// Data structures for WIT code generation

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

struct TypeAliasDef
{
    std::string name;
    std::string target_type;
};

struct FlagsDef
{
    std::string name;
    std::vector<std::string> flags;
};

struct ResourceDef
{
    std::string name;
    // For now, we don't need to track methods/constructors as we're just generating type aliases
};

struct UseStatement
{
    std::string source_interface;                    // Interface being imported from (e.g., "f" or "poll")
    std::string source_package;                      // Package name if cross-package (e.g., "wasi:poll")
    std::vector<std::string> imported_types;         // Types being imported (e.g., ["fd"])
    std::map<std::string, std::string> type_renames; // Renamed types: original_name -> new_name (e.g., {"t2": "t2-renamed"})
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
    std::vector<TypeAliasDef> type_aliases;
    std::vector<FlagsDef> flags;
    std::vector<ResourceDef> resources;
    std::vector<UseStatement> use_statements;
    bool is_standalone_function = false; // True if this is a synthetic interface for a world-level function
    bool is_world_level = false;         // True if this contains world-level types (no namespace wrapping)
};
