cmake_minimum_required(VERSION 3.18)
project(VeyonInternetControlPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_compile_definitions(QT_NO_VERSION_TAGGING)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Generate veyonconfig.h expected by Veyon core headers
set(VERSION_MAJOR 4)
set(VERSION_MINOR 10)
set(VERSION_PATCH 2)
set(VERSION_STRING "4.10.2")
set(VEYON_PLUGIN_DIR "plugins")
set(VEYON_TRANSLATIONS_DIR "translations")
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/veyon/core/src/veyonconfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/veyonconfig.h
)

# Reference official installed veyon-core.dll for linking
set(VEYON_INSTALL_DIR "C:/Program Files/Veyon")
add_library(veyon-core SHARED IMPORTED GLOBAL)
set_target_properties(veyon-core PROPERTIES
    IMPORTED_LOCATION "${VEYON_INSTALL_DIR}/veyon-core.dll"
    IMPORTED_IMPLIB "${VEYON_INSTALL_DIR}/veyon-core.dll"
)

# Plugin target
add_library(internetcontrol SHARED
    src/InternetControlPlugin.cpp
    src/InternetControlPlugin.h
    src/InternetControlDialog.cpp
    src/InternetControlDialog.h
    src/platform/WindowsFirewallManager.cpp
    src/platform/WindowsFirewallManager.h
    src/platform/WindowsPolicyManager.cpp
    src/platform/WindowsPolicyManager.h
    src/resources/internetcontrol.qrc
)

target_include_directories(internetcontrol PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/veyon/core/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/veyon/core/src/Configuration
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(internetcontrol PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    veyon-core
    iphlpapi
    ws2_32
    userenv
    advapi32
    ole32
    oleaut32
    dnsapi
)

set_target_properties(internetcontrol PROPERTIES
    PREFIX ""
    OUTPUT_NAME "internetcontrol"
)

# Custom target to deploy directly to Veyon plugins directory
add_custom_target(deploy
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:internetcontrol>" "${VEYON_INSTALL_DIR}/plugins/internetcontrol.dll"
    DEPENDS internetcontrol
    COMMENT "Deploying internetcontrol.dll to ${VEYON_INSTALL_DIR}/plugins/"
)
