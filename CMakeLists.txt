cmake_minimum_required(VERSION 3.21...3.31)

project(WeChatVoiceExport
    VERSION 0.1.0
    DESCRIPTION "Export favorited WeChat voice messages from local WeChat 4.x data on Windows"
    LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(WECHATVOICEEXPORT_IS_TOP_LEVEL_DEFAULT ON)
else()
    set(WECHATVOICEEXPORT_IS_TOP_LEVEL_DEFAULT OFF)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(WECHATVOICEEXPORT_ENABLE_ASAN "Enable AddressSanitizer where supported" OFF)
option(WECHATVOICEEXPORT_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer where supported" OFF)
option(WECHATVOICEEXPORT_ENABLE_COVERAGE "Enable coverage flags where supported" OFF)
option(WECHATVOICEEXPORT_BUILD_CLI "Build the wxvoice CLI" ${WECHATVOICEEXPORT_IS_TOP_LEVEL_DEFAULT})
option(WECHATVOICEEXPORT_BUILD_TESTING "Build CTest tests" ${WECHATVOICEEXPORT_IS_TOP_LEVEL_DEFAULT})

set(WECHATVOICEEXPORT_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/wechatvoiceexport_version.h.in"
    "${WECHATVOICEEXPORT_GENERATED_INCLUDE_DIR}/wechatvoiceexport/version.h"
    @ONLY
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ThirdParty.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Sources.cmake")

add_library(wechatvoiceexport_core ${WECHATVOICEEXPORT_CORE_SOURCES})
add_library(WeChatVoiceExport::wechatvoiceexport_core ALIAS wechatvoiceexport_core)
target_link_libraries(wechatvoiceexport_core PRIVATE silk_decoder)
if(WIN32)
    # SHGetKnownFolderPath / FOLDERID_Documents / CoTaskMemFree.
    target_link_libraries(wechatvoiceexport_core PRIVATE shell32 ole32 uuid)
endif()

target_compile_features(wechatvoiceexport_core PUBLIC c_std_11)

target_compile_definitions(wechatvoiceexport_core
    PRIVATE
        WECHATVOICEEXPORT_BUILDING_LIBRARY
)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(wechatvoiceexport_core PUBLIC WECHATVOICEEXPORT_SHARED)
endif()
set_target_properties(wechatvoiceexport_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(wechatvoiceexport_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${WECHATVOICEEXPORT_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)
wechatvoiceexport_target_defaults(wechatvoiceexport_core)

if(WECHATVOICEEXPORT_BUILD_CLI)
    # Windows version-info resource
    if(WIN32)
        set(WECHATVOICEEXPORT_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
        set(WECHATVOICEEXPORT_VERSION_MINOR ${PROJECT_VERSION_MINOR})
        set(WECHATVOICEEXPORT_VERSION_PATCH ${PROJECT_VERSION_PATCH})
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/res/wxvoice.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/wxvoice.rc"
            @ONLY
        )
    endif()
    add_executable(wxvoice ${WECHATVOICEEXPORT_CLI_SOURCES})
    target_include_directories(wxvoice PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(WIN32)
        target_sources(wxvoice PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/generated/wxvoice.rc"
        )
    endif()
    target_link_libraries(wxvoice PRIVATE wechatvoiceexport_core)
    wechatvoiceexport_target_defaults(wxvoice)
    set_target_properties(wxvoice PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

if(WECHATVOICEEXPORT_BUILD_TESTING)
    enable_testing()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Tests.cmake")
endif()
