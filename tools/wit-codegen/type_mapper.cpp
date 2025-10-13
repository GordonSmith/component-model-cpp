#include "type_mapper.hpp"
#include "package_registry.hpp"

#include <cctype>
#include <optional>

namespace
{
    enum class InterfaceTypeKind
    {
        Enum,
        Variant,
        Record,
        Flags,
        Resource,
        Alias
    };

    std::optional<InterfaceTypeKind> find_interface_type_kind(const InterfaceInfo &iface, const std::string &type_name)
    {
        auto matches = [&](const std::string &candidate)
        {
            return candidate == type_name;
        };

        for (const auto &variant : iface.variants)
        {
            if (matches(variant.name))
                return InterfaceTypeKind::Variant;
        }

        for (const auto &enumDef : iface.enums)
        {
            if (matches(enumDef.name))
                return InterfaceTypeKind::Enum;
        }

        for (const auto &record : iface.records)
        {
            if (matches(record.name))
                return InterfaceTypeKind::Record;
        }

        for (const auto &flagsDef : iface.flags)
        {
            if (matches(flagsDef.name))
                return InterfaceTypeKind::Flags;
        }

        for (const auto &resourceDef : iface.resources)
        {
            if (matches(resourceDef.name))
                return InterfaceTypeKind::Resource;
        }

        for (const auto &typeAlias : iface.type_aliases)
        {
            if (matches(typeAlias.name))
                return InterfaceTypeKind::Alias;
        }

        return std::nullopt;
    }

    std::string qualify_identifier(const std::string &text,
                                   const std::string &identifier,
                                   const std::string &qualification)
    {
        if (text.find(qualification) != std::string::npos)
            return text;

        auto is_identifier_char = [](char ch)
        {
            return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == ':';
        };

        std::string result;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t found = text.find(identifier, pos);
            if (found == std::string::npos)
            {
                result.append(text.substr(pos));
                break;
            }

            bool valid_start = (found == 0) || !is_identifier_char(text[found - 1]);
            bool valid_end = (found + identifier.size() >= text.size()) ||
                             !is_identifier_char(text[found + identifier.size()]);

            if (valid_start && valid_end)
            {
                result.append(text.substr(pos, found - pos));
                result.append(qualification);
            }
            else
            {
                result.append(text.substr(pos, found - pos + identifier.size()));
            }

            pos = found + identifier.size();
        }

        return result;
    }
} // namespace

// Define static members
const std::vector<InterfaceInfo> *TypeMapper::all_interfaces = nullptr;
PackageRegistry *TypeMapper::package_registry = nullptr;

void TypeMapper::setInterfaces(const std::vector<InterfaceInfo> *interfaces)
{
    all_interfaces = interfaces;
}

void TypeMapper::setPackageRegistry(PackageRegistry *registry)
{
    package_registry = registry;
}

