#include "dependency_resolver.hpp"
#include <fstream>
#include <regex>
#include <algorithm>
#include <iostream>
#include <functional>

std::vector<std::filesystem::path> DependencyResolver::discover_dependencies(
    const std::filesystem::path &root_path) const
{
    std::vector<std::filesystem::path> dependencies;

    // Determine the base directory
    std::filesystem::path base_dir;
    if (std::filesystem::is_directory(root_path))
    {
        base_dir = root_path;
    }
    else
    {
        base_dir = root_path.parent_path();
    }

    // Look for deps/ folder
    auto deps_dir = base_dir / "deps";
    if (!std::filesystem::exists(deps_dir) || !std::filesystem::is_directory(deps_dir))
    {
        return dependencies; // No dependencies
    }

    // Scan deps/ folder for WIT files and directories
    // According to WIT spec: deps/ is flat - no recursive deps/ folders
    for (const auto &entry : std::filesystem::directory_iterator(deps_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wit")
        {
            // Direct .wit file in deps/
            dependencies.push_back(entry.path());
        }
        else if (entry.is_directory())
        {
            // Directory in deps/ - find all .wit files within it (non-recursive for deps/)
            find_wit_files_recursive(entry.path(), dependencies, true);
        }
    }

    return dependencies;
}

std::filesystem::path DependencyResolver::find_root_wit_file(
    const std::filesystem::path &dir_path) const
{
    if (!std::filesystem::is_directory(dir_path))
    {
        return {};
    }

    // Look for WIT files in the directory
    for (const auto &entry : std::filesystem::directory_iterator(dir_path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wit")
        {
            // Check if this file declares a package
            auto package_name = extract_package_name(entry.path());
            if (!package_name.empty())
            {
                return entry.path();
            }
        }
    }

    // If no package declaration found, just return the first .wit file
    for (const auto &entry : std::filesystem::directory_iterator(dir_path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wit")
        {
            return entry.path();
        }
    }

    return {};
}

std::vector<std::filesystem::path> DependencyResolver::sort_by_dependencies(
    const std::vector<std::filesystem::path> &wit_files) const
{
    // Build dependency graph
    std::map<std::string, std::filesystem::path> package_to_file;
    std::map<std::string, std::set<std::string>> dependencies_map;

    // First pass: extract package names
    for (const auto &file : wit_files)
    {
        auto package_name = extract_package_name(file);
        if (!package_name.empty())
        {
            package_to_file[package_name] = file;
        }
    }

    // Second pass: extract dependencies
    for (const auto &file : wit_files)
    {
        auto package_name = extract_package_name(file);
        if (package_name.empty())
            continue;

        auto deps = extract_dependencies(file);
        dependencies_map[package_name] = deps;
    }

    // Simple topological sort using Kahn's algorithm
    std::vector<std::filesystem::path> sorted;
    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<void(const std::string &)> visit = [&](const std::string &package)
    {
        if (visited.count(package))
            return;
        if (visiting.count(package))
        {
            // Cycle detected - just add it anyway
            return;
        }

        visiting.insert(package);

        // Visit dependencies first
        if (dependencies_map.count(package))
        {
            for (const auto &dep : dependencies_map[package])
            {
                if (package_to_file.count(dep))
                {
                    visit(dep);
                }
            }
        }

        visiting.erase(package);
        visited.insert(package);

        if (package_to_file.count(package))
        {
            sorted.push_back(package_to_file[package]);
        }
    };

    // Visit all packages
    for (const auto &[package, file] : package_to_file)
    {
        visit(package);
    }

    // Add any files that don't have package declarations
    for (const auto &file : wit_files)
    {
        if (std::find(sorted.begin(), sorted.end(), file) == sorted.end())
        {
            sorted.push_back(file);
        }
    }

    return sorted;
}

void DependencyResolver::find_wit_files_recursive(
    const std::filesystem::path &dir,
    std::vector<std::filesystem::path> &results,
    bool is_deps_folder) const
{
    for (const auto &entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wit")
        {
            results.push_back(entry.path());
        }
        else if (entry.is_directory() && !is_deps_folder)
        {
            // Only recurse if we're not already in a deps/ folder
            // (deps/ folders are flat per WIT spec)
            find_wit_files_recursive(entry.path(), results, false);
        }
    }
}

std::string DependencyResolver::extract_package_name(
    const std::filesystem::path &wit_file) const
{
    std::ifstream file(wit_file);
    if (!file.is_open())
        return {};

    // Look for "package namespace:name@version" or "package namespace:name"
    std::regex package_regex(R"(^\s*package\s+([a-z][a-z0-9-]*:[a-z][a-z0-9-]*(?:@[0-9]+\.[0-9]+\.[0-9]+[a-z0-9.-]*)?))");

    std::string line;
    while (std::getline(file, line))
    {
        std::smatch match;
        if (std::regex_search(line, match, package_regex))
        {
            return match[1].str();
        }
    }

    return {};
}

std::set<std::string> DependencyResolver::extract_dependencies(
    const std::filesystem::path &wit_file) const
{
    std::set<std::string> deps;
    std::ifstream file(wit_file);
    if (!file.is_open())
        return deps;

    // Look for "use namespace:name@version" or "import namespace:name@version"
    // Pattern matches: use my:dep@0.1.0.{...} or import my:dep/interface@0.1.0
    std::regex use_regex(R"(\b(?:use|import)\s+([a-z][a-z0-9-]*:[a-z][a-z0-9-]*(?:@[0-9]+\.[0-9]+\.[0-9]+[a-z0-9.-]*)?))");

    std::string line;
    while (std::getline(file, line))
    {
        std::smatch match;
        std::string::const_iterator search_start(line.cbegin());
        while (std::regex_search(search_start, line.cend(), match, use_regex))
        {
            deps.insert(match[1].str());
            search_start = match.suffix().first;
        }
    }

    return deps;
}
