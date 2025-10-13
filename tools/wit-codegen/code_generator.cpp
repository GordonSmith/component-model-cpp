#include "code_generator.hpp"
#include "type_mapper.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <sstream>

void CodeGenerator::generateHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename)
{
    // Set interfaces for TypeMapper to resolve cross-namespace references
    TypeMapper::setInterfaces(&interfaces);

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

    // Helper lambda to topologically sort interfaces based on use dependencies
    auto sort_by_dependencies = [](std::vector<const InterfaceInfo *> &ifaces)
    {
        // Build dependency map
        std::map<std::string, std::set<std::string>> deps;
        std::map<std::string, const InterfaceInfo *> name_to_iface;

        for (const auto *iface : ifaces)
        {
            name_to_iface[iface->name] = iface;
            std::set<std::string> iface_deps;

            // Extract dependencies from use statements
            for (const auto &useStmt : iface->use_statements)
            {
                // Only track dependencies to other interfaces in the same list
                if (name_to_iface.count(useStmt.source_interface) ||
                    std::any_of(ifaces.begin(), ifaces.end(),
                                [&](const InterfaceInfo *i)
                                { return i->name == useStmt.source_interface; }))
                {
                    iface_deps.insert(useStmt.source_interface);
                }
            }
            deps[iface->name] = iface_deps;
        }

        // Topological sort using DFS
        std::vector<const InterfaceInfo *> sorted;
        std::set<std::string> visited;
        std::set<std::string> visiting;

        std::function<void(const std::string &)> visit = [&](const std::string &name)
        {
            if (visited.count(name))
                return;
            if (visiting.count(name))
            {
                // Cyclic dependency - just continue
                return;
            }

            visiting.insert(name);

            // Visit dependencies first
            if (deps.count(name))
            {
                for (const auto &dep : deps[name])
                {
                    if (name_to_iface.count(dep))
                    {
                        visit(dep);
                    }
                }
            }

            visiting.erase(name);
            visited.insert(name);

            if (name_to_iface.count(name))
            {
                sorted.push_back(name_to_iface[name]);
            }
        };

        // Visit all interfaces
        for (const auto *iface : ifaces)
        {
            visit(iface->name);
        }

        ifaces = sorted;
    };

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

    // Sort interfaces by use dependencies
    sort_by_dependencies(imports);
    sort_by_dependencies(exports);

    // ====================================================================================
    // PHASE 1: Generate TYPE DEFINITIONS for both namespaces
    // This ensures ALL types (guest + host) are defined before ANY functions reference them
    // ====================================================================================

    // Phase 1a: Generate guest namespace with ONLY type definitions
    if (!exports.empty())
    {
        out << "// Phase 1: Type definitions\n";
        out << "namespace guest {\n\n";

        for (const auto *iface : exports)
        {
            // Skip standalone functions in type generation phase
            if (iface->is_standalone_function)
            {
                continue;
            }

            // Regular interface - create namespace (unless it's world-level types)
            if (!iface->is_world_level)
            {
                out << "// Interface: " << iface->name << "\n";
                if (!iface->package_name.empty())
                {
                    out << "// Package: " << iface->package_name << "\n";
                }
                out << "namespace " << sanitize_identifier(iface->name) << " {\n\n";
            }
            else
            {
                out << "// World-level types\n";
                if (!iface->package_name.empty())
                {
                    out << "// Package: " << iface->package_name << "\n";
                }
                // Add using directives for all non-world interfaces so world types can reference them
                for (const auto *otherIface : exports)
                {
                    if (otherIface != iface && !otherIface->is_world_level && !otherIface->is_standalone_function)
                    {
                        out << "using namespace " << sanitize_identifier(otherIface->name) << ";\n";
                    }
                }
                if (!exports.empty())
                {
                    out << "\n";
                }
            }

            // Generate ONLY type definitions (no functions yet)
            generateTypeDefinitions(out, *iface);

            // Close namespace (unless world-level)
            if (!iface->is_world_level)
            {
                out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
            }
        }

        out << "} // namespace guest\n\n";
    }

    // Phase 1b: Generate host namespace with ONLY type definitions
    if (!imports.empty())
    {
        out << "namespace host {\n\n";

        for (const auto *iface : imports)
        {
            // Skip standalone functions in type generation phase
            if (iface->is_standalone_function)
            {
                continue;
            }

            // Regular interface - create namespace
            out << "// Interface: " << iface->name << "\n";
            if (!iface->package_name.empty())
            {
                out << "// Package: " << iface->package_name << "\n";
            }
            out << "namespace " << sanitize_identifier(iface->name) << " {\n\n";

            // Generate ONLY type definitions (no functions yet)
            generateTypeDefinitions(out, *iface);

            out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
        }

        out << "} // namespace host\n\n";
    }

    // ====================================================================================
    // PHASE 2: Generate FUNCTION DEFINITIONS for both namespaces
    // Now all types are defined, so functions can safely reference cross-namespace types
    // ====================================================================================

    // Phase 2a: Reopen guest namespace and generate function definitions
    if (!exports.empty())
    {
        out << "// Phase 2: Function definitions\n";
        out << "namespace guest {\n\n";

        for (const auto *iface : exports)
        {
            // Handle standalone functions
            if (iface->is_standalone_function)
            {
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
            else if (!iface->is_world_level)
            {
                // Regular interface with functions
                out << "// Interface: " << iface->name << "\n";
                out << "namespace " << sanitize_identifier(iface->name) << " {\n\n";

                // Generate using declarations for same-namespace imports
                if (!iface->use_statements.empty())
                {
                    bool hasUsings = false;
                    for (const auto &useStmt : iface->use_statements)
                    {
                        // Skip cross-package use statements (we don't have their definitions)
                        if (!useStmt.source_package.empty())
                        {
                            continue;
                        }

                        // Skip cross-interface use statements (types defined in other files)
                        if (useStmt.source_interface != iface->name)
                        {
                            continue;
                        }

                        // Find the source interface to determine its namespace
                        const InterfaceInfo *sourceIface = nullptr;
                        for (const auto &other : interfaces)
                        {
                            if (other.name == useStmt.source_interface)
                            {
                                sourceIface = &other;
                                break;
                            }
                        }

                        // Only generate using declarations for same-namespace imports
                        // Cross-namespace references will use fully qualified names
                        if (!sourceIface || sourceIface->kind == iface->kind)
                        {
                            for (const auto &typeName : useStmt.imported_types)
                            {
                                out << "using " << sanitize_identifier(typeName) << " = "
                                    << sanitize_identifier(useStmt.source_interface) << "::"
                                    << sanitize_identifier(typeName) << ";\n";
                                hasUsings = true;
                            }
                        }
                    }
                    if (hasUsings)
                    {
                        out << "\n";
                    }
                }

                // Generate function type aliases
                for (const auto &func : iface->functions)
                {
                    generateGuestFunctionTypeAlias(out, func, iface);
                }

                out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
            }
        }

        out << "} // namespace guest\n\n";
    }

    // Phase 2b: Reopen host namespace and generate function definitions
    if (!imports.empty())
    {
        out << "namespace host {\n\n";

        for (const auto *iface : imports)
        {
            // Handle standalone functions
            if (iface->is_standalone_function)
            {
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
                // Regular interface with functions
                out << "// Interface: " << iface->name << "\n";
                out << "namespace " << sanitize_identifier(iface->name) << " {\n\n";

                // Generate using declarations for same-namespace imports
                if (!iface->use_statements.empty())
                {
                    bool hasUsings = false;
                    for (const auto &useStmt : iface->use_statements)
                    {
                        // Skip cross-package use statements (we don't have their definitions)
                        if (!useStmt.source_package.empty())
                        {
                            continue;
                        }

                        // Skip cross-interface use statements (types defined in other files)
                        if (useStmt.source_interface != iface->name)
                        {
                            continue;
                        }

                        // Find the source interface to determine its namespace
                        const InterfaceInfo *sourceIface = nullptr;
                        for (const auto &other : interfaces)
                        {
                            if (other.name == useStmt.source_interface)
                            {
                                sourceIface = &other;
                                break;
                            }
                        }

                        // Only generate using declarations for same-namespace imports
                        // Cross-namespace references will use fully qualified names
                        if (!sourceIface || sourceIface->kind == iface->kind)
                        {
                            for (const auto &typeName : useStmt.imported_types)
                            {
                                out << "using " << sanitize_identifier(typeName) << " = "
                                    << sanitize_identifier(useStmt.source_interface) << "::"
                                    << sanitize_identifier(typeName) << ";\n";
                                hasUsings = true;
                            }
                        }
                    }
                    if (hasUsings)
                    {
                        out << "\n";
                    }
                }

                // Generate function declarations
                for (const auto &func : iface->functions)
                {
                    generateFunctionDeclaration(out, func, iface);
                }

                out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
            }
        }

        out << "} // namespace host\n\n";
    }

    out << "#endif // " << guard << "\n";
}

void CodeGenerator::generateImplementation(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &headerName)
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

void CodeGenerator::generateWAMRBindings(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &headerFile, const std::string &wamrHeaderFile)
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

void CodeGenerator::generateWAMRHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &sampleHeaderFile)
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

