#include "package_registry.hpp"
#include "wit_parser.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

// PackageId implementation

std::string PackageId::to_string() const
{
    std::string result = namespace_name + ":" + package_name;
    if (!version.empty())
    {
        result += "@" + version;
    }
    return result;
}

std::optional<PackageId> PackageId::parse(const std::string &spec)
{
    // Expected format: "namespace:package@version" or "namespace:package"
    // Also handle "namespace:package/interface@version" by extracting just the package part

    std::regex pattern(R"(([^:/@]+):([^:/@]+)(?:@([^/@]+))?)");
    std::smatch match;

    if (std::regex_search(spec, match, pattern))
    {
        PackageId id;
        id.namespace_name = match[1].str();
        id.package_name = match[2].str();
        if (match[3].matched)
        {
            id.version = match[3].str();
        }
        return id;
    }

    return std::nullopt;
}

bool PackageId::matches(const PackageId &other, bool ignore_version) const
{
    if (namespace_name != other.namespace_name)
        return false;
    if (package_name != other.package_name)
        return false;
    if (ignore_version)
        return true;
    return version == other.version;
}

std::string PackageId::to_cpp_namespace() const
{
    // Convert "my:dep@0.1.0" to "ext_my_dep_v0_1_0"
    std::string result = "ext_" + namespace_name + "_" + package_name;

    if (!version.empty())
    {
        // Replace dots with underscores and add version prefix
        std::string version_part = version;
        std::replace(version_part.begin(), version_part.end(), '.', '_');
        std::replace(version_part.begin(), version_part.end(), '-', '_');
        result += "_v" + version_part;
    }

    return result;
}

bool PackageId::operator==(const PackageId &other) const
{
    return namespace_name == other.namespace_name &&
           package_name == other.package_name &&
           version == other.version;
}

bool PackageId::operator<(const PackageId &other) const
{
    if (namespace_name != other.namespace_name)
    {
        return namespace_name < other.namespace_name;
    }
    if (package_name != other.package_name)
    {
        return package_name < other.package_name;
    }
    return version < other.version;
}

// WitPackage implementation

InterfaceInfo *WitPackage::get_interface(const std::string &name)
{
    auto it = interface_map.find(name);
    if (it != interface_map.end())
    {
        return it->second;
    }
    return nullptr;
}

const InterfaceInfo *WitPackage::get_interface(const std::string &name) const
{
    auto it = interface_map.find(name);
    if (it != interface_map.end())
    {
        return it->second;
    }
    return nullptr;
}

void WitPackage::add_interface(const InterfaceInfo &interface)
{
    interfaces.push_back(interface);
    interface_map[interface.name] = &interfaces.back();
}

// PackageRegistry implementation

bool PackageRegistry::load_package(const std::filesystem::path &path)
{
    try
    {
        // Parse the WIT file
        auto parse_result = WitGrammarParser::parse(path.string());

        // Skip if no package name defined
        if (parse_result.packageName.empty())
        {
            return false;
        }

        // Parse package ID from package name
        auto package_id = PackageId::parse(parse_result.packageName);
        if (!package_id)
        {
            return false;
        }

        // Create package
        auto package = std::make_unique<WitPackage>();
        package->id = *package_id;
        package->source_path = path;

        // Add all interfaces
        for (const auto &interface : parse_result.interfaces)
        {
            package->add_interface(interface);
        }

        // Add to registry
        add_package(std::move(package));

        return true;
    }
    catch (const std::exception &e)
    {
        // Failed to parse
        return false;
    }
}

WitPackage *PackageRegistry::get_package(const PackageId &id)
{
    auto key = id.to_string();
    auto it = package_map_.find(key);
    if (it != package_map_.end())
    {
        return it->second;
    }
    return nullptr;
}

const WitPackage *PackageRegistry::get_package(const PackageId &id) const
{
    auto key = id.to_string();
    auto it = package_map_.find(key);
    if (it != package_map_.end())
    {
        return it->second;
    }
    return nullptr;
}

WitPackage *PackageRegistry::get_package(const std::string &spec)
{
    auto id = PackageId::parse(spec);
    if (!id)
        return nullptr;
    return get_package(*id);
}

const WitPackage *PackageRegistry::get_package(const std::string &spec) const
{
    auto id = PackageId::parse(spec);
    if (!id)
        return nullptr;
    return get_package(*id);
}

InterfaceInfo *PackageRegistry::resolve_interface(const std::string &package_spec,
                                                  const std::string &interface_name)
{
    auto package = get_package(package_spec);
    if (!package)
        return nullptr;
    return package->get_interface(interface_name);
}

const InterfaceInfo *PackageRegistry::resolve_interface(const std::string &package_spec,
                                                        const std::string &interface_name) const
{
    auto package = get_package(package_spec);
    if (!package)
        return nullptr;
    return package->get_interface(interface_name);
}

bool PackageRegistry::has_package(const PackageId &id) const
{
    return package_map_.find(id.to_string()) != package_map_.end();
}

bool PackageRegistry::has_package(const std::string &spec) const
{
    auto id = PackageId::parse(spec);
    if (!id)
        return false;
    return has_package(*id);
}

std::vector<PackageId> PackageRegistry::get_package_ids() const
{
    std::vector<PackageId> ids;
    for (const auto &package : packages_)
    {
        ids.push_back(package->id);
    }
    return ids;
}

void PackageRegistry::clear()
{
    packages_.clear();
    package_map_.clear();
}

void PackageRegistry::add_package(std::unique_ptr<WitPackage> package)
{
    auto key = package->id.to_string();
    auto *ptr = package.get();
    packages_.push_back(std::move(package));
    package_map_[key] = ptr;
}
