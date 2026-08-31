#!/usr/bin/env bash
set -euo pipefail

build_dir=/src/build-appimage
app_dir=/src/dist/Virgin.AppDir

rm -rf "${build_dir}" "${app_dir}"
mkdir -p /src/dist

cmake -S /src -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DVIRGIN_ENABLE_HARDENING=ON \
  -DVIRGIN_WARNINGS_AS_ERRORS=ON
cmake --build "${build_dir}" --parallel 2
ctest --test-dir "${build_dir}" --output-on-failure -E AdBlockBenchmark
DESTDIR="${app_dir}" cmake --install "${build_dir}"

export QMAKE=/usr/bin/qmake6
export EXTRA_QT_PLUGINS="sqldrivers;platforms;platformthemes;imageformats;iconengines;tls"
export OUTPUT=/src/dist/Virgin-0.1.0-x86_64.AppImage

/usr/local/bin/linuxdeploy \
  --appdir "${app_dir}" \
  --desktop-file /src/packaging/linux/virgin.desktop \
  --icon-file /src/packaging/linux/virgin.svg \
  --library /usr/lib/x86_64-linux-gnu/libOpenGL.so.0 \
  --plugin qt \
  --output appimage

"${OUTPUT}" --runtime-version
