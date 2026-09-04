#!/usr/bin/env sh
set -eu

if [ -z "${MINGW_PREFIX:-}" ]; then
    echo "Execute este script no terminal MSYS2 UCRT64." >&2
    exit 1
fi

build_dir="${BUILD_DIR:-build-package-windows}"
cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
cpack --config "$build_dir/CPackConfig.cmake" -G NSIS -B "$build_dir"
cpack --config "$build_dir/CPackConfig.cmake" -G ZIP -B "$build_dir"
echo "Instalador e pacote portátil gerados em $build_dir"
