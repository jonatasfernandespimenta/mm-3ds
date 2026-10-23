#!/usr/bin/env bash
# Build do Majora's Mask 3DS: libultraship (3DS) e depois mm_3ds.
# No macOS rode com o bash do devkitPro: /opt/devkitpro/pacman/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PATH="$PWD/../.bin:$DEVKITPRO/tools/bin:$PATH"
JOBS="${MM_BUILD_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"

# CONTROLLERBUTTONS_T=uint32_t: o 2S2H usa botões extras da ocarina (bits 16+)
# e o define globalmente no build de PC, inclusive para a LUS. Sem isso os
# símbolos de ControlDeck não batem (uint16_t vs uint32_t).
# N3DS_BUILD=ON: tira o backend OpenGL da LUS.
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