// Helper to extract type names from a WIT type string
void CodeGenerator::extractTypeDependencies(const std::string &witType, const InterfaceInfo &iface, std::set<std::string> &deps)
{
    // Check if this type is a locally defined variant or record
    for (const auto &variant : iface.variants)
    {
        if (witType.find(variant.name) != std::string::npos)
        {
            deps.insert(variant.name);
        }
    }
    for (const auto &record : iface.records)
    {
        if (witType.find(record.name) != std::string::npos)
        {
            deps.insert(record.name);
        }
    }
    for (const auto &typeAlias : iface.type_aliases)
    {
        if (witType.find(typeAlias.name) != std::string::npos)
        {
            deps.insert(typeAlias.name);
        }
    }
}

// Unified topological sort for all user-defined types
std::vector<CodeGenerator::TypeDef> CodeGenerator::sortTypesByDependencies(const InterfaceInfo &iface)
{
    // Build dependency graph for all types
    std::map<std::string, std::set<std::string>> deps;
    std::map<std::string, TypeDef> typeMap;

    // Add variants to graph
    for (size_t i = 0; i < iface.variants.size(); ++i)
    {
        const auto &variant = iface.variants[i];
        TypeDef typeDef{TypeDef::Kind::Variant, variant.name, i};
        typeMap[variant.name] = typeDef;

        std::set<std::string> variantDeps;
        for (const auto &case_ : variant.cases)
        {
            if (!case_.type.empty())
            {
                extractTypeDependencies(case_.type, iface, variantDeps);
            }
        }
        variantDeps.erase(variant.name); // Remove self-dependency
        deps[variant.name] = variantDeps;
    }

    // Add records to graph
    for (size_t i = 0; i < iface.records.size(); ++i)
    {
        const auto &record = iface.records[i];
        TypeDef typeDef{TypeDef::Kind::Record, record.name, i};
        typeMap[record.name] = typeDef;

        std::set<std::string> recordDeps;
        for (const auto &field : record.fields)
        {
            extractTypeDependencies(field.type, iface, recordDeps);
        }
        recordDeps.erase(record.name); // Remove self-dependency
        deps[record.name] = recordDeps;
    }

    // Add type aliases to graph
    for (size_t i = 0; i < iface.type_aliases.size(); ++i)
    {
        const auto &typeAlias = iface.type_aliases[i];
        TypeDef typeDef{TypeDef::Kind::TypeAlias, typeAlias.name, i};
        typeMap[typeAlias.name] = typeDef;

        std::set<std::string> aliasDeps;
        extractTypeDependencies(typeAlias.target_type, iface, aliasDeps);
        aliasDeps.erase(typeAlias.name); // Remove self-dependency
        deps[typeAlias.name] = aliasDeps;
    }

    // Topological sort using DFS
    std::vector<TypeDef> sorted;
    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<void(const std::string &)> visit = [&](const std::string &name)
    {
        if (visited.count(name))
            return;
        if (visiting.count(name))
        {
            // Cyclic dependency - just continue (will handle with incomplete types)
            return;
        }

        visiting.insert(name);
        for (const auto &dep : deps[name])
        {
            visit(dep);
        }
        visiting.erase(name);
        visited.insert(name);

        // Add the type definition
        if (typeMap.count(name))
        {
            sorted.push_back(typeMap[name]);
        }
    };

    // Visit all types
    for (const auto &[name, _] : typeMap)
    {
        visit(name);
    }

    return sorted;
}

