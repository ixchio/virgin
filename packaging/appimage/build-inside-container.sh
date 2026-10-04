#!/usr/bin/env bash
set -euo pipefail

build_dir=/src/build-appimage
app_dir=/src/dist/Virgin.AppDir
release_version="$(sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([^[:space:]]+).*/\1/p' /src/CMakeLists.txt | head -n 1)"

if [[ -z "${release_version}" ]]; then
  echo "Could not determine the Virgin version from CMakeLists.txt" >&2
  exit 1
fi

rm -rf "${build_dir}" "${app_dir}"
mkdir -p /src/dist

cmake -S /src -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DVIRGIN_ENABLE_HARDENING=ON \
  -DVIRGIN_WARNINGS_AS_ERRORS=ON
cmake --build "${build_dir}" --parallel 2
ctest --test-dir "${build_dir}" --output-on-failure
DESTDIR="${app_dir}" cmake --install "${build_dir}"

export QMAKE=/usr/bin/qmake6
export EXTRA_QT_PLUGINS="sqldrivers;platforms;platformthemes;imageformats;iconengines;tls"
export OUTPUT="/src/dist/Virgin-${release_version}-x86_64.AppImage"
rm -f -- "${OUTPUT}"

/usr/local/bin/linuxdeploy \
  --appdir "${app_dir}" \
  --desktop-file /src/packaging/linux/virgin.desktop \
  --icon-file /src/packaging/linux/virgin.svg \
  --library /usr/lib/x86_64-linux-gnu/libOpenGL.so.0 \
  --plugin qt \
  --output appimage

"${OUTPUT}" --runtime-version
