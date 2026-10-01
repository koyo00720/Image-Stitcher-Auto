#!/usr/bin/env bash

set -Eeuo pipefail

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

[[ "$(uname -s)" == "Darwin" ]] || die "macOS builds require macOS and Xcode Command Line Tools"
for command_name in cmake xcrun ditto mktemp; do
    command -v "${command_name}" >/dev/null 2>&1 || die "required command not found: ${command_name}"
done
xcrun --find clang++ >/dev/null

script_dir="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${BUILD_DIR:-${source_dir}/build/macos-release}"
dist_dir="${DIST_DIR:-${source_dir}/dist/macos}"

cmake -S "${source_dir}" -B "${build_dir}" \
    -G "${CMAKE_GENERATOR:-Ninja}" \
    -DCMAKE_BUILD_TYPE=Release \
    "$@"
cmake --build "${build_dir}" --config Release --parallel

project_version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "${build_dir}/CMakeCache.txt")"
[[ -n "${project_version}" ]] || die "could not read the project version"
mkdir -p "${dist_dir}"
dist_dir="$(cd -- "${dist_dir}" && pwd)"
# A fresh output directory avoids mixing old architectures/libraries and never
# overwrites an existing distribution. Both the .app and ZIP are kept here.
output_dir="$(mktemp -d "${dist_dir}/Image_Stitcher_Auto-${project_version}-XXXXXX")"
cmake --install "${build_dir}" --config Release --prefix "${output_dir}"
app_bundle="${output_dir}/Image_Stitcher_Auto.app"
cmake "-DAPP_BUNDLE=${app_bundle}" -P "${build_dir}/macos-deploy-Release.cmake"

architectures="$(xcrun lipo -archs "${app_bundle}/Contents/MacOS/Image_Stitcher_Auto")"
archive="${output_dir}/Image_Stitcher_Auto-${project_version}-macos-${architectures// /-}.zip"
ditto -c -k --sequesterRsrc --keepParent "${app_bundle}" "${archive}"
printf 'macOS build complete (%s):\n  %s\n  %s\n' "${architectures}" "${app_bundle}" "${archive}"
printf '%s\n' 'The bundle is ad-hoc signed for local use, not Developer ID signed or notarized.'
