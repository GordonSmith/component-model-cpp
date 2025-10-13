#pragma once

#include <string>
#include <vector>
#include <set>
#include <optional>
#include "types.hpp"
#include "package_registry.hpp" // Need full definition for std::optional<PackageId>

// Result of parsing a WIT file
struct ParseResult
{
    std::vector<InterfaceInfo> interfaces;
    std::string packageName;
    std::set<std::string> worldImports;
    std::set<std::string> worldExports;
    bool hasWorld = false;

    // NEW: External package dependencies referenced in this file
    // e.g., "my:dep@0.1.0", "wasi:clocks@0.3.0"
    std::set<std::string> external_dependencies;

    // NEW: Structured package ID (parsed from packageName)
    std::optional<PackageId> package_id;
};

// Parse WIT file using ANTLR grammar
class WitGrammarParser
{
public:
    static ParseResult parse(const std::string &filename);
};
