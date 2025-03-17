vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-${VERSION}-x86_64-linux.tar.gz"
    FILENAME "wasi-sdk-${VERSION}-x86_64-linux.tar.gz"
    SHA512 716acc4b737ad6f51c6b32c3423612c03df9a3165bde3d6e24df5c86779b8be9463f5a79e620f2fc49707275563a6c9710242caca27e1ad9dd2c69e8fce8a766
)

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(COPY ${SOURCE_PATH}/. DESTINATION ${CURRENT_PACKAGES_DIR}/wasi-sdk)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/wasi-sdk/share/wasi-sysroot/include/net" "${CURRENT_PACKAGES_DIR}/wasi-sdk/share/wasi-sysroot/include/scsi")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/share/misc/config.guess DESTINATION ${CURRENT_PACKAGES_DIR}/share/wasi-sdk RENAME copyright)
