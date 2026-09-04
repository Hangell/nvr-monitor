#!/usr/bin/env sh
set -eu

build_dir="${BUILD_DIR:-build-release}"
if [ ! -f "$build_dir/install_manifest.txt" ]; then
    echo "Manifesto não encontrado em $build_dir. Informe BUILD_DIR usado na instalação." >&2
    exit 1
fi

prefix="${PREFIX:-/usr/local}"
case "$prefix" in
    "$HOME"|"$HOME"/*) cmake --build "$build_dir" --target uninstall ;;
    *) sudo cmake --build "$build_dir" --target uninstall ;;
esac

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q "$prefix/share/applications" 2>/dev/null || true
fi
