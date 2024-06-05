#!/bin/bash

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )"
ROOT_DIR="$(pwd)"

echo "SCRIPT_DIR: ${SCRIPT_DIR}"
echo "ROOT_DIR: $ROOT_DIR"

function cleanup()
{
    CONTAINER_ID=$(jq -r .containerId ${SCRIPT_DIR}/up.json)
    docker rm -f "${CONTAINER_ID}"
    rm ${SCRIPT_DIR}/up.json
}

trap cleanup EXIT

npx -y @devcontainers/cli up --workspace-folder ${SCRIPT_DIR} | jq . > "${SCRIPT_DIR}/up.json"

npx -y @devcontainers/cli exec --workspace-folder ${SCRIPT_DIR} --config wasm/.devcontainer/devcontainer.json mkdir -p ./build 
npx -y @devcontainers/cli exec --workspace-folder ${SCRIPT_DIR} --config wasm/.devcontainer/devcontainer.json sh -c 'cmake --preset wasi-sdk'
npx -y @devcontainers/cli exec --workspace-folder ${SCRIPT_DIR} --config wasm/.devcontainer/devcontainer.json cmake --build ./build --parallel
