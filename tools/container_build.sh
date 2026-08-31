#!/usr/bin/env bash
set -euo pipefail

image_name="virgin-build:qt6"
build_dir="${1:-build-container}"

docker build -f Dockerfile.build -t "${image_name}" .
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "${PWD}:/src" \
  "${image_name}" \
  bash -lc "cmake -S /src -B /src/${build_dir} -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DVIRGIN_WARNINGS_AS_ERRORS=ON \
    && cmake --build /src/${build_dir} \
    && ctest --test-dir /src/${build_dir} --output-on-failure"
