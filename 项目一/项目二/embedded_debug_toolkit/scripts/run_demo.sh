#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

if [ ! -x "./embedded_debug_toolkit" ]; then
    echo "backend binary not found, building first..."
    make
fi

IFACE="${EDT_IFACE:-eth0}"
DURATION="${EDT_DURATION:-20}"

echo "Running embedded_debug_toolkit on iface=$IFACE duration=${DURATION}s"
exec ./embedded_debug_toolkit --iface "$IFACE" --duration "$DURATION"
