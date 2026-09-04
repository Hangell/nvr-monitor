#!/usr/bin/env sh
set -eu

build_dir="${BUILD_DIR:-build-release}"
prefix="${PREFIX:-/usr/local}"

cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

case "$prefix" in
    "$HOME"|"$HOME"/*) cmake --install "$build_dir" ;;
    *) sudo cmake --install "$build_dir" ;;
esac

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q "$prefix/share/applications" 2>/dev/null || true
fi

echo "NVR Monitor instalado. Execute: nvr-monitor"
