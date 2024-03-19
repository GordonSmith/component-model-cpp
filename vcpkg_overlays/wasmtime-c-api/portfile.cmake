if (WIN32)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/bytecodealliance/wasmtime/releases/download/v${VERSION}/wasmtime-v${VERSION}-x86_64-windows-c-api.zip"
        FILENAME "wasmtime-v${VERSION}-x86_64-windows-c-api.zip"
        SHA512 0
    )
elseif (APPLE)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/bytecodealliance/wasmtime/releases/download/v${VERSION}/wasmtime-v${VERSION}-x86_64-macos-c-api.tar.xz"
        FILENAME "wasmtime-v${VERSION}-x86_64-macos-c-api.tar.xz"
        SHA512 0
    )
elseif (LINUX)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/bytecodealliance/wasmtime/releases/download/v${VERSION}/wasmtime-v${VERSION}-x86_64-linux-c-api.tar.xz"
        FILENAME "wasmtime-v${VERSION}-x86_64-linux-c-api.tar.xz"
        SHA512 35aa6de438fead5ab043eb9de5bb688c0c7da0ba3b8336f6f670675087b44af1385559037162cf5951cb9dadbd201db18c0678275bd7704fdc078bc75bc08326
    )
endif()

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(COPY ${SOURCE_PATH}/include/. DESTINATION ${CURRENT_PACKAGES_DIR}/include/wasmtime-c-api)
if (WIN32)
    file(COPY ${SOURCE_PATH}/lib/. DESTINATION ${CURRENT_PACKAGES_DIR}/debug/bin)
    file(COPY ${SOURCE_PATH}/lib/. DESTINATION ${CURRENT_PACKAGES_DIR}/bin)
else ()
    file(COPY ${SOURCE_PATH}/lib/. DESTINATION ${CURRENT_PACKAGES_DIR}/debug/lib)
    file(COPY ${SOURCE_PATH}/lib/. DESTINATION ${CURRENT_PACKAGES_DIR}/lib)
endif ()

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/wasmtime-c-api RENAME copyright)

