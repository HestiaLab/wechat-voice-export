# 测试注册辅助函数。
#
# 每个测试源文件生成一个独立可执行文件并注册到 CTest。
# 测试断言使用 Unity（vendored 到 tests/vendor/unity/，MIT）。

# Unity vendored 源码（编译一次，供所有测试 target 复用）。
set(WECHATVOICEEXPORT_UNITY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/vendor/unity/unity.c
)

# 合成 fixture 使用 sin()；Linux 的 libm 需要显式链接，macOS 接受 -lm。
set(WECHATVOICEEXPORT_TEST_MATH_LIB "")
if(UNIX)
    set(WECHATVOICEEXPORT_TEST_MATH_LIB m)
endif()

function(wechatvoiceexport_add_test target)
    cmake_parse_arguments(WECHATVOICEEXPORT_TEST
        ""
        ""
        "SOURCES;LIBS"
        ${ARGN}
    )

    if(NOT WECHATVOICEEXPORT_TEST_SOURCES)
        message(FATAL_ERROR "wechatvoiceexport_add_test(${target}) requires SOURCES")
    endif()

    add_executable(${target}
        ${WECHATVOICEEXPORT_TEST_SOURCES}
        ${WECHATVOICEEXPORT_UNITY_SOURCES}
    )

    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/support
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/vendor/unity
    )

    target_compile_definitions(${target} PRIVATE
        TEST_TMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}/ctest-tmp"
    )

    if(WECHATVOICEEXPORT_TEST_LIBS)
        target_link_libraries(${target} PRIVATE ${WECHATVOICEEXPORT_TEST_LIBS})
    endif()

    wechatvoiceexport_target_defaults(${target})

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${target} PROPERTIES LABELS "unit")
    list(APPEND WECHATVOICEEXPORT_TEST_TARGETS ${target})
    set(WECHATVOICEEXPORT_TEST_TARGETS "${WECHATVOICEEXPORT_TEST_TARGETS}" PARENT_SCOPE)
endfunction()

set(WECHATVOICEEXPORT_TEST_TARGETS "")

# Architecture gate: every public function declaration in include/ must have a
# matching definition in src/. The Python interpreter is REQUIRED for testing:
# without it these gates would silently disappear from the CTest inventory and
# the suite would still report green with fewer tests.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    # Absolute floors keep the gate from passing vacuously if the scan surface
    # collapses because of a parsing or layout change.
    add_test(NAME wechatvoiceexport_decl_def_check
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_decl_def.py
            ${CMAKE_CURRENT_SOURCE_DIR}
            --min-declarations 1
            --min-definitions 1)
    set_tests_properties(wechatvoiceexport_decl_def_check PROPERTIES LABELS "architecture")
else()
    message(FATAL_ERROR
        "Python3 interpreter not found, but WECHATVOICEEXPORT_BUILD_TESTING is ON and "
        "the architecture gates require it. Install Python 3, or reconfigure "
        "with -DWECHATVOICEEXPORT_BUILD_TESTING=OFF to build without the test suite.")
endif()

# Gate self-tests: assert the quality-gate scripts can actually fail. Pure
# stdlib unittest; requires the Python interpreter proven present above.
add_test(NAME wechatvoiceexport_gate_behaviors
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/tests/test_gate_behaviors.py)
set_tests_properties(wechatvoiceexport_gate_behaviors PROPERTIES LABELS "architecture")

wechatvoiceexport_add_test(wechatvoiceexport_scalar_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scalar.c
)

wechatvoiceexport_add_test(wechatvoiceexport_md5_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_md5.c
    LIBS
        wechatvoiceexport_core
)

wechatvoiceexport_add_test(wechatvoiceexport_silk_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_silk.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/silk_fixture.c
    LIBS
        wechatvoiceexport_core
        silk_decoder
        ${WECHATVOICEEXPORT_TEST_MATH_LIB}
)

wechatvoiceexport_add_test(wechatvoiceexport_capture_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_capture.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/silk_fixture.c
    LIBS
        wechatvoiceexport_core
        silk_decoder
        ${WECHATVOICEEXPORT_TEST_MATH_LIB}
)

wechatvoiceexport_add_test(wechatvoiceexport_selection_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_selection.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/silk_fixture.c
    LIBS
        wechatvoiceexport_core
        silk_decoder
        ${WECHATVOICEEXPORT_TEST_MATH_LIB}
)

wechatvoiceexport_add_test(wechatvoiceexport_wav_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wav.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/test_util.c
    LIBS
        wechatvoiceexport_core
)

wechatvoiceexport_add_test(wechatvoiceexport_asset_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_asset.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/test_util.c
    LIBS
        wechatvoiceexport_core
)

wechatvoiceexport_add_test(wechatvoiceexport_pipeline_tests
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipeline.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/silk_fixture.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support/test_util.c
    LIBS
        wechatvoiceexport_core
        silk_decoder
        ${WECHATVOICEEXPORT_TEST_MATH_LIB}
)

if(TARGET wxvoice)
    add_test(NAME wechatvoiceexport_cli_smoke
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cli_smoke.py
            $<TARGET_FILE:wxvoice>)
    set_tests_properties(wechatvoiceexport_cli_smoke PROPERTIES
        LABELS "smoke"
        DEPENDS wxvoice)

    add_test(NAME wechatvoiceexport_version_consistency
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_version_consistency.py
            ${CMAKE_CURRENT_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
            --exe $<TARGET_FILE:wxvoice>)
    set_tests_properties(wechatvoiceexport_version_consistency PROPERTIES
        LABELS "architecture"
        DEPENDS wxvoice)
endif()

add_custom_target(wechatvoiceexport_tests
    DEPENDS
        ${WECHATVOICEEXPORT_TEST_TARGETS}
)
