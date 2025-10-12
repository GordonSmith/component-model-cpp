#pragma once

#include <string>
#include <vector>
#include <set>
#include "types.hpp"

// Result of parsing a WIT file
struct ParseResult
{
    std::vector<InterfaceInfo> interfaces;
    std::string packageName;
    std::set<std::string> worldImports;
    std::set<std::string> worldExports;
    bool hasWorld = false;
};

// Parse WIT file using ANTLR grammar
class WitGrammarParser
{
public:
    static ParseResult parse(const std::string &filename);
};
