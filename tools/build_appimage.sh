#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image_name=virgin-appimage-builder:qt6
# Ubuntu 22.04 carries GLIBC 2.35, so this AppImage runs on supported Ubuntu
# desktop releases from 22.04 forward (and newer compatible distributions).
runtime_image=ubuntu:22.04
release_version="$(sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([^[:space:]]+).*/\1/p' "${project_root}/CMakeLists.txt" | head -n 1)"

if [[ -z "${release_version}" ]]; then
  echo "Could not determine the Virgin version from CMakeLists.txt" >&2
  exit 1
fi

docker build --pull -f "${project_root}/packaging/appimage/Dockerfile" \
  -t "${image_name}" "${project_root}"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "${project_root}:/src" \
  "${image_name}"

# Verify on the oldest supported GLIBC baseline with the graphics libraries a
# desktop AppImage may rely on from its host distribution. This catches both
# loader incompatibilities and missing runtime dependencies without requiring a
# display server.
docker run --rm \
  --volume "${project_root}:/src:ro" \
  --env APPIMAGE_EXTRACT_AND_RUN=1 \
  --env DEBIAN_FRONTEND=noninteractive \
  "${runtime_image}" \
  bash -ceu '
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends \
      libasound2 libatspi2.0-0 libcups2 libdbus-1-3 libdrm2 libegl1 \
      libexpat1 libfontconfig1 libfreetype6 libgbm1 libgl1 libglx0 libharfbuzz0b \
      libnss3 libx11-6 libxcb1 libxext6 libxkbcommon0
    exec "$1" --runtime-version
  ' -- "/src/dist/Virgin-${release_version}-x86_64.AppImage"

echo "AppImage: ${project_root}/dist/Virgin-${release_version}-x86_64.AppImage"
