# Copies the CRT filter shaders listed in a manifest, plus the 2Ship presets, into a "shaders" folder.
# Run as: cmake -DSRC=<slang-shaders checkout> -DDEST=<shaders folder> -DMANIFEST=<shader-files.txt>
#                -DPRESET_DIR=<folder with .slangp presets> -P crt-shaders-stage.cmake
# file(COPY) keeps timestamps and skips files that are already up to date.

foreach(var SRC DEST MANIFEST PRESET_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "crt-shaders-stage.cmake: ${var} is not set")
    endif()
endforeach()

file(STRINGS "${MANIFEST}" entries)
set(missing 0)
foreach(entry IN LISTS entries)
    string(STRIP "${entry}" entry)
    if(entry STREQUAL "" OR entry MATCHES "^#")
        continue()
    endif()
    if(NOT EXISTS "${SRC}/${entry}")
        message(WARNING "CRT filter: ${SRC}/${entry} is missing")
        math(EXPR missing "${missing} + 1")
        continue()
    endif()
    get_filename_component(dir "${entry}" DIRECTORY)
    file(COPY "${SRC}/${entry}" DESTINATION "${DEST}/${dir}")
endforeach()

file(GLOB presets "${PRESET_DIR}/*.slangp")
file(COPY ${presets} DESTINATION "${DEST}")

if(missing GREATER 0)
    message(WARNING "CRT filter: ${missing} shader files were not staged, some presets will not load")
endif()
