#pragma once

#include <filesystem>
#include <vector>
#include <set>
#include <map>
#include <string>

/**
 * Resolves WIT package dependencies by discovering files in deps/ folders
 */
class DependencyResolver
{
public:
    /**
     * Find all WIT files in a deps/ directory structure
     * According to WIT spec: deps/ is a flat structure where each dependency
     * may be a file or directory, but directories don't have recursive deps/ folders
     *
     * @param root_path Path to the root WIT file or directory
     * @return Vector of paths to dependency WIT files
     */
    std::vector<std::filesystem::path> discover_dependencies(
        const std::filesystem::path &root_path) const;

    /**
     * Find the root WIT file in a directory
     * Looks for a file that declares a package (has "package" statement)
     *
     * @param dir_path Path to directory
     * @return Path to root WIT file, or empty if not found
     */
    std::filesystem::path find_root_wit_file(
        const std::filesystem::path &dir_path) const;

    /**
     * Sort WIT files by dependency order (dependencies first)
     * This is a simple topological sort based on package references
     *
     * @param wit_files List of WIT file paths to sort
     * @return Sorted list with dependencies first
     */
    std::vector<std::filesystem::path> sort_by_dependencies(
        const std::vector<std::filesystem::path> &wit_files) const;

private:
    /**
     * Recursively find all .wit files in a directory
     */
    void find_wit_files_recursive(
        const std::filesystem::path &dir,
        std::vector<std::filesystem::path> &results,
        bool is_deps_folder = false) const;

    /**
     * Extract package name from a WIT file (simple regex-based extraction)
     */
    std::string extract_package_name(const std::filesystem::path &wit_file) const;

    /**
     * Extract external package references from a WIT file
     */
    std::set<std::string> extract_dependencies(const std::filesystem::path &wit_file) const;
};
