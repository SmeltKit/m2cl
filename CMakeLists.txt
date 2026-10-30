# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

cmake_minimum_required(VERSION 3.21)
project(mclauncher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(MCLAUNCHER_QT_MAJOR "AUTO" CACHE STRING "Qt major version: AUTO, 5 or 6")
set_property(CACHE MCLAUNCHER_QT_MAJOR PROPERTY STRINGS AUTO 5 6)

if(MCLAUNCHER_QT_MAJOR STREQUAL "AUTO")
    find_package(Qt6 6.5 QUIET COMPONENTS Core Widgets Network Concurrent)
    if(Qt6_FOUND)
        set(QT_MAJOR 6)
    else()
        find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Network Concurrent)
        set(QT_MAJOR 5)
    endif()
elseif(MCLAUNCHER_QT_MAJOR MATCHES "^[56]$")
    set(QT_MAJOR ${MCLAUNCHER_QT_MAJOR})
    find_package(Qt${QT_MAJOR} REQUIRED COMPONENTS Core Widgets Network Concurrent)
else()
    message(FATAL_ERROR "MCLAUNCHER_QT_MAJOR must be AUTO, 5 or 6")
endif()

find_package(ZLIB REQUIRED)
find_package(Qt${QT_MAJOR} REQUIRED COMPONENTS LinguistTools)
set(M2CL_TS_FILES
    translations/m2cl_zh_CN.ts translations/m2cl_zh_TW.ts translations/m2cl_en_US.ts)
if(QT_MAJOR EQUAL 6)
    qt6_add_translation(M2CL_QM_FILES ${M2CL_TS_FILES} OPTIONS -nounfinished -compress)
else()
    qt5_add_translation(M2CL_QM_FILES ${M2CL_TS_FILES} OPTIONS -nounfinished -compress)
