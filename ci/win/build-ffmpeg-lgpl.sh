#!/usr/bin/env sh
set -eu
command -v make >/dev/null
command -v nasm >/dev/null
command -v cmp >/dev/null
# Run inside an MSVC environment through the supported UCRT64 shell wrapper.
source_dir="$(cygpath -u "$1")"
output_dir="$(cygpath -m "$2")"
compiler_dir="$(cygpath -u "${VCToolsInstallDir}bin/Hostx64/x64")"
export PATH="$compiler_dir:$PATH"
cd "$source_dir"
./configure --prefix="$output_dir" --toolchain=msvc --arch=x86_64 \
    --disable-gpl --disable-nonfree --disable-version3 \
    --disable-autodetect --disable-everything --enable-shared --disable-static \
    --disable-programs --disable-doc --disable-network --disable-pthreads \
    --enable-w32threads --enable-decoder=h264 --enable-parser=h264 \
    --enable-demuxer=h264 --enable-muxer=mp4,matroska --enable-protocol=file \
    --disable-debug --extra-cflags=-utf-8
make -j8
make install
