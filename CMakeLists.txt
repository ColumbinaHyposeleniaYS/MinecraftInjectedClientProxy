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

# Version shown in the UI ("关于" page / sidebar). Release builds override it
# with the git tag version via -DMICP_APP_VERSION=x.y.z; defaults to the
# project() version above.
if(NOT MICP_APP_VERSION)
    set(MICP_APP_VERSION "${PROJECT_VERSION}")
endif()

# Chinese UI strings are embedded in UTF-8 sources.
if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive-)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(MICP_SOURCES
    src/main.cpp

    # --- application shell -------------------------------------------------
    src/app/Theme.h
    src/app/Theme.cpp
    src/app/Icons.h
    src/app/Icons.cpp
    src/app/Buttons.h
    src/app/Buttons.cpp
    src/app/LineEdit.h
    src/app/LineEdit.cpp
    src/app/ToggleSwitch.h
    src/app/ToggleSwitch.cpp
    src/app/SegmentedControl.h
    src/app/SegmentedControl.cpp
    src/app/Card.h
    src/app/Card.cpp
    src/app/EmptyState.h
    src/app/EmptyState.cpp
    src/app/Stepper.h
    src/app/Stepper.cpp
    src/app/Toast.h
    src/app/Toast.cpp
    src/app/TitleBar.h
    src/app/TitleBar.cpp
    src/app/Sidebar.h
    src/app/Sidebar.cpp
    src/app/Page.h
    src/app/Page.cpp
    src/app/Widgets.h
    src/app/Widgets.cpp
    src/app/ProgressBar.h
    src/app/ProgressBar.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp

    # --- pages -------------------------------------------------------------
    src/pages/OverviewPage.h
    src/pages/OverviewPage.cpp
    src/pages/InjectPage.h
    src/pages/InjectPage.cpp
    src/pages/LogsPage.h
    src/pages/LogsPage.cpp
    src/pages/SettingsPage.h
    src/pages/SettingsPage.cpp
    src/pages/AboutPage.h
    src/pages/AboutPage.cpp

    # --- core services -----------------------------------------------------
    src/core/AppPaths.h
    src/core/AppPaths.cpp
    src/core/Settings.h
    src/core/Settings.cpp
    src/core/ProcessFinder.h
    src/core/ProcessFinder.cpp
    src/core/PortProbe.h
    src/core/PortProbe.cpp
    src/core/Injector.h
    src/core/Injector.cpp
    src/core/InjectionSequence.h
    src/core/InjectionSequence.cpp
    src/core/ProxyMonitor.h
    src/core/ProxyMonitor.cpp
    src/core/LogTail.h
    src/core/LogTail.cpp

    # --- injected-proxy engine (portable C core) ---------------------------
    src/core/injectcore/ReflectiveDLLInjection.h
    src/core/injectcore/RuntimeControl.h
    src/core/injectcore/GetProcAddressR.h
    src/core/injectcore/GetProcAddressR.c
    src/core/injectcore/LoadLibraryR.h
    src/core/injectcore/LoadLibraryR.c
    src/core/injectcore/Inject.h
    src/core/injectcore/Inject.c
)

include(CheckLanguage)
check_language(RC)
if(CHECK_LANGUAGE_RC)
    enable_language(RC)
    list(APPEND MICP_SOURCES resources/appicon.rc)
endif()

qt_add_executable(MinecraftInjectedClientProxy WIN32 ${MICP_SOURCES})

target_include_directories(MinecraftInjectedClientProxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(MinecraftInjectedClientProxy PRIVATE
    MICP_APP_VERSION="${MICP_APP_VERSION}"
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    WIN_X64)

target_link_libraries(MinecraftInjectedClientProxy PRIVATE
    Qt6::Widgets
    dwmapi
    user32
    gdi32
    advapi32
    iphlpapi
    ws2_32
    shell32
    ntdll)

install(TARGETS MinecraftInjectedClientProxy RUNTIME DESTINATION .)
