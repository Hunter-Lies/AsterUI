cmake_minimum_required(VERSION 3.21)

project(AsterUI VERSION 1.1.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AsterUI only supports x64 builds")
endif()

set(
    CEF_ROOT
    "$ENV{ASTERUI_CEF_ROOT}"
    CACHE PATH
    "CEF binary distribution root")
set(
    SCRIPTHOOKV_ROOT
    "$ENV{ASTERUI_SCRIPTHOOKV_ROOT}"
    CACHE PATH
    "ScriptHookV SDK root containing inc and lib")

if(NOT CEF_ROOT OR NOT EXISTS "${CEF_ROOT}/cmake")
    message(FATAL_ERROR
        "Set ASTERUI_CEF_ROOT or pass -DCEF_ROOT to a CEF binary distribution")
endif()
if(NOT SCRIPTHOOKV_ROOT OR
   NOT EXISTS "${SCRIPTHOOKV_ROOT}/inc/main.h" OR
   NOT EXISTS "${SCRIPTHOOKV_ROOT}/lib/ScriptHookV.lib")
    message(FATAL_ERROR
        "Set ASTERUI_SCRIPTHOOKV_ROOT or pass -DSCRIPTHOOKV_ROOT to an official ScriptHookV SDK")
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)

add_library(cef_runtime SHARED IMPORTED)
set_target_properties(cef_runtime PROPERTIES
    IMPORTED_IMPLIB_RELEASE "${CEF_LIB_RELEASE}"
    IMPORTED_IMPLIB_DEBUG "${CEF_LIB_DEBUG}"
    IMPORTED_LOCATION_RELEASE "${CEF_BINARY_DIR_RELEASE}/libcef.dll"
    IMPORTED_LOCATION_DEBUG "${CEF_BINARY_DIR_DEBUG}/libcef.dll"
)

add_library(asterui_common STATIC
    src/common/log.cpp
    src/common/pipe.cpp
    src/common/shared_frame.cpp
)
target_include_directories(asterui_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
)
target_compile_definitions(asterui_common PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)
target_compile_options(asterui_common PRIVATE /W4 /permissive- /EHsc /utf-8)

add_library(AsterUI SHARED
    src/asi/api.cpp
    src/asi/dllmain.cpp
    src/asi/input.cpp
    src/asi/renderer.cpp
    src/asi/supervisor.cpp
)
target_include_directories(AsterUI PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${SCRIPTHOOKV_ROOT}/inc"
)
target_compile_definitions(AsterUI PRIVATE AsterUI_ASI_BUILD)
target_compile_options(AsterUI PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(AsterUI PRIVATE
    asterui_common
    "${SCRIPTHOOKV_ROOT}/lib/ScriptHookV.lib"
    d3d11
    dxgi
    d3dcompiler
    psapi
)
set_target_properties(AsterUI PROPERTIES
    PREFIX ""
    OUTPUT_NAME "AsterUI"
    SUFFIX ".asi"
)

add_library(AsterUI.Demo SHARED
    examples/demo-plugin/main.cpp
)
target_include_directories(AsterUI.Demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    "${SCRIPTHOOKV_ROOT}"
)
target_compile_options(AsterUI.Demo PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(AsterUI.Demo PRIVATE
    "${SCRIPTHOOKV_ROOT}/lib/ScriptHookV.lib"
    user32
)
set_target_properties(AsterUI.Demo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "AsterUI.Demo"
    SUFFIX ".asi"
)

add_executable(AsterUI.Host WIN32
    src/host/app.cpp
    src/host/browser.cpp
    src/host/main.cpp
    src/host/pipe_client.cpp
    src/host/request_policy.cpp
    src/host/ui_scheme.cpp
)
target_include_directories(AsterUI.Host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    "${CEF_INCLUDE_PATH}"
)
target_compile_definitions(AsterUI.Host PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WRAPPING_CEF_SHARED
)
target_compile_options(AsterUI.Host PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(AsterUI.Host PRIVATE
    asterui_common
    cef_runtime
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
    kernel32
    user32
    shell32
    ole32
)
set_target_properties(AsterUI.Host PROPERTIES OUTPUT_NAME "AsterUI.Host")

option(ASTERUI_BUILD_TESTS "Build AsterUI smoke tests" ON)
if(ASTERUI_BUILD_TESTS)
    add_executable(AsterUI.HostSmoke tests/host_smoke.cpp)
    target_include_directories(AsterUI.HostSmoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    )
    target_compile_options(AsterUI.HostSmoke PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(AsterUI.HostSmoke PRIVATE asterui_common)
    set_target_properties(AsterUI.HostSmoke PROPERTIES
        OUTPUT_NAME "AsterUI.HostSmoke")

    add_executable(AsterUI.PipeCancelSmoke tests/pipe_cancel.cpp)
    target_include_directories(AsterUI.PipeCancelSmoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_options(AsterUI.PipeCancelSmoke PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(AsterUI.PipeCancelSmoke PRIVATE asterui_common)
    set_target_properties(
        AsterUI.PipeCancelSmoke
        PROPERTIES OUTPUT_NAME "AsterUI.PipeCancelSmoke")

    add_executable(AsterUI.RequestPolicySmoke
        tests/request_policy.cpp
        src/host/request_policy.cpp
    )
    target_include_directories(AsterUI.RequestPolicySmoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_options(AsterUI.RequestPolicySmoke PRIVATE
        /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(AsterUI.RequestPolicySmoke PRIVATE asterui_common)
    set_target_properties(
        AsterUI.RequestPolicySmoke
        PROPERTIES OUTPUT_NAME "AsterUI.RequestPolicySmoke")
endif()

install(TARGETS AsterUI AsterUI.Demo AsterUI.Host RUNTIME DESTINATION .)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY sdk/ DESTINATION sdk)
install(DIRECTORY examples/ DESTINATION examples)
