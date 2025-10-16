#include "package_registry.hpp"
#include "wit_parser.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <fstream>

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
    // Reserve space to minimize reallocations
    if (interfaces.capacity() == interfaces.size())
    {
        interfaces.reserve(interfaces.size() * 2 + 8);
    }

    interfaces.push_back(interface);

    // Rebuild the entire map to ensure all pointers are valid
    // This is necessary because vector may have reallocated
    rebuild_interface_map();
}

void WitPackage::rebuild_interface_map()
{
    interface_map.clear();
    for (auto &interface : interfaces)
    {
        interface_map[interface.name] = &interface;
    }
}

// PackageRegistry implementation

bool PackageRegistry::load_package(const std::filesystem::path &path)
{
    try
    {
        // Parse the WIT file
        auto parse_result = WitGrammarParser::parse(path.string());

        // If no package name defined, try to infer it from directory structure or sibling files
        PackageId package_id_val;
        if (parse_result.packageName.empty())
        {
            auto inferred_id = infer_package_from_path(path);
            if (!inferred_id)
            {
                // Can't infer package - skip this file
                return true; // Not an error, just skip
            }
            package_id_val = *inferred_id;
        }
        else
        {
            // Parse package ID from package name
            auto package_id = PackageId::parse(parse_result.packageName);
            if (!package_id)
            {
                return false;
            }
            package_id_val = *package_id;
        }

        // Check if package already exists in registry
        auto *existing_package = get_package(package_id_val);

        if (existing_package)
        {
            // Package already exists - merge interfaces into it
            for (const auto &interface : parse_result.interfaces)
            {
                existing_package->add_interface(interface);
            }
        }
        else
        {
            // Create new package
            auto package = std::make_unique<WitPackage>();
            package->id = package_id_val;
            package->source_path = path;

            // Add all interfaces
            for (const auto &interface : parse_result.interfaces)
            {
                package->add_interface(interface);
            }

            // Add to registry
            add_package(std::move(package));
        }

        return true;
    }
    catch (const std::exception &e)
    {
        // Failed to parse
        return false;
    }
}

std::optional<PackageId> PackageRegistry::infer_package_from_path(const std::filesystem::path &path)
{
    // Check sibling files in the same directory for package declarations
    auto parent_dir = path.parent_path();

    try
    {
        for (const auto &entry : std::filesystem::directory_iterator(parent_dir))
        {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".wit" &&
                entry.path() != path)
            {
                try
                {
                    // Read first 1KB of the file to check for package declaration
                    // This avoids full parsing which could cause issues
                    std::ifstream file(entry.path());
                    if (!file.is_open())
                        continue;

                    std::string content;
                    std::string line;
                    size_t bytes_read = 0;
                    const size_t MAX_BYTES = 1024;

                    while (std::getline(file, line) && bytes_read < MAX_BYTES)
                    {
                        bytes_read += line.size() + 1;
                        content += line + "\n";

                        // Look for "package namespace:name@version;" pattern
                        size_t package_pos = line.find("package");
                        if (package_pos != std::string::npos)
                        {
                            // Extract the package name
                            size_t start = package_pos + 7; // After "package"
                            // Skip whitespace
                            while (start < line.size() && std::isspace(line[start]))
                                start++;

                            // Find the semicolon
                            size_t end = line.find(';', start);
                            if (end != std::string::npos)
                            {
                                std::string package_spec = line.substr(start, end - start);
                                // Trim trailing whitespace
                                while (!package_spec.empty() && std::isspace(package_spec.back()))
                                    package_spec.pop_back();

                                auto package_id = PackageId::parse(package_spec);
                                if (package_id)
                                {
                                    return package_id;
                                }
                            }
                        }
                    }
                }
                catch (...)
                {
                    // Ignore errors reading sibling files
                    continue;
                }
            }
        }
    }
    catch (...)
    {
        // Directory iteration error
    }

    return std::nullopt;
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
