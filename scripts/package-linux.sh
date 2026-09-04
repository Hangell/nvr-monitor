#!/usr/bin/env sh
set -eu

build_dir="${BUILD_DIR:-build-package-linux}"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
cpack --config "$build_dir/CPackConfig.cmake" -G DEB -B "$build_dir"
cpack --config "$build_dir/CPackConfig.cmake" -G TGZ -B "$build_dir"
echo "Pacotes gerados em $build_dir"
