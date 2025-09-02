# https://github.com/WebAssembly/wasi-sdk/releases

if(APPLE)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-${VERSION}-arm64-macos.tar.gz"
        FILENAME "wasi-sdk-${VERSION}-x86_64-linux.tar.gz"
        SHA512 fa4852de1995eaaf5aa57dab9896604a27f157b6113ca0daa27fe7588f4276e18362e650bdb6c65fd83f14d4b8347f8134c9b531a8b872ad83c18d481eeef6c5
    )
elseif(UNIX)
vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-${VERSION}-x86_64-linux.tar.gz"
    FILENAME "wasi-sdk-${VERSION}-x86_64-linux.tar.gz"
    SHA512 716acc4b737ad6f51c6b32c3423612c03df9a3165bde3d6e24df5c86779b8be9463f5a79e620f2fc49707275563a6c9710242caca27e1ad9dd2c69e8fce8a766
)

elseif(WIN32)
vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-${VERSION}-x86_64-windows.tar.gz"
    FILENAME "wasi-sdk-${VERSION}-x86_64-windows.tar.gz"
    SHA512 e8bdae827dbbb967bf9815603aeff76ac40344c79cf6a1c388e63931c77cdc5560860c6f2ec74f3c7895fab08b93940f60e9e26365b6f4ba354ca3a921803be7
)
endif()

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(COPY ${SOURCE_PATH}/. DESTINATION ${CURRENT_PACKAGES_DIR}/wasi-sdk)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/wasi-sdk/share/wasi-sysroot/include/net" "${CURRENT_PACKAGES_DIR}/wasi-sdk/share/wasi-sysroot/include/scsi")

# Handle copyright
file(INSTALL ${SOURCE_PATH}/share/misc/config.guess DESTINATION ${CURRENT_PACKAGES_DIR}/share/wasi-sdk RENAME copyright)