endif()
add_custom_target(translations ALL DEPENDS ${M2CL_QM_FILES})
add_custom_target(update_translations
    COMMAND Qt${QT_MAJOR}::lupdate "${CMAKE_CURRENT_SOURCE_DIR}/src" -no-obsolete -locations relative -ts ${M2CL_TS_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Update Qt translation catalogs; review and complete new entries before release")
configure_file(resources/translations.qrc.in "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc" @ONLY)
find_package(Qt${QT_MAJOR} REQUIRED COMPONENTS Svg)
add_executable(app_icon_renderer EXCLUDE_FROM_ALL tools/render_app_icon.cpp)
target_link_libraries(app_icon_renderer PRIVATE Qt${QT_MAJOR}::Core Qt${QT_MAJOR}::Gui Qt${QT_MAJOR}::Svg)
add_library(launcher_core
    src/core/i18n.cpp src/core/i18n.h
    "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc"
    src/core/accounts.cpp src/core/accounts.h
    src/core/compatibility.cpp src/core/compatibility.h
    src/core/packs.cpp src/core/packs.h
    src/core/memory.cpp src/core/memory.h
    src/core/launchoptions.cpp src/core/launchoptions.h
    src/core/gameplan.cpp src/core/gameplan.h
    src/core/gamelog.cpp src/core/gamelog.h
    src/core/javaarchive.cpp src/core/javaarchive.h
    src/core/catalog.cpp src/core/catalog.h
    src/core/java.cpp src/core/java.h
    src/core/settings.cpp src/core/settings.h
    src/core/resources.cpp src/core/resources.h
    src/core/mods.cpp src/core/mods.h
)
add_dependencies(launcher_core translations)
target_include_directories(launcher_core PUBLIC src)
target_link_libraries(launcher_core PUBLIC Qt${QT_MAJOR}::Core ZLIB::ZLIB)

add_library(launcher_network src/network/accountmanager.cpp src/network/accountmanager.h src/network/microsoftauth.cpp src/network/microsoftauth.h src/network/credentialvault.cpp src/network/credentialvault.h src/network/networkerror.cpp src/network/networkerror.h src/network/downloadsource.cpp src/network/downloadsource.h src/network/optifine.cpp src/network/optifine.h src/network/resourceclient.cpp src/network/resourceclient.h
    src/network/modinstaller.cpp src/network/modinstaller.h
    src/network/gameinstaller.cpp src/network/gameinstaller.h
    src/network/forgeinstaller.cpp src/network/forgeinstaller.h
    src/network/javaprovider.cpp src/network/javaprovider.h)
target_link_libraries(launcher_network PUBLIC launcher_core Qt${QT_MAJOR}::Network Qt${QT_MAJOR}::Concurrent)
if(APPLE)
    find_library(M2CL_SECURITY_FRAMEWORK Security REQUIRED)
    target_link_libraries(launcher_network PRIVATE ${M2CL_SECURITY_FRAMEWORK})
elseif(WIN32)
    target_link_libraries(launcher_network PRIVATE Advapi32)
endif()

# Pinned MD4C 0.5.2; upstream sources and MIT notice are kept intact.
add_library(md4c_vendor STATIC third_party/md4c/md4c.c third_party/md4c/md4c-html.c third_party/md4c/entity.c)
target_include_directories(md4c_vendor PUBLIC third_party/md4c)
set_target_properties(md4c_vendor PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
# Public application identifier registered for M2CL; this is not a credential.
set(M2CL_MICROSOFT_CLIENT_ID "796724ab-c62a-48dd-983b-d6fecf88ce48" CACHE STRING "M2CL Microsoft public application ID (no client secret)")
add_library(launcher_ui
    src/ui/projectlegal.cpp src/ui/projectlegal.h
    src/ui/accountspanel.cpp src/ui/accountspanel.h
    src/ui/networknotice.cpp src/ui/networknotice.h
    src/ui/versioncomponentsdialog.cpp src/ui/versioncomponentsdialog.h
    src/ui/launchoptionswidget.cpp src/ui/launchoptionswidget.h
    src/ui/mainwindow.cpp src/ui/mainwindow.h
    src/ui/javamanager.cpp src/ui/javamanager.h
    src/ui/modversionview.cpp src/ui/modversionview.h
    src/ui/modplanview.cpp src/ui/modplanview.h
    src/ui/installplandialog.cpp src/ui/installplandialog.h
    src/ui/projectreadme.cpp src/ui/projectreadme.h
    src/ui/instancemods.cpp src/ui/instancemods.h
    src/ui/instancepacks.cpp src/ui/instancepacks.h
    src/ui/theme.cpp src/ui/theme.h
    src/ui/launcherwidgets.cpp src/ui/launcherwidgets.h src/ui/selectionpopup.cpp
    resources/covers.qrc resources/licenses.qrc
)
target_compile_definitions(launcher_ui PRIVATE M2CL_MICROSOFT_CLIENT_ID="${M2CL_MICROSOFT_CLIENT_ID}")
target_link_libraries(launcher_ui PUBLIC launcher_network Qt${QT_MAJOR}::Widgets Qt${QT_MAJOR}::Concurrent Qt${QT_MAJOR}::Svg PRIVATE md4c_vendor)
if(APPLE)
    enable_language(OBJCXX)
    set_target_properties(launcher_ui PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON)
    target_sources(launcher_ui PRIVATE src/ui/macwindow.mm src/ui/macwindow.h)
    find_library(M2CL_APPKIT_FRAMEWORK AppKit REQUIRED)
    target_link_libraries(launcher_ui PRIVATE ${M2CL_APPKIT_FRAMEWORK})
endif()
add_executable(mclauncher WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(mclauncher PRIVATE launcher_ui)
set_target_properties(mclauncher PROPERTIES
    OUTPUT_NAME "m2cl"
    MACOSX_BUNDLE_BUNDLE_NAME "Mercury MC Launcher"
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.mclauncher.desktop"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
if(APPLE)
    set(M2CL_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/m2cl.icns")
    set_source_files_properties(${M2CL_APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(mclauncher PRIVATE ${M2CL_APP_ICON})
    set_target_properties(mclauncher PROPERTIES MACOSX_BUNDLE_ICON_FILE m2cl.icns)
elseif(WIN32)
    enable_language(RC)
    configure_file(resources/m2cl.rc.in "${CMAKE_CURRENT_BINARY_DIR}/m2cl.rc" @ONLY)
    target_sources(mclauncher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/m2cl.rc")
endif()
if(APPLE)
    set(M2CL_NOTICE_DIR "$<TARGET_BUNDLE_DIR:mclauncher>/Contents/Resources/licenses")
else()
    set(M2CL_NOTICE_DIR "$<TARGET_FILE_DIR:mclauncher>/licenses")
endif()
add_custom_command(TARGET mclauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${M2CL_NOTICE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${M2CL_NOTICE_DIR}/M2CL-GPL-3.0.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ADDITIONAL_TERMS.md" "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE" "${M2CL_NOTICE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c/LICENSE.md" "${M2CL_NOTICE_DIR}/md4c-LICENSE.md")
if(MSVC)
    target_compile_options(launcher_core PRIVATE /utf-8 /W4)
    target_compile_options(launcher_network PRIVATE /utf-8 /W4)
    target_compile_options(launcher_ui PRIVATE /utf-8 /W4)
    target_compile_options(mclauncher PRIVATE /utf-8 /W4)
else()
    target_compile_options(launcher_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(launcher_network PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(launcher_ui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(mclauncher PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(i18n_tests tests/i18n_tests.cpp)
    target_link_libraries(i18n_tests PRIVATE launcher_ui Qt${QT_MAJOR}::Test)
    if(MSVC)
        target_compile_options(i18n_tests PRIVATE /utf-8)
    endif()
    add_test(NAME i18n_tests COMMAND i18n_tests)
    set_tests_properties(i18n_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME translation_catalogs COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_translations.py)
    endif()
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE launcher_core Qt${QT_MAJOR}::Test)
    if(MSVC)
        target_compile_options(core_tests PRIVATE /utf-8)
    endif()
    add_test(NAME core_tests COMMAND core_tests)
    add_executable(auth_tests tests/auth_tests.cpp)
    target_link_libraries(auth_tests PRIVATE launcher_network Qt${QT_MAJOR}::Test)
    add_test(NAME auth_tests COMMAND auth_tests)
    if(MSVC)
        target_compile_options(auth_tests PRIVATE /utf-8)
    endif()
    add_executable(network_tests tests/network_tests.cpp)
    target_link_libraries(network_tests PRIVATE launcher_network Qt${QT_MAJOR}::Test)
    add_test(NAME network_tests COMMAND network_tests)
    add_executable(mods_tests tests/mods_tests.cpp)
    target_link_libraries(mods_tests PRIVATE launcher_network Qt${QT_MAJOR}::Test)
    add_test(NAME mods_tests COMMAND mods_tests)
    add_executable(mod_install_probe EXCLUDE_FROM_ALL tests/mod_install_probe.cpp)
    target_link_libraries(mod_install_probe PRIVATE launcher_network)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE launcher_ui Qt${QT_MAJOR}::Test)
    add_test(NAME ui_tests COMMAND ui_tests)
    set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(readme_tests tests/readme_tests.cpp)
    target_link_libraries(readme_tests PRIVATE launcher_ui Qt${QT_MAJOR}::Test)
    add_test(NAME readme_tests COMMAND readme_tests)
    set_tests_properties(readme_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(java_ui_probe EXCLUDE_FROM_ALL tests/java_ui_probe.cpp)
    target_link_libraries(java_ui_probe PRIVATE launcher_ui)
    add_executable(java_probe EXCLUDE_FROM_ALL tests/java_probe.cpp)
    target_link_libraries(java_probe PRIVATE launcher_network)
    add_executable(java_tests tests/java_tests.cpp)
    target_link_libraries(java_tests PRIVATE launcher_network Qt${QT_MAJOR}::Test)
    add_test(NAME java_tests COMMAND java_tests)
    add_executable(game_probe EXCLUDE_FROM_ALL tests/game_probe.cpp)
    target_link_libraries(game_probe PRIVATE launcher_network)
    add_executable(game_tests tests/game_tests.cpp)
    target_link_libraries(game_tests PRIVATE launcher_network Qt${QT_MAJOR}::Test)
    add_test(NAME game_tests COMMAND game_tests)
    add_executable(mod_ui_probe EXCLUDE_FROM_ALL tests/mod_ui_probe.cpp)
    target_link_libraries(mod_ui_probe PRIVATE launcher_ui)
    add_executable(installed_mods_probe EXCLUDE_FROM_ALL tests/installed_mods_probe.cpp)
    target_link_libraries(installed_mods_probe PRIVATE launcher_ui)
    add_executable(mod_plan_probe EXCLUDE_FROM_ALL tests/mod_plan_probe.cpp)
    target_link_libraries(mod_plan_probe PRIVATE launcher_ui)
    add_executable(modrinth_probe EXCLUDE_FROM_ALL tests/modrinth_probe.cpp)
    target_link_libraries(modrinth_probe PRIVATE launcher_network)
    if(MSVC)
        target_compile_options(network_tests PRIVATE /utf-8)
        target_compile_options(mods_tests PRIVATE /utf-8)
        target_compile_options(mod_install_probe PRIVATE /utf-8)
        target_compile_options(ui_tests PRIVATE /utf-8)
        target_compile_options(java_tests PRIVATE /utf-8)
        target_compile_options(java_probe PRIVATE /utf-8)
        target_compile_options(java_ui_probe PRIVATE /utf-8)
        target_compile_options(mod_plan_probe PRIVATE /utf-8)
        target_compile_options(installed_mods_probe PRIVATE /utf-8)
        target_compile_options(game_tests PRIVATE /utf-8)
        target_compile_options(game_probe PRIVATE /utf-8)
    endif()
endif()
install(TARGETS mclauncher BUNDLE DESTINATION . RUNTIME DESTINATION bin)
if(NOT APPLE)
    include(GNUInstallDirs)
    if(WIN32)
        set(M2CL_INSTALL_NOTICE_DIR bin/licenses)
    else()
        set(M2CL_INSTALL_NOTICE_DIR ${CMAKE_INSTALL_DATADIR}/licenses/m2cl)
    endif()
    install(FILES LICENSE DESTINATION ${M2CL_INSTALL_NOTICE_DIR} RENAME M2CL-GPL-3.0.txt)
    install(FILES ADDITIONAL_TERMS.md NOTICE DESTINATION ${M2CL_INSTALL_NOTICE_DIR})
    install(FILES third_party/md4c/LICENSE.md DESTINATION ${M2CL_INSTALL_NOTICE_DIR} RENAME md4c-LICENSE.md)
endif()
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(FILES resources/app.mclauncher.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/branding/m2cl.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    foreach(icon_size IN ITEMS 16 32 48 64 128 256 512)
        install(FILES assets/branding/m2cl-${icon_size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_size}x${icon_size}/apps RENAME m2cl.png)
    endforeach()
endif()

add_executable(resource_probe EXCLUDE_FROM_ALL tests/resource_probe.cpp)
target_link_libraries(resource_probe PRIVATE launcher_network)
