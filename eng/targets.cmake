file(READ "${CMAKE_SOURCE_DIR}/eng/architecture.json" NENE_ARCHITECTURE)

# 警告の集合はここが唯一の出どころ（ADR 0003 の実測に対応する）。並びは eng/probes/language.json の
# flagSets.clangStrict と同じにする（Phase 0 が「この集合で C8-strict-clean-clang が通る」と測った並び）。
# -Wno-language-extension-token: COM の __uuidof / IID_PPV_ARGS が -Wpedantic の
# -Wlanguage-extension-token で落ちる（W1-uuidof-under-pedantic）。名指しで 1 つだけ外す。
set(NENENIB_CXX_OPTIONS
    /W4 /WX /utf-8 /EHsc
    -Wextra -Wpedantic
    -Wno-switch-default
    -Wswitch-enum -Wcovered-switch-default
    -Wcast-qual -Wconversion -Wsign-conversion -Wshadow
    -Wimplicit-fallthrough
    -Wdouble-promotion -Wformat=2 -Wundef -Wunused-result
    -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wextra-semi
    -Wzero-as-null-pointer-constant -Wmissing-prototypes
    -Werror=vla-cxx-extension
    -Wno-language-extension-token)

# 検出層（CPP-016 / ADR 0003）。Debug 構成の全 target を ASan / UBSan で計装し、回復させない。
# ASan は Debug CRT（-MTd）と共存できないので、CRT は CMakeLists.txt が常に /MT に固定する（A1）。
# ランタイムは clang-cl の resource dir から lld-link へ明示的に結ぶ（CMake は link を driver 経由で呼ばない）。
execute_process(COMMAND "${CMAKE_CXX_COMPILER}" /clang:-print-resource-dir
                OUTPUT_VARIABLE NENENIB_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
set(NENENIB_SANITIZE_OPTIONS
    -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all)
set(NENENIB_SANITIZE_RUNTIME "${NENENIB_RESOURCE_DIR}/lib/windows")
set(NENENIB_SANITIZE_LIBRARIES
    "${NENENIB_SANITIZE_RUNTIME}/clang_rt.asan-x86_64.lib"
    "${NENENIB_SANITIZE_RUNTIME}/clang_rt.asan_cxx-x86_64.lib"
    "${NENENIB_SANITIZE_RUNTIME}/clang_rt.ubsan_standalone-x86_64.lib"
    "${NENENIB_SANITIZE_RUNTIME}/clang_rt.ubsan_standalone_cxx-x86_64.lib")

function(nenenib_target target module kind)
    string(JSON module_path ERROR_VARIABLE module_error GET "${NENE_ARCHITECTURE}" modules "${module}" path)
    if(module_error)
        message(FATAL_ERROR "ARC-002: unapproved module ${module}")
    endif()
    if(NOT ARGN)
        message(FATAL_ERROR "ARC-002: empty future modules are forbidden")
    endif()
    foreach(source IN LISTS ARGN)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE absolute_source)
        set(module_root "${CMAKE_SOURCE_DIR}/${module_path}")
        cmake_path(IS_PREFIX module_root "${absolute_source}" NORMALIZE inside)
        if(NOT inside)
            message(FATAL_ERROR "ARC-002: ${source} does not belong to ${module}")
        endif()
    endforeach()
    if(kind STREQUAL "EXECUTABLE")
        add_executable(${target} ${ARGN})
        target_link_options(${target} PRIVATE
            "$<$<CONFIG:Debug>:/INCREMENTAL:NO;/WHOLEARCHIVE:${NENENIB_SANITIZE_RUNTIME}/clang_rt.asan-x86_64.lib>")
        foreach(library IN LISTS NENENIB_SANITIZE_LIBRARIES)
            target_link_libraries(${target} PRIVATE "$<$<CONFIG:Debug>:${library}>")
        endforeach()
    elseif(kind STREQUAL "STATIC")
        add_library(${target} STATIC ${ARGN})
    else()
        message(FATAL_ERROR "ARC-002: unsupported target kind ${kind}")
    endif()
    set_property(TARGET ${target} PROPERTY NENE_MODULE "${module}")
    target_compile_options(${target} PRIVATE ${NENENIB_CXX_OPTIONS}
                           "$<$<CONFIG:Debug>:${NENENIB_SANITIZE_OPTIONS}>")
    target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/${module_path}")
    if(module MATCHES "^(adapters_win32|ui_win32|app)$")
        target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endfunction()

function(nenenib_system_link target library)
    get_target_property(module ${target} NENE_MODULE)
    string(JSON allowed ERROR_VARIABLE error GET "${NENE_ARCHITECTURE}" platformLibraries "${module}")
    if(error)
        message(FATAL_ERROR "ARC-002: no platform libraries for ${module}")
    endif()
    string(JSON count LENGTH "${allowed}")
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON candidate GET "${allowed}" ${index})
        if(candidate STREQUAL library)
            target_link_libraries(${target} PRIVATE ${library})
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "ARC-002: forbidden platform library ${module} -> ${library}")
endfunction()

function(nenenib_link target dependency)
    get_target_property(module ${target} NENE_MODULE)
    get_target_property(destination ${dependency} NENE_MODULE)
    string(JSON allowed GET "${NENE_ARCHITECTURE}" modules "${module}" dependencies)
    string(JSON count LENGTH "${allowed}")
    set(found FALSE)
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON candidate GET "${allowed}" ${index})
            if(candidate STREQUAL destination)
                set(found TRUE)
            endif()
        endforeach()
    endif()
    if(NOT found)
        message(FATAL_ERROR "ARC-002: forbidden dependency ${module} -> ${destination}")
    endif()
    target_link_libraries(${target} PRIVATE ${dependency})
endfunction()
