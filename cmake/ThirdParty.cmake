# Vendored Skype SILK SDK (decoder path only).
#
# The SDK sources carry their own BSD-style license with an explicit patent
# disclaimer; see third_party/silk/LICENSE. They are kept out of the project's
# formatting and static-analysis scan surface and compiled with warnings off.

file(GLOB WECHATVOICEEXPORT_SILK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/silk/src/*.c"
)

add_library(silk_decoder STATIC ${WECHATVOICEEXPORT_SILK_SOURCES})

target_include_directories(silk_decoder
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/silk/interface"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/silk/src"
)

target_compile_features(silk_decoder PRIVATE c_std_11)

if(MSVC)
    target_compile_options(silk_decoder PRIVATE /W0)
    target_compile_definitions(silk_decoder PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(silk_decoder PRIVATE -w)
endif()
