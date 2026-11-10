import QtQuick

// The real SVG wrapper for the QML DEV MODE only (POKERTH_QML_DIR, see
// src/pokerth.cpp: the client then loads the interface from this tree instead
// of the compiled-in qrc).
//
// In a normal build this file is NOT a resource: CMake aliases either
// SvgIconVector.qml (Qt >= 6.8) or SvgIconImage.qml (older) to
// "components/SvgIcon.qml" in the embedded qrc (svgicon.qrc.in). The qrc wins
// because a normal run loads qrc:/pokerth.qml, so the alias is what the call
// sites see.
//
// When the tree is loaded from DISK there is no generated alias, and a bare
// `SvgIcon { }` fails with "SvgIcon is not a type" - the window does not open at
// all (hit 2026-11-10, the console blinked and the client exited). This file
// closes exactly that hole. It takes the Qt >= 6.8 variant, because the dev mode
// runs with the CI Qt (6.9.3); an older Qt would only need SvgIconImage here.
//
// Do NOT add this file to qml.qrc: it would collide with the alias entry.
SvgIconVector {
}
