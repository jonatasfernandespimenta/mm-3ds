# ============================================================================
# Majora's Mask (2 Ship 2 Harkinian) on 3DS
# ============================================================================
# Espelha a seção soh_decomp/soh_enhancement do CMakeLists.txt raiz, mas para
# third_party/2ship/mm. Mesma divisão: decomp em C, camada 2s2h em C++.
set(MM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/2ship/mm")
set(TS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/2ship")

# Prefixos de CVar e defines de log, como o CMakeLists raiz do 2S2H faz.
include("${TS_DIR}/CMake/2ship-cvars.cmake")
file(READ "${TS_DIR}/CMake/lus-cvars.cmake" _mm_lus_cvars)
string(REPLACE "include(\"libultraship/cmake/cvars.cmake\")" "" _mm_lus_cvars "${_mm_lus_cvars}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mm-lus-cvars-local.cmake" "${_mm_lus_cvars}")
include("${CMAKE_CURRENT_BINARY_DIR}/mm-lus-cvars-local.cmake")
include("${LUS_DIR}/cmake/cvars.cmake")

set(MM_COMMON_DEFS
    F3DEX_GBI_2 NDEBUG INCLUDE_GAME_PRINTF NON_MATCHING NON_EQUIVALENT
    CONTROLLERBUTTONS_T=uint32_t
    SPDLOG_ACTIVE_LEVEL=6 LOG_LEVEL_GAME_PRINTS=6)

set(MM_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${MM_DIR}/include" "${MM_DIR}/include/PR" "${MM_DIR}/src" "${MM_DIR}"
    "${MM_DIR}/2s2h" "${MM_DIR}/assets"
    "${TS_DIR}/ZAPDTR/ZAPD/resource/type"
    "${LUS_DIR}/include" "${LUS_DIR}/src"
    "${DEVKITPRO}/libctru/include" "${PORTLIBS}/include")

# --- decomp (mm/src) --------------------------------------------------------
file(GLOB_RECURSE MM_DECOMP_SOURCES "${MM_DIR}/src/*.c")
# Mesma lista de exclusão do mm/CMakeLists.txt upstream: o 2S2H tira toda a
# libultra (a LUS fornece), dmadata e elf_message.
foreach(_ex dmadata elf_message libultra)
    list(FILTER MM_DECOMP_SOURCES EXCLUDE REGEX "/mm/src/${_ex}/")
endforeach()
list(LENGTH MM_DECOMP_SOURCES MM_DECOMP_COUNT)
message(STATUS "mm decomp: ${MM_DECOMP_COUNT} translation units")

add_library(mm_decomp STATIC ${MM_DECOMP_SOURCES})
target_include_directories(mm_decomp PRIVATE ${MM_INCLUDES})
target_compile_definitions(mm_decomp PRIVATE ${MM_COMMON_DEFS})
# ponytail: -w -fpermissive pelo mesmo motivo do soh_decomp (GCC 16 promove
# conversões ponteiro/inteiro a erro); -Os pelo orçamento de .text.
target_compile_options(mm_decomp PRIVATE -w -fpermissive -Os)
set_target_properties(mm_decomp PROPERTIES C_STANDARD 11 EXCLUDE_FROM_ALL TRUE)

# --- camada 2s2h (mm/2s2h) --------------------------------------------------
file(GLOB_RECURSE MM_ENH_SOURCES "${MM_DIR}/2s2h/*.cpp" "${MM_DIR}/2s2h/*.c")
# Extrator roda no PC (mm.o2r vai pronto no SD).
list(FILTER MM_ENH_SOURCES EXCLUDE REGEX "/2s2h/Extractor/")
# ponytail: pictógrafo colorido precisa de libpng (3ds-libpng exige root no
# dkp-pacman). Se auto-registra, então tirar o arquivo só some com a opção.
list(FILTER MM_ENH_SOURCES EXCLUDE REGEX "/Enhancements/Items/ColorPictograph\\.cpp$")
list(LENGTH MM_ENH_SOURCES MM_ENH_COUNT)
message(STATUS "mm 2s2h layer: ${MM_ENH_COUNT} translation units")

add_library(mm_enhancement STATIC ${MM_ENH_SOURCES})
# O mixer do 2S2H inclui <opus/opus.h>; o opus vendorizado expõe os headers
# direto em include/. Um symlink resolve sem mexer no fonte.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mm-shim")
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus/include"
    "${CMAKE_CURRENT_BINARY_DIR}/mm-shim/opus" SYMBOLIC)
target_include_directories(mm_enhancement PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/mm-shim")
target_include_directories(mm_enhancement PRIVATE ${MM_INCLUDES}
    "${LUS_BUILD}/_deps/threadpool-src/include"
    "${LUS_BUILD}/_deps/imgui-src"
    "${LUS_BUILD}/_deps/stb"
    "${LUS_BUILD}/_deps/nlohmann_json-src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dr_libs"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compat3ds"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libogg/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libvorbis/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opusfile/include"
    "${PORTLIBS}/include/SDL2")
target_compile_definitions(mm_enhancement PRIVATE ${MM_COMMON_DEFS})
# O 2S2H depende dos PCH para ordem de include (vários .cpp incluem BenPort.h
# dentro de extern "C" assumindo que já foi incluído). Mesma lista do upstream.
target_precompile_headers(mm_enhancement PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<libultraship/bridge/consolevariablebridge.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<ship/Context.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<ship/resource/ResourceManager.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<ship/window/Window.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<spdlog/spdlog.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<variant$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:${MM_DIR}/2s2h/GameInteractor/GameInteractor.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:${MM_DIR}/include/variables.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:${MM_DIR}/include/functions.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:${MM_DIR}/include/macros.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:${MM_DIR}/include/z64.h>")
target_compile_options(mm_enhancement PRIVATE -w -fpermissive -Os)
set_target_properties(mm_enhancement PROPERTIES CXX_STANDARD 20 EXCLUDE_FROM_ALL TRUE)

# --- o jogo: mm_3ds ---------------------------------------------------------
# Mesmo esqueleto do soh_3ds_old: main do decomp (mm/src/code/main.c) chama
# InitOTR do BenPort.cpp; .o2r pré-extraídos no SD, sem extrator no console.
add_executable(mm_3ds tests/soh_3ds_main.cpp
    src/compat3ds/logging_3ds.cpp
    src/compat3ds/sleep_3ds.cpp
    src/compat3ds/excluded_stubs.cpp
    src/setup/mm_prebuilt_3ds.cpp
    src/compat3ds/mm_throw_trace.cpp)
target_compile_definitions(mm_3ds PRIVATE SOH3DS_DATA_DIR="sdmc:/3ds/2ship")
target_include_directories(mm_3ds PRIVATE
    src tests src/compat3ds
    "${LUS_BUILD}/_deps/imgui-src"
    "${LUS_BUILD}/_deps/imgui-src/backends"
    "${LUS_BUILD}/_deps/nlohmann_json-src/include"
    "${LUS_BUILD}/_deps/threadpool-src/include"
    ${MM_INCLUDES} "${PORTLIBS}/include/SDL2")
target_link_libraries(mm_3ds PRIVATE
    -Wl,--start-group
    -Wl,--whole-archive mm_enhancement -Wl,--no-whole-archive
    mm_decomp ${SOH3DS_GFX_LIB}
    "${LUS_BUILD}/src/libultraship.a"
    -Wl,--end-group
    "${LUS_BUILD}/libImGui.a"
    "${LUS_BUILD}/_deps/prism-build/libprism.a"
    "${LUS_BUILD}/libstb.a"
    "${LUS_BUILD}/libmonocypher.a"
    "${PORTLIBS}/lib/libzip.a" "${PORTLIBS}/lib/libspdlog.a"
    "${PORTLIBS}/lib/libtinyxml2.a" "${PORTLIBS}/lib/libzlibstatic.a"
    ${LIB_SDL2} ${LIB_CITRO3D} ${LIB_CTRU} m)
target_compile_options(mm_3ds PRIVATE -w -fpermissive -Os)
# Mesmos wraps do soh_3ds (pilha mínima de threads, malloc instrumentado).
target_link_options(mm_3ds PRIVATE -Wl,--wrap=__syscall_thread_create -Wl,--wrap=threadCreate
    -Wl,--wrap=malloc
    -Wl,--wrap=_ZSt20__throw_out_of_rangePKc -Wl,--wrap=_ZSt24__throw_out_of_range_fmtPKcz)
set_target_properties(mm_3ds PROPERTIES CXX_STANDARD 20 EXCLUDE_FROM_ALL TRUE)

# CIA: reaproveita o app.rsf do soh-3ds (178 MB no New 3DS). Título próprio
# vem depois; por ora é só pra rodar no Azahar.
add_custom_command(TARGET mm_3ds POST_BUILD
    COMMAND "${DEVKITPRO}/tools/bin/3dsxtool" $<TARGET_FILE:mm_3ds> $<TARGET_FILE:mm_3ds>.3dsx
    COMMENT "3dsxtool -> mm_3ds.3dsx")

# CIA: mesmo app.rsf do soh-3ds com título/ID próprios (não sobrescreve a
# instalação do SoH) e 178 MB no New 3DS, como o perfil do soh_3ds_old.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/platform/3ds/cia/app.rsf" _mm_rsf)
string(REPLACE "Title                   : Ship of Harkinian" "Title                   : 2 Ship 2 Harkinian" _mm_rsf "${_mm_rsf}")
string(REPLACE "ProductCode             : CTR-P-SOH3" "ProductCode             : CTR-P-2SH3" _mm_rsf "${_mm_rsf}")
string(REPLACE "UniqueId                : 0x53480" "UniqueId                : 0x25348" _mm_rsf "${_mm_rsf}")
string(REPLACE "SystemModeExt                 : 124MB" "SystemModeExt                 : 178MB" _mm_rsf "${_mm_rsf}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mm3ds.rsf" CONTENT "${_mm_rsf}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mm3ds-romfs")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mm3ds-romfs/README.txt"
    "Copie mm.o2r e 2ship.o2r para /3ds/2ship no cartao SD.\n")
add_custom_command(TARGET mm_3ds POST_BUILD
    COMMAND "${DEVKITPRO}/tools/bin/makerom" -f cia
        -o "${CMAKE_CURRENT_BINARY_DIR}/mm_3ds.cia" -elf "$<TARGET_FILE:mm_3ds>"
        -rsf "${CMAKE_CURRENT_BINARY_DIR}/mm3ds.rsf"
        -icon "${CMAKE_CURRENT_SOURCE_DIR}/platform/3ds/cia/icon.icn"
        -banner "${CMAKE_CURRENT_SOURCE_DIR}/platform/3ds/cia/banner.bnr"
        "-DROMFS_ROOT=${CMAKE_CURRENT_BINARY_DIR}/mm3ds-romfs" -target t -exefslogo
    COMMENT "makerom -> mm_3ds.cia"
    VERBATIM)
