cmake_minimum_required(VERSION 3.22)
project(pokeemerald_android C ASM)

if(NOT DEFINED POKEEMERALD_ROOT OR NOT EXISTS "${POKEEMERALD_ROOT}/pc/Makefile")
    message(FATAL_ERROR "POKEEMERALD_ROOT does not point to the pokeemerald source tree")
endif()
if(NOT DEFINED SDL2_SOURCE OR NOT EXISTS "${SDL2_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR "SDL2_SOURCE must point to an SDL2 source tree")
endif()
if(NOT EXISTS "${POKEEMERALD_ROOT}/build/assets")
    message(FATAL_ERROR "build/assets is missing; build the ROM assets once with 'make modern'")
endif()

execute_process(
    COMMAND make -C tools/preproc
    WORKING_DIRECTORY "${POKEEMERALD_ROOT}"
    RESULT_VARIABLE TOOLS_RESULT
)
if(NOT TOOLS_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not build the host preprocessing tools")
endif()

add_link_options("-Wl,-z,max-page-size=16384")
# Gradle's debuggable APK normally leaves dependencies at -O0. Keep symbols,
# but do not run SDL's renderer and event loop unoptimized on the device.
add_compile_options($<$<CONFIG:Debug>:-O2>)
add_subdirectory("${SDL2_SOURCE}" SDL2 EXCLUDE_FROM_ALL)

set(SDL_OPENSLES_SOURCE "${SDL2_SOURCE}/src/audio/openslES/SDL_openslES.c")
set_property(
    SOURCE "${SDL_OPENSLES_SOURCE}"
    TARGET_DIRECTORY SDL2
    APPEND PROPERTY COMPILE_OPTIONS
    "-I${SDL2_SOURCE}/src"
    "-include${POKEEMERALD_ROOT}/android/sdl_opensles_buffering.h"
)

set(PREPROC "${POKEEMERALD_ROOT}/tools/preproc/preproc")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}/game" "${GENERATED_DIR}/data" "${GENERATED_DIR}/songs")

set(COMMON_DEFINES -DPLATFORM_PC=1 -DPLATFORM_ANDROID=1 -DPLATFORM_RELATIVE_POINTERS=1 -DMODERN=1)
set(COMMON_INCLUDES -I${POKEEMERALD_ROOT}/pc/include -iquote${POKEEMERALD_ROOT}/include -iquote${POKEEMERALD_ROOT})
set(ASM_CPP_DEFINES -DPLATFORM_PC=1 -DPLATFORM_ANDROID=1 -DMODERN=1)
string(JOIN " " COMMON_DEFINES_SHELL ${COMMON_DEFINES})
string(JOIN " " COMMON_INCLUDES_SHELL ${COMMON_INCLUDES})
string(JOIN " " ASM_CPP_DEFINES_SHELL ${ASM_CPP_DEFINES})

file(GLOB GAME_C_SOURCES "${POKEEMERALD_ROOT}/src/*.c")
list(FILTER GAME_C_SOURCES EXCLUDE REGEX "/(agb_flash|agb_flash_1m|agb_flash_le|agb_flash_mx|libisagbprn|librfu_intr|multiboot|rom_header_gf|siirtc)\\.c$")
set(GAME_GENERATED)
foreach(SOURCE IN LISTS GAME_C_SOURCES)
    get_filename_component(NAME "${SOURCE}" NAME_WE)
    set(OUTPUT "${GENERATED_DIR}/game/${NAME}.c")
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND /bin/sh -c "\"${CMAKE_C_COMPILER}\" ${COMMON_DEFINES_SHELL} ${COMMON_INCLUDES_SHELL} -MMD -MF \"${OUTPUT}.d\" -MT \"${OUTPUT}\" -E \"${SOURCE}\" | \"${PREPROC}\" -i -g \"${POKEEMERALD_ROOT}/build/assets\" \"${SOURCE}\" \"${POKEEMERALD_ROOT}/charmap.txt\" > \"${OUTPUT}\""
        DEPENDS "${SOURCE}" "${PREPROC}"
        DEPFILE "${OUTPUT}.d"
        WORKING_DIRECTORY "${POKEEMERALD_ROOT}"
        VERBATIM
    )
    list(APPEND GAME_GENERATED "${OUTPUT}")
endforeach()

