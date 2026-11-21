#!/usr/bin/env bash
#
# OpenWrt v25.12.5 - LinkIt Smart 7688 build environment preparation
#
# Run this script from an already-cloned parent repository.
# Expected layout:
#   <repository>/
#   ├── README.md
#   ├── docs/
#   │   └── initial_setup.md
#   ├── scripts/
#   │   └── setup.sh
#   └── linkit/
#
# This script does not clone the parent repository, switch branches,
# commit changes, pull, or push to GitHub.

set -Eeuo pipefail

# ------------------------------------------------------------
# CONFIGURATION
# ------------------------------------------------------------

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"
LINKIT_DIR="$REPO_DIR/linkit"

OPENWRT_URL="https://github.com/openwrt/openwrt.git"
OPENWRT_TAG="v25.12.5"

DEBUG=0
INSTALL_DEPS=1

STEP=0
CURRENT_DESCRIPTION="Not started"

# ------------------------------------------------------------
# COMMAND-LINE ARGUMENTS
# ------------------------------------------------------------

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Prepare OpenWrt ${OPENWRT_TAG} source for the MediaTek LinkIt Smart 7688.

Options:
  --debug, -d       Enable Bash command tracing.
  --skip-deps       Skip Ubuntu package installation.
  --help, -h        Display this help message.

Run this script from an already-cloned parent repository. The script
is expected at <repository>/scripts/setup.sh and resolves the repository
root as the parent directory of scripts/.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug|-d)
            DEBUG=1
            shift
            ;;
        --skip-deps)
            INSTALL_DEPS=0
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------
# STEP EXECUTION AND ERROR HANDLING
# ------------------------------------------------------------

run_step() {
    local description="$1"
    shift

    STEP=$((STEP + 1))
    CURRENT_DESCRIPTION="$description"

    echo
    echo "============================================================"
    printf 'STEP %02d: %s\n' "$STEP" "$description"
    echo "------------------------------------------------------------"
    printf 'COMMAND:'
    printf ' %q' "$@"
    echo
    echo "============================================================"

    if [[ "$DEBUG" -eq 1 ]]; then
        set -x
    fi

    "$@"

    if [[ "$DEBUG" -eq 1 ]]; then
        set +x
    fi

    echo "[SUCCESS] $description"
}

error_handler() {
    local exit_code=$?
    trap - ERR
    set +x

    echo >&2
    echo "============================================================" >&2
    echo "[ERROR] SETUP FAILED" >&2
    echo "Failed step        : $STEP" >&2
    echo "Failed description : $CURRENT_DESCRIPTION" >&2
    echo "Exit code          : $exit_code" >&2
    echo "Failed command     : ${BASH_COMMAND}" >&2
    echo "============================================================" >&2
    exit "$exit_code"
}

trap error_handler ERR

# ------------------------------------------------------------
# STEP 1: VERIFY EXISTING PARENT REPOSITORY
# ------------------------------------------------------------

if ! command -v git >/dev/null 2>&1; then
    echo "[ERROR] Git is not installed. Install Git and rerun this script." >&2
    exit 1
fi

if ! git -C "$REPO_DIR" rev-parse --show-toplevel >/dev/null 2>&1; then
    echo "[ERROR] The scripts directory must be inside an already-cloned Git repository." >&2
    echo "Repository path: $REPO_DIR" >&2
    exit 1
fi

GIT_ROOT="$(git -C "$REPO_DIR" rev-parse --show-toplevel)"
GIT_ROOT="$(cd "$GIT_ROOT" && pwd -P)"

if [[ "$GIT_ROOT" != "$REPO_DIR" ]]; then
    echo "[ERROR] setup.sh must be located in the repository's scripts/ directory." >&2
    echo "Detected Git root : $GIT_ROOT" >&2
    echo "Script directory  : $SCRIPT_DIR" >&2
    exit 1
fi


