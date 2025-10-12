#pragma once

#include <string>
#include <set>
#include <algorithm>
#include <cctype>
#include <antlr4-runtime.h>

// Utility functions for WIT code generation

// Convert WIT identifiers to valid C++ identifiers
inline std::string sanitize_identifier(const std::string &name)
{
    std::string result = name;
    std::replace(result.begin(), result.end(), '-', '_');
    std::replace(result.begin(), result.end(), '.', '_');
    std::replace(result.begin(), result.end(), ':', '_');
    std::replace(result.begin(), result.end(), '/', '_');
    std::replace(result.begin(), result.end(), '%', '_');
    std::replace(result.begin(), result.end(), '@', '_');
    std::replace(result.begin(), result.end(), '#', '_');
    std::replace(result.begin(), result.end(), '$', '_');
    std::replace(result.begin(), result.end(), '!', '_');
    std::replace(result.begin(), result.end(), '&', '_');
    std::replace(result.begin(), result.end(), '*', '_');
    std::replace(result.begin(), result.end(), '(', '_');
    std::replace(result.begin(), result.end(), ')', '_');
    std::replace(result.begin(), result.end(), '+', '_');
    std::replace(result.begin(), result.end(), '=', '_');
    std::replace(result.begin(), result.end(), '[', '_');
    std::replace(result.begin(), result.end(), ']', '_');
    std::replace(result.begin(), result.end(), '{', '_');
    std::replace(result.begin(), result.end(), '}', '_');
    std::replace(result.begin(), result.end(), '|', '_');
    std::replace(result.begin(), result.end(), '\\', '_');
    std::replace(result.begin(), result.end(), ';', '_');
    std::replace(result.begin(), result.end(), '\'', '_');
    std::replace(result.begin(), result.end(), '"', '_');
    std::replace(result.begin(), result.end(), '<', '_');
    std::replace(result.begin(), result.end(), '>', '_');
    std::replace(result.begin(), result.end(), ',', '_');
    std::replace(result.begin(), result.end(), '?', '_');
    std::replace(result.begin(), result.end(), '~', '_');
    std::replace(result.begin(), result.end(), '`', '_');
    std::replace(result.begin(), result.end(), ' ', '_');

    // Handle C++ keywords by appending underscore
    static const std::set<std::string> keywords = {
        "and", "or", "not", "xor", "bool", "char", "int", "float", "double",
        "void", "return", "if", "else", "while", "for", "do", "switch",
        "case", "default", "break", "continue", "namespace", "class", "struct",
        "enum", "union", "typedef", "using", "public", "private", "protected",
        "virtual", "override", "final", "const", "static", "extern", "inline",
        "explicit", "mutable", "volatile", "constexpr", "noexcept", "nullptr",
        "this", "new", "delete", "operator", "sizeof", "alignof", "decltype",
        "auto", "template", "typename", "try", "catch", "throw", "friend",
        "goto", "asm", "register", "signed", "unsigned", "short", "long",
        "errno"}; // errno is a system macro from <errno.h>

    if (keywords.find(result) != keywords.end())
    {
        result += "_";
    }

    return result;
}

// Extract identifier from WIT id context, stripping leading % if present
inline std::string extract_identifier(antlr4::tree::ParseTree *idCtx)
{
    if (!idCtx)
        return "";

    std::string text = idCtx->getText();
    // Strip leading % used for escaping keywords/special identifiers in WIT
    if (!text.empty() && text[0] == '%')
    {
        return text.substr(1);
    }
    return text;
}

// Trim whitespace from both ends of a string
inline std::string trim(const std::string &str)
{
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch)
                                  { return std::isspace(ch); });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch)
                                { return std::isspace(ch); })
                   .base();
    return (start < end) ? std::string(start, end) : std::string();
}

// Extract content between matching angle brackets
// For "list<tuple<a, b>>" returns "tuple<a, b>"
// Handles nested brackets correctly
inline std::string extract_template_content(const std::string &type)
{
    size_t start = type.find('<');
    if (start == std::string::npos)
    {
        return "";
    }

    start++; // Move past the opening '<'
    int depth = 1;
    size_t i = start;

    while (i < type.length() && depth > 0)
    {
        if (type[i] == '<')
        {
            depth++;
        }
        else if (type[i] == '>')
        {
            depth--;
        }
        i++;
    }

    if (depth == 0)
    {
        // i is now one past the matching '>'
        return type.substr(start, i - start - 1);
    }

    // Unmatched brackets - return empty
    return "";
}

// Split a string by commas, respecting nested angle brackets
// For "a, list<b, c>, d" returns ["a", "list<b, c>", "d"]
inline std::vector<std::string> split_respecting_brackets(const std::string &str)
{
    std::vector<std::string> result;
    std::string current;
    int depth = 0;

    for (char ch : str)
    {
        if (ch == '<')
        {
            depth++;
            current += ch;
        }
        else if (ch == '>')
        {
            depth--;
            current += ch;
        }
        else if (ch == ',' && depth == 0)
        {
            result.push_back(trim(current));
            current.clear();
        }
        else
        {
            current += ch;
        }
    }

    if (!current.empty())
    {
        result.push_back(trim(current));
    }

    return result;
}
