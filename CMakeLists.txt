cmake_minimum_required(VERSION 3.15)

set(QT_MIN_VERSION "6.7.0")

project(POKERTH)

# Windows: enable the RC language so that pokerth.rc.in (app icon pokerth.ico and
# version info, see cmake/PokerthWindowsResource.cmake) can be embedded into the
# client .exe. project() only enables C/CXX; without this
# CMake would not compile the .rc. CMAKE_RC_COMPILER (windres) comes via -D from
# build_windows.sh in the MinGW cross build.
if(WIN32)
    enable_language(RC)
endif()

# Disable vcpkg applocal deployment for cross-compilation
if(CMAKE_CROSSCOMPILING)
    set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "" FORCE)
    set(X_VCPKG_APPLOCAL_DEPS_INSTALL OFF CACHE BOOL "" FORCE)
endif()

option(CMAKE_AUTORCC "" ON)
option(CMAKE_AUTOUIC "" ON)
option(CMAKE_AUTOMOC "" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(WIN32)
    include(PokerthWindowsResource)
endif()

# macOS-only workaround for Qt 6.9 setStyleSheet crashes on Monterey
# Using __APPLE__ preprocessor define in source code instead
# if(APPLE)
#     add_definitions(-DDISABLE_COMPLEX_STYLESHEETS_MACOS)
# endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# The ChangeLog is shown in the about dialog of the client and read from
# <AppDataDir>misc/ for that - like agpl.html and third_party_libs.txt. Only the
# file in the project root directory is still maintained; it is mirrored here at
# configure time into data/misc/ (COPYONLY registers it as a configure
# dependency, so a change triggers a re-configure).
# That way it automatically ends up in all packages, because they copy or
# install data/ completely - and on Android in the resource bundle of the
# QML client, which globs data/misc/* anyway.
# data/misc/ChangeLog is therefore generated and not checked in (.gitignore).
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ChangeLog"
               "${CMAKE_CURRENT_SOURCE_DIR}/data/misc/ChangeLog" COPYONLY)

find_package(Boost REQUIRED COMPONENTS iostreams random thread filesystem program_options)
find_package(OpenSSL REQUIRED)

if(ANDROID)
    # 16 KB memory pages (a Play requirement since Android 15, otherwise the app
    # does not start on such devices): every shipped .so has to align its
    # LOAD segments to 16 KB. NDK r28 already links that way, older NDKs do
    # not - set the flags explicitly so that the result does not depend on the
    # NDK version of the build runner. With r28 they are a no-op.
    # The result is checked on the finished package (build_android_qml.sh,
    # "16 KB page alignment").
    add_link_options(-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)
endif()

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Sql Xml Qml Quick QuickControls2 Widgets Svg LinguistTools Multimedia REQUIRED)

# POKERTH.lib

set(POKERTH_LIB_SRC 
    src/engine/game.cpp 
    src/playerdata.cpp 
    src/config/configfile.cpp 
    src/state/statestore.cpp
    src/state/wallet.cpp
    src/state/tournament.cpp
    src/state/roster.cpp
    src/engine/silentgui.cpp
    src/engine/tournamentrun.cpp
    src/engine/boardinterface.cpp 
    src/engine/enginefactory.cpp 
    src/engine/handinterface.cpp 
    src/engine/playerinterface.cpp 
    src/engine/berointerface.cpp 
    src/gui/guiinterface.cpp 
    src/core/thread.cpp 
    src/core/crypthelper.cpp 
    src/core/avatarmanager.cpp 
    src/core/pokerthexception.cpp 
    src/engine/local_engine/cardsvalue.cpp 
    src/engine/local_engine/handevaluator.cpp 
    src/engine/local_engine/holecardsrange.cpp 
    src/engine/local_engine/equitycalculator.cpp 
    src/engine/local_engine/preflopstrength.cpp 
    src/engine/local_engine/aiengine4.cpp 
    src/engine/local_engine/botprofile.cpp 
    src/engine/local_engine/localboard.cpp 
    src/engine/local_engine/localenginefactory.cpp 
    src/engine/local_engine/localhand.cpp 
    src/engine/local_engine/localplayer.cpp 
    src/engine/local_engine/localberopreflop.cpp 
    src/engine/local_engine/localberoflop.cpp 
    src/engine/local_engine/localberoturn.cpp 
    src/engine/local_engine/localberoriver.cpp 
    src/engine/local_engine/localberopostriver.cpp 
    src/engine/local_engine/localbero.cpp 
    src/engine/local_engine/localexception.cpp 
    src/engine/local_engine/arraydata.cpp 
    src/engine/log.cpp 
    src/gui/generic/serverguiwrapper.cpp 
    src/gui/qttoolsinterface.cpp)

list(APPEND POKERTH_LIB_SRC "src/engine/local_engine/tools.cpp")

add_library(pokerth_lib STATIC ${POKERTH_LIB_SRC})

target_include_directories(pokerth_lib PRIVATE "${Boost_INCLUDE_DIRS}")
target_include_directories(pokerth_lib PRIVATE "${OPENSSL_INCLUDE_DIR}")
target_include_directories(pokerth_lib PUBLIC
    src
    src/engine src/engine/local_engine 
    src/gui 
    src/config
    src/core
    src/state)

target_link_libraries(pokerth_lib PUBLIC Boost::iostreams Boost::random Boost::thread Boost::filesystem Boost::program_options)
if(TARGET OpenSSL::SSL)
    target_link_libraries(pokerth_lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)
else()
    target_link_libraries(pokerth_lib PUBLIC "${OPENSSL_LIBRARIES}")
endif()
target_link_libraries(pokerth_lib PRIVATE Qt6::Xml Qt6::Sql)
target_compile_options(pokerth_lib PUBLIC -fexceptions -frtti
    $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>)
target_compile_definitions(pokerth_lib PUBLIC HAVE_OPENSSL BOOST_FILESYSTEM_DEPRECATED)


add_subdirectory(src)
add_subdirectory(src/gui/qt6-qml)
# Headless simulator: console only and EXCLUDE_FROM_ALL, so it is never part of
# a release build - `--target pokerth_sim` builds it explicitly.
add_subdirectory(src/sim)

# zlib_compress utility tool for compressing serverlist
add_executable(zlib_compress src/zlib_compress.cpp)
target_link_libraries(zlib_compress 
    PRIVATE 
    Boost::filesystem 
    Boost::iostreams
)
