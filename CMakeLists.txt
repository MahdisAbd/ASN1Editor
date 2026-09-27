cmake_minimum_required(VERSION 3.20)
project(asn1viewer VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    # Produce a portable Windows build without requiring the MSVC runtime
    # redistributable to be installed on the target machine.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASN1VIEWER_BUILD_TESTS "Build the decoder tests" ON)
option(ASN1VIEWER_BUILD_UI "Build the Qt desktop application when Qt6 is available" ON)

add_library(asn1viewer_core
    src/ber.cpp
    src/schema.cpp
)
target_include_directories(asn1viewer_core PUBLIC include)
target_compile_features(asn1viewer_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(asn1viewer_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(asn1viewer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(asn1viewer src/main.cpp)
target_link_libraries(asn1viewer PRIVATE asn1viewer_core)

if(ASN1VIEWER_BUILD_UI)
    if(WIN32)
        add_executable(asn1viewer_gui WIN32 src/win32_main.cpp assets/asn1viewer.rc)
        target_link_libraries(asn1viewer_gui PRIVATE asn1viewer_core comctl32 comdlg32)
    else()
        find_package(Qt6 QUIET COMPONENTS Widgets)
        if(Qt6_FOUND)
            add_executable(asn1viewer_gui src/ui_main.cpp)
            target_link_libraries(asn1viewer_gui PRIVATE asn1viewer_core Qt6::Widgets)
            set_target_properties(asn1viewer_gui PROPERTIES WIN32_EXECUTABLE TRUE)
        else()
            message(STATUS "Qt6 not found; skipping asn1viewer_gui. The CLI remains available.")
        endif()
    endif()
endif()

if(ASN1VIEWER_BUILD_TESTS)
    enable_testing()
    add_executable(asn1viewer_tests tests/test_decoder.cpp)
    target_link_libraries(asn1viewer_tests PRIVATE asn1viewer_core)
    add_test(NAME decoder COMMAND asn1viewer_tests)
endif()

# Portable distribution. The application searches the specs directory beside
# its executable, so the install tree can be copied to any Windows PC.
include(GNUInstallDirs)
install(TARGETS asn1viewer RUNTIME DESTINATION .)
if(TARGET asn1viewer_gui)
    install(TARGETS asn1viewer_gui RUNTIME DESTINATION .)
endif()
file(GLOB ASN1_SPEC_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/specs/*.asn")
install(FILES
    ${ASN1_SPEC_FILES}
    DESTINATION specs
)
install(FILES
    assets/asn1viewer-logo.ico
    PORTABLE_README.txt
    DESTINATION .
)

set(CPACK_PACKAGE_NAME "ASN1Viewer")
set(CPACK_PACKAGE_VENDOR "ASN1Viewer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Portable ASN.1 and telecom CDR viewer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "ASN1Viewer-${PROJECT_VERSION}-win64")
set(CPACK_GENERATOR "ZIP")
include(CPack)
