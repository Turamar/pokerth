pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Universal
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window

import "config" as Config
import "pages"
import "components"

ApplicationWindow {
    id: mainWindow

    Universal.theme: Config.StaticData.isDark ? Universal.Dark : Universal.Light

    // portraitMode is now provided by Config.Responsive.portrait
    // Hide the top bar icons in the splash/PreLoader – otherwise a click that is
    // too early can push a page above the PreLoader, which then messes up the
    // stack with its replaceCurrentItem(startPage).
    readonly property bool topBarIconsVisible:
        mainStackView.currentItem
        && mainStackView.currentItem.objectName !== "preLoaderPage"

    // The game table lies on top of the stack. On the table the common header is
    // hidden (the table needs the vertical room and has a status bar of its own);
    // the header controls float above the page instead (see the floating
    // buttons below mainLayout).
    readonly property bool gamePageActive:
        mainStackView.currentItem
        && mainStackView.currentItem.objectName === "gamePage"

    // The wallet: balance, counters and the rebuy. A normal page on the stack,
    // so the back caret (and Escape) returns to whatever was open before.
    function openWallet() {
        var current = mainStackView.currentItem
        if (current && current.objectName === "walletPage")
            return
        mainStackView.push("pages/WalletPage.qml")
    }

    // The menu/back/door button, shared by the header and the floating button on
    // the table: back out of the page first (the door asks before leaving a
    // game), otherwise toggle the side menu.
    function toggleSideMenu() {
        if (!navigateBackFromTopBar()) {
            topBarMenuIcon.source = !sideMenu.visible
                ? "resources/caretLeft.svg" : "resources/threeLines.svg"
            sideMenu.visible = !sideMenu.visible
        }
    }

    // The tooltip of that button – the floating button on the table shows the
    // same text as the header.
    function menuButtonTooltip() {
        var src = String(topBarMenuIcon.source)
        if (src.indexOf("doorExit") !== -1)
            return qsTr("Leave Game")
        if (src.indexOf("caretLeft") !== -1)
            return qsTr("Back")
        return qsTr("Menu")
    }

    // The overlay pages of the top bar icons. Settings is the only one left in the
    // single-player build; community/ranking and forum news were removed with the
    // network (PLAN.md §1, §8).
    readonly property var settingsSectionPages: ["settingsPage"]

    // All overlay pages together – the basis of closeTopBarOverlay().
    readonly property var overlaySectionPages: settingsSectionPages

    // Active = the topmost page is the settings page → highlight the gear.
    readonly property bool settingsSectionActive:
        topBarSectionOpen(settingsSectionPages)

    function topBarSectionOpen(sectionPages) {
        var c = mainStackView.currentItem
        return c && sectionPages.indexOf(c.objectName) !== -1
    }

    // Pop the settings page off the stack, so that the base page below (start page,
    // wallet, table) appears again.
    function closeTopBarOverlay() {
        var overlay = overlaySectionPages
        for (var i = mainStackView.depth - 1; i >= 0; --i) {
            var item = mainStackView.get(i)
            if (!item || overlay.indexOf(item.objectName) === -1) {
                mainStackView.pop(item)
                return
            }
        }
    }

    // The top bar gear as a toggle: if settings is already open, close it down to
    // the base page; otherwise open it.
    function toggleTopBarSection(entryUrl, sectionPages) {
        var open = topBarSectionOpen(sectionPages)
        closeTopBarOverlay()
        if (!open)
            mainStackView.push(entryUrl)
        sideMenu.visible = false
    }

    property StartPage startPage: StartPage {}
    property SideMenu sideMenu: SideMenu {}
    // The start resolution = the default size of the Qt widgets client at the game table
    // (gametable.ui: 1024×621). When the component is built, the size is
    // additionally clamped to the available screen.
    width: 1024
    height: 621
    // The initial portrait width as a lower bound – the window must not become
    // narrower than the standard portrait mode, so that the layout
    // (slot columns, the self box, the action buttons) always fits into the picture completely.
    //
    // DESKTOP ONLY. On Android/iOS there is no freely scalable window: the
    // area IS the screen. A minimum width that lies above the logical
    // display width makes Qt draw the scene wider than the
    // display – the edge is cut off, the player sees "not the
    // full width". Exactly that happens on widespread 1080p phones: a
    // Samsung S20 FE (1080×2400) reports 360×800 dp depending on the rounded
    // devicePixelRatio – 30 dp narrower than the 390 demanded here.
    minimumWidth: Config.Responsive.isMobile ? 0 : 390
    minimumHeight: Config.Responsive.isMobile ? 0 : 600
    // TRY to center the window, doesn't work on my Ubuntu but should work on other platforms.
    visible: true
    title: qsTr("PokerTH - v2.1.11")

    // Android hardware back button: intercept close and navigate back instead
    // of destroying the QML scene while background threads are still running.
    onClosing: (close) => {
        if (mainStackView.depth > 1) {
            close.accepted = false
            navigateBackFromTopBar()
        }
        // depth === 1: allow close → app.exec() returns → proper C++ cleanup
    }

    // Keep Responsive singleton in sync with the actual window dimensions
    onWidthChanged: {
        Config.Responsive.windowWidth = width
        Config.Theme.windowWidth      = width
    }
    onHeightChanged: {
        Config.Responsive.windowHeight = height
        Config.Theme.windowHeight      = height
    }

    Component.onCompleted: {
        // An aspect preserving clamp to the available screen. 2316×1080
        // is the phone landscape test size (aspect 2.144); on notebooks with
        // 1920×1080 or 2560×1440 the window would otherwise either stick out
        // or lose its aspect ratio — both disable the
        // landscapeCompact mode (the aspect threshold 1.85).
        //
        // Purely DESKTOP logic: the size, the minimum size and the centring only make
        // sense where there is a window IN a screen. On Android/
        // iOS the system determines the area (fullscreen); any geometry of our own
        // works against it. Concretely the clamp computed on a 360×800 dp
        // phone: scale = (360−20)/1024 = 0.33 → width = max(390, 340) = 390 and
        // x = 180 − 195 = −15, i.e. a window WIDER than the display and
        // offset to the left as well → both edges of the scene lie outside
        // the display. So do not touch anything at all on mobile.
        if (!Config.Responsive.isMobile && screen) {
            var maxW = screen.width  - 20
            var maxH = screen.height - 60   // Taskleiste/Titelbar
            var scale = Math.min(maxW / width, maxH / height, 1.0)
            if (scale < 1.0) {
                width  = Math.max(minimumWidth,  Math.floor(width  * scale))
                height = Math.max(minimumHeight, Math.floor(height * scale))
            }
            x = screen.width / 2 - width / 2
            y = screen.height / 2 - height / 2
        }
        Config.Responsive.windowWidth  = width
        Config.Responsive.windowHeight = height
        Config.Theme.windowWidth       = width
        Config.Theme.windowHeight      = height
        // The language comes from the ConfigFile (the key "Language") – the same value that
        // the widgets client uses as well. Parameters.language is only the
        // runtime value for the user interface.
        Config.Parameters.language = Config.StaticData.configLanguageToLocale(
                    SettingsManager ? SettingsManager.language : "")
        LanguageManager.switchLanguage(Config.Parameters.language)
        // Initialise dark/light mode from the stored preference. "Automatic"
        // (2) follows the system – the value comes from C++ (darkmode.h) and is
        // updated by systemDarkSync when the system theme changes.
        var dm = SettingsManager ? SettingsManager.readConfigInt("DarkMode") : 1
        applySystemDark()
        Config.StaticData.darkMode = dm
        Config.Theme.darkMode = dm
        // Decorative effects (shadow/glow/blur) from the persistent setting.
        Config.Theme.effectsEnabled = SettingsManager
            ? SettingsManager.readConfigInt("QmlReduceEffects") === 0 : true
    }

    // Mirror the light/dark of the operating system into the singletons (which
    // cannot read a context property themselves). It only applies with DarkMode =
    // "automatic"; with a fixed light/dark setting the value stays
    // unused.
    function applySystemDark() {
        var sd = SettingsManager ? SettingsManager.systemDark : true
        Config.StaticData.systemDark = sd
        Config.Theme.systemDark      = sd
    }

    // System-Theme-Wechsel im laufenden Betrieb (Windows/macOS Hell↔Dunkel).
    Connections {
        id: systemDarkSync
        target: SettingsManager
        function onSystemDarkChanged() { mainWindow.applySystemDark() }
    }

    function navigateBackFromTopBar() {
        if (mainStackView.depth <= 1)
            return false

        var current = mainStackView.currentItem

        // The waiting room and a running game: ALWAYS ask before leaving
        // (no matter whether via Esc, Android back or the door icon), so that an
        // accidental keystroke does not leave the game unintentionally. The
        // actual leaving is done by performLeaveGame() after the confirmation.
        if (current && current.objectName === "gamePage") {
            leaveGameConfirmPopup.open()
            return true
        }

        // Pages with a back step of their own (e.g. the login form →
        // the selection) take precedence over leaving the page. Escape
        // does not reach the pages itself: the shortcut above applies BEFORE the
        // Keys handlers of the items, a Keys.onEscapePressed on a page
        // would never fire. That is why the navigation asks here.
        // qmllint disable missing-property
        if (current && typeof current.handleBack === "function" && current.handleBack())
            return true
        // qmllint enable missing-property

        mainStackView.pop()
        return true
    }

    function performLeaveGame() {
        var current = mainStackView.currentItem
        var isGamePage = current && current.objectName === "gamePage"
        var localGame = isGamePage
                        && (typeof GameTable !== "undefined")
                        && GameTable
                        && GameTable.isLocalGameRunning()

        if (localGame)
            GameTable.endLocalGame()

        mainStackView.pop()
        if (localGame && mainStackView.depth > 1)
            mainStackView.pop()
    }

    // ── Edge-to-edge display (Android 15+) ──────────────────────────────────
    // From targetSdk 35 on, Android draws edge to edge MANDATORILY: the "fullscreen" flag
    // of the app theme is ignored, the status and navigation bar lie above
    // the window content. The insets that have to stay free for them are reported by
    // SafeArea (QtQuick 6.9) – encapsulated in a file of its own, because the type
    // does not exist yet on Qt 6.7 (the Android 8 APK variant) and a direct
    // access there would make the whole window fail to load. If the
    // loader fails, the insets stay 0 and everything is as before; likewise when the
    // platform already keeps the bars free itself.
    Loader {
        id: safeAreaLoader
        anchors.fill: parent
        z: -1
        active: Config.Responsive.isMobile
        source: "components/SafeAreaInsets.qml"
    }
    readonly property real safeAreaTop:
        safeAreaLoader.item ? safeAreaLoader.item.insetTop : 0
    readonly property real safeAreaBottom:
        safeAreaLoader.item ? safeAreaLoader.item.insetBottom : 0
    readonly property real safeAreaLeft:
        safeAreaLoader.item ? safeAreaLoader.item.insetLeft : 0
    readonly property real safeAreaRight:
        safeAreaLoader.item ? safeAreaLoader.item.insetRight : 0

    // The background deliberately WITHOUT safe area insets: the system bars should lie on
    // the app background colour, not on a black strip.
    Rectangle {
        anchors.fill: parent
        color: Config.StaticData.palette.secondary.col700
    }

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        // Keep the content out of the status/navigation bar and the notch.
        anchors.topMargin:    mainWindow.safeAreaTop
        anchors.bottomMargin: mainWindow.safeAreaBottom
        anchors.leftMargin:   mainWindow.safeAreaLeft
        anchors.rightMargin:  mainWindow.safeAreaRight
        Layout.alignment: Qt.AlignTop
        spacing: 0

        Rectangle {
            id: topBar
            Layout.preferredWidth: parent.width
            // A compact app header on short landscape phones (it saves vertical
            // room for the table -> less opponent box overlap).
            // On the game table the header is hidden and its controls float
            // above the page instead (the table gains the vertical room;
            // an invisible layout item takes no height in a ColumnLayout).
            visible: !mainWindow.gamePageActive
            Layout.preferredHeight: Config.Responsive.landscapeCompact ? 30 : 38
            Layout.alignment: Qt.AlignTop
            color: Config.Theme.colorBox

            RowLayout {
                id: topBarColumns
                anchors.fill: parent
                spacing: 8

                SvgIcon {
                    id: topBarMenuIcon
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    Layout.margins: Config.Responsive.landscapeCompact ? 2 : 6
                    source: "resources/threeLines.svg"
                    visible: mainWindow.topBarIconsVisible
                    // The tooltip follows the function of the button: the door icon = leave the game
                    // (depending on the page), the caret = back, otherwise the menu.
                    ToolTip.visible: menuArea.containsMouse
                                     && !Config.Responsive.isMobile && Config.Parameters.showTooltips
                    ToolTip.delay: 600
                    ToolTip.text: mainWindow.menuButtonTooltip()
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: menuArea.containsMouse
                            ? Config.StaticData.palette.secondary.col100
                            : Config.StaticData.palette.secondary.col200
                    }

                    MouseArea {
                        id: menuArea
                        anchors.fill: topBarMenuIcon
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true

                        onClicked: mainWindow.toggleSideMenu()
                    }
                }

                Item {
                    id: topBarMenuSpace
                    Layout.fillWidth: true
                    Layout.horizontalStretchFactor: 2
                }

                // The player's money. It is visible on every page - the balance is
                // the one number that always matters - and a click opens the wallet.
                Item {
                    id: topBarWallet
                    Layout.preferredWidth: walletRow.implicitWidth + 8
                    Layout.preferredHeight: 22
                    Layout.margins: Config.Responsive.landscapeCompact ? 2 : 6
                    visible: mainWindow.topBarIconsVisible
                             && (typeof Wallet !== "undefined") && Wallet !== null

                    ToolTip.visible: walletArea.containsMouse && !Config.Responsive.isMobile
                                     && Config.Parameters.showTooltips
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Wallet")

                    Row {
                        id: walletRow
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        spacing: 4

                        SvgIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            source: "resources/chip.svg"
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: walletArea.containsMouse
                                    ? Config.StaticData.palette.secondary.col100
                                    : Config.StaticData.palette.secondary.col300
                            }
                        }
                        AppText {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Wallet ? Wallet.money(Wallet.balance) : ""
                            color: walletArea.containsMouse
                                ? Config.StaticData.palette.secondary.col100
                                : Config.StaticData.palette.secondary.col200
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: walletArea
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: mainWindow.openWallet()
                    }
                }

                SvgIcon {
                    id: topBarSettingsIcon
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    Layout.margins: Config.Responsive.landscapeCompact ? 2 : 6
                    source: "resources/settings.svg"
                    visible: mainWindow.topBarIconsVisible
                    ToolTip.visible: settingsArea.containsMouse
                                     && !Config.Responsive.isMobile && Config.Parameters.showTooltips
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Settings")
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        colorization: 1.0
                        colorizationColor: mainWindow.settingsSectionActive
                            ? Config.Theme.colorAccent
                            : settingsArea.containsMouse
                                ? Config.StaticData.palette.secondary.col100
                                : Config.StaticData.palette.secondary.col200
                    }

                    MouseArea {
                        id: settingsArea
                        anchors.fill: topBarSettingsIcon
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true

                        onClicked: mainWindow.toggleTopBarSection(
                            "pages/SettingsPage.qml",
                            mainWindow.settingsSectionPages)
                    }
                }
            }
        }

        StackView {
            id: mainStackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop
            initialItem: PreLoader {}

            replaceEnter: Transition {
                YAnimator {
                    from: (mainStackView.mirrored ? -1 : 1) * -mainStackView.height
                    to: 0
                    duration: 400
                    easing.type: Easing.OutCubic
                }
            }

            replaceExit: Transition {
                YAnimator {
                    from: 0
                    to: (mainStackView.mirrored ? -1 : 1) * mainStackView.height
                    duration: 400
                    easing.type: Easing.OutCubic
                }
            }

            onCurrentItemChanged: {
                var isGame = (currentItem && currentItem.objectName === "gamePage");
                // The visibility of the top bar icons follows the binding
                // topBarIconsVisible (off in the splash) – here only the source icon
                // of the menu/back button is chosen depending on the page.
                if (depth <= 1) {
                    topBarMenuIcon.source = sideMenu.visible ? "resources/caretLeft.svg" : "resources/threeLines.svg";
                } else if (isGame) {
                    // On the table: a door icon for leaving.
                    topBarMenuIcon.source = "resources/doorExit.svg";
                } else {
                    topBarMenuIcon.source = "resources/caretLeft.svg";
                }
                // Keep the screen awake during the game (Android: FLAG_KEEP_SCREEN_ON
                // via JNI). Release it when leaving.
                ScreenHelper.setKeepScreenOn(isGame);
                // The local action clock only runs while the table is actually on
                // screen (settings audit, PLAN.md §7): a page above it – the
                // settings, the wallet, the stats – suspends it, coming back
                // starts it again from its full length.
                if (typeof GameTable !== "undefined" && GameTable) {
                    if (isGame)
                        GameTable.resumeLocalActionClock()
                    else
                        GameTable.suspendLocalActionClock()
                }
            }
        }
    }

    // ── The floating table controls ──────────────────────────────────────────
    // The common header is hidden on the game page (gamePageActive) – this
    // table needs the vertical room and has a status bar of its own. Its
    // controls stay reachable as separate buttons above the page, WITHOUT a
    // common background bar. On every other page they are invisible and the
    // header is unchanged.
    Rectangle {
        id: tableMenuButton
        z: 900
        x: mainWindow.safeAreaLeft + 8
        y: mainWindow.safeAreaTop + 8
        width: 34
        height: 34
        radius: 17
        color: Qt.rgba(0, 0, 0, 0.45)
        visible: mainWindow.gamePageActive && mainWindow.topBarIconsVisible

        // The icon source (three lines / caret / door) is kept up to date on the
        // header icon by mainStackView.onCurrentItemChanged (door on the table).
        ToolTip.visible: tableMenuArea.containsMouse
                         && !Config.Responsive.isMobile && Config.Parameters.showTooltips
        ToolTip.delay: 600
        ToolTip.text: mainWindow.menuButtonTooltip()

        SvgIcon {
            anchors.centerIn: parent
            width: 20
            height: 20
            source: topBarMenuIcon.source
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: tableMenuArea.containsMouse
                    ? Config.Theme.colorAccent
                    : "white"
            }
        }

        MouseArea {
            id: tableMenuArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: mainWindow.toggleSideMenu()
        }
    }

    Rectangle {
        id: tableSettingsButton
        z: 900
        anchors.right: parent.right
        anchors.rightMargin: mainWindow.safeAreaRight + 8
        anchors.top: parent.top
        anchors.topMargin: mainWindow.safeAreaTop + 8
        width: 34
        height: 34
        radius: 17
        color: Qt.rgba(0, 0, 0, 0.45)
        visible: mainWindow.gamePageActive && mainWindow.topBarIconsVisible

        ToolTip.visible: tableSettingsArea.containsMouse
                         && !Config.Responsive.isMobile && Config.Parameters.showTooltips
        ToolTip.delay: 600
        ToolTip.text: qsTr("Settings")

        SvgIcon {
            anchors.centerIn: parent
            width: 20
            height: 20
            source: "resources/settings.svg"
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: mainWindow.settingsSectionActive
                    ? Config.Theme.colorAccent
                    : "white"
            }
        }

        MouseArea {
            id: tableSettingsArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: mainWindow.toggleTopBarSection(
                "pages/SettingsPage.qml", mainWindow.settingsSectionPages)
        }
    }

    // The balance as a pill left of the settings gear: the one number that
    // always matters; a click opens the wallet, as in the header.
    Rectangle {
        id: tableWalletPill
        z: 900
        anchors.right: tableSettingsButton.left
        anchors.rightMargin: 6
        anchors.verticalCenter: tableSettingsButton.verticalCenter
        width: tableWalletRow.implicitWidth + 20
        height: 28
        radius: 14
        color: Qt.rgba(0, 0, 0, 0.45)
        visible: mainWindow.gamePageActive && mainWindow.topBarIconsVisible

        ToolTip.visible: tableWalletArea.containsMouse
                         && !Config.Responsive.isMobile && Config.Parameters.showTooltips
        ToolTip.delay: 600
        ToolTip.text: qsTr("Wallet")

        Row {
            id: tableWalletRow
            anchors.centerIn: parent
            spacing: 4

            SvgIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                source: "resources/chip.svg"
                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: "white"
                }
            }
            AppText {
                anchors.verticalCenter: parent.verticalCenter
                // Wallet is a context property that does not exist yet in the
                // splash – never touch it blindly.
                text: (typeof Wallet !== "undefined" && Wallet)
                      ? Wallet.money(Wallet.balance) : ""
                color: "white"
                font.pixelSize: 13
                font.bold: true
            }
        }

        MouseArea {
            id: tableWalletArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            onClicked: mainWindow.openWallet()
        }
    }

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (!navigateBackFromTopBar() && sideMenu.visible) {
                sideMenu.visible = false
                topBarMenuIcon.source = "resources/threeLines.svg"
            }
        }
    }

    Shortcut {
        sequence: StandardKey.Back
        onActivated: {
            navigateBackFromTopBar()
        }
    }

    // Fullscreen: it applies on EVERY page (the start page, wallet, settings, the table) –
    // which is why it is on the window and not on the GamePage. Spectators included.
    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: mainWindow.visibility = (mainWindow.visibility === Window.FullScreen)
                                             ? Window.Windowed : Window.FullScreen
    }

    Shortcut {
        sequence: "Alt+S"
        onActivated: {
            // Do not open it in the splash/PreLoader (a stack reset, see topBarIconsVisible).
            if (mainWindow.topBarIconsVisible)
                mainWindow.toggleTopBarSection(
                    "pages/SettingsPage.qml", mainWindow.settingsSectionPages)
        }
    }

    SideMenu {}

    Connections {
        target: mainStackView
        Component.onDestruction: topBarMenuIcon.source = mainStackView.depth === 1 ? "resources/threeLines.svg" : "resources/caretLeft.svg"
    }

    // ── The confirmation when leaving a running game ─────────────────────
    // It appears on Esc / Android back / the door icon while you are on the
    // GamePage, so that an accidental keystroke does not
    // end the running game unintentionally.
    Popup {
        id: leaveGameConfirmPopup
        // Without focus:true the popup would get no keyboard input: Escape would be
        // swallowed (the popup would stay open, and the Escape shortcut of the window
        // does not apply with an open popup either) and Enter would go nowhere.
        focus: true
        // The initial focus deliberately on CANCEL: according to
        // navigateBackFromTopBar() this prompt exists exactly so that an accidental
        // keystroke does not leave the game. Enter must not undermine that;
        // to leave, press Tab once.
        onOpened: leaveGameCancelButton.forceActiveFocus()
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        padding: 20
        width: Math.min(mainWindow.width * 0.85, 380)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Config.Theme.colorBox
            border.color: Config.StaticData.palette.secondary.col400
            border.width: 1
            radius: 8
        }

        ColumnLayout {
            spacing: 12
            width: leaveGameConfirmPopup.availableWidth

            AppLabel {
                Layout.fillWidth: true
                text: qsTr("Leave Game")
                color: Config.StaticData.palette.secondary.col100
                font.pixelSize: 15
                font.bold: true
            }
            AppLabel {
                Layout.fillWidth: true
                text: qsTr("Attention! Do you really want to leave the current game?")
                color: Config.StaticData.palette.secondary.col200
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                CustomButton {
                    id: leaveGameCancelButton
                    Layout.fillWidth: true
                    text: qsTr("Cancel")
                    onClicked: leaveGameConfirmPopup.close()
                }
                CustomButton {
                    Layout.fillWidth: true
                    text: qsTr("Leave Game")
                    onClicked: {
                        leaveGameConfirmPopup.close()
                        mainWindow.performLeaveGame()
                    }
                }
            }
        }
    }

    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive) {
                // Re-apply FLAG_KEEP_SCREEN_ON after foreground transitions:
                // Android may clear window flags during lifecycle changes.
                var item = mainStackView.currentItem
                ScreenHelper.setKeepScreenOn(item !== null && item.objectName === "gamePage")
            }
        }
    }
}