// Legacy function kept for compatibility
std::vector<VariantDef> CodeGenerator::sortVariantsByDependencies(const std::vector<VariantDef> &variants, const InterfaceInfo &iface)
{
    // Build dependency graph
    std::map<std::string, std::set<std::string>> deps;
    for (const auto &variant : variants)
    {
        std::set<std::string> variantDeps;
        for (const auto &case_ : variant.cases)
        {
            if (!case_.type.empty())
            {
                extractTypeDependencies(case_.type, iface, variantDeps);
            }
        }
        // Remove self-dependency (recursive types)
        variantDeps.erase(variant.name);
        deps[variant.name] = variantDeps;
    }

    // Topological sort using Kahn's algorithm
    std::vector<VariantDef> sorted;
    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<void(const std::string &)> visit = [&](const std::string &name)
    {
        if (visited.count(name))
            return;
        if (visiting.count(name))
        {
            // Cyclic dependency - just continue (will handle with incomplete types)
            return;
        }

        visiting.insert(name);
        for (const auto &dep : deps[name])
        {
            visit(dep);
        }
        visiting.erase(name);
        visited.insert(name);

        // Find and add the variant
        for (const auto &variant : variants)
        {
            if (variant.name == name)
            {
                sorted.push_back(variant);
                break;
            }
        }
    };

    for (const auto &variant : variants)
    {
        visit(variant.name);
    }

    return sorted;
}

