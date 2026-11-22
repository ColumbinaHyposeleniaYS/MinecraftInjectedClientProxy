cmake_minimum_required(VERSION 3.21)

project(MinecraftInjectedClientProxy
    VERSION 1.0.5
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

# Static CRT (/MT) for every MSVC target:
#   - the proxy DLL is injected into a JVM process that carries no VC runtime;
#   - the GUI then ships as a fully self-contained exe (no VC++ Redist needed).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
    src/app/Lang.cpp
    src/app/Paint.cpp
    src/app/Theme.cpp
    src/app/Ui.cpp
    src/core/AppPaths.cpp
    src/core/EmbeddedDll.cpp
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
    iphlpapi ws2_32 ole32 msimg32) # msimg32: AlphaBlend for page transitions

set_target_properties(micp PROPERTIES
    OUTPUT_NAME "MinecraftProxy"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    # The manifest is embedded from resources/appicon.rc; disable the linker's
    # auto-generated one (CVT1100: duplicate resource MANIFEST).
    target_link_options(micp PRIVATE /MANIFEST:NO)
endif()

# ---------------------------------------------------------------------------
# Aurora.dll - the resident proxy core injected into client A.
#
# Ported from the upstream NativeBox project (its native/ tree lives here as
# proxy/). proxy/sources.txt is the single source list for the production DLL;
# new files must be registered there (editing it re-runs CMake automatically).
#
# MSVC x64 only: the reflective loader and the JVM bridge are MSVC code paths.
# MinGW builds compile the GUI alone and skip the DLL with a notice below.
# ---------------------------------------------------------------------------
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS proxy/sources.txt)

if(MSVC)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/proxy/sources.txt" AURORA_SOURCES)

    add_library(aurora SHARED ${AURORA_SOURCES})
    target_include_directories(aurora PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/proxy
        ${CMAKE_CURRENT_SOURCE_DIR}/proxy/include
        # RuntimeControl.h / LoadLibraryR.h: the resume-event naming contract
        # and the reflective loader are shared with the GUI injector core.
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/injectcore)
    target_compile_definitions(aurora PRIVATE
        REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN WIN_X64 NOMINMAX _CRT_SECURE_NO_WARNINGS)
    # Build the DLL exactly like upstream NativeBox: permissive (not
    # /permissive-) and EHsc; /O2 mirrors its Release settings.
    target_compile_options(aurora PRIVATE
        /permissive
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
        $<$<CONFIG:Release>:/O2>)
    target_link_libraries(aurora PRIVATE psapi ws2_32 kernel32 user32 advapi32)
    set_target_properties(aurora PROPERTIES
        OUTPUT_NAME Aurora
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

    # Embed Aurora.dll into the GUI exe as RCDATA (MICP_HAS_EMBEDDED_DLL):
    # the injector extracts it to %TEMP% when no external DLL is found.
    # The copy must run before micp's resources are compiled, hence the
    # explicit target-level dependency.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/aurora_embed.rc"
        "AURORA_DLL RCDATA \"Aurora.dll\"\n")
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Aurora.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:aurora>" "${CMAKE_CURRENT_BINARY_DIR}/Aurora.dll"
        DEPENDS aurora
        COMMENT "Copying Aurora.dll for RCDATA embedding")
    add_custom_target(copy_aurora_dll
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Aurora.dll")
    add_dependencies(micp copy_aurora_dll)
    target_sources(micp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/aurora_embed.rc")
    target_compile_definitions(micp PRIVATE MICP_HAS_EMBEDDED_DLL)
else()
    message(STATUS "Aurora.dll (proxy core) needs the MSVC toolchain - building the GUI only.")
endif()

# Convenience target: run the freshly built exe.
if(MSVC)
    add_custom_target(run
        COMMAND "${CMAKE_BINARY_DIR}/bin/Release/$<TARGET_FILE_NAME:micp>"
        DEPENDS micp
        COMMENT "Launching MinecraftProxy")
else()
    add_custom_target(run
        COMMAND "${CMAKE_BINARY_DIR}/bin/$<TARGET_FILE_NAME:micp>"
        DEPENDS micp
        COMMENT "Launching MinecraftProxy")
endif()
