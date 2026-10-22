set(WECHATVOICEEXPORT_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/fs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/asset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/silk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/wav.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/capture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/selection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wechatvoiceexport/pipeline.c
)

set(WECHATVOICEEXPORT_CLI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
)
