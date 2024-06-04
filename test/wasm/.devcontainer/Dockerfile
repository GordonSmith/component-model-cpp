# See:  https://github.com/devcontainers/images/blob/main/src/cpp/.devcontainer/Dockerfile
ARG VARIANT=ubuntu-22.04
FROM mcr.microsoft.com/devcontainers/base:${VARIANT}
USER root

# Install needed packages. Use a separate RUN statement to add your own dependencies.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install cmake curl git tar xz-utils ninja-build \
    && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*

ARG USERNAME=vscode

# Install wasi-sdk https://github.com/WebAssembly/wasi-sdk
ARG WASI_VERSION="21"
ENV WASI_VERSION_FULL="${WASI_VERSION}.0"
ENV WASI_LOCATION="${LOCATION:-"/usr/local"}/lib"
ENV WASI_FILE="wasi-sdk-${WASI_VERSION_FULL}-linux.tar.gz"
ENV WASI_SDK_PREFIX="/usr/local/lib/wasi-sdk-${WASI_VERSION_FULL}"
RUN curl -L https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_VERSION}/${WASI_FILE} --output ${WASI_FILE}
RUN tar -C ${WASI_LOCATION} -xvf ${WASI_FILE}
RUN rm ${WASI_FILE}

# Install wit-bindgen https://github.com/bytecodealliance/wit-bindgen
ARG WIT_BINDGEN_VERSION="0.22.0"
RUN curl -L https://github.com/bytecodealliance/wit-bindgen/releases/download/v${WIT_BINDGEN_VERSION}/wit-bindgen-${WIT_BINDGEN_VERSION}-x86_64-linux.tar.gz --output wit-bindgen-${WIT_BINDGEN_VERSION}-x86_64-linux.tar.gz
RUN tar -xvzf wit-bindgen-${WIT_BINDGEN_VERSION}-x86_64-linux.tar.gz
RUN cp wit-bindgen-${WIT_BINDGEN_VERSION}-x86_64-linux/wit-bindgen ${LOCATION}/bin/wit-bindgen
RUN rm -r wit-bindgen-${WIT_BINDGEN_VERSION}-x86_64-linux*

# Install wasm-tools https://github.com/bytecodealliance/wasm-tools
ARG WASM_TOOLS_VERSION="1.201.0"
RUN curl -L https://github.com/bytecodealliance/wasm-tools/releases/download/v${WASM_TOOLS_VERSION}/wasm-tools-${WASM_TOOLS_VERSION}-x86_64-linux.tar.gz --output wasm-tools-${WASM_TOOLS_VERSION}-x86_64-linux.tar.gz
RUN tar -xvzf wasm-tools-${WASM_TOOLS_VERSION}-x86_64-linux.tar.gz
RUN cp wasm-tools-${WASM_TOOLS_VERSION}-x86_64-linux/wasm-tools ${LOCATION}/bin/wasm-tools
RUN rm -r wasm-tools-${WASM_TOOLS_VERSION}-x86_64-linux*

# Install wasi adapter https://github.com/bytecodealliance/wasmtime
ARG WASMTIME_VERSION="19.0.0"
RUN curl -L -O https://github.com/bytecodealliance/wasmtime/releases/download/v${WASMTIME_VERSION}/wasi_snapshot_preview1.reactor.wasm
RUN mv wasi_snapshot_preview1.reactor.wasm ${LOCATION}/lib/wasi_snapshot_preview1.reactor.wasm

# # Install wasmtime
# ENV WASMTIME_VERSION="${WASMTIME_VERSION:-"v9.0.4"}"
# ENV INSTALLER=./wasmtime-install.sh
# RUN curl https://wasmtime.dev/install.sh -L --output ${INSTALLER}
# RUN chmod a+x ${INSTALLER}
# RUN ${INSTALLER} --version ${WASMTIME_VERSION}
# RUN cp ${HOME}/.wasmtime/bin/wasmtime /usr/bin/wasmtime

