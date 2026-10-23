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
list(LENGTH MM_ENH_SOURCES MM_ENH_COUNT)
message(STATUS "mm 2s2h layer: ${MM_ENH_COUNT} translation units")

add_library(mm_enhancement STATIC ${MM_ENH_SOURCES})
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
target_compile_options(mm_enhancement PRIVATE -w -fpermissive -Os)
set_target_properties(mm_enhancement PROPERTIES CXX_STANDARD 20 EXCLUDE_FROM_ALL TRUE)