# ------------------------------------------------------------
# STEP 2: INSTALL UBUNTU BUILD DEPENDENCIES
# ------------------------------------------------------------

if [[ "$INSTALL_DEPS" -eq 1 ]]; then
    if ! command -v sudo >/dev/null 2>&1; then
        echo "[ERROR] sudo is required to install build dependencies." >&2
        exit 1
    fi

    run_step "Update Ubuntu package lists" sudo apt-get update

    run_step "Install OpenWrt build dependencies" sudo apt-get install -y \
        build-essential \
        clang \
        flex \
        bison \
        gawk \
        gcc-multilib \
        g++-multilib \
        gettext \
        git \
        libncurses-dev \
        libssl-dev \
        libelf-dev \
        zlib1g-dev \
        liblzma-dev \
        libtool-bin \
        python3 \
        python3-dev \
        python3-setuptools \
        python3-yaml \
        python3-pyelftools \
        python3-venv \
        rsync \
        unzip \
        wget \
        curl \
        file \
        swig \
        time
else
    echo "[INFO] Skipping Ubuntu dependency installation (--skip-deps)."
fi

# ------------------------------------------------------------
# STEP 3: PREPARE LINKIT DIRECTORY
# ------------------------------------------------------------

run_step "Create or verify LinkIt source directory" mkdir -p "$LINKIT_DIR"

# Preserve the project's documentation directory.
mkdir -p "$LINKIT_DIR/docs"

# ------------------------------------------------------------
# STEP 4: CHECK EXISTING OPENWRT SOURCE
# ------------------------------------------------------------

if [[ -f "$LINKIT_DIR/Makefile" && -f "$LINKIT_DIR/include/toplevel.mk" ]]; then
    echo "[INFO] A valid OpenWrt source tree already exists."
    echo "[INFO] Existing source will be preserved; no clone or overwrite is needed."
    SOURCE_READY=1
else
    SOURCE_READY=0

    # Do not overwrite an incomplete or unknown source directory.
    unexpected_entry="$(
        find "$LINKIT_DIR" -mindepth 1 -maxdepth 1 \
            ! -name docs \
            -print -quit
    )"

    if [[ -n "$unexpected_entry" ]]; then
        echo "[ERROR] The LinkIt directory contains files, but is not a valid OpenWrt source tree." >&2
        echo "Unexpected entry: $unexpected_entry" >&2
        echo "Please back up or move the incomplete contents, then rerun the script." >&2
        exit 1
    fi
fi

# ------------------------------------------------------------
# STEP 5: FETCH OPENWRT SOURCE INTO LINKIT DIRECTORY
# ------------------------------------------------------------

if [[ "$SOURCE_READY" -eq 0 ]]; then
    if ! command -v rsync >/dev/null 2>&1; then
        echo "[ERROR] rsync is required. Install it or rerun without --skip-deps." >&2
        exit 1
    fi

    TEMP_DIR="$(mktemp -d)"
    cleanup() {
        rm -rf "$TEMP_DIR"
    }
    trap cleanup EXIT

    run_step "Clone OpenWrt ${OPENWRT_TAG} into temporary directory" \
        git clone --depth 1 --branch "$OPENWRT_TAG" "$OPENWRT_URL" "$TEMP_DIR/openwrt"

    run_step "Verify OpenWrt source tag" \
        git -C "$TEMP_DIR/openwrt" describe --tags --exact-match

    # Copy source files while preserving linkit/docs.
    # Do not copy upstream Git metadata into the parent repository.
    run_step "Copy OpenWrt source into linkit directory" \
        rsync -a --exclude='.git/' "$TEMP_DIR/openwrt/" "$LINKIT_DIR/"

    cleanup
    trap - EXIT
fi

# ------------------------------------------------------------
# STEP 6: VERIFY REQUIRED OPENWRT SOURCE FILES
# ------------------------------------------------------------

for required_file in \
    "$LINKIT_DIR/Makefile" \
    "$LINKIT_DIR/include/toplevel.mk" \
    "$LINKIT_DIR/scripts/feeds" \
    "$LINKIT_DIR/feeds.conf.default"
