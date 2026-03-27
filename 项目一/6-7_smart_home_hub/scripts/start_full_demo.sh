#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CFG_PATH="${1:-${ROOT_DIR}/config/hub_config.json}"
HUB_BIN="${ROOT_DIR}/smart_home_hub"
GUI_BIN="${ROOT_DIR}/qt_gui/qt59_control_panel/qt59_control_panel"

if [ ! -x "${HUB_BIN}" ]; then
    echo "hub binary not found: ${HUB_BIN}"
    exit 1
fi

echo "starting hub..."
"${HUB_BIN}" "${CFG_PATH}" &
HUB_PID=$!
echo "hub pid=${HUB_PID}"

if [ -x "${GUI_BIN}" ]; then
    echo "starting qt gui..."
    "${GUI_BIN}" &
else
    echo "qt gui not found, skip: ${GUI_BIN}"
fi

wait "${HUB_PID}"

