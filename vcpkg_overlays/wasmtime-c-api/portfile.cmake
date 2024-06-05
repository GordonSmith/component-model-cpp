if (WIN32)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/bytecodealliance/wasmtime/releases/download/v${VERSION}/wasmtime-v${VERSION}-x86_64-windows-c-api.zip"
        FILENAME "wasmtime-v${VERSION}-x86_64-windows-c-api.zip"
        SHA512 4ebe734178ebc14d647ac1646f7a6aa9058fecb542e34200062c9d9a338b4e94a9c071b1bb7b148d7fc66851903769022e775833626df4c9e4ef073417aa4199
    )
elseif (APPLE)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/bytecodealliance/wasmtime/releases/download/v${VERSION}/wasmtime-v${VERSION}-x86_64-macos-c-api.tar.xz"
        FILENAME "wasmtime-v${VERSION}-x86_64-macos-c-api.tar.xz"
        SHA512 ec3c3c71bbb295c6ecda7ce5d8e9624e5fe108081f5ca21ccfd0a1095524676f45e7d401ed976e8298e903565040ec94fa8843969835bdbfe659412d866d8532
    )
elseif (LINUX)
    vcpkg_download_distfile(ARCHIVE
        URLS "https://github.com/bytecodealliance/wasmtime/releases/download/v${VERSION}/wasmtime-v${VERSION}-x86_64-linux-c-api.tar.xz"
        FILENAME "wasmtime-v${VERSION}-x86_64-linux-c-api.tar.xz"
        SHA512 2d100ce77f971209a99146ea6bc94f94096b7b2dbd296755f9f873f2ac2e4907dd8fb2a5eefb863ac1c633ad0ffe4ad3126e0f2952836be4a03bcb37c4e11299
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

