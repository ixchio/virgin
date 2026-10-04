cmake_minimum_required(VERSION 3.22)

project(
    Virgin
    VERSION 0.2.0
    DESCRIPTION "Virgin — Privacy-First Native Browser (Chromium renders. Virgin controls trust.)"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Options
option(VIRGIN_ENABLE_HARDENING "Enable security hardening flags" ON)
option(VIRGIN_ENABLE_SANITIZERS "Enable sanitizers in Debug" OFF)
option(VIRGIN_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/Version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app/Version.hpp
    @ONLY
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Hardening)
include(Sanitizers)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Widgets
    WebEngineWidgets
    WebEngineCore
    Sql
    Network
)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

# ---- Sources ----
set(VIRGIN_SOURCES
    src/app/main.cpp
    src/app/VirginApp.cpp
    src/app/Paths.cpp

    src/browser/BrowserWindow.cpp
    src/browser/BrowserTab.cpp
    src/browser/TabManager.cpp
    src/browser/VirginPage.cpp
    src/browser/NavigationController.cpp
    src/browser/VirginSchemeHandler.cpp

    src/profiles/ProfileManager.cpp
    src/profiles/VirginProfile.cpp
    src/profiles/ContainerManager.cpp

    src/network/RequestInterceptor.cpp
    src/network/RequestClassifier.cpp
    src/network/HttpsFirstPolicy.cpp

    src/adblock/AdBlockEngine.cpp
    src/adblock/FilterParser.cpp
    src/adblock/FilterCompiler.cpp
    src/adblock/RuleIndex.cpp
    src/adblock/DomainTrie.cpp
    src/adblock/AhoCorasick.cpp
    src/adblock/CosmeticEngine.cpp
    src/adblock/FilterUpdater.cpp

    src/privacy/PermissionManager.cpp
    src/privacy/CookiePolicy.cpp
    src/privacy/CookieFilter.cpp

    src/downloads/DownloadManager.cpp
    src/downloads/DownloadShelf.cpp

    src/storage/Database.cpp
    src/storage/HistoryStore.cpp
    src/storage/BookmarkStore.cpp
    src/storage/SessionStore.cpp
    src/storage/SettingsStore.cpp

    src/security/CertificatePolicy.cpp
    src/security/UrlSafety.cpp
    src/security/RuntimeSecurityCheck.cpp

    src/ui/Omnibox.cpp
    src/ui/PixelIcons.cpp
    src/ui/PixelTheme.cpp
    src/ui/PrivacyPanel.cpp
    src/ui/PermissionDialog.cpp
    src/ui/CertificateErrorDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/HistoryDialog.cpp
    src/ui/BookmarkDialog.cpp
)

set(VIRGIN_HEADERS
    src/app/VirginApp.hpp
    src/app/Paths.hpp
    src/browser/BrowserWindow.hpp
    src/browser/BrowserTab.hpp
    src/browser/TabManager.hpp
    src/browser/VirginPage.hpp
    src/browser/NavigationController.hpp
    src/browser/VirginSchemeHandler.hpp
    src/profiles/ProfileManager.hpp
    src/profiles/VirginProfile.hpp
    src/profiles/ContainerManager.hpp
    src/network/RequestInterceptor.hpp
    src/network/RequestClassifier.hpp
    src/network/HttpsFirstPolicy.hpp
    src/adblock/AdBlockEngine.hpp
    src/adblock/FilterParser.hpp
    src/adblock/FilterCompiler.hpp
    src/adblock/RuleIndex.hpp
    src/adblock/DomainTrie.hpp
    src/adblock/AhoCorasick.hpp
    src/adblock/CosmeticEngine.hpp
    src/adblock/FilterUpdater.hpp
    src/privacy/PermissionManager.hpp
    src/privacy/PermissionTypes.hpp
    src/privacy/CookiePolicy.hpp
    src/privacy/CookieFilter.hpp
    src/downloads/DownloadManager.hpp
    src/downloads/DownloadShelf.hpp
    src/storage/Database.hpp
    src/storage/HistoryStore.hpp
    src/storage/BookmarkStore.hpp
    src/storage/SessionStore.hpp
    src/storage/SettingsStore.hpp
    src/security/CertificatePolicy.hpp
    src/security/UrlSafety.hpp
    src/security/RuntimeSecurityCheck.hpp
    src/ui/Omnibox.hpp
    src/ui/PixelIcons.hpp
    src/ui/PixelTheme.hpp
    src/ui/PrivacyPanel.hpp
    src/ui/PermissionDialog.hpp
    src/ui/CertificateErrorDialog.hpp
    src/ui/SettingsDialog.hpp
    src/ui/HistoryDialog.hpp
    src/ui/BookmarkDialog.hpp
)

qt_add_executable(virgin
    ${VIRGIN_SOURCES}
    ${VIRGIN_HEADERS}
    resources.qrc
)

target_include_directories(virgin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_definitions(virgin PRIVATE
    VIRGIN_SOURCE_FILTER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/filters/builtin"
    VIRGIN_INSTALLED_FILTER_DIR="${CMAKE_INSTALL_FULL_DATADIR}/virgin/filters"
)

target_link_libraries(virgin PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Sql
    Qt6::Network
)

virgin_set_warnings(virgin)
virgin_enable_hardening(virgin)

if(VIRGIN_ENABLE_SANITIZERS)
    virgin_enable_sanitizers(virgin)
endif()

# ---- Install ----
install(TARGETS virgin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES packaging/linux/virgin.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/linux/virgin.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES filters/builtin/easylist.txt filters/builtin/easyprivacy.txt
              filters/builtin/virgin-protection.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/virgin/filters)
install(FILES README.md LICENSE THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})

# ---- Tests ----
enable_testing()
add_subdirectory(tests)

message(STATUS "Virgin Browser v${PROJECT_VERSION}")
message(STATUS "  Hardening: ${VIRGIN_ENABLE_HARDENING}")
message(STATUS "  Sanitizers: ${VIRGIN_ENABLE_SANITIZERS}")
