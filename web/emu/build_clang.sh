#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (cursedsound) the browser preview with plain clang and a WASI libc instead of Emscripten: the same wasm, no imports.
#   Debian / Ubuntu: apt install clang lld wasi-libc libclang-rt-dev-wasm32 (or libclang-rt-18-dev-wasm32)
#   macOS: brew install llvm wasi-libc   (WASI_SYSROOT=$(brew --prefix wasi-libc)/share/wasi-sysroot)
#   web/emu/build_clang.sh  ->  build/emu/{index.html, worklet.js, stems.js, fm1backup.js, felucca.wasm, fonts/}
# Run after the generated headers (./build.sh once, or: python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()")
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: generate the headers first (build/gen)"; exit 1; }
CLANG=${CLANG:-clang}
if [ -z "$WASI_SYSROOT" ]; then                       # Debian's wasi-libc: headers and libs under wasm32-wasi, linked here as a sysroot
    WASI_SYSROOT=build/wasi-sysroot
    mkdir -p "$WASI_SYSROOT/include" "$WASI_SYSROOT/lib"
    ln -sfn /usr/include/wasm32-wasi "$WASI_SYSROOT/include/wasm32-wasi"
    ln -sfn /usr/lib/wasm32-wasi "$WASI_SYSROOT/lib/wasm32-wasi"
fi
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
mkdir -p build/emu/fonts
"$CLANG" --target=wasm32-wasi --sysroot="$WASI_SYSROOT" -Iweb/emu/wasi-shim -mexec-model=reactor -O2 -ffp-contract=off \
    -std=gnu99 -w "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    -Wl,--export-dynamic -Wl,-z,stack-size=1048576 -Wl,--initial-memory=33554432 -o build/emu/felucca.wasm web/emu/felucca_web.c
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js web/emu/stems.js web/fm1backup.js build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, $VERSION)"
