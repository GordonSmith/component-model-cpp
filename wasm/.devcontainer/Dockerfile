# See:  https://github.com/devcontainers/images/blob/main/src/cpp/.devcontainer/Dockerfile
ARG VARIANT=ubuntu-22.04
FROM mcr.microsoft.com/devcontainers/base:${VARIANT}
USER root

# Install needed packages. Use a separate RUN statement to add your own dependencies.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install \
    autoconf \
    autogen \
    automake \
    clang \
    cmake \
    curl \
    git \
    libtool \
    lld \
    llvm \
    make \
    ninja-build \
    tar \
    wget \
    xz-utils \
    && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*


# List of current vertsion can be found in https://github.com/WebAssembly/wasi-sdk/releases  ---
ARG WASI_VERSION=25
ARG WASI_MINOR_VERSION=0
ARG WASI_VERSION_FULL=${WASI_VERSION}.${WASI_MINOR_VERSION}
RUN wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_VERSION}/wasi-sdk-${WASI_VERSION_FULL}-x86_64-linux.tar.gz
RUN tar xvf wasi-sdk-${WASI_VERSION_FULL}-x86_64-linux.tar.gz && rm wasi-sdk-${WASI_VERSION_FULL}-x86_64-linux.tar.gz
RUN mv wasi-sdk-${WASI_VERSION_FULL}-x86_64-linux /wasi-sdk
    
USER vscode
ARG USERNAME=vscode

RUN curl --proto '=https' --tlsv1.3 https://sh.rustup.rs -sSf | sh -s -- -y

SHELL ["/bin/bash", "--login", "-c"]

# List of current vertsion can be found in https://github.com/bytecodealliance/wit-bindgen/releases  ---
RUN cargo install wasm-tools
RUN cargo install wit-bindgen-cli

# # Install wasmtime
# ENV WASMTIME_VERSION="${WASMTIME_VERSION:-"v9.0.4"}"
# ENV INSTALLER=./wasmtime-install.sh
# RUN curl https://wasmtime.dev/install.sh -L --output ${INSTALLER}
# RUN chmod a+x ${INSTALLER}
# RUN ${INSTALLER} --version ${WASMTIME_VERSION}
# RUN cp ${HOME}/.wasmtime/bin/wasmtime /usr/bin/wasmtime
