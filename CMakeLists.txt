cmake_minimum_required(VERSION 3.16)

project(plasma-workspace)
set(PROJECT_VERSION "5.25.2")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.15.2")
set(KF5_MIN_VERSION "5.94")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set(INSTALL_SDDM_THEME TRUE)
option(INSTALL_SDDM_WAYLAND_SESSION OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMMarkNonGuiExecutable)
include(CMakePackageConfigHelpers)
include(WriteBasicConfigVersionFile)
include(CheckIncludeFiles)
include(FeatureSummary)
include(ECMOptionalAddSubdirectory)
include(ECMQtDeclareLoggingCategory)
include(ECMQueryQt)
include(ECMInstallIcons)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMConfiguredInstall)
include(ECMGenerateDBusServiceFile)
include(ECMFindQmlModule)
include(ECMGenerateExportHeader)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Svg Widgets Quick QuickWidgets Concurrent Test Network)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
                    Auth Plasma Runner Notifications NotifyConfig NewStuff Wallet IdleTime
                    Declarative I18n KCMUtils TextWidgets Crash GlobalAccel DBusAddons Wayland
                    CoreAddons People ActivitiesStats Activities KIO Prison PlasmaQuick Package
                    GuiAddons Archive ItemModels IconThemes UnitConversion ItemModels Init TextEditor
                    OPTIONAL_COMPONENTS DocTools)

find_package(KDED CONFIG REQUIRED)

find_package(KF5NetworkManagerQt ${KF5_MIN_VERSION})
set_package_properties(KF5NetworkManagerQt PROPERTIES DESCRIPTION "Qt wrapper for NetworkManager API"
                       TYPE OPTIONAL
                       PURPOSE "Needed by geolocation data engine."
                      )

find_package(KF5Kirigami2 ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5Kirigami2 PROPERTIES
    DESCRIPTION "A QtQuick based components set"
    TYPE RUNTIME
)

find_package(KF5QuickCharts ${KF5_MIN_VERSION} CONFIG)
set_package_properties(KF5QuickCharts PROPERTIES
    DESCRIPTION "Used for rendering charts"
    TYPE RUNTIME
)

find_package(KUserFeedback)
find_package(KSysGuard CONFIG REQUIRED)

find_package(KF5Baloo)
set_package_properties(KF5Baloo PROPERTIES DESCRIPTION "File Searching"
                       TYPE RECOMMENDED
                       PURPOSE "Needed for the File Search runner."
                      )
find_package(Qalculate)
set_package_properties(Qalculate PROPERTIES DESCRIPTION "Qalculate Library"
                       URL "https://qalculate.github.io/"
                       TYPE REQUIRED
                       PURPOSE "Needed for precise computation in the calculator runner."
                      )

find_package(KWinDBusInterface CONFIG REQUIRED)

find_package(KF5Screen CONFIG REQUIRED)
find_package(KScreenLocker 5.13.80 REQUIRED)
find_package(ScreenSaverDBusInterface CONFIG REQUIRED)
find_package(LayerShellQt CONFIG REQUIRED)
find_package(KF5Holidays)
set_package_properties(KF5Holidays PROPERTIES DESCRIPTION "Holidays provider for Plasma calendar"
                       TYPE OPTIONAL
                       PURPOSE "Needed to for holidays plugin for Plasma Calendar."
                      )

find_package(Phonon4Qt${QT_MAJOR_VERSION} 4.6.60 REQUIRED NO_MODULE)
set_package_properties(Phonon4Qt${QT_MAJOR_VERSION} PROPERTIES
   DESCRIPTION "Qt-based audio library"
   TYPE REQUIRED)

find_package(Breeze ${PROJECT_VERSION} CONFIG)
set_package_properties(Breeze PROPERTIES
                       TYPE OPTIONAL
                       PURPOSE "For setting the default window decoration plugin")

find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "https://www.zlib.net"
                       TYPE REQUIRED
                      )

