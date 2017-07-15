#!/bin/sh

set -e

project_dir=$(realpath "$0" | xargs dirname)

source_dir="$project_dir/src"

build_dir="./build"
test -n "$1" && build_dir="$1"
mkdir -p "$build_dir"
build_dir=$(realpath "$build_dir")

echo "Source directory: $source_dir"
echo "Build directory: $build_dir"

cd "$build_dir"
cmake "$source_dir"
make
make test