file(GLOB DATA_S_SOURCES "${POKEEMERALD_ROOT}/data/*.s")
file(GLOB_RECURSE ASM_MACRO_DEPENDENCIES CONFIGURE_DEPENDS
    "${POKEEMERALD_ROOT}/asm/macros/*.inc"
)
list(APPEND ASM_MACRO_DEPENDENCIES "${POKEEMERALD_ROOT}/asm/macros.inc")
set(DATA_SHARED_DEPENDENCIES
    "${POKEEMERALD_ROOT}/data/script_cmd_table.inc"
    "${POKEEMERALD_ROOT}/data/specials.inc"
)
file(GLOB MAP_ASM_DEPENDENCIES CONFIGURE_DEPENDS
    "${POKEEMERALD_ROOT}/data/layouts/*.inc"
    "${POKEEMERALD_ROOT}/data/maps/*.inc"
    "${POKEEMERALD_ROOT}/data/maps/*/*.inc"
)
set(DATA_GENERATED)
foreach(SOURCE IN LISTS DATA_S_SOURCES)
    get_filename_component(NAME "${SOURCE}" NAME_WE)
    set(OUTPUT "${GENERATED_DIR}/data/${NAME}.s")
    set(DATA_EXTRA_DEPENDS)
    if(NAME STREQUAL "maps")
        set(DATA_EXTRA_DEPENDS ${MAP_ASM_DEPENDENCIES})
    endif()
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND /bin/sh -c "\"${PREPROC}\" \"${SOURCE}\" \"${POKEEMERALD_ROOT}/charmap.txt\" | \"${CMAKE_C_COMPILER}\" ${ASM_CPP_DEFINES_SHELL} -E -x assembler-with-cpp -I\"${POKEEMERALD_ROOT}/include\" -I\"${POKEEMERALD_ROOT}\" - | \"${PREPROC}\" -ie \"${SOURCE}\" \"${POKEEMERALD_ROOT}/charmap.txt\" | awk -f \"${POKEEMERALD_ROOT}/android/prepare_asm.awk\" > \"${OUTPUT}\""
        DEPENDS "${SOURCE}" "${PREPROC}" "${POKEEMERALD_ROOT}/android/prepare_asm.awk" ${ASM_MACRO_DEPENDENCIES} ${DATA_SHARED_DEPENDENCIES} ${DATA_EXTRA_DEPENDS}
        WORKING_DIRECTORY "${POKEEMERALD_ROOT}"
        VERBATIM
    )
    list(APPEND DATA_GENERATED "${OUTPUT}")
endforeach()

if(ANDROID_ABI MATCHES "x86")
    set(SONG_WORD_DEFINE -Dword=long)
else()
    set(SONG_WORD_DEFINE "")
endif()
file(GLOB SONG_S_SOURCES "${POKEEMERALD_ROOT}/sound/songs/midi/*.s")
set(SONG_GENERATED)
foreach(SOURCE IN LISTS SONG_S_SOURCES)
    get_filename_component(NAME "${SOURCE}" NAME_WE)
    set(OUTPUT "${GENERATED_DIR}/songs/${NAME}.s")
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND /bin/sh -c "cd \"${POKEEMERALD_ROOT}/sound\" && \"${PREPROC}\" \"songs/midi/${NAME}.s\" \"../charmap.txt\" | awk -v song_name=\"${NAME}\" -f \"../android/prepare_song.awk\" | \"${CMAKE_C_COMPILER}\" ${ASM_CPP_DEFINES_SHELL} ${SONG_WORD_DEFINE} -E -x assembler-with-cpp -I../include -I.. -I. - | \"${PREPROC}\" -ie \"songs/midi/${NAME}.s\" \"../charmap.txt\" | awk -f \"../android/prepare_asm.awk\" > \"${OUTPUT}\""
        DEPENDS "${SOURCE}" "${POKEEMERALD_ROOT}/sound/MPlayDef.s" "${POKEEMERALD_ROOT}/android/prepare_song.awk" "${POKEEMERALD_ROOT}/android/prepare_asm.awk" "${PREPROC}"
        VERBATIM
    )
    list(APPEND SONG_GENERATED "${OUTPUT}")
endforeach()

set(PLATFORM_SOURCES
    "${POKEEMERALD_ROOT}/pc/audio.c"
    "${POKEEMERALD_ROOT}/pc/bios.c"
    "${POKEEMERALD_ROOT}/pc/core_main.c"
    "${POKEEMERALD_ROOT}/pc/diagnostics.c"
    "${POKEEMERALD_ROOT}/pc/link.c"
    "${POKEEMERALD_ROOT}/pc/platform.c"
    "${POKEEMERALD_ROOT}/pc/profiles.c"
    "${POKEEMERALD_ROOT}/pc/ppu.c"
    "${POKEEMERALD_ROOT}/pc/services.c"
)