find_package(Fontconfig)
set_package_properties(Fontconfig PROPERTIES DESCRIPTION "Font access configuration library"
                       URL "https://www.freedesktop.org/wiki/Software/fontconfig"
                       TYPE OPTIONAL
                       PURPOSE "Needed to build font configuration and installation tools"
                      )


find_package(X11)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                        URL "https://www.x.org"
                        TYPE OPTIONAL
                        PURPOSE "Required for building the X11 based workspace")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PipeWire IMPORTED_TARGET libpipewire-0.3)
add_feature_info(PipeWire PipeWire_FOUND "Required for Wayland screencasting")

if(PipeWire_FOUND)
  find_package(Libdrm REQUIRED)
endif()

if(QT_MAJOR_VERSION EQUAL "5")
  find_package(QtWaylandScanner REQUIRED)
  find_package(Qt5XkbCommonSupport)
else()
  find_package(Qt6 REQUIRED CONFIG COMPONENTS WaylandCompositor)
endif()
find_package(Qt${QT_MAJOR_VERSION}WaylandClient)
find_package(PlasmaWaylandProtocols 1.6 REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client Server) # Server is used in autotests

if(FONTCONFIG_FOUND)
  # kfontinst
  find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS PrintSupport)
endif()

if(X11_FOUND)
  find_package(XCB MODULE REQUIRED COMPONENTS XCB RANDR IMAGE)
  set_package_properties(XCB PROPERTIES TYPE REQUIRED)
  if(NOT X11_SM_FOUND)
    message(FATAL_ERROR "\nThe X11 Session Management (SM) development package could not be found.\nPlease install libSM.\n")
  endif(NOT X11_SM_FOUND)

  if (QT_MAJOR_VERSION EQUAL "5")
    find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS X11Extras)
  endif()
endif()

if(X11_FOUND AND XCB_XCB_FOUND)
  set(HAVE_X11 1)
endif()

find_package(AppStreamQt 0.10.6)
set_package_properties(AppStreamQt PROPERTIES DESCRIPTION "Access metadata for listing available software"
                       URL "https://www.freedesktop.org/wiki/Distributions/AppStream/"
                       TYPE OPTIONAL)

if(${AppStreamQt_FOUND})
    set(HAVE_APPSTREAMQT true)
endif()

find_package(PackageKitQt${QT_MAJOR_VERSION})
set_package_properties(PackageKitQt${QT_MAJOR_VERSION}
        PROPERTIES DESCRIPTION "Software Manager integration"
        TYPE OPTIONAL
        PURPOSE "Used to install additional language packages on demand"
        )
if(PackageKitQt${QT_MAJOR_VERSION}_FOUND)
    set(HAVE_PACKAGEKIT TRUE)
endif()


find_package(KIOExtras)
set_package_properties(KIOExtras PROPERTIES DESCRIPTION "Common KIO slaves for operations."
                                       PURPOSE "Show thumbnails in wallpaper selection."
                                       TYPE RUNTIME
                                        )

find_package(KIOFuse)
set_package_properties(KIOFuse PROPERTIES DESCRIPTION "Provide KIO support to legacy applications. "
                                      TYPE RUNTIME
                                      )

# Clipboard applet
ecm_find_qmlmodule(org.kde.prison 1.0)

include(ConfigureChecks.cmake)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x055800)
add_definitions(-DKITEMMODELS_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054F00)

configure_file(config-workspace.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-workspace.h)
configure_file(config-unix.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-unix.h )
configure_file(config-X11.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-X11.h)
configure_file(config-appstream.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-appstream.h )

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

# locate qdbus in the Qt path because not every distro makes a symlink at /usr/bin/qdbus
ecm_query_qt(QtBinariesDir QT_INSTALL_BINS)

option(PLASMA_WAYLAND_DEFAULT_SESSION "Use Wayland session by default for Plasma" FALSE)

add_subdirectory(applets)

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
