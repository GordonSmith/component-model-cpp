vcpkg_download_distfile(ARCHIVE
    URLS "https://codeload.github.com/bytecodealliance/wasmtime-cpp/zip/da579e78f799aca0a472875b7e348f74b3a04145"
    FILENAME "wasmome-cpp-da579e78f799acaoa472875b7e348f74b3a04145.zip"
    SHA512 917da1e41ea3e62c4167e1e42afe1d26537bb8351acddbd59cfe24d5ab535d11ce0e0980d38b5fbfca008303b50d552d042f1db835d8d13d59757b269c4f3887
)

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(COPY ${SOURCE_PATH}/include/. DESTINATION ${CURRENT_PACKAGES_DIR}/include/wasmtime-cpp-api)

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/wasmtime-cpp-api RENAME copyright)

