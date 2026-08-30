#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd -- "${script_dir}/../.." && pwd)"
machine_arch="${APPIMAGE_ARCH:-$(uname -m)}"
build_dir="${BUILD_DIR:-${source_dir}/build/linux-release}"
dist_dir="${DIST_DIR:-${source_dir}/dist}"
linuxdeploy_source="${LINUXDEPLOY:-${source_dir}/tools/linuxdeploy-${machine_arch}.AppImage}"
qt_plugin_source="${LINUXDEPLOY_PLUGIN_QT:-${source_dir}/tools/linuxdeploy-plugin-qt-${machine_arch}.AppImage}"
cmake_generator="${CMAKE_GENERATOR:-Ninja}"

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

[[ "$(uname -s)" == "Linux" ]] || die "Linux packages must be built on Linux"

for command_name in cmake cpack install mktemp; do
    require_command "${command_name}"
done

[[ -r "${linuxdeploy_source}" ]] || die "linuxdeploy not found: ${linuxdeploy_source}"
[[ -r "${qt_plugin_source}" ]] || die "linuxdeploy Qt plugin not found: ${qt_plugin_source}"

qmake_bin="${QMAKE:-}"
if [[ -z "${qmake_bin}" ]]; then
    if command -v qmake6 >/dev/null 2>&1; then
        qmake_bin="$(command -v qmake6)"
    elif command -v qmake >/dev/null 2>&1; then
        qmake_bin="$(command -v qmake)"
    else
        die "qmake was not found; install the Qt development tools"
    fi
fi
[[ -x "${qmake_bin}" ]] || die "qmake is not executable: ${qmake_bin}"

mkdir -p "${build_dir}" "${dist_dir}"

cmake \
    -S "${source_dir}" \
    -B "${build_dir}" \
    -G "${cmake_generator}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "$@"
cmake --build "${build_dir}" --config Release --parallel

project_version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "${build_dir}/CMakeCache.txt")"
[[ -n "${project_version}" ]] || die "could not read the project version from CMakeCache.txt"

temporary_root="${TMPDIR:-/tmp}"
[[ -d "${temporary_root}" && -w "${temporary_root}" ]] || \
    die "temporary directory is not writable: ${temporary_root}"

work_dir="$(mktemp -d "${temporary_root%/}/image-stitcher-auto-appimage.XXXXXX")"
cleanup() {
    cmake -E remove_directory "${work_dir}"
}
trap cleanup EXIT

appdir="${work_dir}/AppDir"
deb_output_dir="${work_dir}/deb"
linuxdeploy_bin="${work_dir}/linuxdeploy-${machine_arch}.AppImage"
qt_plugin_bin="${work_dir}/linuxdeploy-plugin-qt"
temporary_appimage="${work_dir}/Image_Stitcher_Auto-${project_version}-${machine_arch}.AppImage"
final_appimage="${dist_dir}/Image_Stitcher_Auto-${project_version}-${machine_arch}.AppImage"

mkdir -p "${deb_output_dir}"
cpack \
    --config "${build_dir}/CPackConfig.cmake" \
    -G DEB \
    -B "${deb_output_dir}"

shopt -s nullglob
deb_packages=("${deb_output_dir}"/image-stitcher-auto_"${project_version}"_*.deb)
shopt -u nullglob
[[ "${#deb_packages[@]}" -eq 1 ]] || die "expected one Debian package, found ${#deb_packages[@]}"
final_deb="${dist_dir}/$(basename -- "${deb_packages[0]}")"
cmake -E copy "${deb_packages[0]}" "${final_deb}"

# AppDir requires real symbolic links. Keeping it in a native temporary
# directory also supports source trees stored on filesystems without symlinks.
install -m 0755 "${linuxdeploy_source}" "${linuxdeploy_bin}"
install -m 0755 "${qt_plugin_source}" "${qt_plugin_bin}"
DESTDIR="${appdir}" cmake --install "${build_dir}" --config Release --strip

(
    cd -- "${work_dir}"
    export APPIMAGE_EXTRACT_AND_RUN=1
    export ARCH="${machine_arch}"
    export DEPLOY_PLATFORM_THEMES=1
    export EXTRA_PLATFORM_PLUGINS="libqminimal.so;libqoffscreen.so"
    export LDAI_OUTPUT="${temporary_appimage}"
    export QMAKE="${qmake_bin}"

    "${linuxdeploy_bin}" \
        --appdir "${appdir}" \
        --executable "${appdir}/usr/bin/Image_Stitcher_Auto" \
        --desktop-file "${source_dir}/packaging/linux/image-stitcher-auto.desktop" \
        --icon-file "${source_dir}/packaging/linux/image-stitcher-auto.png" \
        --plugin qt \
        --output appimage
)

[[ -s "${temporary_appimage}" ]] || die "AppImage was not generated"
cmake -E copy "${temporary_appimage}" "${final_appimage}"
if [[ ! -x "${final_appimage}" ]]; then
    printf '%s\n' \
        "warning: the destination filesystem does not preserve executable bits; run chmod +x after copying the AppImage to a native Linux filesystem" \
        >&2
fi

printf 'Linux packages created in %s:\n' "${dist_dir}"
printf '  %s\n' "${final_deb}" "${final_appimage}"
