# CRT filter shaders
#
# The presets in mm/crt-filter build on libretro/slang-shaders (the crt-guest-advanced family and the other
# CRT presets the menu offers). The shader sources are fetched once at configure time, as a sparse, blob-less
# clone pinned to a commit, unless CRT_SHADER_SOURCE_DIR points at an existing checkout. The files the bundled
# presets need (mm/crt-filter/shader-files.txt) are then staged, together with the presets, into a "shaders"
# folder next to the game, or into the NRO's romfs on the Nintendo Switch.
#
# On Windows the DirectX 11 renderer runs the presets through librashader.dll, which is copied from
# CRT_DEPS_DIR/librashader when present (https://github.com/SnowflakePowered/librashader/releases). The OpenGL
# renderer, which the Switch uses, has its own runtime in libultraship and needs nothing else.

option(CRT_FILTER_SHADERS "Bundle the CRT filter shaders with the game" ON)
set(CRT_SHADERS_GIT_TAG "afb1416b6b85d3e53c6e586a9209cb9097c7b4a4" CACHE STRING
    "libretro/slang-shaders commit the CRT filter is tested with")
set(CRT_SHADER_SOURCE_DIR "" CACHE PATH "Existing libretro/slang-shaders checkout (fetched when empty)")
set(CRT_DEPS_DIR "${CMAKE_SOURCE_DIR}/../crt-deps" CACHE PATH
    "Optional local staging folder holding librashader/ and slang-shaders/")

set(CRT_SHADER_MANIFEST "${CMAKE_SOURCE_DIR}/mm/crt-filter/shader-files.txt")
set(CRT_PRESET_DIR "${CMAKE_SOURCE_DIR}/mm/crt-filter")
set(CRT_STAGE_SCRIPT "${CMAKE_SOURCE_DIR}/CMake/crt-shaders-stage.cmake")

# Locates or fetches the slang-shaders checkout, leaving its path in out_var (empty on failure)
function(crt_shaders_source out_var)
    if(CRT_SHADER_SOURCE_DIR AND EXISTS "${CRT_SHADER_SOURCE_DIR}/crt")
        set(${out_var} "${CRT_SHADER_SOURCE_DIR}" PARENT_SCOPE)
        return()
    endif()
    if(EXISTS "${CRT_DEPS_DIR}/slang-shaders/crt")
        set(${out_var} "${CRT_DEPS_DIR}/slang-shaders" PARENT_SCOPE)
        return()
    endif()

    set(dir "${CMAKE_BINARY_DIR}/_deps/slang-shaders")
    if(NOT EXISTS "${dir}/crt/crt-guest-advanced.slangp")
        find_package(Git QUIET)
        if(NOT GIT_EXECUTABLE)
            message(WARNING "CRT filter: git was not found, the shaders can't be fetched")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()
        message(STATUS "CRT filter: fetching libretro/slang-shaders ${CRT_SHADERS_GIT_TAG}")
        file(REMOVE_RECURSE "${dir}")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --quiet --filter=blob:none --no-checkout
                    https://github.com/libretro/slang-shaders.git "${dir}"
            RESULT_VARIABLE rc)
        if(rc EQUAL 0)
            # Only the folders the manifest draws from; the repository also carries large bezel packs
            execute_process(COMMAND ${GIT_EXECUTABLE} -C "${dir}" sparse-checkout set crt include blurs
                            RESULT_VARIABLE rc)
        endif()
        if(rc EQUAL 0)
            execute_process(COMMAND ${GIT_EXECUTABLE} -C "${dir}" checkout --quiet --detach ${CRT_SHADERS_GIT_TAG}
                            RESULT_VARIABLE rc)
        endif()
        if(NOT rc EQUAL 0)
            message(WARNING "CRT filter: could not fetch libretro/slang-shaders, the filter will be unavailable. "
                            "Set CRT_SHADER_SOURCE_DIR to a checkout to build without network access.")
            set(${out_var} "" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${out_var} "${dir}" PARENT_SCOPE)
endfunction()

# Stages the shaders for the given game target. Desktop builds get a "shaders" folder next to the executable
# after every build, the Switch build gets them in ${CMAKE_BINARY_DIR}/romfs before the NRO is created.
function(crt_shaders_setup target)
    if(NOT CRT_FILTER_SHADERS)
        return()
    endif()
    crt_shaders_source(source)
    if(NOT source)
        return()
    endif()
    message(STATUS "CRT filter: shaders from ${source}")

    set(stage ${CMAKE_COMMAND} "-DSRC=${source}" "-DMANIFEST=${CRT_SHADER_MANIFEST}" "-DPRESET_DIR=${CRT_PRESET_DIR}")
    if(CMAKE_SYSTEM_NAME MATCHES "NintendoSwitch")
        set(romfs "${CMAKE_BINARY_DIR}/romfs")
        file(MAKE_DIRECTORY "${romfs}/shaders")
        add_custom_target(CrtShadersRomfs
            COMMAND ${stage} "-DDEST=${romfs}/shaders" -P "${CRT_STAGE_SCRIPT}"
            COMMENT "Staging CRT filter shaders into the romfs" VERBATIM)
        add_dependencies(${target} CrtShadersRomfs)
        set(CRT_ROMFS_DIR "${romfs}" PARENT_SCOPE)
        return()
    endif()

    set(commands COMMAND ${stage} "-DDEST=$<TARGET_FILE_DIR:${target}>/shaders" -P "${CRT_STAGE_SCRIPT}")
    if(WIN32 AND EXISTS "${CRT_DEPS_DIR}/librashader/librashader.dll")
        list(APPEND commands COMMAND ${CMAKE_COMMAND} -E copy_if_different
             "${CRT_DEPS_DIR}/librashader/librashader.dll" $<TARGET_FILE_DIR:${target}>)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD ${commands} COMMENT "Staging CRT filter shaders..." VERBATIM)

    if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
        # The staged folder goes out with the packaged game as well
        install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" \"-DSRC=${source}\" \"-DMANIFEST=${CRT_SHADER_MANIFEST}\"
                    \"-DPRESET_DIR=${CRT_PRESET_DIR}\" \"-DDEST=\${CMAKE_INSTALL_PREFIX}/shaders\" -P \"${CRT_STAGE_SCRIPT}\")"
                COMPONENT 2s2h)
    endif()
endfunction()
