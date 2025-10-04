# Release Process

This document describes the automated release process for cmcpp.

## Overview

The project uses automated CI/CD to build and publish release packages when version tags are pushed to the repository. The process is managed by GitHub Actions and Release Please.

## Release Workflow

### 1. Conventional Commits

The project uses [Conventional Commits](https://www.conventionalcommits.org/) to automatically generate changelogs and determine version bumps:

- `feat:` - New features (minor version bump)
- `fix:` - Bug fixes (patch version bump)
- `feat!:` or `BREAKING CHANGE:` - Breaking changes (major version bump)
- `docs:`, `chore:`, `style:`, `refactor:`, `test:` - No version bump

**Examples:**
```
feat: add support for async streaming operations
fix: correct memory alignment in string encoding
feat!: change API to use std::span instead of raw pointers
docs: update installation instructions
```

### 2. Release Please Automation

[Release Please](https://github.com/googleapis/release-please) monitors commits and automatically:

1. Opens/updates a **Release PR** when unreleased changes accumulate
2. Updates `CHANGELOG.md` with release notes
3. Bumps version numbers in relevant files
4. Creates a Git tag when the Release PR is merged

**No manual version bumping required!**

### 3. Automated Package Building

When a version tag (e.g., `v0.2.0`) is pushed, the GitHub Actions release workflow automatically:

#### Linux Build (Ubuntu 24.04)
- Builds Release configuration
- Runs all tests
- Creates packages:
  - `cmcpp-<version>-Linux.tar.gz` (portable tarball)
  - `cmcpp_<version>_amd64.deb` (Debian/Ubuntu package)
  - `cmcpp-<version>-Linux.rpm` (Red Hat/Fedora package)
- Uploads packages to GitHub Release

#### Windows Build (Windows Server 2022)
- Builds Release configuration with Visual Studio 2022
- Runs all tests
- Creates packages:
  - `cmcpp-<version>-win64.zip` (Windows archive)
- Uploads packages to GitHub Release

#### macOS Build (macOS 14 ARM)
- Builds Release configuration
- Runs all tests
- Creates packages:
  - `cmcpp-<version>-Darwin.tar.gz` (macOS tarball)
  - `cmcpp-<version>-Darwin.zip` (macOS archive)
- Uploads packages to GitHub Release

### 4. Package Contents

All packages include:

- **Headers**: Complete C++20 header-only library
  - `include/cmcpp/` - Component Model types
  - `include/cmcpp.hpp` - Main header
  - `include/wamr.hpp` - WAMR integration
  - `include/boost/pfr/` - Boost.PFR for reflection

- **CMake Integration**:
  - `lib/cmake/cmcpp/cmcppConfig.cmake`
  - `lib/cmake/cmcpp/cmcppConfigVersion.cmake`
  - `lib/cmake/cmcpp/cmcppTargets.cmake`

- **Tools** (if `BUILD_GRAMMAR=ON`):
  - `bin/wit-codegen` - WIT code generator executable

- **Documentation**:
  - `LICENSE` - Apache 2.0 license
  - `README.md` - Usage documentation

## Making a Release

### Automated Process (Recommended)

1. **Commit changes** using conventional commit messages:
   ```bash
   git commit -m "feat: add new stream processing API"
   git push origin main
   ```

2. **Wait for Release PR**: Release Please will automatically open/update a PR titled "chore: release X.Y.Z"

3. **Review the Release PR**:
   - Check the generated `CHANGELOG.md`
   - Verify version bumps are correct
   - Review the changes

4. **Merge the Release PR**: This triggers:
   - Tag creation (e.g., `v0.2.0`)
   - Automated package builds
   - GitHub Release creation
   - Package uploads

5. **Verify the Release**: Check https://github.com/GordonSmith/component-model-cpp/releases

### Manual Tag Process (Not Recommended)

If you need to manually create a release:

```bash
# Ensure your working directory is clean
git status

# Create and push a tag
git tag v0.2.0
git push origin v0.2.0
```

This will trigger the automated build and upload process.

## Release Checklist

Before merging a Release PR, verify:

- [ ] All CI checks are passing (Ubuntu, Windows, macOS)
- [ ] Version number follows semantic versioning
- [ ] `CHANGELOG.md` is accurate and complete
- [ ] Breaking changes are clearly documented
- [ ] New features are documented in README

## Troubleshooting

### Build Fails on One Platform

If a platform-specific build fails:

1. Check the GitHub Actions logs for that platform
2. Fix the issue in a new PR
3. Release Please will include the fix in the next release

### Wrong Version Number

If Release Please calculates the wrong version:

1. Check commit messages follow conventional commits format
2. If a breaking change wasn't marked with `!` or `BREAKING CHANGE:`, the version will be incorrect
3. Create a new commit with the correct type to trigger a new Release PR

### Package Not Uploaded

If a package is missing from a release:

1. Check the GitHub Actions workflow logs
2. Verify CPack succeeded in creating the package
3. Check the `softprops/action-gh-release` step logs
4. Re-run the failed workflow if needed

## CI/CD Configuration

### GitHub Actions Workflows

**`.github/workflows/release.yml`**: Triggered on version tags
- Builds for Linux, Windows, and macOS
- Runs tests on all platforms
- Creates platform-specific packages
- Uploads to GitHub Releases

**`.github/workflows/ubuntu.yml`**: Continuous integration for Linux
- Runs on every push and PR
- Does not create packages

**`.github/workflows/windows.yml`**: Continuous integration for Windows
- Runs on every push and PR
- Does not create packages

**`.github/workflows/macos.yml`**: Continuous integration for macOS
- Runs on every push and PR
- Does not create packages

### Required Secrets

The release workflow requires:

- `GITHUB_TOKEN` - Automatically provided by GitHub Actions (no setup needed)

### Permissions

The release workflow requires:

- `contents: write` - To upload release assets

These are configured in `.github/workflows/release.yml`.

## Version History

Versions follow [Semantic Versioning](https://semver.org/):

- **Major** (X.0.0): Breaking changes
- **Minor** (0.X.0): New features (backward compatible)
- **Patch** (0.0.X): Bug fixes

See `CHANGELOG.md` for complete version history.

## Post-Release Tasks

After a successful release:

1. **Verify Packages**: Download and test packages from GitHub Releases
2. **Update Documentation**: Ensure README reflects new version
3. **Announce**: Share release notes with users
4. **Monitor Issues**: Watch for bug reports related to new release

## Future Enhancements

Planned improvements to the release process:

- [ ] Publish to vcpkg registry
- [ ] Publish to Conan Center
- [ ] Docker images with pre-installed cmcpp
- [ ] Signed packages for security
- [ ] Checksum files for verification
- [ ] NuGet packages for Windows C++ developers

## References

- [Release Please Documentation](https://github.com/googleapis/release-please)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Semantic Versioning](https://semver.org/)
- [CPack Documentation](https://cmake.org/cmake/help/latest/module/CPack.html)
- [GitHub Actions - Creating Releases](https://docs.github.com/en/actions/using-workflows/events-that-trigger-workflows#release)
