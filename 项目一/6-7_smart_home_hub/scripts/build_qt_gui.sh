#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
GUI_DIR="${ROOT_DIR}/qt_gui/qt59_control_panel"

if [ ! -f "${GUI_DIR}/qt59_control_panel.pro" ]; then
    echo "qt project not found: ${GUI_DIR}/qt59_control_panel.pro"
    exit 1
fi

cd "${GUI_DIR}"
qmake qt59_control_panel.pro
if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
else
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
fi
make -j"${JOBS}"

echo "qt gui built at: ${GUI_DIR}/qt59_control_panel"
