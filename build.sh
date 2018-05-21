#!/bin/sh

# Defaults
enable_ut="yes"     # Unit tests enabled
build_type="Debug"  # Build type


usage() {
    cat <<HERE
Usage: $0 [OPTIONS] [-- buildchain parameters]

OPTIONS:
    -h or --help                Print this usage help and exit
    -b or --build-dir <dir>     Use <dir> as build directory
                                (<PROJECT_DIR>/build by default)
    -t or --build-type <type>   Build type (Debug, Release, RelWithDebInfo
                                and MinSizeRel, default: $build_type)
    -d or --build-debug         Same as -t Debug
    -r or --build-release       Same as -t Release
    -c or --clean               Clean build directory before build starts
                                (BEWARE, removed the whole build directory)
    -u or --enable-ut           Enable unit tests run (default: $enable_ut)
    -U or --disable-ut          Disable unit tests run
    -D or --devel               Run in development mode (don't run UTs,
                                generate debug symbols,
                                force colours and use less pager)
    -C or --cxx-flags <flags>   Add custom compiler flags (note that
                                the build script also honours CXXFLAGS
                                environment variable content)

Buildchain parameters are forwarded to underlying buildchain.
Use it e.g. to pass parameters such like -j to make...

EXAMPLE (build in custom directory and set parallel build limit):
$ $0 --build-dir /var/tmp/my_build -- -j4

HERE
}


echo_colour() {
    colour="$1"; shift
    cmake -E cmake_echo_color "--$colour" "$*"
}


set -e

# Parse options
build_dir=""
clean="no"
pager=""
devel_mode="no"
cxx_flags="${CXXFLAGS}"

args=$(
    getopt \
        -n "$0" \
        -o hb:t:drcuUDC: \
        --long help,build-dir:,build-type:,build-debug,build-release,clean,enable-ut,disable-ut,devel,cxx-flags: \
        -- "$@" \
    || (echo >&2; usage >&2; exit 1)
)

eval set -- "$args"
while true; do
    case "$1" in
        -h|--help)
            usage; exit 0
            ;;

        -b|--build-dir)
            build_dir="$2"; shift; shift
            ;;

        -t|--build-type)
            build_type="$2"; shift; shift
            ;;

        -d|--build-debug)
            build_type="Debug"; shift
            ;;

        -r|--build-release)
            build_type="Release"; shift
            ;;

        -c|--clean)
            clean="yes"; shift
            ;;

        -u|--enable-ut)
            enable_ut="yes"; shift
            ;;

        -U|--disable-ut)
            enable_ut="no"; shift
            ;;

        -D|--devel)
            devel_mode="yes"
            enable_ut="no"
            cxx_flags="$cxx_flags -g -fdiagnostics-color=always"
            pager="2>&1 | less -R"
            shift
            ;;

        -C|--cxx-flags)
            cxx_flags="$cxx_flags $2"; shift; shift
            ;;

        --) shift; break
            ;;

        *)  echo "INTERNAL ERROR: unresolved legal option '$1'" >&2
            exit 64
            ;;
    esac
done


# Resolve directories
project_dir=$(realpath "$0" | xargs dirname)
source_dir="$project_dir/src"

if test -n "$build_dir"; then
    build_dir=$(realpath "$build_dir")
else
    build_dir="$project_dir/build"
fi


# Report
echo_colour yellow "Source directory: $source_dir"
echo_colour yellow "Build directory: $build_dir"
echo_colour yellow "Build type: $build_type"
echo_colour yellow "Clean build: $clean"
echo_colour yellow "Buildchain parameters: $@"
echo_colour yellow "Extra compiler flags: $cxx_flags"
echo_colour yellow "Unit tests enabled: $enable_ut"
echo


# Clean build
test "$clean" = "yes" && rm -rf "$build_dir"

# Build
mkdir -p "$build_dir"
cd "$build_dir"
CXXFLAGS="$cxx_flags" cmake -DCMAKE_BUILD_TYPE="$build_type" "$source_dir"
eval cmake --build . -- "$@" $pager

test "$devel_mode" = "yes" && exit 0  # skip the rest in devel mode


# Unit testing
test "$enable_ut" = "yes" && make test


# All done
echo_colour green "
------------------
Built SUCCESSFULLY
------------------
"
