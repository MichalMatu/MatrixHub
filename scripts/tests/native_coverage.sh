#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_ENV="${BUILD_ENV:-native_coverage}"
BUILD_DIR="${ROOT_DIR}/.pio/build/${BUILD_ENV}"
COVERAGE_DIR="${BUILD_DIR}/coverage"
PROGRAM_PATH="${BUILD_DIR}/program"

# Resolve llvm tools: prefer xcrun on macOS, fall back to PATH for Linux CI.
resolve_llvm_tool() {
    local tool="$1"
    if command -v xcrun >/dev/null 2>&1; then
        if path="$(xcrun --find "${tool}" 2>/dev/null)"; then
            printf '%s' "${path}"
            return 0
        fi
    fi
    if path="$(command -v "${tool}")"; then
        printf '%s' "${path}"
        return 0
    fi
    echo "Required tool not found: ${tool}" >&2
    return 1
}

LLVM_COV_BIN="$(resolve_llvm_tool llvm-cov)"
LLVM_PROFDATA_BIN="$(resolve_llvm_tool llvm-profdata)"
if [ -z "${PIO_BIN:-}" ]; then
    if command -v pio >/dev/null 2>&1; then
        PIO_BIN="$(command -v pio)"
    elif command -v platformio >/dev/null 2>&1; then
        PIO_BIN="$(command -v platformio)"
    else
        echo "Required tool not found: pio or platformio" >&2
        exit 1
    fi
fi

if [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1; then
    export CC="$(command -v clang)"
fi

if [ -z "${CXX:-}" ] && command -v clang++ >/dev/null 2>&1; then
    export CXX="$(command -v clang++)"
fi

FILTERED_RUN=false
PASSTHROUGH_ARGS=()

while (($#)); do
    case "$1" in
        -f|--filter)
            current_flag="$1"
            FILTERED_RUN=true
            PASSTHROUGH_ARGS+=("$1")
            shift
            if (($# == 0)); then
                echo "Missing value for ${current_flag}" >&2
                exit 1
            fi
            PASSTHROUGH_ARGS+=("$1")
            shift
            ;;
        *)
            PASSTHROUGH_ARGS+=("$1")
            shift
            ;;
    esac
done

rm -rf "${COVERAGE_DIR}"
mkdir -p "${COVERAGE_DIR}"

cd "${ROOT_DIR}"

report_regex='(^|/)(test|test/stubs|lib|\.pio)/'

collect_profiles() {
    local profile_dir="$1"
    shopt -s nullglob
    local profiles=("${profile_dir}"/*.profraw)
    shopt -u nullglob

    if [ "${#profiles[@]}" -eq 0 ]; then
        echo "No LLVM profile files were generated in ${profile_dir}" >&2
        exit 1
    fi

    printf '%s\n' "${profiles[@]}"
}

write_suite_artifacts() {
    local label="$1"
    shift

    local suite_dir="${COVERAGE_DIR}/${label}"
    local profile_dir="${suite_dir}/profiles"
    local merged_profile="${suite_dir}/merged.profdata"
    local report_file="${suite_dir}/report.txt"

    mkdir -p "${profile_dir}"
    rm -f "${profile_dir}"/*.profraw "${merged_profile}" "${report_file}"

    LLVM_PROFILE_FILE="${profile_dir}/%p.profraw" \
        "${PIO_BIN}" test -e "${BUILD_ENV}" "$@" >&2

    local profile_list=()
    while IFS= read -r profile; do
        profile_list+=("${profile}")
    done < <(collect_profiles "${profile_dir}")

    if [ ! -x "${PROGRAM_PATH}" ]; then
        echo "Missing test program at ${PROGRAM_PATH}" >&2
        exit 1
    fi

    cp "${PROGRAM_PATH}" "${suite_dir}/program"

    "${LLVM_PROFDATA_BIN}" merge -sparse "${profile_list[@]}" -o "${merged_profile}"

    local report_output
    report_output="$("${LLVM_COV_BIN}" report \
        "${suite_dir}/program" \
        -instr-profile="${merged_profile}" \
        -ignore-filename-regex="${report_regex}")"

    printf '%s\n' "${report_output}" > "${report_file}"
    printf '%-48s %s\n' "${label}" "$(printf '%s\n' "${report_output}" | tail -n 1)"
}

list_native_suites() {
    "${PIO_BIN}" test -e "${BUILD_ENV}" --list-tests \
        | awk -v env_name="${BUILD_ENV}" '$1 == env_name { print $2 }' \
        | awk '$1 != "stubs"'
}

echo
echo "Coverage summary for ${BUILD_ENV}:"

if [ "${FILTERED_RUN}" = true ]; then
    write_suite_artifacts "filtered" "${PASSTHROUGH_ARGS[@]}"
else
    summary_file="${COVERAGE_DIR}/summary.txt"
    : > "${summary_file}"

    while IFS= read -r suite; do
        [ -n "${suite}" ] || continue
        suite_summary="$(write_suite_artifacts "${suite}" -f "${suite}")"
        printf '%s\n' "${suite_summary}" | tee -a "${summary_file}"
    done < <(list_native_suites)
fi
