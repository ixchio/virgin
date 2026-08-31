#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image_name=virgin-appimage-builder:qt6

docker build --pull -f "${project_root}/packaging/appimage/Dockerfile" \
  -t "${image_name}" "${project_root}"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "${project_root}:/src" \
  "${image_name}"

echo "AppImage: ${project_root}/dist/Virgin-0.1.0-x86_64.AppImage"
