#!/usr/bin/env bash
# Majora's Mask 3DS build: libultraship (3DS), then mm_3ds.
# On macOS run with devkitPro's bash: /opt/devkitpro/pacman/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PATH="$PWD/../.bin:$DEVKITPRO/tools/bin:$PATH"
JOBS="${MM_BUILD_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"

# CONTROLLERBUTTONS_T=uint32_t: 2S2H uses extra ocarina buttons (bits 16+)
# and defines it globally in the PC build, LUS included. Without it the
# ControlDeck symbols don't match (uint16_t vs uint32_t).
# N3DS_BUILD=ON: drops the LUS OpenGL backend.
cmake -S third_party/libultraship -B third_party/libultraship/build-3ds \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/3DS.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$DEVKITPRO/portlibs/3ds" \
    -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -DCONTROLLERBUTTONS_T=uint32_t" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -DCONTROLLERBUTTONS_T=uint32_t" \
    -DGBI_UCODE:STRING=F3DEX_GBI_2 \
    -DINCLUDE_MPQ_SUPPORT=OFF -DGFX_DEBUG_DISASSEMBLER=OFF \
    -DDISABLE_DLL_LOADER=ON -DN3DS_BUILD=ON
cmake --build third_party/libultraship/build-3ds -j"$JOBS"

cmake -S . -B build-3ds-mk \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/3DS.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-3ds-mk -j"$JOBS" --target mm_3ds
echo "build ok: build-3ds-mk/mm_3ds.3dsx"
