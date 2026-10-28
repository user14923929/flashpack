cmake_minimum_required(VERSION 3.21)
project(flashpack VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_executable(flashpack WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/core/Device.h
    src/core/Device.cpp
    src/core/DeviceInfo.h
    src/core/DeviceInfo.cpp
    src/core/DeviceManager.h
    src/core/DeviceManager.cpp
    src/backend/IBackend.h
    src/backend/CliBackend.h
    src/backend/CliBackend.cpp
    src/backend/Parsers.h
    src/backend/Parsers.cpp
    src/backend/ToolLocator.h
    src/backend/ToolLocator.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/PartitionDelegate.h
    src/ui/WideComboBox.h
)

target_include_directories(flashpack PRIVATE src)
target_link_libraries(flashpack PRIVATE Qt6::Widgets)
target_compile_definitions(flashpack PRIVATE FLASHPACK_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(flashpack PRIVATE /W4 /utf-8)
else()
    target_compile_options(flashpack PRIVATE -Wall -Wextra)
endif()

set_target_properties(flashpack PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "FlashPack"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.user14923929.flashpack"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

include(GNUInstallDirs)
install(TARGETS flashpack
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES data/org.user14923929.flashpack.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES data/org.user14923929.flashpack.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
