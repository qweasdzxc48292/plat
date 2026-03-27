#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN_PATH="${ROOT_DIR}/smart_home_hub"
CFG_PATH="${1:-${ROOT_DIR}/config/hub_config.json}"

if [ ! -x "${BIN_PATH}" ]; then
    echo "binary not found: ${BIN_PATH}"
    exit 1
fi

exec "${BIN_PATH}" "${CFG_PATH}"

