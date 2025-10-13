#pragma once

#include "types.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>

// Forward declaration
class PackageRegistry;

// Type mapper from WIT types to cmcpp types
class TypeMapper
{
private:
    static const std::vector<InterfaceInfo> *all_interfaces;
    static PackageRegistry *package_registry;

public:
    static void setInterfaces(const std::vector<InterfaceInfo> *interfaces);
    static void setPackageRegistry(PackageRegistry *registry);
    static std::string mapType(const std::string &witType, const InterfaceInfo *iface = nullptr);

    // NEW: Resolve external package type reference
    // e.g., "my:dep/a@0.1.0.{foo}" -> "::ext_my_dep_v0_1_0::a::foo"
    static std::string resolveExternalType(const std::string &package_spec,
                                           const std::string &interface_name,
                                           const std::string &type_name);
};