# These two translation units contain the per-pixel PPU and per-sample mixer
# loops. Match the desktop build's optimization level for the mobile hot path.
set_source_files_properties(
    "${POKEEMERALD_ROOT}/pc/audio.c"
    "${POKEEMERALD_ROOT}/pc/ppu.c"
    PROPERTIES COMPILE_OPTIONS "-O3"
)

add_library(pokeemerald_core SHARED
    ${PLATFORM_SOURCES}
    ${GAME_GENERATED}
    ${DATA_GENERATED}
    ${SONG_GENERATED}
)
target_compile_definitions(pokeemerald_core PRIVATE
    PLATFORM_PC=1 PLATFORM_ANDROID=1 PLATFORM_RELATIVE_POINTERS=1 MODERN=1
)
target_compile_options(pokeemerald_core PRIVATE
    -O2 -g0 -fcommon -fno-omit-frame-pointer
    -Wno-address-of-packed-member -Wno-constant-logical-operand
    -Wno-gnu-alignof-expression -Wno-ignored-attributes
    -Wno-implicit-function-declaration -Wno-incompatible-pointer-types-discards-qualifiers
    -Wno-pointer-to-int-cast -Wno-tautological-constant-out-of-range-compare
    -Wno-unknown-attributes -Wno-unused-function -Wno-unused-variable
    $<$<COMPILE_LANGUAGE:C>:-iquote${POKEEMERALD_ROOT}/pc/include>
    $<$<COMPILE_LANGUAGE:C>:-iquote${POKEEMERALD_ROOT}/include>
    $<$<COMPILE_LANGUAGE:C>:-iquote${POKEEMERALD_ROOT}>
    $<$<COMPILE_LANGUAGE:C>:-iquote${POKEEMERALD_ROOT}/sound>
    $<$<COMPILE_LANGUAGE:ASM>:-I${POKEEMERALD_ROOT}/include>
    $<$<COMPILE_LANGUAGE:ASM>:-I${POKEEMERALD_ROOT}>
    $<$<COMPILE_LANGUAGE:ASM>:-I${POKEEMERALD_ROOT}/sound>
    $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_options(pokeemerald_core PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-Wa,-defsym,PLATFORM_64BIT=1>
    )
endif()
if(ANDROID_ABI STREQUAL "arm64-v8a")
    target_compile_options(pokeemerald_core PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-Wa,-defsym,PLATFORM_AARCH64=1>
    )
elseif(ANDROID_ABI STREQUAL "x86_64")
    target_compile_options(pokeemerald_core PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-Wa,-defsym,PLATFORM_X86_64=1>
    )
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_options(pokeemerald_core PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-Wa,-defsym,PLATFORM_ARM=1>
    )
elseif(ANDROID_ABI STREQUAL "x86")
    target_compile_options(pokeemerald_core PRIVATE
        $<$<COMPILE_LANGUAGE:ASM>:-Wa,-defsym,PLATFORM_X86=1>
    )
else()
    message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
endif()
target_compile_options(pokeemerald_core PRIVATE
    $<$<COMPILE_LANGUAGE:ASM>:-Wa,-defsym,PLATFORM_RELATIVE_POINTERS=1>
)
target_link_libraries(pokeemerald_core PRIVATE log m dl)
target_link_options(pokeemerald_core PRIVATE
    "-Wl,--version-script=${POKEEMERALD_ROOT}/android/core.exports"
)
set_target_properties(pokeemerald_core PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME pokeemerald_core
)

add_library(main SHARED
    "${POKEEMERALD_ROOT}/android/frontend.c"
    "${POKEEMERALD_ROOT}/android/core_loader.c"
    "${POKEEMERALD_ROOT}/pc/crash_report.c"
    "${POKEEMERALD_ROOT}/pc/profiles.c"
)
target_compile_definitions(main PRIVATE PLATFORM_PC=1 PLATFORM_ANDROID=1)
target_compile_options(main PRIVATE
    -Wall -Wextra
    -iquote${POKEEMERALD_ROOT}/pc/include
)
target_link_libraries(main PRIVATE SDL2::SDL2 GLESv2 android log dl)
