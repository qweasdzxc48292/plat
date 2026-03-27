#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
BUILD_ROOT="$ROOT_DIR/qt_gui/build"
APP_BUILD="$BUILD_ROOT/app"
PLUGIN_BUILD="$BUILD_ROOT/plugins"

mkdir -p "$APP_BUILD" "$PLUGIN_BUILD"

if ! command -v qmake >/dev/null 2>&1; then
    echo "qmake not found. Please install Qt 5.9+ toolchain."
    exit 1
fi

echo "[1/4] Build debug_workbench"
cd "$APP_BUILD"
qmake "$ROOT_DIR/qt_gui/debug_workbench/debug_workbench.pro"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

echo "[2/4] Build logic_view_plugin"
cd "$PLUGIN_BUILD"
qmake "$ROOT_DIR/qt_gui/plugins/logic_view_plugin/logic_view_plugin.pro"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

echo "[3/4] Build system_monitor_plugin"
qmake "$ROOT_DIR/qt_gui/plugins/system_monitor_plugin/system_monitor_plugin.pro"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

echo "[4/4] Build capture_tools_plugin"
qmake "$ROOT_DIR/qt_gui/plugins/capture_tools_plugin/capture_tools_plugin.pro"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

mkdir -p "$APP_BUILD/plugins"
cp -f "$BUILD_ROOT/plugins"/*.so "$APP_BUILD/plugins/" 2>/dev/null || true
cp -f "$BUILD_ROOT/plugins"/*.dll "$APP_BUILD/plugins/" 2>/dev/null || true
cp -f "$BUILD_ROOT/plugins"/*.dylib "$APP_BUILD/plugins/" 2>/dev/null || true

echo "Qt GUI build finished: $APP_BUILD"
