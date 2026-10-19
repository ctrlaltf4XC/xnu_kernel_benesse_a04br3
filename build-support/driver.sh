#!/usr/bin/env bash
# Adapted from blacktop/darwin-xnu-build, MIT; see CREDITS.md.
# All dependency and kernel sources are copied from this repository; no patches/clones.
set -euo pipefail
running() { printf '%s\n' "$*"; }
info() { printf '%s\n' "$*"; }
error() { printf '%s\n' "$*" >&2; }
warning() { printf '%s\n' "$*" >&2; }
WORK_DIR=$(cd "$1" && pwd)
BUILD_DIR="$WORK_DIR/build"
FAKEROOT_DIR="$WORK_DIR/fakeroot"
DSTROOT="$FAKEROOT_DIR"
CACHE_DIR="$WORK_DIR/.cache"
HAVE_WE_INSTALLED_HEADERS_YET="$FAKEROOT_DIR/.xnu_headers_installed"
KERNEL_CONFIG=DEVELOPMENT
ARCH_CONFIG=ARM64
MACHINE_CONFIG=A04BR3
KERNEL_TYPE=development.a04br3
JSONDB=0
MACOS_VERSION=13.5
RC_DARWIN_KERNEL_VERSION=22.6.0
TIGHTBEAMC=tightbeamc-not-supported
MEMORY_SIZE_OVERRIDE=
PHYS_CPU_OVERRIDE=
LOGICAL_CPU_OVERRIDE=
KERNEL_PARALLELISM_OVERRIDE=1
KDKROOT=${KDKROOT:-/Library/Developer/KDKs/KDK_13.5_22G74.kdk}
export KERNEL_BUILD_DATE='Wed Jul 5 00:00:00 UTC 2023'
export KERNEL_BUILD_OBJROOT='xnu-8796.141.3-a04br3'
kernel_version_env() { :; }
cd "$WORK_DIR"
mkdir -p "$BUILD_DIR" "$FAKEROOT_DIR"


function setup_xcode_toolchain() {
    # Honor DEVELOPER_DIR from the environment so CI or a second Xcode install can pick the
    # toolchain; otherwise use the xcode-select default.
    DEVELOPER_DIR="${DEVELOPER_DIR:-$(xcode-select -p 2>/dev/null || true)}"

    if [ -z "${DEVELOPER_DIR}" ] || [ ! -d "${DEVELOPER_DIR}" ]; then
        error "Xcode Command Line Tools not found. Please run install_deps first."
        return 1
    fi

    info "Using Xcode at: ${DEVELOPER_DIR}"

    # Set DEVELOPER_DIR explicitly
    export DEVELOPER_DIR

    # Build a clean PATH that prioritizes Xcode tools over Homebrew
    # This ensures we use Apple's toolchain, not GNU coreutils or Homebrew LLVM
    local XCODE_TOOLCHAIN="${DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/usr/bin"
    local XCODE_USR_BIN="${DEVELOPER_DIR}/usr/bin"
    local SYSTEM_PATHS="/usr/bin:/bin:/usr/sbin:/sbin"

    # Only add Homebrew to the end if it exists, so user tools are still available
    local HOMEBREW_PATHS=""
    if [ -d "/opt/homebrew/bin" ]; then
        HOMEBREW_PATHS=":/opt/homebrew/bin:/opt/homebrew/sbin"
    elif [ -d "/usr/local/bin" ]; then
        HOMEBREW_PATHS=":/usr/local/bin:/usr/local/sbin"
    fi

    export PATH="${XCODE_TOOLCHAIN}:${XCODE_USR_BIN}:${SYSTEM_PATHS}${HOMEBREW_PATHS}"

    # Explicitly set compiler variables to Xcode's tools
    export CC="$(xcrun -find clang)"
    export CXX="$(xcrun -find clang++)"
    export LD="$(xcrun -find ld)"
    export AR="$(xcrun -find ar)"
    export RANLIB="$(xcrun -find ranlib)"
    export STRIP="$(xcrun -find strip)"
    export LIBTOOL="$(xcrun -find libtool)"

    # Verify we're using Xcode's clang, not Homebrew's
    local CLANG_PATH
    CLANG_PATH="$(which clang)"
    if [[ ! "${CLANG_PATH}" =~ ^"${DEVELOPER_DIR}".* ]]; then
        warning "clang is not from Xcode: ${CLANG_PATH}"
        warning "This may cause build failures. Check your PATH."
    else
        info "Using Xcode clang: ${CLANG_PATH}"
    fi

    # Check for common Homebrew interference
    if echo "${PATH}" | grep -q "/opt/homebrew/opt/coreutils" || echo "${PATH}" | grep -q "/opt/homebrew/opt/llvm"; then
        warning "Detected Homebrew GNU coreutils or LLVM in PATH before system tools"
        warning "This has been reordered to prioritize Xcode's toolchain"
    fi
}

