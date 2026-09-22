cmake_minimum_required(VERSION 3.20)
project(vita_self_updater_core LANGUAGES CXX)

include(CTest)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_library(vita_updater_core STATIC src/vita_updater_core.cpp)
target_compile_features(vita_updater_core PUBLIC cxx_std_20)
target_include_directories(vita_updater_core PUBLIC include)

if(BUILD_TESTING)
    add_executable(updater_core_test tests/updater_core_test.cpp)
    target_link_libraries(updater_core_test PRIVATE vita_updater_core)
    add_test(NAME updater_core COMMAND updater_core_test)

    add_library(vita_updater_core_test SHARED src/vita_updater_core.cpp)
    target_compile_features(vita_updater_core_test PUBLIC cxx_std_20)
    target_include_directories(vita_updater_core_test PUBLIC include)
    add_test(
        NAME updater_crypto
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/updater_crypto_test.py
            $<TARGET_FILE:vita_updater_core_test>
    )
    add_test(
        NAME updater_package_head
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/updater_head_test.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/make-head-bin.py
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/head.bin
    )
    add_test(
        NAME updater_signing_tools
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/signing_tools_test.py
    )
endif()