do
    if [[ ! -f "$required_file" ]]; then
        echo "[ERROR] Required OpenWrt source file is missing: $required_file" >&2
        exit 1
    fi
done

echo "[INFO] Required OpenWrt source files are present."

# ------------------------------------------------------------
# STEP 7: CONFIGURE FEEDS
# ------------------------------------------------------------

if [[ ! -f "$LINKIT_DIR/feeds.conf" ]]; then
    run_step "Create feeds.conf from default configuration" \
        cp "$LINKIT_DIR/feeds.conf.default" "$LINKIT_DIR/feeds.conf"
else
    echo "[INFO] Existing feeds.conf found; preserving it."
fi

# ------------------------------------------------------------
# STEP 8: UPDATE AND INSTALL FEEDS
# ------------------------------------------------------------

run_step "Update OpenWrt feeds" \
    "$LINKIT_DIR/scripts/feeds" update -a

run_step "Install OpenWrt feeds" \
    "$LINKIT_DIR/scripts/feeds" install -a

# ------------------------------------------------------------
# STEP 9: CONFIGURE LINKIT SMART 7688 TARGET
# ------------------------------------------------------------

if [[ ! -f "$LINKIT_DIR/.config" ]]; then
    run_step "Create initial LinkIt Smart 7688 configuration" \
        bash -c '
            cat > "$1/.config" <<EOF
CONFIG_TARGET_ramips=y
CONFIG_TARGET_ramips_mt76x8=y
CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y
EOF
        ' _ "$LINKIT_DIR"
else
    echo "[INFO] Existing .config found; preserving it."
fi

# ------------------------------------------------------------
# STEP 10: GENERATE OPENWRT DEFCONFIG
# ------------------------------------------------------------

run_step "Generate OpenWrt defconfig" \
    make -C "$LINKIT_DIR" defconfig

# Confirm that the requested device remains selected.
if ! grep -qx \
    'CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y' \
    "$LINKIT_DIR/.config"; then
    echo "[ERROR] LinkIt Smart 7688 is not selected in the resulting .config." >&2
    echo "Review the existing configuration with: make -C linkit menuconfig" >&2
    exit 1
fi

# ------------------------------------------------------------
# STEP 11: CHECK BUILD PREREQUISITES
# ------------------------------------------------------------

run_step "Check OpenWrt build prerequisites" \
    make -C "$LINKIT_DIR" prereq

# ------------------------------------------------------------
# STEP 12: FINAL VERIFICATION
# ------------------------------------------------------------

run_step "Verify OpenWrt top-level Makefile" \
    test -f "$LINKIT_DIR/Makefile"

run_step "Verify OpenWrt top-level build include" \
    test -f "$LINKIT_DIR/include/toplevel.mk"

run_step "Verify LinkIt Smart 7688 target configuration" \
    grep -F \
    'CONFIG_TARGET_ramips_mt76x8_DEVICE_mediatek_linkit-smart-7688=y' \
    "$LINKIT_DIR/.config"

# ------------------------------------------------------------
# FINAL SUMMARY
# ------------------------------------------------------------

echo
echo "============================================================"
echo "       OPENWRT BUILD ENVIRONMENT PREPARATION COMPLETE"
echo "============================================================"
echo
echo "Parent repository : $REPO_DIR"
echo "OpenWrt source    : $LINKIT_DIR"
echo "OpenWrt version   : $OPENWRT_TAG"
echo "Target            : MediaTek LinkIt Smart 7688"
echo "Debug mode        : $DEBUG"
echo
echo "The parent repository branch and Git history were not changed."
echo
echo "Next steps:"
echo "  cd \"$LINKIT_DIR\""
echo "  make menuconfig"
echo "  make download -j\$(nproc)"
echo "  make -j\$(nproc) V=s"
echo
echo "============================================================"