// Topologically sort records based on dependencies
std::vector<RecordDef> CodeGenerator::sortRecordsByDependencies(const std::vector<RecordDef> &records, const InterfaceInfo &iface)
{
    // Build dependency graph
    std::map<std::string, std::set<std::string>> deps;
    for (const auto &record : records)
    {
        std::set<std::string> recordDeps;
        for (const auto &field : record.fields)
        {
            extractTypeDependencies(field.type, iface, recordDeps);
        }
        // Remove self-dependency (recursive types)
        recordDeps.erase(record.name);
        deps[record.name] = recordDeps;
    }

    // Topological sort using DFS
    std::vector<RecordDef> sorted;
    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<void(const std::string &)> visit = [&](const std::string &name)
    {
        if (visited.count(name))
            return;
        if (visiting.count(name))
        {
            // Cyclic dependency - just continue (will handle with incomplete types)
            return;
        }

        visiting.insert(name);
        for (const auto &dep : deps[name])
        {
            visit(dep);
        }
        visiting.erase(name);
        visited.insert(name);

        // Find and add the record
        for (const auto &record : records)
        {
            if (record.name == name)
            {
                sorted.push_back(record);
                break;
            }
        }
    };

    for (const auto &record : records)
    {
        visit(record.name);
    }

    return sorted;
}