build_bootstrap_cmds() {
    if [ ! "$(find "${FAKEROOT_DIR}" -name 'mig' | wc -l)" -gt 0 ]; then
        running "📦 Building bootstrap_cmds"


        SRCROOT="${WORK_DIR}/bootstrap_cmds"
        OBJROOT="${BUILD_DIR}/bootstrap_cmds.obj"
        SYMROOT="${BUILD_DIR}/bootstrap_cmds.sym"


        CLONED_BOOTSTRAP_VERSION=bootstrap_cmds-129

        cd "${SRCROOT}"
        env LD="$(xcrun -find clang)" LDPLUSPLUS="$(xcrun -find clang++)" \
            xcodebuild install -sdk macosx -project mig.xcodeproj ARCHS="arm64 x86_64" CODE_SIGN_IDENTITY="-" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" RC_ProjectNameAndSourceVersion="${CLONED_BOOTSTRAP_VERSION}"
        cd "${WORK_DIR}"
    fi
}

build_dtrace() {
    if [ ! "$(find "${FAKEROOT_DIR}" -name 'ctfmerge' | wc -l)" -gt 0 ]; then
        running "📦 Building dtrace"
        SRCROOT="${WORK_DIR}/dtrace"
        OBJROOT="${BUILD_DIR}/dtrace.obj"
        SYMROOT="${BUILD_DIR}/dtrace.sym"
        cd "${SRCROOT}"
        env LD="$(xcrun -find clang)" LDPLUSPLUS="$(xcrun -find clang++)" \
            xcodebuild install -sdk macosx -target ctfconvert -target ctfdump -target ctfmerge ARCHS="arm64 x86_64" CODE_SIGN_IDENTITY="-" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}"
        cd "${WORK_DIR}"
    fi
}

build_availabilityversions() {
    if [ ! "$(find "${FAKEROOT_DIR}" -name 'availability.pl' | wc -l)" -gt 0 ]; then
        running "📦 Building AvailabilityVersions"
        SRCROOT="${WORK_DIR}/AvailabilityVersions"
        OBJROOT="${BUILD_DIR}/"
        SYMROOT="${BUILD_DIR}/"
        cd "${SRCROOT}"
        make install -j8 OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}"
        cd "${WORK_DIR}"
    fi
}

xnu_headers() {
    if [ ! -f "${HAVE_WE_INSTALLED_HEADERS_YET}" ]; then
        running "Installing xnu headers"
        ensure_hw_env_overrides
        SRCROOT="${WORK_DIR}/xnu"
        OBJROOT="${BUILD_DIR}/xnu-hdrs.obj"
        SYMROOT="${BUILD_DIR}/xnu-hdrs.sym"
        cd "${SRCROOT}"
        make installhdrs SDKROOT=macosx ARCH_CONFIGS="X86_64 ARM64" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" FAKEROOT_DIR="${FAKEROOT_DIR}" KDKROOT="${KDKROOT}" TIGHTBEAMC=${TIGHTBEAMC} RC_DARWIN_KERNEL_VERSION=${RC_DARWIN_KERNEL_VERSION} MEMORY_SIZE="${MEMORY_SIZE_OVERRIDE}" SYSCTL_HW_PHYSICALCPU="${PHYS_CPU_OVERRIDE}" SYSCTL_HW_LOGICALCPU="${LOGICAL_CPU_OVERRIDE}" KERNEL_BUILDS_IN_PARALLEL="${KERNEL_PARALLELISM_OVERRIDE:-1}"
        cd "${WORK_DIR}"
        touch "${HAVE_WE_INSTALLED_HEADERS_YET}"
    fi
}

libsystem_headers() {
    if [ ! -d "${FAKEROOT_DIR}/System/Library/Frameworks/System.framework" ]; then
        running "Installing Libsystem headers"
        SRCROOT="${WORK_DIR}/Libsystem"
        OBJROOT="${BUILD_DIR}/Libsystem.obj"
        SYMROOT="${BUILD_DIR}/Libsystem.sym"
        cd "${SRCROOT}"
        env LD="$(xcrun -find clang)" LDPLUSPLUS="$(xcrun -find clang++)" \
            xcodebuild installhdrs -sdk macosx ARCHS="arm64 arm64e" VALID_ARCHS="arm64 arm64e" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" FAKEROOT_DIR="${FAKEROOT_DIR}"
        cd "${WORK_DIR}"
    fi
}

