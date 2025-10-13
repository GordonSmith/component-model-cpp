#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <filesystem>
#include <optional>

// Forward declarations
struct InterfaceInfo;

/**
 * Represents a WIT package identifier with namespace, package name, and optional version.
 * Examples: "my:dep@0.1.0", "wasi:clocks@0.3.0", "foo:bar"
 */
struct PackageId
{
    std::string namespace_name; // e.g., "my", "wasi"
    std::string package_name;   // e.g., "dep", "clocks"
    std::string version;        // e.g., "0.1.0" (empty if no version)

    /**
     * Format as "namespace:package@version" or "namespace:package" if no version
     */
    std::string to_string() const;

    /**
     * Parse a package ID from string like "my:dep@0.1.0"
     * Returns nullopt if parsing fails
     */
    static std::optional<PackageId> parse(const std::string &spec);

    /**
     * Check if this package ID matches another
     * @param other The package ID to compare against
     * @param ignore_version If true, only compare namespace and package name
     */
    bool matches(const PackageId &other, bool ignore_version = false) const;

    /**
     * Generate a valid C++ namespace name from this package ID
     * e.g., "my:dep@0.1.0" -> "ext_my_dep_v0_1_0"
     */
    std::string to_cpp_namespace() const;

    bool operator==(const PackageId &other) const;
    bool operator<(const PackageId &other) const; // For use in std::map
};

/**
 * Represents a parsed WIT package with its interfaces and metadata
 */
struct WitPackage
{
    PackageId id;
    std::vector<InterfaceInfo> interfaces;
    std::map<std::string, InterfaceInfo *> interface_map; // interface name -> interface
    std::filesystem::path source_path;

    /**
     * Get an interface by name
     */
    InterfaceInfo *get_interface(const std::string &name);
    const InterfaceInfo *get_interface(const std::string &name) const;

    /**
     * Add an interface to this package
     */
    void add_interface(const InterfaceInfo &interface);

    /**
     * Rebuild the interface map to point to current vector elements
     * Call this after adding multiple interfaces or when vector may have reallocated
     */
    void rebuild_interface_map();
};

/**
 * Registry that manages multiple WIT packages and provides lookup capabilities
 */
class PackageRegistry
{
public:
    PackageRegistry() = default;

    /**
     * Load a WIT package from a file or directory
     * @param path Path to .wit file or directory containing .wit files
     * @return true if loading succeeded
     */
    bool load_package(const std::filesystem::path &path);

    /**
     * Get a package by its ID
     * @param id The package identifier to look up
     * @return Pointer to package, or nullptr if not found
     */
    WitPackage *get_package(const PackageId &id);
    const WitPackage *get_package(const PackageId &id) const;

    /**
     * Get a package by string specification (e.g., "my:dep@0.1.0")
     * @param spec The package spec string
     * @return Pointer to package, or nullptr if not found or invalid spec
     */
    WitPackage *get_package(const std::string &spec);
    const WitPackage *get_package(const std::string &spec) const;

    /**
     * Find an interface across all packages
     * @param package_spec Package identifier (e.g., "my:dep@0.1.0")
     * @param interface_name Name of the interface
     * @return Pointer to interface, or nullptr if not found
     */
    InterfaceInfo *resolve_interface(const std::string &package_spec,
                                     const std::string &interface_name);
    const InterfaceInfo *resolve_interface(const std::string &package_spec,
                                           const std::string &interface_name) const;

    /**
     * Get all loaded packages
     */
    const std::vector<std::unique_ptr<WitPackage>> &get_packages() const
    {
        return packages_;
    }

    /**
     * Check if a package is loaded
     */
    bool has_package(const PackageId &id) const;
    bool has_package(const std::string &spec) const;

    /**
     * Get all package IDs that have been loaded
     */
    std::vector<PackageId> get_package_ids() const;

    /**
     * Clear all loaded packages
     */
    void clear();

    /**
     * Infer package ID from directory structure or sibling files
     * Used when loading files without package declarations
     * @param path Path to the WIT file
     * @return Package ID if inferrable, nullopt otherwise
     */
    std::optional<PackageId> infer_package_from_path(const std::filesystem::path &path);

private:
    std::vector<std::unique_ptr<WitPackage>> packages_;
    std::map<std::string, WitPackage *> package_map_; // "namespace:package@version" -> package

    /**
     * Add a package to the registry
     */
    void add_package(std::unique_ptr<WitPackage> package);
};
