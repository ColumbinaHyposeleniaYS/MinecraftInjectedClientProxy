cmake_minimum_required(VERSION 3.21)

project(MinecraftInjectedClientProxy
    VERSION 1.0.0
    DESCRIPTION "Minecraft 1.20.1 injected client proxy - desktop GUI"
    LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "MinecraftInjectedClientProxy is a Windows-only (x64) application.")
endif()

# The reflective loader and the injected engine are both 64-bit.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build with an x64 toolchain (MSVC x64 recommended).")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Version shown in the UI ("关于" page / sidebar footer). Release builds
# override it with the git tag version via -DMICP_APP_VERSION=x.y.z.
if(NOT MICP_APP_VERSION)
    set(MICP_APP_VERSION "${PROJECT_VERSION}")
endif()

# Chinese UI strings are embedded in UTF-8 sources.
if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive-)
endif()

add_executable(micp WIN32
    src/main.cpp
    src/app/App.cpp
    src/app/Paint.cpp
    src/app/Theme.cpp
    src/app/Ui.cpp
    src/core/AppPaths.cpp
    src/core/InjectionSequence.cpp
    src/core/Injector.cpp
    src/core/LogTail.cpp
    src/core/PortProbe.cpp
    src/core/ProcessFinder.cpp
    src/core/ProxyMonitor.cpp
    src/core/Settings.cpp
    src/core/injectcore/GetProcAddressR.c
    src/core/injectcore/Inject.c
    src/core/injectcore/LoadLibraryR.c
    resources/appicon.rc)

target_include_directories(micp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(micp PRIVATE
    MICP_APP_VERSION="${MICP_APP_VERSION}"
    UNICODE _UNICODE
    # iphlpapi / winsock helpers
    _WINSOCK_DEPRECATED_NO_WARNINGS)

target_link_libraries(micp PRIVATE
    user32 gdi32 gdiplus dwmapi shell32 comdlg32 advapi32
    iphlpapi ws2_32 ole32)

set_target_properties(micp PROPERTIES
    OUTPUT_NAME "MinecraftInjectedClientProxy"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    # The manifest is embedded from resources/appicon.rc; disable the linker's
    # auto-generated one (CVT1100: duplicate resource MANIFEST).
    target_link_options(micp PRIVATE /MANIFEST:NO)
endif()

# Convenience target: run the freshly built exe.
if(MSVC)
    add_custom_target(run
        COMMAND "${CMAKE_BINARY_DIR}/bin/Release/$<TARGET_FILE_NAME:micp>"
        DEPENDS micp
        COMMENT "Launching MinecraftInjectedClientProxy")
else()
    add_custom_target(run
        COMMAND "${CMAKE_BINARY_DIR}/bin/$<TARGET_FILE_NAME:micp>"
        DEPENDS micp
        COMMENT "Launching MinecraftInjectedClientProxy")
endif()