libsyscall_headers() {
    if [ ! -f "${FAKEROOT_DIR}/usr/include/os/proc.h" ]; then
        running "Installing libsyscall headers"
        SRCROOT="${WORK_DIR}/xnu/libsyscall"
        OBJROOT="${BUILD_DIR}/libsyscall.obj"
        SYMROOT="${BUILD_DIR}/libsyscall.sym"
        cd "${SRCROOT}"
        env LD="$(xcrun -find clang)" LDPLUSPLUS="$(xcrun -find clang++)" \
            xcodebuild installhdrs -sdk macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" ARCHS="arm64 arm64e" VALID_ARCHS="arm64 arm64e" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" FAKEROOT_DIR="${FAKEROOT_DIR}"
        cd "${WORK_DIR}"
    fi
}

build_libplatform() {
    if [ ! -f "${FAKEROOT_DIR}/usr/local/include/_simple.h" ]; then
        running "📦 Building libplatform"
        SRCROOT="${WORK_DIR}/libplatform"
        cd "${SRCROOT}"
        ditto "${SRCROOT}/include" "${DSTROOT}/usr/local/include"
        ditto "${SRCROOT}/private" "${DSTROOT}/usr/local/include"
        cd "${WORK_DIR}"
    fi
}

build_libdispatch() {
    if [ ! -f "${FAKEROOT_DIR}/usr/local/lib/kernel/libfirehose_kernel.a" ]; then
        running "📦 Building libdispatch"
        SRCROOT="${WORK_DIR}/libdispatch"
        OBJROOT="${BUILD_DIR}/libfirehose_kernel.obj"
        SYMROOT="${BUILD_DIR}/libfirehose_kernel.sym"
        # libfirehose_kernel patch
        cd "${SRCROOT}"
        env LD="$(xcrun -find clang)" LDPLUSPLUS="$(xcrun -find clang++)" \
            xcodebuild install -target libfirehose_kernel -sdk macosx ARCHS="x86_64 arm64e arm64" VALID_ARCHS="x86_64 arm64e arm64" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" FAKEROOT_DIR="${FAKEROOT_DIR}"
        cd "${WORK_DIR}"
        mv "${FAKEROOT_DIR}/usr/local/lib/kernel/liblibfirehose_kernel.a" "${FAKEROOT_DIR}/usr/local/lib/kernel/libfirehose_kernel.a"
    fi
}

