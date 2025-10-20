#include "code_generator.hpp"
#include "type_mapper.hpp"
#include "utils.hpp"
#include "package_registry.hpp"
#include "wit_parser.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <sstream>

void CodeGenerator::generateHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const PackageRegistry *registry, const std::set<std::string> *external_deps, const std::set<std::string> *world_imports, const std::set<std::string> *world_exports, const std::string &source_wit_file)
{
    // Set interfaces for TypeMapper to resolve cross-namespace references
    TypeMapper::setInterfaces(&interfaces);

    std::ofstream out(filename);
    if (!out.is_open())
    {
        throw std::runtime_error("Cannot create header file: " + filename);
    }

    // Generate header guard using just the filename (not full path)
    std::filesystem::path filepath(filename);
    std::string guard = "GENERATED_" + sanitize_identifier(filepath.filename().string());
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);

    out << "#pragma once\n\n";
    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";
    out << "#include <cmcpp.hpp>\n\n";
    out << "// Generated host function declarations from WIT\n";
    out << "// - 'host' namespace: Guest imports (host implements these)\n";
    out << "// - 'guest' namespace: Guest exports (guest implements these, host calls them)\n\n";

    // Generate external package stubs if we have a registry
    if (registry && (external_deps || !interfaces.empty()))
    {
        generateExternalPackageStubs(out, interfaces, registry, external_deps);
    }

    // If no interfaces, check if we have world imports/exports
    if (interfaces.empty())
    {
        out << "// Note: This WIT file contains no concrete interface definitions.\n";
        out << "// It may reference external packages that are defined elsewhere.\n\n";

        // Collect same-package exports to generate includes
        // Note: For same-package exports, we need to find which WIT file defines the interface.
        // Since the interface name may differ from the filename (e.g., "incoming-handler"
        // is defined in "handler.wit"), we need to check sibling WIT files.
        std::map<std::string, std::string> interface_to_file; // interface name -> wit filename (without extension)

        // Build mapping by parsing sibling WIT files if source file is provided
        if (!source_wit_file.empty() && std::filesystem::exists(source_wit_file))
        {
            std::filesystem::path source_path(source_wit_file);
            auto parent_dir = source_path.parent_path();

            // Scan for other .wit files in the same directory
            for (const auto &entry : std::filesystem::directory_iterator(parent_dir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".wit")
                {
                    try
                    {
                        // Quick parse to extract interface names from this file
                        auto sibling_result = WitGrammarParser::parse(entry.path().string());
                        std::string wit_basename = entry.path().stem().string();

                        // Map each interface name to this WIT file
                        for (const auto &iface : sibling_result.interfaces)
                        {
                            std::string sanitized_iface = sanitize_identifier(iface.name);
                            interface_to_file[sanitized_iface] = sanitize_identifier(wit_basename);
                        }
                    }
                    catch (...)
                    {
                        // Skip files that fail to parse
                    }
                }
            }
        }

        std::set<std::string> same_package_exports;
        if (world_exports && !world_exports->empty())
        {
            for (const auto &export_name : *world_exports)
            {
                // Check if this is a same-package export (no ':' and '/')
                if (export_name.find(':') == std::string::npos && export_name.find('/') == std::string::npos)
                {
                    same_package_exports.insert(export_name);
                }
            }
        }

        // Generate includes for same-package exports
        if (!same_package_exports.empty())
        {
            out << "// Include same-package interface headers\n";
            for (const auto &iface_name : same_package_exports)
            {
                std::string sanitized_iface = sanitize_identifier(iface_name);

                // Look up the actual WIT filename for this interface
                std::string wit_file = sanitized_iface; // Default: assume interface name matches file name
                auto it = interface_to_file.find(sanitized_iface);
                if (it != interface_to_file.end())
                {
                    wit_file = it->second; // Use mapped filename
                }

                out << "#include \"../" << wit_file << "/" << wit_file << ".hpp\"\n";
            }
            out << "\n";
        }

        // Generate host namespace (for world imports - host implements these)
        if (world_imports && !world_imports->empty())
        {
            out << "// Host implements these interfaces (world imports)\n";
            out << "namespace host {\n";

            // Track interface names to avoid conflicts when multiple versions are imported
            std::map<std::string, int> interface_name_counts;
            for (const auto &import : *world_imports)
            {
                if (import.find(':') != std::string::npos && import.find('/') != std::string::npos)
                {
                    size_t slash_pos = import.find('/');
                    std::string after_slash = import.substr(slash_pos + 1);
                    size_t at_pos = after_slash.find('@');
                    std::string interface_name = (at_pos != std::string::npos) ? after_slash.substr(0, at_pos) : after_slash;
                    interface_name_counts[interface_name]++;
                }
            }

            for (const auto &import : *world_imports)
            {
                // Parse the import: "my:dep/a@0.1.0" -> package="my:dep@0.1.0", interface="a"
                if (import.find(':') != std::string::npos && import.find('/') != std::string::npos)
                {
                    size_t slash_pos = import.find('/');
                    std::string before_slash = import.substr(0, slash_pos);
                    std::string after_slash = import.substr(slash_pos + 1);

                    // Extract interface name and version
                    std::string interface_name = after_slash;
                    std::string package_spec = before_slash;

                    size_t at_pos = after_slash.find('@');
                    if (at_pos != std::string::npos)
                    {
                        interface_name = after_slash.substr(0, at_pos);
                        std::string version = after_slash.substr(at_pos);
                        package_spec = before_slash + version;
                    }

                    // Parse package ID and get namespace
                    auto pkg_id = PackageId::parse(package_spec);
                    if (pkg_id)
                    {
                        std::string cpp_namespace = pkg_id->to_cpp_namespace();
                        out << "    // Interface: " << import << "\n";

                        // If there are multiple versions of the same interface, use versioned namespace names
                        std::string local_namespace = sanitize_identifier(interface_name);
                        if (interface_name_counts[interface_name] > 1)
                        {
                            // Sanitize version for namespace name
                            std::string version_suffix = pkg_id->version;
                            // Remove leading @ if present
                            if (!version_suffix.empty() && version_suffix[0] == '@')
                            {
                                version_suffix = version_suffix.substr(1);
                            }
                            std::replace(version_suffix.begin(), version_suffix.end(), '.', '_');
                            std::replace(version_suffix.begin(), version_suffix.end(), '-', '_');
                            local_namespace += "_v" + version_suffix;
                        }

                        out << "    namespace " << local_namespace << " {\n";

                        // Check if the interface exists in the registry
                        if (registry)
                        {
                            auto iface_info = registry->resolve_interface(package_spec, interface_name);
                            if (iface_info)
                            {
                                // Interface found - use using directive
                                out << "        using namespace ::" << cpp_namespace << "::" << sanitize_identifier(interface_name) << ";\n";
                            }
                            else
                            {
                                // Interface not found in registry - this is an error
                                throw std::runtime_error("External interface not loaded: " + import +
                                                         "\nPackage '" + package_spec + "' interface '" + interface_name +
                                                         "' was not found in the registry. Make sure to load all dependent packages before generating code.");
                            }
                        }
                        else
                        {
                            // No registry - assume external stub exists
                            out << "        using namespace ::" << cpp_namespace << "::" << sanitize_identifier(interface_name) << ";\n";
                        }

                        out << "    }\n";
                    }
                }
            }
            out << "} // namespace host\n\n";
        }
        else
        {
            out << "namespace host {}\n\n";
        }

        // Generate guest namespace (for world exports - guest implements these)
        if (world_exports && !world_exports->empty())
        {
            out << "// Guest implements these interfaces (world exports)\n";
            out << "namespace guest {\n";
            for (const auto &export_name : *world_exports)
            {
                // Check if this is a cross-package export (contains ':' and '/')
                if (export_name.find(':') != std::string::npos && export_name.find('/') != std::string::npos)
                {
                    // Parse the export: "my:dep/a@0.2.0" -> package="my:dep@0.2.0", interface="a"
                    size_t slash_pos = export_name.find('/');
                    std::string before_slash = export_name.substr(0, slash_pos);
                    std::string after_slash = export_name.substr(slash_pos + 1);

                    // Extract interface name and version
                    std::string interface_name = after_slash;
                    std::string package_spec = before_slash;

                    size_t at_pos = after_slash.find('@');
                    if (at_pos != std::string::npos)
                    {
                        interface_name = after_slash.substr(0, at_pos);
                        std::string version = after_slash.substr(at_pos);
                        package_spec = before_slash + version;
                    }

                    // Parse package ID and get namespace
                    auto pkg_id = PackageId::parse(package_spec);
                    if (pkg_id)
                    {
                        std::string cpp_namespace = pkg_id->to_cpp_namespace();
                        out << "    // Interface: " << export_name << "\n";
                        out << "    namespace " << sanitize_identifier(interface_name) << " {\n";

                        // Check if the interface exists in the registry
                        if (registry)
                        {
                            auto iface_info = registry->resolve_interface(package_spec, interface_name);
                            if (iface_info)
                            {
                                // Interface found - use using directive
                                out << "        using namespace ::" << cpp_namespace << "::" << sanitize_identifier(interface_name) << ";\n";
                            }
                            else
                            {
                                // Interface not found in registry - this is an error
                                throw std::runtime_error("External interface not loaded: " + export_name +
                                                         "\nPackage '" + package_spec + "' interface '" + interface_name +
                                                         "' was not found in the registry. Make sure to load all dependent packages before generating code.");
                            }
                        }
                        else
                        {
                            // No registry - assume external stub exists
                            out << "        using namespace ::" << cpp_namespace << "::" << sanitize_identifier(interface_name) << ";\n";
                        }

                        out << "    }\n";
                    }
                }
                else
                {
                    // Same-package export (e.g., "run" without package qualifier)
                    // Re-export the interface namespace so the world header
                    // exposes the guest-facing symbols defined in the
                    // corresponding interface header.
                    out << "    // Exported interface: " << export_name << "\n";
                    out << "    namespace " << sanitize_identifier(export_name) << " {\n";
                    out << "        using namespace ::guest::" << sanitize_identifier(export_name) << ";\n";
                    out << "    }\n";
                }
            }
            out << "} // namespace guest\n\n";
        }
        else
        {
            out << "namespace guest {}\n\n";
        }

        out << "#endif // " << guard << "\n";
        return;
    }

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

        // First pass: Generate all regular interface types (non-world-level)
        for (const auto *iface : exports)
        {
            // Skip standalone functions and world-level types in first pass
            if (iface->is_standalone_function || iface->is_world_level)
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

            // Close namespace
            out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
        }

        // Second pass: Generate world-level types AFTER all interface namespaces are defined
        for (const auto *iface : exports)
        {
            if (!iface->is_world_level)
            {
                continue;
            }

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

            // Generate ONLY type definitions (no functions yet)
            generateTypeDefinitions(out, *iface);
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

void CodeGenerator::generateWAMRBindings(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &headerFile, const std::string &wamrHeaderFile, const PackageRegistry *registry, const std::set<std::string> *world_imports, const std::set<std::string> *world_exports)
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
    out << "// NOTE: You must implement the functions declared in the host namespace\n";
    out << "// (See " << headerFile << " for declarations, provide implementations in your host code)\n\n";

    out << "using namespace cmcpp;\n\n";

    // If world imports/exports are provided, use them to determine which interfaces to register
    // Otherwise fall back to InterfaceKind
    bool use_world_declarations = (world_imports && !world_imports->empty()) || (world_exports && !world_exports->empty());

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
        if (iface->functions.empty())
        {
            out << "NativeSymbol " << arrayName << "[1] = {};\n\n";
            continue;
        }

        out << "NativeSymbol " << arrayName << "[] = {\n";

        for (const auto &func : iface->functions)
        {
            std::string funcName = getSanitizedFunctionName(func, iface);

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

    // Also generate symbol arrays for world imports from external packages
    if (registry && world_imports && !world_imports->empty())
    {
        // Track interface name counts to match the versioning logic from generateHeader
        std::map<std::string, int> interface_name_counts;
        for (const auto &import_spec : *world_imports)
        {
            if (import_spec.find(':') != std::string::npos && import_spec.find('/') != std::string::npos)
            {
                size_t slash_pos = import_spec.find('/');
                std::string after_slash = import_spec.substr(slash_pos + 1);
                size_t at_pos = after_slash.find('@');
                std::string interface_name = (at_pos != std::string::npos) ? after_slash.substr(0, at_pos) : after_slash;
                interface_name_counts[interface_name]++;
            }
        }

        for (const auto &import_spec : *world_imports)
        {
            // Parse "my:dep/a@0.1.0" into package and interface
            if (import_spec.find(':') != std::string::npos && import_spec.find('/') != std::string::npos)
            {
                size_t slash_pos = import_spec.find('/');
                std::string before_slash = import_spec.substr(0, slash_pos);
                std::string after_slash = import_spec.substr(slash_pos + 1);

                std::string interface_name = after_slash;
                std::string package_spec = before_slash;

                size_t at_pos = after_slash.find('@');
                if (at_pos != std::string::npos)
                {
                    interface_name = after_slash.substr(0, at_pos);
                    std::string version = after_slash.substr(at_pos);
                    package_spec = before_slash + version;
                }

                // Parse package ID
                auto pkg_id = PackageId::parse(package_spec);
                if (!pkg_id)
                    continue;

                // Look up the interface in the registry
                auto iface_info = registry->resolve_interface(package_spec, interface_name);
                if (!iface_info || iface_info->functions.empty())
                    continue;

                // Determine the local namespace name (with versioning if needed)
                std::string local_namespace = interface_name;
                if (interface_name_counts[interface_name] > 1)
                {
                    std::string version_suffix = pkg_id->version;
                    if (!version_suffix.empty() && version_suffix[0] == '@')
                        version_suffix = version_suffix.substr(1);
                    std::replace(version_suffix.begin(), version_suffix.end(), '.', '_');
                    std::replace(version_suffix.begin(), version_suffix.end(), '-', '_');
                    local_namespace += "_v" + version_suffix;
                }

                std::string arrayName = sanitize_identifier(local_namespace) + "_symbols";
                std::string moduleName = import_spec; // Full spec like "my:dep/a@0.1.0"

                out << "// World import (external package): " << import_spec << "\n";
                out << "// Register with: wasm_runtime_register_natives_raw(\"" << moduleName << "\", " << arrayName << ", " << iface_info->functions.size() << ")\n";
                out << "NativeSymbol " << arrayName << "[] = {\n";

                for (const auto &func : iface_info->functions)
                {
                    std::string funcName = getSanitizedFunctionName(func, iface_info);

                    out << "    host_function(\"" << func.name << "\", host::" << sanitize_identifier(local_namespace) << "::" << funcName << "),\n";
                }

                out << "};\n\n";
            }
        }
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

    // Also add world imports from external packages
    if (registry && world_imports && !world_imports->empty())
    {
        std::map<std::string, int> interface_name_counts;
        for (const auto &import_spec : *world_imports)
        {
            if (import_spec.find(':') != std::string::npos && import_spec.find('/') != std::string::npos)
            {
                size_t slash_pos = import_spec.find('/');
                std::string after_slash = import_spec.substr(slash_pos + 1);
                size_t at_pos = after_slash.find('@');
                std::string interface_name = (at_pos != std::string::npos) ? after_slash.substr(0, at_pos) : after_slash;
                interface_name_counts[interface_name]++;
            }
        }

        for (const auto &import_spec : *world_imports)
        {
            if (import_spec.find(':') != std::string::npos && import_spec.find('/') != std::string::npos)
            {
                size_t slash_pos = import_spec.find('/');
                std::string before_slash = import_spec.substr(0, slash_pos);
                std::string after_slash = import_spec.substr(slash_pos + 1);

                std::string interface_name = after_slash;
                std::string package_spec = before_slash;

                size_t at_pos = after_slash.find('@');
                if (at_pos != std::string::npos)
                {
                    interface_name = after_slash.substr(0, at_pos);
                    std::string version = after_slash.substr(at_pos);
                    package_spec = before_slash + version;
                }

                auto pkg_id = PackageId::parse(package_spec);
                if (!pkg_id)
                    continue;

                auto iface_info = registry->resolve_interface(package_spec, interface_name);
                if (!iface_info || iface_info->functions.empty())
                    continue;

                std::string local_namespace = interface_name;
                if (interface_name_counts[interface_name] > 1)
                {
                    std::string version_suffix = pkg_id->version;
                    if (!version_suffix.empty() && version_suffix[0] == '@')
                        version_suffix = version_suffix.substr(1);
                    std::replace(version_suffix.begin(), version_suffix.end(), '.', '_');
                    std::replace(version_suffix.begin(), version_suffix.end(), '-', '_');
                    local_namespace += "_v" + version_suffix;
                }

                std::string arrayName = sanitize_identifier(local_namespace) + "_symbols";
                std::string moduleName = import_spec;

                out << "        {\"" << moduleName << "\", " << arrayName << ", " << iface_info->functions.size() << "},\n";
            }
        }
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

    out << "} // namespace wasm_utils\n";
}

void CodeGenerator::generateWAMRHeader(const std::vector<InterfaceInfo> &interfaces, const std::string &filename, const std::string &packageName, const std::string &sampleHeaderFile, const PackageRegistry *registry, const std::set<std::string> *world_imports, const std::set<std::string> *world_exports)
{
    std::ofstream out(filename);
    if (!out.is_open())
    {
        throw std::runtime_error("Cannot create WAMR header file: " + filename);
    }

    // Header guard using just the filename (not full path)
    std::filesystem::path filepath(filename);
    std::string guard = "GENERATED_" + sanitize_identifier(filepath.filename().string()) + "_HPP";
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
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

    // Generate guest function wrappers
    generateGuestFunctionWrappers(out, interfaces, packageName);

    out << "#endif // " << guard << "\n";
}

// Generate guest function wrappers for all exported functions
void CodeGenerator::generateGuestFunctionWrappers(std::ofstream &out, const std::vector<InterfaceInfo> &interfaces, const std::string &packageName)
{
    // Collect all export interfaces
    std::vector<const InterfaceInfo *> exportInterfaces;
    for (const auto &iface : interfaces)
    {
        if (iface.kind == InterfaceKind::Export)
        {
            exportInterfaces.push_back(&iface);
        }
    }

    if (exportInterfaces.empty())
    {
        return; // No exports, no wrappers needed
    }

    out << "// ==============================================================================\n";
    out << "// Guest Function Wrappers (Exports - Guest implements, Host calls)\n";
    out << "// ==============================================================================\n";
    out << "// These functions create pre-configured guest function wrappers for all exported\n";
    out << "// functions from the guest module. Use these instead of manually calling\n";
    out << "// guest_function<T>() with the export name.\n\n";

    out << "namespace guest_wrappers {\n\n";

    // Group functions by interface
    for (const auto *iface : exportInterfaces)
    {
        if (iface->is_standalone_function)
        {
            // Skip standalone functions for now, handle them separately below
            continue;
        }

        if (iface->functions.empty())
        {
            continue; // Skip interfaces with no functions
        }

        out << "// Interface: " << packageName << "/" << iface->name << "\n";
        out << "namespace " << sanitize_identifier(iface->name) << " {\n";

        for (const auto &func : iface->functions)
        {
            // Generate wrapper function
            std::string function_name = sanitize_identifier(func.name);
            std::string type_alias_name = func.name;
            std::replace(type_alias_name.begin(), type_alias_name.end(), '-', '_');
            std::replace(type_alias_name.begin(), type_alias_name.end(), '.', '_');

            // If this is a resource method, prefix with resource name to match type alias generation
            if (!func.resource_name.empty())
            {
                std::string resource_name = func.resource_name;
                std::replace(resource_name.begin(), resource_name.end(), '-', '_');
                std::replace(resource_name.begin(), resource_name.end(), '.', '_');
                type_alias_name = resource_name + "_" + type_alias_name;
                function_name = resource_name + "_" + function_name;
            }

            // Build the export name (e.g., "example:sample/booleans#and")
            std::string export_name = packageName + "/" + iface->name + "#" + func.name;

            out << "    inline auto " << function_name << "(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, \n";
            out << "                    " << std::string(function_name.length(), ' ') << " cmcpp::LiftLowerContext& ctx) {\n";
            out << "        return cmcpp::guest_function<::guest::" << sanitize_identifier(iface->name)
                << "::" << type_alias_name << "_t>(\n";
            out << "            module_inst, exec_env, ctx, \"" << export_name << "\");\n";
            out << "    }\n";
        }

        out << "} // namespace " << sanitize_identifier(iface->name) << "\n\n";
    }

    // Handle standalone functions (not in an interface)
    for (const auto *iface : exportInterfaces)
    {
        if (!iface->is_standalone_function)
        {
            continue;
        }

        for (const auto &func : iface->functions)
        {
            std::string function_name = sanitize_identifier(func.name);
            std::string type_alias_name = func.name;
            std::replace(type_alias_name.begin(), type_alias_name.end(), '-', '_');
            std::replace(type_alias_name.begin(), type_alias_name.end(), '.', '_');

            out << "// Standalone function: " << func.name << "\n";
            out << "inline auto " << function_name << "(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, \n";
            out << "                  " << std::string(function_name.length(), ' ') << " cmcpp::LiftLowerContext& ctx) {\n";
            out << "    return cmcpp::guest_function<::guest::" << type_alias_name << "_t>(\n";
            out << "        module_inst, exec_env, ctx, \"" << func.name << "\");\n";
            out << "}\n\n";
        }
    }

    out << "} // namespace guest_wrappers\n\n";
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
                    // Case with no payload - use unique empty type to prevent duplicate monostate
                    // empty_case<i> is a distinct type for each index i
                    out << "cmcpp::empty_case<" << i << ">";
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

bool CodeGenerator::functionNameConflictsWithType(const InterfaceInfo *iface, const std::string &sanitizedName)
{
    if (!iface)
    {
        return false;
    }

    auto matches = [&](const std::string &candidate) -> bool
    {
        return sanitize_identifier(candidate) == sanitizedName;
    };

    for (const auto &variant : iface->variants)
    {
        if (matches(variant.name))
        {
            return true;
        }
    }
    for (const auto &record : iface->records)
    {
        if (matches(record.name))
        {
            return true;
        }
    }
    for (const auto &enumDef : iface->enums)
    {
        if (matches(enumDef.name))
        {
            return true;
        }
    }
    for (const auto &flagsDef : iface->flags)
    {
        if (matches(flagsDef.name))
        {
            return true;
        }
    }
    for (const auto &resourceDef : iface->resources)
    {
        if (matches(resourceDef.name))
        {
            return true;
        }
    }
    for (const auto &typeAlias : iface->type_aliases)
    {
        if (matches(typeAlias.name))
        {
            return true;
        }
    }

    return false;
}

std::string CodeGenerator::getSanitizedFunctionName(const FunctionSignature &func, const InterfaceInfo *iface)
{
    std::string function_name = sanitize_identifier(func.name);

    if (!func.resource_name.empty())
    {
        function_name = sanitize_identifier(func.resource_name) + "_" + function_name;
    }

    if (functionNameConflictsWithType(iface, function_name))
    {
        function_name += "_";
    }

    return function_name;
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

    // Sanitize function name and check for type conflicts
    std::string function_name = getSanitizedFunctionName(func, iface);

    // Generate host function declaration
    out << return_type << " " << function_name << "(";

    // Parameters
    for (size_t i = 0; i < func.parameters.size(); ++i)
    {
        if (i > 0)
            out << ", ";
        // Also sanitize parameter names that conflict with types
        std::string param_name = sanitize_identifier(func.parameters[i].name);
        if (functionNameConflictsWithType(iface, param_name))
        {
            param_name += "_";
        }
        out << TypeMapper::mapType(func.parameters[i].type, iface) << " " << param_name;
    }

    out << ");\n\n";
}

void CodeGenerator::generateGuestFunctionTypeAlias(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
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

    // Generate only the function signature typedef for guest functions
    // Note: No need to sanitize the function name here since _t suffix prevents keyword conflicts
    std::string type_alias_name = func.name;
    std::replace(type_alias_name.begin(), type_alias_name.end(), '-', '_');
    std::replace(type_alias_name.begin(), type_alias_name.end(), '.', '_');

    // If this is a resource method, prefix with resource name to match other declarations
    if (!func.resource_name.empty())
    {
        std::string resource_name = func.resource_name;
        std::replace(resource_name.begin(), resource_name.end(), '-', '_');
        std::replace(resource_name.begin(), resource_name.end(), '.', '_');
        type_alias_name = resource_name + "_" + type_alias_name;
    }

    out << "// Guest function signature for use with guest_function<" << type_alias_name << "_t>()\n";
    out << "using " << type_alias_name << "_t = " << return_type << "(";

    // Parameters for function signature
    if (func.parameters.empty())
    {
        out << "void"; // Explicit void for zero-parameter functions to avoid ambiguity
    }
    else
    {
        for (size_t i = 0; i < func.parameters.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << TypeMapper::mapType(func.parameters[i].type, iface);
        }
    }

    out << ");\n\n";
}

void CodeGenerator::generateFunctionImplementation(std::ofstream &out, const FunctionSignature &func, const InterfaceInfo *iface)
{
    // Helper: check if function name conflicts with a type name in the interface
    auto function_name_conflicts_with_type = [&](const std::string &name) -> bool
    {
        if (!iface)
            return false;

        // Check all type names in the interface (sanitized to match C++ identifiers)
        for (const auto &variant : iface->variants)
            if (sanitize_identifier(variant.name) == name)
                return true;
        for (const auto &record : iface->records)
            if (sanitize_identifier(record.name) == name)
                return true;
        for (const auto &enumDef : iface->enums)
            if (sanitize_identifier(enumDef.name) == name)
                return true;
        for (const auto &flagsDef : iface->flags)
            if (sanitize_identifier(flagsDef.name) == name)
                return true;
        for (const auto &resourceDef : iface->resources)
            if (sanitize_identifier(resourceDef.name) == name)
                return true;
        for (const auto &typeAlias : iface->type_aliases)
            if (sanitize_identifier(typeAlias.name) == name)
                return true;

        return false;
    };

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

    // Sanitize function name and check for type conflicts
    std::string function_name = sanitize_identifier(func.name);

    // If this is a resource method, prefix with resource name to avoid collisions
    if (!func.resource_name.empty())
    {
        function_name = sanitize_identifier(func.resource_name) + "_" + function_name;
    }

    // Check if the (possibly prefixed) function name conflicts with a type
    if (function_name_conflicts_with_type(function_name))
    {
        function_name += "_";
    }

    out << return_type << " " << function_name << "(";

    // Parameters
    for (size_t i = 0; i < func.parameters.size(); ++i)
    {
        if (i > 0)
            out << ", ";
        // Also sanitize parameter names that conflict with types
        std::string param_name = sanitize_identifier(func.parameters[i].name);
        if (function_name_conflicts_with_type(func.parameters[i].name))
        {
            param_name += "_";
        }
        out << TypeMapper::mapType(func.parameters[i].type, iface) << " " << param_name;
    }

    out << ") {\n";
    out << "    // TODO: Implement " << func.name << "\n";

    if (return_type != "void")
    {
        out << "    return {}; // Return default value\n";
    }

    out << "}\n\n";
}

void CodeGenerator::generateExternalPackageStubs(std::ofstream &out,
                                                 const std::vector<InterfaceInfo> &interfaces,
                                                 const PackageRegistry *registry,
                                                 const std::set<std::string> *external_deps)
{
    if (!registry)
        return;

    // Collect all external package references from use statements
    std::set<std::string> external_packages;
    std::map<std::string, std::set<std::string>> package_interfaces; // package -> interface names

    for (const auto &iface : interfaces)
    {
        for (const auto &use_stmt : iface.use_statements)
        {
            if (!use_stmt.source_package.empty())
            {
                external_packages.insert(use_stmt.source_package);
                package_interfaces[use_stmt.source_package].insert(use_stmt.source_interface);
            }
        }
    }

    // Also add external dependencies from world imports/exports
    if (external_deps)
    {
        for (const auto &dep : *external_deps)
        {
            external_packages.insert(dep);
            // Extract interface names from deps (format: "namespace:package/interface@version")
            // For now, we'll just note the package is referenced
        }
    }

    // Recursively collect transitive dependencies
    // When an external package references another package via use statements,
    // we need to generate stubs for those packages too
    std::set<std::string> packages_to_process = external_packages;
    while (!packages_to_process.empty())
    {
        std::set<std::string> newly_discovered;
        for (const auto &package_spec : packages_to_process)
        {
            auto package = registry->get_package(package_spec);
            if (!package)
                continue;

            // Check all interfaces in this package for use statements
            for (const auto &iface : package->interfaces)
            {
                for (const auto &use_stmt : iface.use_statements)
                {
                    if (!use_stmt.source_package.empty())
                    {
                        if (!use_stmt.source_interface.empty())
                        {
                            package_interfaces[use_stmt.source_package].insert(use_stmt.source_interface);
                        }

                        if (external_packages.find(use_stmt.source_package) == external_packages.end())
                        {
                            newly_discovered.insert(use_stmt.source_package);
                            external_packages.insert(use_stmt.source_package);
                        }
                    }
                }
            }
        }
        packages_to_process = newly_discovered;
    }

    if (external_packages.empty())
        return;

    out << "// External package stub declarations\n";
    out << "// These are minimal type stubs for packages referenced from external sources\n\n";

    // Sort packages by dependencies (topological sort)
    // We need to ensure that if package A uses types from package B, B is generated before A
    std::vector<std::string> sorted_packages;
    std::set<std::string> processed;
    std::map<std::string, std::set<std::string>> package_deps; // package -> packages it depends on

    // First, build the dependency map
    for (const auto &package_spec : external_packages)
    {
        auto package = registry->get_package(package_spec);
        if (!package)
            continue;

        std::set<std::string> deps;
        for (const auto &iface : package->interfaces)
        {
            for (const auto &use_stmt : iface.use_statements)
            {
                if (!use_stmt.source_package.empty() &&
                    external_packages.find(use_stmt.source_package) != external_packages.end())
                {
                    deps.insert(use_stmt.source_package);
                }
            }
        }
        package_deps[package_spec] = deps;
    }

    // Topological sort using DFS
    std::function<void(const std::string &)> visit = [&](const std::string &pkg)
    {
        if (processed.find(pkg) != processed.end())
            return;
        processed.insert(pkg);

        // Visit dependencies first
        if (package_deps.count(pkg))
        {
            for (const auto &dep : package_deps[pkg])
            {
                visit(dep);
            }
        }

        sorted_packages.push_back(pkg);
    };

    for (const auto &package_spec : external_packages)
    {
        visit(package_spec);
    }

    for (const auto &package_spec : sorted_packages)
    {
        auto package_id = PackageId::parse(package_spec);
        if (!package_id)
            continue;

        auto package = registry->get_package(*package_id);
        if (!package)
        {
            // Package not loaded - generate placeholder comment
            out << "// External package not loaded: " << package_spec << "\n";
            continue;
        }

        std::string cpp_namespace = package_id->to_cpp_namespace();
        out << "// Package: " << package_spec << "\n";
        out << "namespace " << cpp_namespace << " {\n";

        // Determine which interfaces to generate
        std::set<std::string> requested_interfaces;
        if (package_interfaces.count(package_spec) && !package_interfaces[package_spec].empty())
        {
            // Specific interfaces requested via use statements
            requested_interfaces = package_interfaces[package_spec];
        }
        else
        {
            // Generate all interfaces in the package (for world imports/exports)
            for (const auto &iface : package->interfaces)
            {
                requested_interfaces.insert(iface.name);
            }
        }

        // Expand requested interfaces to include same-package dependencies via use statements
        std::set<std::string> interfaces_to_generate;
        std::function<void(const std::string &)> add_interface_with_deps = [&](const std::string &iface_name)
        {
            if (!interfaces_to_generate.insert(iface_name).second)
            {
                return; // already processed
            }

            if (auto *iface_ptr_dep = package->get_interface(iface_name))
            {
                for (const auto &use_stmt_dep : iface_ptr_dep->use_statements)
                {
                    if (use_stmt_dep.source_package.empty() && !use_stmt_dep.source_interface.empty())
                    {
                        add_interface_with_deps(use_stmt_dep.source_interface);
                    }
                }
            }
        };

        if (!requested_interfaces.empty())
        {
            for (const auto &iface_name : requested_interfaces)
            {
                add_interface_with_deps(iface_name);
            }
        }
        else
        {
            for (const auto &iface : package->interfaces)
            {
                add_interface_with_deps(iface.name);
            }
        }

        // Build dependency map for ordering (same-package dependencies only)
        std::map<std::string, std::set<std::string>> interface_deps;
        for (const auto &iface_name : interfaces_to_generate)
        {
            if (auto *iface_ptr_dep = package->get_interface(iface_name))
            {
                for (const auto &use_stmt_dep : iface_ptr_dep->use_statements)
                {
                    if (use_stmt_dep.source_package.empty() && !use_stmt_dep.source_interface.empty() &&
                        interfaces_to_generate.count(use_stmt_dep.source_interface))
                    {
                        interface_deps[iface_name].insert(use_stmt_dep.source_interface);
                    }
                }
            }
        }

        std::vector<std::string> interface_order;
        std::set<std::string> visited_interfaces;
        std::function<void(const std::string &)> visit_interface = [&](const std::string &iface_name)
        {
            if (visited_interfaces.count(iface_name))
            {
                return;
            }
            visited_interfaces.insert(iface_name);

            if (interface_deps.count(iface_name))
            {
                for (const auto &dep : interface_deps[iface_name])
                {
                    visit_interface(dep);
                }
            }

            interface_order.push_back(iface_name);
        };

        for (const auto &iface_name : interfaces_to_generate)
        {
            visit_interface(iface_name);
        }

        // Generate stub interfaces
        for (const auto &iface_name : interface_order)
        {
            auto *iface_ptr = package->get_interface(iface_name);
            if (!iface_ptr)
            {
                throw std::runtime_error("Interface '" + iface_name + "' not found in package '" + package_spec + "'");
            }

            out << "    namespace " << sanitize_identifier(iface_name) << " {\n";

            // Generate use declarations (type imports from other packages or interfaces)
            auto emit_using_decls = [&](const UseStatement &use_stmt, const std::string &ns_prefix)
            {
                for (const auto &type_name : use_stmt.imported_types)
                {
                    std::string local_name = use_stmt.type_renames.count(type_name)
                                                 ? use_stmt.type_renames.at(type_name)
                                                 : type_name;
                    out << "        using " << sanitize_identifier(local_name)
                        << " = ::" << ns_prefix << "::" << sanitize_identifier(use_stmt.source_interface)
                        << "::" << sanitize_identifier(type_name) << ";\n";
                }
            };

            bool emitted_use_decl = false;
            for (const auto &use_stmt : iface_ptr->use_statements)
            {
                if (!use_stmt.source_package.empty())
                {
                    // Cross-package use statement
                    if (auto use_pkg_id = PackageId::parse(use_stmt.source_package))
                    {
                        emit_using_decls(use_stmt, use_pkg_id->to_cpp_namespace());
                        emitted_use_decl = true;
                    }
                }
                else if (!use_stmt.source_interface.empty() && use_stmt.source_interface != iface_name)
                {
                    // Same-package interface reference
                    emit_using_decls(use_stmt, cpp_namespace);
                    emitted_use_decl = true;
                }
            }

            if (emitted_use_decl)
            {
                out << "\n";
            }

            // Generate resource handle stubs
            if (!iface_ptr->resources.empty())
            {
                for (const auto &resource : iface_ptr->resources)
                {
                    out << "        // Resource type (handle represented as uint32_t): "
                        << resource.name << "\n";
                    out << "        using " << sanitize_identifier(resource.name) << " = uint32_t;\n";
                }
                out << "\n";
            }

            // Generate flag definitions
            if (!iface_ptr->flags.empty())
            {
                for (const auto &flags_def : iface_ptr->flags)
                {
                    out << "        using " << sanitize_identifier(flags_def.name) << " = cmcpp::flags_t<";
                    for (size_t i = 0; i < flags_def.flags.size(); ++i)
                    {
                        out << "\"" << sanitize_identifier(flags_def.flags[i]) << "\"";
                        if (i < flags_def.flags.size() - 1)
                        {
                            out << ", ";
                        }
                    }
                    out << ">;\n";
                }
                out << "\n";
            }

            // Generate type aliases for types used from this interface
            // For now, we'll generate stubs for all types in the interface
            for (const auto &type_alias : iface_ptr->type_aliases)
            {
                out << "        using " << sanitize_identifier(type_alias.name)
                    << " = " << TypeMapper::mapType(type_alias.target_type, iface_ptr) << ";\n";
            }

            // Generate record stubs
            for (const auto &record : iface_ptr->records)
            {
                out << "        struct " << sanitize_identifier(record.name) << " {\n";
                for (const auto &field : record.fields)
                {
                    out << "            " << TypeMapper::mapType(field.type, iface_ptr)
                        << " " << sanitize_identifier(field.name) << ";\n";
                }
                out << "        };\n";
            }

            // Generate enum stubs
            for (const auto &enum_def : iface_ptr->enums)
            {
                out << "        enum class " << sanitize_identifier(enum_def.name) << " {\n";
                for (size_t i = 0; i < enum_def.values.size(); ++i)
                {
                    out << "            " << sanitize_identifier(enum_def.values[i]);
                    if (i < enum_def.values.size() - 1)
                        out << ",";
                    out << "\n";
                }
                out << "        };\n";
            }

            // Generate variant stubs
            for (const auto &variant : iface_ptr->variants)
            {
                out << "        using " << sanitize_identifier(variant.name)
                    << " = cmcpp::variant_t<";
                for (size_t i = 0; i < variant.cases.size(); ++i)
                {
                    if (i > 0)
                        out << ", ";
                    if (variant.cases[i].type.empty() || variant.cases[i].type == "_")
                    {
                        // Use unique empty type to prevent duplicate monostate
                        out << "cmcpp::empty_case<" << i << ">";
                    }
                    else
                    {
                        out << TypeMapper::mapType(variant.cases[i].type, iface_ptr);
                    }
                }
                out << ">;\n";
            }

            // Generate function declarations
            for (const auto &func : iface_ptr->functions)
            {
                // Determine return type
                // For external package stubs, use simple type names since we generate
                // all necessary using declarations above
                std::string return_type = "void";
                if (func.results.size() == 1)
                {
                    std::string result_type = func.results[0];
                    // Remove whitespace
                    result_type.erase(std::remove_if(result_type.begin(), result_type.end(), ::isspace), result_type.end());

                    // Check if this is a type available via use statement
                    bool is_imported = false;
                    for (const auto &use_stmt : iface_ptr->use_statements)
                    {
                        for (const auto &imported : use_stmt.imported_types)
                        {
                            std::string local_name = use_stmt.type_renames.count(imported)
                                                         ? use_stmt.type_renames.at(imported)
                                                         : imported;
                            if (result_type == local_name)
                            {
                                is_imported = true;
                                break;
                            }
                        }
                        if (is_imported)
                            break;
                    }

                    // If it's imported or a local type alias, use the simple name
                    if (is_imported || std::any_of(iface_ptr->type_aliases.begin(), iface_ptr->type_aliases.end(),
                                                   [&](const TypeAliasDef &ta)
                                                   { return ta.name == result_type; }))
                    {
                        return_type = sanitize_identifier(result_type);
                    }
                    else
                    {
                        return_type = TypeMapper::mapType(func.results[0], iface_ptr);
                    }
                }
                else if (func.results.size() > 1)
                {
                    return_type = "cmcpp::tuple_t<";
                    for (size_t i = 0; i < func.results.size(); ++i)
                    {
                        if (i > 0)
                            return_type += ", ";
                        return_type += TypeMapper::mapType(func.results[i], iface_ptr);
                    }
                    return_type += ">";
                }

                out << "        " << return_type << " " << sanitize_identifier(func.name) << "(";

                // Parameters
                for (size_t i = 0; i < func.parameters.size(); ++i)
                {
                    if (i > 0)
                        out << ", ";
                    out << TypeMapper::mapType(func.parameters[i].type, iface_ptr) << " " << sanitize_identifier(func.parameters[i].name);
                }

                out << ");\n";
            }

            out << "    } // namespace " << sanitize_identifier(iface_name) << "\n";
        }

        out << "} // namespace " << cpp_namespace << "\n\n";
    }
}
