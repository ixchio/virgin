#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "$script_dir/.." && pwd)"
appimage_source="${1:-$project_dir/dist/Virgin-0.1.0-x86_64.AppImage}"

if [[ ! -f "$appimage_source" ]]; then
    echo "Virgin AppImage not found: $appimage_source" >&2
    echo "Build it first with ./tools/build_appimage.sh" >&2
    exit 1
fi

data_root="${XDG_DATA_HOME:-$HOME/.local/share}"
bin_root="${XDG_BIN_HOME:-$HOME/.local/bin}"
install_dir="$data_root/virgin/app"
application_dir="$data_root/applications"
icon_dir="$data_root/icons/hicolor/scalable/apps"
installed_appimage="$install_dir/Virgin.AppImage"
installed_desktop="$application_dir/virgin.desktop"
desktop_temp="$(mktemp)"

cleanup() {
    rm -f -- "$desktop_temp"
}
trap cleanup EXIT

install -d -- "$install_dir" "$bin_root" "$application_dir" "$icon_dir"
install -m 0755 -- "$appimage_source" "$installed_appimage"
ln -sfn -- "$installed_appimage" "$bin_root/virgin"
install -m 0644 -- "$project_dir/packaging/linux/virgin.svg" "$icon_dir/virgin.svg"

sed "s|^Exec=.*|Exec=\"$installed_appimage\" %U|" \
    "$project_dir/packaging/linux/virgin.desktop" > "$desktop_temp"
install -m 0644 -- "$desktop_temp" "$installed_desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$application_dir" >/dev/null 2>&1 || true
fi

echo "Virgin installed for the current user."
echo "Open it from the application menu, or run: virgin"