// Generate type definitions (variants, enums, records)
void CodeGenerator::generateTypeDefinitions(std::ofstream &out, const InterfaceInfo &iface)
{
    // Generate type aliases for renamed use-statement types
    for (const auto &useStmt : iface.use_statements)
    {
        // Skip cross-package use statements (types not available in this compilation unit)
        if (!useStmt.source_package.empty())
        {
            continue;
        }

        // For cross-interface references, we need to check if the source interface is actually defined
        // in this file or if it's a world-level reference to a regular interface
        bool isCrossInterface = (useStmt.source_interface != iface.name);
        if (isCrossInterface && !iface.is_world_level)
        {
            // Regular interface referencing another interface - skip (types in separate file)
            continue;
        }

        for (const auto &rename : useStmt.type_renames)
        {
            const std::string &originalName = rename.first;
            const std::string &renamedName = rename.second;

            // Generate: using t2_renamed = types_interface::t2;
            out << "using " << sanitize_identifier(renamedName) << " = "
                << sanitize_identifier(useStmt.source_interface) << "::"
                << sanitize_identifier(originalName) << ";\n";
        }
    }
    if (!iface.use_statements.empty())
    {
        bool hasRenames = false;
        for (const auto &useStmt : iface.use_statements)
        {
            if (!useStmt.type_renames.empty())
            {
                hasRenames = true;
                break;
            }
        }
        if (hasRenames)
            out << "\n";
    }

    // Generate resource forward declarations as handle types (uint32_t)
    for (const auto &resourceDef : iface.resources)
    {
        out << "// Resource type (handle represented as uint32_t): " << resourceDef.name << "\n";
        out << "using " << sanitize_identifier(resourceDef.name) << " = uint32_t;\n\n";
    }

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

    // Generate flags with type aliases
    for (const auto &flagsDef : iface.flags)
    {
        // Generate the flags_t type alias with string literals
        out << "using " << sanitize_identifier(flagsDef.name) << " = cmcpp::flags_t<";
        for (size_t i = 0; i < flagsDef.flags.size(); ++i)
        {
            out << "\"" << sanitize_identifier(flagsDef.flags[i]) << "\"";
            if (i < flagsDef.flags.size() - 1)
                out << ", ";
        }
        out << ">;\n\n";
    }

    // Generate all user-defined types in unified dependency order
    auto sortedTypes = sortTypesByDependencies(iface);
    for (const auto &typeDef : sortedTypes)
    {
        switch (typeDef.kind)
        {
        case TypeDef::Kind::Variant:
        {
            const auto &variant = iface.variants[typeDef.originalIndex];
            out << "using " << sanitize_identifier(variant.name) << " = cmcpp::variant_t<";
            for (size_t i = 0; i < variant.cases.size(); ++i)
            {
                if (i > 0)
                    out << ", ";
                if (variant.cases[i].type.empty())
                {
                    // Case with no payload - use monostate
                    out << "cmcpp::monostate";
                }
                else
                {
                    out << TypeMapper::mapType(variant.cases[i].type, &iface);
                }
            }
            out << ">;\n\n";
            break;
        }
        case TypeDef::Kind::Record:
        {
            const auto &record = iface.records[typeDef.originalIndex];
            // Generate the underlying struct
            std::string struct_name = sanitize_identifier(record.name) + "_data";
            out << "struct " << struct_name << " {\n";
            for (const auto &field : record.fields)
            {
                std::string fieldName = sanitize_identifier(field.name);
                std::string fieldType = TypeMapper::mapType(field.type, &iface);

                // If field name matches the field's type name (without namespace qualifiers),
                // add underscore suffix to avoid "changes meaning" error
                if (fieldType.find("::") == std::string::npos && fieldType == fieldName)
                {
                    fieldName += "_";
                }

                out << "    " << fieldType << " " << fieldName << ";\n";
            }
            out << "};\n";

            // Wrap it with cmcpp::record_t for ValTrait support
            out << "using " << sanitize_identifier(record.name) << " = cmcpp::record_t<" << struct_name << ">;\n\n";
            break;
        }
        case TypeDef::Kind::TypeAlias:
        {
            const auto &typeAlias = iface.type_aliases[typeDef.originalIndex];
            out << "using " << sanitize_identifier(typeAlias.name) << " = "
                << TypeMapper::mapType(typeAlias.target_type, &iface) << ";\n\n";
            break;
        }
        }
    }
}

void CodeGenerator::generateFunctionDeclaration(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
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

void CodeGenerator::generateGuestFunctionTypeAlias(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
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

void CodeGenerator::generateFunctionImplementation(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
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
