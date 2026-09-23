cmake_minimum_required(VERSION 3.20)

project(ecl-client VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ECL_BUILD_CLI "Build the ecl-post command-line tool" ON)
option(ECL_BUILD_PYTHON "Build the Python extension" OFF)
option(ECL_BUILD_TESTS "Build tests" ON)

find_package(CURL 7.62 REQUIRED)
find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS Crypto)

add_library(ecl src/ecl.cpp)
add_library(ECL::ecl ALIAS ecl)
target_compile_features(ecl PUBLIC cxx_std_17)
target_compile_definitions(ecl PRIVATE ECL_BUILDING_LIBRARY)
target_include_directories(ecl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(ecl PRIVATE CURL::libcurl OpenSSL::Crypto)
set_target_properties(ecl PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    POSITION_INDEPENDENT_CODE YES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(ECL_BUILD_CLI)
    add_executable(ecl-post src/cli.cpp)
    target_compile_features(ecl-post PRIVATE cxx_std_17)
    target_link_libraries(ecl-post PRIVATE ECL::ecl)
endif()

if(ECL_BUILD_PYTHON)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(_native MODULE python/bindings.cpp)
    target_compile_features(_native PRIVATE cxx_std_17)
    target_link_libraries(_native PRIVATE ECL::ecl)
    install(TARGETS _native LIBRARY DESTINATION ecl_client)
endif()

if(ECL_BUILD_TESTS)
    enable_testing()
    add_executable(ecl-unit-tests tests/test_ecl.cpp)
    target_compile_features(ecl-unit-tests PRIVATE cxx_std_17)
    target_link_libraries(ecl-unit-tests PRIVATE ECL::ecl)
    add_test(NAME ecl-unit-tests COMMAND ecl-unit-tests)

    add_executable(ecl-c-api-tests tests/test_c_api.c)
    target_link_libraries(ecl-c-api-tests PRIVATE ECL::ecl)
    add_test(NAME ecl-c-api-tests COMMAND ecl-c-api-tests)

    if(ECL_BUILD_CLI)
        find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)
        add_test(
            NAME ecl-mock-server-tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mock_server.py
                    $<TARGET_FILE:ecl-post>
        )
        add_test(
            NAME ecl-track-tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ecl_track.py
                    $<TARGET_FILE:ecl-post>
        )
    endif()

    if(ECL_BUILD_PYTHON)
        if(NOT Python3_Interpreter_FOUND)
            find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)
        endif()
        add_test(
            NAME ecl-python-binding-tests
            COMMAND ${CMAKE_COMMAND} -E env
                    "PYTHONPATH=$<TARGET_FILE_DIR:_native>"
                    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_python.py
        )
    endif()
endif()

install(TARGETS ecl EXPORT eclTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(ECL_BUILD_CLI)
    install(TARGETS ecl-post RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES man/ecl.3 man/ecl_client.3 DESTINATION ${CMAKE_INSTALL_MANDIR}/man3)
if(ECL_BUILD_CLI)
    install(PROGRAMS scripts/ecl-track DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES man/ecl-post.1 man/ecl-track.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ecl-client-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    cmake/ecl-client-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ecl-client-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecl-client
)
install(EXPORT eclTargets
    FILE ecl-client-targets.cmake
    NAMESPACE ECL::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecl-client
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ecl-client-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ecl-client-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ecl-client
)