ensure_hw_env_overrides() {
    if [ -n "${MEMORY_SIZE_OVERRIDE}" ] && [ -n "${PHYS_CPU_OVERRIDE}" ] && [ -n "${LOGICAL_CPU_OVERRIDE}" ] && [ -n "${KERNEL_PARALLELISM_OVERRIDE}" ]; then
        return
    fi
    local mem_bytes phys_cpu log_cpu
    mem_bytes=$(/usr/sbin/sysctl -n hw.memsize 2>/dev/null || true)
    if ! [[ "${mem_bytes:-}" =~ ^[0-9]+$ ]]; then
        mem_bytes=$(python3 -c 'import os, sys
try:
    pages = os.sysconf("SC_PHYS_PAGES")
    page_size = os.sysconf("SC_PAGE_SIZE")
except (AttributeError, ValueError, OSError):
    sys.exit(1)
if pages is None or page_size is None or pages <= 0 or page_size <= 0:
    sys.exit(1)
print(pages * page_size)' 2>/dev/null || true)
    fi
    if ! [[ "${mem_bytes:-}" =~ ^[0-9]+$ ]]; then
        mem_bytes=1073741824
    fi
    phys_cpu=$(/usr/sbin/sysctl -n hw.physicalcpu 2>/dev/null || true)
    if ! [[ "${phys_cpu:-}" =~ ^[0-9]+$ ]]; then
        phys_cpu=$(python3 -c 'import os, sys
count = os.cpu_count()
if count is None or count <= 0:
    sys.exit(1)
print(count)' 2>/dev/null || true)
    fi
    if ! [[ "${phys_cpu:-}" =~ ^[0-9]+$ ]]; then
        phys_cpu=1
    fi
    log_cpu=$(/usr/sbin/sysctl -n hw.logicalcpu 2>/dev/null || true)
    if ! [[ "${log_cpu:-}" =~ ^[0-9]+$ ]]; then
        log_cpu=${phys_cpu}
    fi
    MEMORY_SIZE_OVERRIDE="${mem_bytes}"
    PHYS_CPU_OVERRIDE="${phys_cpu}"
    LOGICAL_CPU_OVERRIDE="${log_cpu}"
    KERNEL_PARALLELISM_OVERRIDE=1
}

ctf_tool_overrides() {
    local tool
    for tool in ctfconvert ctfmerge ctfdump; do
        if [ ! -x "${FAKEROOT_DIR}/usr/local/bin/${tool}" ]; then
            error "${tool} not found in ${FAKEROOT_DIR}/usr/local/bin (build_dtrace should have installed it)"
            exit 1
        fi
    done
    CTF_TOOL_OVERRIDES=(
        "CTFCONVERT=${FAKEROOT_DIR}/usr/local/bin/ctfconvert"
        "CTFMERGE=${FAKEROOT_DIR}/usr/local/bin/ctfmerge"
        "CTFDUMP=${FAKEROOT_DIR}/usr/local/bin/ctfdump"
    )
}

build_xnu() {
    if [ ! -f "${BUILD_DIR}/xnu.obj/kernel.${KERNEL_TYPE}" ]; then
        if [ "$JSONDB" -ne "0" ]; then
            running "📦 Building XNU kernel with JSON compilation database"
                ensure_hw_env_overrides
            SRCROOT="${WORK_DIR}/xnu"
            OBJROOT="${BUILD_DIR}/xnu-compiledb.obj"
            SYMROOT="${BUILD_DIR}/xnu-compiledb.sym"
            rm -rf "${OBJROOT}"
            rm -rf "${SYMROOT}"
            cd "${SRCROOT}"
            make SDKROOT=macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" LOGCOLORS=y BUILD_WERROR=0 BUILD_LTO=0 BUILD_JSON_COMPILATION_DATABASE=1 SRCROOT="${SRCROOT}" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" FAKEROOT_DIR="${FAKEROOT_DIR}" KDKROOT="${KDKROOT}" TIGHTBEAMC=${TIGHTBEAMC} RC_DARWIN_KERNEL_VERSION=${RC_DARWIN_KERNEL_VERSION} MEMORY_SIZE="${MEMORY_SIZE_OVERRIDE}" SYSCTL_HW_PHYSICALCPU="${PHYS_CPU_OVERRIDE}" SYSCTL_HW_LOGICALCPU="${LOGICAL_CPU_OVERRIDE}" KERNEL_BUILDS_IN_PARALLEL="${KERNEL_PARALLELISM_OVERRIDE:-1}" || true
            JSON_COMPILE_DB="$(find "${OBJROOT}" -name compile_commands.json)"
            info "JSON compilation database: ${JSON_COMPILE_DB}"
            cp -f "${JSON_COMPILE_DB}" "${SRCROOT}"
            mkdir -p "${CACHE_DIR}/${MACOS_VERSION}"
            info "Caching JSON compilation database in: ${CACHE_DIR}/${MACOS_VERSION}"
            cp -f "${JSON_COMPILE_DB}" "${CACHE_DIR}/${MACOS_VERSION}"
        else
            running "📦 Building XNU kernel TARGET_CONFIGS=\"$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG\""
                ensure_hw_env_overrides
            SRCROOT="${WORK_DIR}/xnu"
            OBJROOT="${BUILD_DIR}/xnu.obj"
            SYMROOT="${BUILD_DIR}/xnu.sym"
            cd "${SRCROOT}"
            ctf_tool_overrides
            kernel_version_env
            make install -j8 VERBOSE=YES "${CTF_TOOL_OVERRIDES[@]}" SDKROOT=macosx TARGET_CONFIGS="$KERNEL_CONFIG $ARCH_CONFIG $MACHINE_CONFIG" CONCISE=0 LOGCOLORS=y BUILD_WERROR=0 BUILD_LTO=0 SRCROOT="${SRCROOT}" OBJROOT="${OBJROOT}" SYMROOT="${SYMROOT}" DSTROOT="${DSTROOT}" FAKEROOT_DIR="${FAKEROOT_DIR}" KDKROOT="${KDKROOT}" TIGHTBEAMC=${TIGHTBEAMC} RC_DARWIN_KERNEL_VERSION=${RC_DARWIN_KERNEL_VERSION} MEMORY_SIZE="${MEMORY_SIZE_OVERRIDE}" SYSCTL_HW_PHYSICALCPU="${PHYS_CPU_OVERRIDE}" SYSCTL_HW_LOGICALCPU="${LOGICAL_CPU_OVERRIDE}" KERNEL_BUILDS_IN_PARALLEL="${KERNEL_PARALLELISM_OVERRIDE:-1}"
            cd "${WORK_DIR}"
        fi
    else
        info "📦 XNU kernel.${KERNEL_TYPE} already built"
    fi
}

setup_xcode_toolchain
build_bootstrap_cmds
build_dtrace
build_availabilityversions
xnu_headers
libsystem_headers
libsyscall_headers
build_libplatform
build_libdispatch
build_xnu

