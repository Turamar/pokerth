# POKERTH.sim - headless bot/game simulator.
#
# Console only, no window, no network: it links the same engine library the
# clients use and replaces the GUI with SilentGui, which drives the game loop
# instead of showing it (see engine/silentgui.h).
#
# EXCLUDE_FROM_ALL: the release builds must not compile it. Build it explicitly
# with `cmake --build build --target pokerth_sim`.

add_executable(pokerth_sim EXCLUDE_FROM_ALL
    pokerth_sim.cpp
    simdriver.cpp
    # pokerth_lib ships neither a log implementation nor a Qt tools wrapper:
    # every executable picks its own flavour (see the note in the root
    # CMakeLists.txt). The simulator is client side, so it takes the client
    # flavour - loghelper_init() is never called, the messages go to stderr.
    ${CMAKE_SOURCE_DIR}/src/core/loghelper_client.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qttoolsinterface.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qttools/qttoolswrapper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/qttools/qthelper/qthelper.cpp
)

target_include_directories(pokerth_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/config
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/qttools
    ${CMAKE_SOURCE_DIR}/src/core/qttools/qthelper
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/src/engine/local_engine
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/net
)

target_link_libraries(pokerth_sim PRIVATE
    pokerth_lib
    Qt6::Core
    Qt6::Network
    Qt6::Xml
    Qt6::Sql
)
