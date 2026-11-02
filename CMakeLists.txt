cmake_minimum_required(VERSION 3.16)

project(silabificador VERSION 2.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_silabificador_top ON)
else()
    set(_silabificador_top OFF)
endif()
option(SILABIFICADOR_BUILD_TESTS "Build the parity and unit tests" ${_silabificador_top})

add_library(silabificador
    src/unicode.cpp
    src/phonotactics.cpp
    src/graphemes.cpp
    src/nucleus.cpp
    src/morphology.cpp
    src/parser.cpp
    src/stress.cpp
    src/syl.cpp
)
add_library(silabificador::silabificador ALIAS silabificador)
target_include_directories(silabificador
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(silabificador PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(silabificador PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS silabificador EXPORT silabificadorTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT silabificadorTargets
    NAMESPACE silabificador::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/silabificador)
configure_package_config_file(cmake/silabificadorConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/silabificadorConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/silabificador)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/silabificadorConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/silabificadorConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/silabificadorConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/silabificador)

if(SILABIFICADOR_BUILD_TESTS)
    enable_testing()
    find_package(ZLIB REQUIRED)

    add_executable(silabificador_parity tests/parity.cpp)
    target_link_libraries(silabificador_parity PRIVATE silabificador ZLIB::ZLIB)
    add_test(NAME parity COMMAND silabificador_parity ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens)

    add_executable(silabificador_modules tests/modules.cpp)
    target_link_libraries(silabificador_modules PRIVATE silabificador ZLIB::ZLIB)
    add_test(NAME modules COMMAND silabificador_modules ${CMAKE_CURRENT_SOURCE_DIR}/tests/goldens)

    add_executable(silabificador_examples tests/examples.cpp)
    target_link_libraries(silabificador_examples PRIVATE silabificador)
    add_test(NAME examples COMMAND silabificador_examples)
endif()
