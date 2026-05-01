#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${SIGNALROUTE_BUILD_ROOT:-/tmp}"
JOBS="${SIGNALROUTE_JOBS:-2}"
RUN_SANITIZERS="${SIGNALROUTE_RUN_SANITIZERS:-0}"
ASAN_DETECT_LEAKS="${SIGNALROUTE_ASAN_DETECT_LEAKS:-0}"

run_build() {
    local name="$1"
    local build_dir="$2"
    shift 2

    echo "==> ${name}: configure"
    cmake -S "${ROOT_DIR}" -B "${build_dir}" -DSR_BUILD_TESTS=ON "$@"

    echo "==> ${name}: build"
    cmake --build "${build_dir}" -j"${JOBS}"

    echo "==> ${name}: test"
    ctest --test-dir "${build_dir}" --output-on-failure
}

run_build "fallback" "${BUILD_ROOT}/signalroute-build"
run_build "protobuf" "${BUILD_ROOT}/signalroute-protobuf-build" -DSR_ENABLE_PROTOBUF=ON

if [[ "${RUN_SANITIZERS}" == "1" ]]; then
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=${ASAN_DETECT_LEAKS}}"
    run_build "asan-ubsan" \
        "${BUILD_ROOT}/signalroute-asan-ubsan-build" \
        -DSR_ENABLE_ASAN=ON \
        -DSR_ENABLE_UBSAN=ON
else
    echo "==> sanitizer build skipped; set SIGNALROUTE_RUN_SANITIZERS=1 to enable ASan+UBSan"
fi
