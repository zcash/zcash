#!/bin/sh

# Set environment variables for portability and strictness.
export LC_ALL=C
# -e: Exit immediately if a command exits with a non-zero status.
# -u: Treat unset variables as an error.
set -eu

# --- Function Definitions ---

# Use 'g' prefixed commands (like greadlink, gmake) if available, 
# for better compatibility on macOS (which uses BSD utilities).
# Arguments: $1: Variable to assign, $2: Base command name
gprefix() {
    # Check if 'g' prefixed version exists
    if command -v "g$2" >/dev/null; then
        eval "$1=\"g$2\""
    else
        eval "$1=\"$2\""
    fi
}

# --- Command Path Setup ---

# Detect the preferred 'readlink' version and assign it to READLINK.
gprefix READLINK readlink

# Change directory to the repository root.
# Uses READLINK to handle relative paths and symbolic links reliably.
cd "$(dirname "$("$READLINK" -f "$0")")/.."

# --- Environment Variable Defaults ---

# Allow user overrides (e.g., MAKE=gmake ./zcutil/build.sh).
# Set defaults using Parameter Expansion: ${VAR:-default_value}
MAKE=${MAKE:-make}

# Allow overrides for porters (BUILD=i686-pc-linux-gnu ./zcutil/build.sh).
# Determine the build machine type.
BUILD=${BUILD:-$(./depends/config.guess)}
# Assume host is the same as build unless explicitly overridden.
HOST=${HOST:-$BUILD}

# Handle CONFIGURE_FLAGS.
QUIET_FLAG="--quiet"
# Check if V=1 (verbose) argument was passed in.
for arg in "$@"
do
    if [ "$arg" = "V=1" ]; then
        QUIET_FLAG=""
        break
    fi
done

# If CONFIGURE_FLAGS is unset, apply the quiet flag (or empty if V=1 was passed).
CONFIGURE_FLAGS=${CONFIGURE_FLAGS:-$QUIET_FLAG}

# --- Help Message ---

if [ "$*" = '--help' ]; then
    cat <<EOF
Usage:

$0 --help
  Show this help message and exit.

$0 [ MAKEARGS... ]
  Build Zcash and most of its transitive dependencies from
  source. MAKEARGS are applied to both dependencies and Zcash itself.

  Pass flags to ./configure using the CONFIGURE_FLAGS environment variable.
  For example, to enable coverage instrumentation (thus enabling "make cov"
  to work), call:

      CONFIGURE_FLAGS="--enable-lcov --disable-hardening" ./zcutil/build.sh

  For verbose output, use:
      ./zcutil/build.sh V=1
EOF
    exit 0
fi

# Enable command tracing for the build steps (standard for build scripts).
set -x

# --- Dependency Build ---

# Display versions for debugging environment issues.
"$MAKE" --version
as --version

# Check if debug mode is explicitly enabled via CONFIGURE_FLAGS.
DEBUG=""
if echo "$CONFIGURE_FLAGS" | grep -q --enable-debug; then
    DEBUG=1
fi

# Build dependencies. Pass MAKEARGS and the detected DEBUG flag.
HOST="$HOST" BUILD="$BUILD" "$MAKE" "$@" -C ./depends/ DEBUG="$DEBUG"

# Stop if the user only requested the 'depends' stage.
if [ "${BUILD_STAGE:-all}" = "depends" ]; then
    exit 0
fi

# --- Main Project Build ---

# Clean up previous build artifacts.
./zcutil/clean.sh

# Regenerate configure scripts, etc.
./autogen.sh

# Configure the Zcash project.
# CONFIG_SITE ensures the project uses the dependencies built in the 'depends' directory.
CONFIG_SITE="$PWD/depends/$HOST/share/config.site" ./configure $CONFIGURE_FLAGS

# Build the main project.
"$MAKE" "$@"