std::string TypeMapper::mapType(const std::string &witType, const InterfaceInfo *iface)
{
    // Remove whitespace
    std::string type = witType;
    type.erase(std::remove_if(type.begin(), type.end(), ::isspace), type.end());

    // Check if type had % prefix (indicates user wants to escape built-in type names)
    bool hadEscapePrefix = (!type.empty() && type[0] == '%');

    // Strip leading % used for escaping keywords/special identifiers in WIT
    // This matches the behavior of extract_identifier()
    if (hadEscapePrefix)
    {
        type = type.substr(1);
    }

    // Handle underscore (unit/void type in result/option)
    if (type == "_")
        return "cmcpp::monostate";

    // Handle bare 'result' (shorthand for result<_, _>)
    if (type == "result")
        return "cmcpp::result_t<cmcpp::monostate, cmcpp::monostate>";

    // Basic types - but skip if type had % escape prefix (user-defined type with same name)
    if (!hadEscapePrefix)
    {
        if (type == "bool")
            return "cmcpp::bool_t";
        if (type == "u8")
            return "uint8_t";
        if (type == "u16")
            return "uint16_t";
        if (type == "u32")
            return "uint32_t";
        if (type == "u64")
            return "uint64_t";
        if (type == "s8")
            return "int8_t";
        if (type == "s16")
            return "int16_t";
        if (type == "s32")
            return "int32_t";
        if (type == "s64")
            return "int64_t";
        if (type == "f32")
            return "cmcpp::float32_t";
        if (type == "f64")
            return "cmcpp::float64_t";
        if (type == "char")
            return "cmcpp::char_t";
        if (type == "string")
            return "cmcpp::string_t";
        if (type == "error-context")
        {
            // error-context is a handle type (table index), not a value type
            return "uint32_t /* error-context */";
        }
    } // end of basic types (skipped if hadEscapePrefix)

    // Check if it's a locally defined type in the current interface
    if (iface)
    {
        // Check enums - wrap with cmcpp::enum_t<>
        for (const auto &enumDef : iface->enums)
        {
            if (enumDef.name == type)
                return "cmcpp::enum_t<" + sanitize_identifier(enumDef.name) + ">";
        }
        // Check variants
        for (const auto &variant : iface->variants)
        {
            if (variant.name == type)
                return sanitize_identifier(variant.name);
        }
        // Check records
        for (const auto &record : iface->records)
        {
            if (record.name == type)
                return sanitize_identifier(record.name);
        }
        // Check flags - return the type name directly (it's already a flags_t alias)
        for (const auto &flagsDef : iface->flags)
        {
            if (flagsDef.name == type)
                return sanitize_identifier(flagsDef.name);
        }
        // Check resources - return the type name directly (it's already a uint32_t alias)
        for (const auto &resourceDef : iface->resources)
        {
            if (resourceDef.name == type)
                return sanitize_identifier(resourceDef.name);
        }
        // Check type aliases - recursively resolve
        for (const auto &typeAlias : iface->type_aliases)
        {
            if (typeAlias.name == type)
                return mapType(typeAlias.target_type, iface);
        }

        // Check use statements - resolve to fully qualified names for cross-namespace references
        for (const auto &useStmt : iface->use_statements)
        {
            // First check if this is a cross-package use statement
            if (!useStmt.source_package.empty())
            {
                // This is a cross-package import - check if the type is in this use statement
                for (const auto &importedType : useStmt.imported_types)
                {
                    if (importedType == type)
                    {
                        // Treat cross-package types as opaque uint32_t handles
                        return "uint32_t /* " + sanitize_identifier(useStmt.source_package) + "/" +
                               sanitize_identifier(useStmt.source_interface) + "::" +
                               sanitize_identifier(type) + " */";
                    }

                    // Check renamed types
                    auto renameIt = useStmt.type_renames.find(importedType);
                    if (renameIt != useStmt.type_renames.end() && renameIt->second == type)
                    {
                        return "uint32_t /* " + sanitize_identifier(useStmt.source_package) + "/" +
                               sanitize_identifier(useStmt.source_interface) + "::" +
                               sanitize_identifier(importedType) + " (renamed to " +
                               sanitize_identifier(type) + ") */";
                    }
                }
                continue; // Skip to next use statement
            }

            // Same-package use statement - check if this refers to a different interface
            // If source_interface is different from current interface name, it's a cross-interface reference
            bool isCrossInterface = (useStmt.source_interface != iface->name);
            if (isCrossInterface)
            {
                // Cross-interface within same package
                // Check if the source interface is available (in same file)
                const InterfaceInfo *sourceIface = nullptr;
                if (all_interfaces)
                {
                    // First, try to find an interface with the same kind as current interface (prefer same namespace)
                    for (const auto &other : *all_interfaces)
                    {
                        if (other.name == useStmt.source_interface && other.kind == iface->kind)
                        {
                            sourceIface = &other;
                            break;
                        }
                    }

                    // If not found, fall back to any interface with matching name
                    if (!sourceIface)
                    {
                        for (const auto &other : *all_interfaces)
                        {
                            if (other.name == useStmt.source_interface)
                            {
                                sourceIface = &other;
                                break;
                            }
                        }
                    }
                }

                // If source interface is found, use fully qualified name
                if (sourceIface)
                {
                    for (const auto &importedType : useStmt.imported_types)
                    {
                        // Check renamed types
                        auto renameIt = useStmt.type_renames.find(importedType);
                        bool isMatch = (importedType == type) ||
                                       (renameIt != useStmt.type_renames.end() && renameIt->second == type);

                        if (isMatch)
                        {
                            std::string sourceType = importedType; // Original name in source interface

                            std::string prefix = (sourceIface->kind == InterfaceKind::Import) ? "::host::" : "::guest::";
                            auto type_kind = find_interface_type_kind(*sourceIface, sourceType);

                            if (type_kind)
                            {
                                if (*type_kind == InterfaceTypeKind::Alias)
                                {
                                    return prefix + sanitize_identifier(useStmt.source_interface) + "::" + sanitize_identifier(sourceType);
                                }

                                std::string resolvedType = mapType(sourceType, sourceIface);
                                std::string sourceIdent = sanitize_identifier(sourceType);
                                std::string qualified = prefix + sanitize_identifier(useStmt.source_interface) + "::" + sourceIdent;
                                return qualify_identifier(resolvedType, sourceIdent, qualified);
                            }

                            // Recursively resolve the type in the source interface context
                            // This handles chained imports like: world uses e2.{x}, e2 uses e1.{x}
                            std::string resolvedType = mapType(sourceType, sourceIface);
                            return resolvedType;
                        }
                    }
                }
                else
                {
                    // Source interface not found - treat as opaque handle
                    for (const auto &importedType : useStmt.imported_types)
                    {
                        if (importedType == type)
                        {
                            return "uint32_t /* " + sanitize_identifier(useStmt.source_interface) + "::" +
                                   sanitize_identifier(importedType) + " */";
                        }

                        // Check renamed types
                        auto renameIt = useStmt.type_renames.find(importedType);
                        if (renameIt != useStmt.type_renames.end() && renameIt->second == type)
                        {
                            return "uint32_t /* " + sanitize_identifier(useStmt.source_interface) + "::" +
                                   sanitize_identifier(importedType) + " (renamed to " +
                                   sanitize_identifier(type) + ") */";
                        }
                    }
                }
                continue; // Skip to next use statement
            }

            // Same-interface use statement - check if type is from this interface
            for (const auto &importedType : useStmt.imported_types)
            {
                // Check if the type matches either the original name or the renamed name
                std::string lookupName = importedType;
                bool isRenamed = false;

                // Check if this type has been renamed
                auto renameIt = useStmt.type_renames.find(importedType);
                if (renameIt != useStmt.type_renames.end())
                {
                    // Type was renamed - check if we're looking for the renamed version
                    if (renameIt->second == type)
                    {
                        lookupName = importedType; // Use original name for lookup
                        isRenamed = true;
                    }
                }

                if (importedType == type || isRenamed)
                {
                    // Find the source interface to determine its namespace
                    const InterfaceInfo *sourceIface = nullptr;
                    if (all_interfaces)
                    {
                        // First, try to find an interface with the same kind as current interface (prefer same namespace)
                        for (const auto &other : *all_interfaces)
                        {
                            if (other.name == useStmt.source_interface && other.kind == iface->kind)
                            {
                                sourceIface = &other;
                                break;
                            }
                        }

                        // If not found, fall back to any interface with matching name
                        if (!sourceIface)
                        {
                            for (const auto &other : *all_interfaces)
                            {
                                if (other.name == useStmt.source_interface)
                                {
                                    sourceIface = &other;
                                    break;
                                }
                            }
                        }
                    }

                    // If source is in different namespace, use fully qualified name
                    if (sourceIface && sourceIface->kind != iface->kind)
                    {
                        std::string prefix = (sourceIface->kind == InterfaceKind::Import) ? "::host::" : "::guest::";
                        auto type_kind = find_interface_type_kind(*sourceIface, lookupName);

                        if (type_kind)
                        {
                            if (*type_kind == InterfaceTypeKind::Alias)
                            {
                                return prefix + sanitize_identifier(useStmt.source_interface) + "::" + sanitize_identifier(lookupName);
                            }

                            std::string resolvedType = mapType(lookupName, sourceIface);
                            std::string sourceIdent = sanitize_identifier(lookupName);
                            std::string qualified = prefix + sanitize_identifier(useStmt.source_interface) + "::" + sourceIdent;
                            return qualify_identifier(resolvedType, sourceIdent, qualified);
                        }

                        // Recursively resolve the type in the source interface context
                        // This handles chained imports like: world uses e2.{x}, e2 uses e1.{x}
                        std::string resolvedType = mapType(lookupName, sourceIface);
                        return resolvedType;
                    }
                    else if (sourceIface)
                    {
                        auto type_kind = find_interface_type_kind(*sourceIface, lookupName);

                        if (type_kind)
                        {
                            if (*type_kind == InterfaceTypeKind::Alias)
                            {
                                return sanitize_identifier(useStmt.source_interface) + "::" + sanitize_identifier(lookupName);
                            }

                            std::string resolvedType = mapType(lookupName, sourceIface);
                            std::string sourceIdent = sanitize_identifier(lookupName);
                            std::string qualified = sanitize_identifier(useStmt.source_interface) + "::" + sourceIdent;
                            return qualify_identifier(resolvedType, sourceIdent, qualified);
                        }

                        // Same namespace - recursively resolve in source interface
                        std::string resolvedType = mapType(lookupName, sourceIface);
                        return resolvedType;
                    }
                    else
                    {
                        // Source interface not found, return unqualified
                        return sanitize_identifier(lookupName);
                    }
                }
            }
        }
    }

    // Check world-level types and use statements from all interfaces
    if (all_interfaces)
    {
        for (const auto &other : *all_interfaces)
        {
            if (other.is_world_level)
            {
                // Check use statements in world-level interface (for standalone functions)
                for (const auto &useStmt : other.use_statements)
                {
                    // Skip cross-package use statements
                    if (!useStmt.source_package.empty())
                    {
                        continue;
                    }

                    // Check if type is a renamed import (e.g., use a.{t8 as u8})
                    for (const auto &rename : useStmt.type_renames)
                    {
                        if (rename.second == type) // rename.second is the new name (e.g., u8)
                        {
                            // Find the source interface to determine its namespace
                            const InterfaceInfo *sourceIface = nullptr;
                            // First, try to find an interface with Export kind for world-level types
                            for (const auto &srcIface : *all_interfaces)
                            {
                                if (srcIface.name == useStmt.source_interface && srcIface.kind == InterfaceKind::Export)
                                {
                                    sourceIface = &srcIface;
                                    break;
                                }
                            }

                            // If not found, fall back to any interface with matching name
                            if (!sourceIface)
                            {
                                for (const auto &srcIface : *all_interfaces)
                                {
                                    if (srcIface.name == useStmt.source_interface)
                                    {
                                        sourceIface = &srcIface;
                                        break;
                                    }
                                }
                            }

                            // Return the qualified source type
                            if (sourceIface)
                            {
                                std::string prefix = (sourceIface->kind == InterfaceKind::Import) ? "::host::" : "::guest::";
                                auto type_kind = find_interface_type_kind(*sourceIface, rename.first);

                                if (type_kind)
                                {
                                    if (*type_kind == InterfaceTypeKind::Alias)
                                    {
                                        return prefix + sanitize_identifier(useStmt.source_interface) + "::" + sanitize_identifier(rename.first);
                                    }

                                    std::string resolvedType = mapType(rename.first, sourceIface);
                                    std::string sourceIdent = sanitize_identifier(rename.first);
                                    std::string qualified = prefix + sanitize_identifier(useStmt.source_interface) + "::" + sourceIdent;
                                    return qualify_identifier(resolvedType, sourceIdent, qualified);
                                }

                                std::string resolvedType = mapType(rename.first, sourceIface);
                                return resolvedType;
                            }
                        }
                    }

                    // Check if type is imported from another interface
                    for (const auto &importedType : useStmt.imported_types)
                    {
                        if (importedType == type)
                        {
                            // Find the source interface to determine its namespace
                            const InterfaceInfo *sourceIface = nullptr;
                            // First, try to find an interface with Export kind for world-level types
                            for (const auto &srcIface : *all_interfaces)
                            {
                                if (srcIface.name == useStmt.source_interface && srcIface.kind == InterfaceKind::Export)
                                {
                                    sourceIface = &srcIface;
                                    break;
                                }
                            }

                            // If not found, fall back to any interface with matching name
                            if (!sourceIface)
                            {
                                for (const auto &srcIface : *all_interfaces)
                                {
                                    if (srcIface.name == useStmt.source_interface)
                                    {
                                        sourceIface = &srcIface;
                                        break;
                                    }
                                }
                            }

                            // Determine the correct namespace qualification
                            if (sourceIface)
                            {
                                // Source interface is in a specific namespace, fully qualify it
                                std::string prefix = (sourceIface->kind == InterfaceKind::Import) ? "::host::" : "::guest::";
                                auto type_kind = find_interface_type_kind(*sourceIface, importedType);

                                if (type_kind)
                                {
                                    if (*type_kind == InterfaceTypeKind::Alias)
                                    {
                                        return prefix + sanitize_identifier(useStmt.source_interface) + "::" + sanitize_identifier(importedType);
                                    }

                                    std::string resolvedType = mapType(importedType, sourceIface);
                                    std::string sourceIdent = sanitize_identifier(importedType);
                                    std::string qualified = prefix + sanitize_identifier(useStmt.source_interface) + "::" + sourceIdent;
                                    return qualify_identifier(resolvedType, sourceIdent, qualified);
                                }

                                std::string resolvedType = mapType(importedType, sourceIface);
                                return resolvedType;
                            }
                        }
                    }
                }

                // Check enums
                for (const auto &enumDef : other.enums)
                {
                    if (enumDef.name == type)
                    {
                        // If we're in a host interface, need to qualify with ::guest::
                        if (iface && iface->kind == InterfaceKind::Import)
                        {
                            return "cmcpp::enum_t<::guest::" + sanitize_identifier(enumDef.name) + ">";
                        }
                        return "cmcpp::enum_t<" + sanitize_identifier(enumDef.name) + ">";
                    }
                }
                // Check variants
                for (const auto &variant : other.variants)
                {
                    if (variant.name == type)
                    {
                        if (iface && iface->kind == InterfaceKind::Import)
                        {
                            return "::guest::" + sanitize_identifier(variant.name);
                        }
                        return sanitize_identifier(variant.name);
                    }
                }
                // Check records
                for (const auto &record : other.records)
                {
                    if (record.name == type)
                    {
                        if (iface && iface->kind == InterfaceKind::Import)
                        {
                            return "::guest::" + sanitize_identifier(record.name);
                        }
                        return sanitize_identifier(record.name);
                    }
                }
                // Check flags
                for (const auto &flagsDef : other.flags)
                {
                    if (flagsDef.name == type)
                    {
                        if (iface && iface->kind == InterfaceKind::Import)
                        {
                            return "::guest::" + sanitize_identifier(flagsDef.name);
                        }
                        return sanitize_identifier(flagsDef.name);
                    }
                }
                // Check type aliases - recursively resolve
                for (const auto &typeAlias : other.type_aliases)
                {
                    if (typeAlias.name == type)
                    {
                        // Recursively resolve the target type
                        std::string resolvedType = mapType(typeAlias.target_type, &other);
                        // If we're in a host interface looking at guest world-level type, need to qualify
                        if (iface && iface->kind == InterfaceKind::Import && resolvedType.find("::") == std::string::npos)
                        {
                            // Only add ::guest:: if the resolved type isn't already qualified
                            return "::guest::" + resolvedType;
                        }
                        return resolvedType;
                    }
                }
            }
        }
    }

    // List types
    if (type.find("list<") == 0)
    {
        std::string innerType = extract_template_content(type);
        return "cmcpp::list_t<" + mapType(innerType, iface) + ">";
    }

    // Option types
    if (type.find("option<") == 0)
    {
        std::string innerType = extract_template_content(type);
        return "cmcpp::option_t<" + mapType(innerType, iface) + ">";
    }

    // Own resource types (for now, just use the resource handle type)
    if (type.find("own<") == 0)
    {
        std::string resourceType = extract_template_content(type);
        return mapType(resourceType, iface); // Recursively resolve the inner type
    }

    // Borrow resource types (for now, just use the resource handle type)
    if (type.find("borrow<") == 0)
    {
        std::string resourceType = extract_template_content(type);
        return mapType(resourceType, iface); // Recursively resolve the inner type
    }

    // Stream types (async streams)
    if (type.find("stream<") == 0)
    {
        std::string innerType = extract_template_content(type);
        return "uint32_t /* stream<" + sanitize_identifier(innerType) + "> */";
    }

    // Bare stream (stream without type parameter)
    if (type == "stream")
    {
        return "uint32_t /* stream */";
    }

    // Future types (async futures)
    if (type.find("future<") == 0)
    {
        std::string innerType = extract_template_content(type);
        return "uint32_t /* future<" + sanitize_identifier(innerType) + "> */";
    }

    // Bare future (future without type parameter)
    if (type == "future")
    {
        return "uint32_t /* future */";
    }

    // Result types
    if (type.find("result<") == 0)
    {
        std::string innerTypes = extract_template_content(type);
        auto types = split_respecting_brackets(innerTypes);

        if (types.size() >= 2)
        {
            std::string okType = mapType(types[0], iface);
            std::string errType = mapType(types[1], iface);
            return "cmcpp::result_t<" + okType + ", " + errType + ">";
        }
        else if (types.size() == 1)
        {
            // result<T> is shorthand for result<T, _>
            std::string okType = mapType(types[0], iface);
            return "cmcpp::result_t<" + okType + ", cmcpp::monostate>";
        }
    }

    // Tuple types
    if (type.find("tuple<") == 0)
    {
        std::string innerTypes = extract_template_content(type);
        auto types = split_respecting_brackets(innerTypes);

        std::string result = "cmcpp::tuple_t<";
        for (size_t i = 0; i < types.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += mapType(types[i], iface);
        }
        result += ">";
        return result;
    }

    // Unknown type - return as-is (but sanitize the identifier)
    return sanitize_identifier(type);
}

std::string TypeMapper::resolveExternalType(const std::string &package_spec,
                                            const std::string &interface_name,
                                            const std::string &type_name)
{
    if (!package_registry)
    {
        // No registry available - generate a placeholder
        return "uint32_t /* external: " + package_spec + "/" + interface_name + "::" + type_name + " */";
    }

    // Parse package ID from spec
    auto package_id = PackageId::parse(package_spec);
    if (!package_id)
    {
        return "uint32_t /* invalid package spec: " + package_spec + " */";
    }

    // Look up the package
    auto package = package_registry->get_package(*package_id);
    if (!package)
    {
        // Package not found - generate a placeholder
        return "uint32_t /* missing package: " + package_spec + " */";
    }

    // Get the C++ namespace for this package
    std::string cpp_namespace = package_id->to_cpp_namespace();

    // Build the fully qualified type reference
    // e.g., "::ext_my_dep_v0_1_0::a::foo"
    return "::" + cpp_namespace + "::" + interface_name + "::" + type_name;
}
