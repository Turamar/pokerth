import QtQuick

import "../config" as Config

// Small info window of the running table: it opens above the info button in the
// lower-left corner of the table. The caller sets the width and the position;
// the height follows the content, so the window is two rows high at a cash table
// and grows by one row per tournament fact. In a tournament it shows the blinds
// of the running level, the next level with the hands left, the level, the place
// by stack, how many places are paid and the hand number; at a cash table only
// the blinds of the limit and the hand. The surface is the matte table bar of
// the theme (like the docked chat/info panels of the table): a hairline edge,
// radius 10, no translucency. There is deliberately no title and no close
// button – the caller's button toggles the window.
Rectangle {
    id: panel

    // ── Surface ───────────────────────────────────────────────────────────────
    color: Config.Theme.tableBarSurface
    border.color: Config.Theme.tableBarEdge
    border.width: 1
    radius: 10

    // Inner padding of the surface and the gap between the rows. One row is a
    // single line of the 12 px table type, so with the gap a row is about 18 px.
    readonly property int _padding: 12
    readonly property int _rowGap: 2

    // Height by content – the caller only gives the width and the position.
    implicitHeight: content.implicitHeight + 2 * _padding

    // ── Data ──────────────────────────────────────────────────────────────────
    // GameTable is the GameHandler of the table (context property, used the same
    // way in BetChip and the player boxes). The live tournament numbers
    // (tournamentLevel, tournamentHandsToLevel, tournamentNextSmallBlind,
    // tournamentAlive, tournamentRank, tournamentPaidPlaces) are added to the
    // C++ side in parallel, so a binary built before that change does not have
    // them. Every such read is guarded with an undefined check – without it QML
    // reports an error for a missing property. A missing number reads as 0 and
    // its row is hidden (the level shows a dash instead).
    readonly property bool _hasTable: typeof GameTable !== "undefined" && GameTable !== null
    readonly property bool _tournament: _hasTable && GameTable.tournamentMode === true
    readonly property int _smallBlind: _hasTable ? GameTable.smallBlind : 0
    readonly property int _bigBlind: _hasTable ? GameTable.bigBlind : 0
    readonly property int _hand: _hasTable ? GameTable.handNumber : 0
    readonly property int _seats: _hasTable ? GameTable.tournamentSeats : 0
    readonly property int _level:
        (_hasTable && GameTable.tournamentLevel !== undefined) ? GameTable.tournamentLevel : 0
    readonly property int _handsToLevel:
        (_hasTable && GameTable.tournamentHandsToLevel !== undefined)
            ? GameTable.tournamentHandsToLevel : 0
    readonly property int _nextSmallBlind:
        (_hasTable && GameTable.tournamentNextSmallBlind !== undefined)
            ? GameTable.tournamentNextSmallBlind : 0
    readonly property int _alive:
        (_hasTable && GameTable.tournamentAlive !== undefined) ? GameTable.tournamentAlive : 0
    readonly property int _rank:
        (_hasTable && GameTable.tournamentRank !== undefined) ? GameTable.tournamentRank : 0
    readonly property int _paidPlaces:
        (_hasTable && GameTable.tournamentPaidPlaces !== undefined)
            ? GameTable.tournamentPaidPlaces : 0

    // An amount as the table talks about it: chips ("1 500") in a tournament,
    // money ("$0.40") at a cash table. The unit is chosen by the handler, the
    // window only asks it for the number.
    function _amount(value) {
        return _hasTable ? GameTable.amount(value) : "—"
    }

    // A row is one translated sentence with the numbers inside it: the label
    // part stays muted (Text.color of the rows) while the numbers carry their
    // own colour. Only the arguments get the markup, so every qsTr() source
    // stays clean and can go into the .ts files as it is.
    function _value(text) {
        return "<font color='#FFFFFF'>" + text + "</font>"
    }

    // The blinds and the level are the loud numbers of the window – the key
    // numbers of the table keep the accent green (#b7ea2f) there as well.
    function _accent(text) {
        return "<font color='#b7ea2f'>" + text + "</font>"
    }

    // Muted label colour: the white of the values at 55 %. withAlpha() needs a
    // colour value – a bare "#FFFFFF" string would be read as black, hence
    // Qt.rgba(1, 1, 1, 1) instead of the hex literal.
    readonly property color _labelColor:
        Config.Theme.withAlpha(Qt.rgba(1, 1, 1, 1), 0.55)

    // ── The lines ─────────────────────────────────────────────────────────────
    // The qsTr() sources are the interface strings for the .ts files; the blind
    // numbers are the running ones in both modes (amount() picks the unit).
    readonly property string _blindsLine:
        qsTr("Blinds: %1 / %2")
            .arg(_accent(_amount(_smallBlind)))
            .arg(_accent(_amount(_bigBlind)))

    // Only while the blinds can still rise: the controller returns the running
    // blind as the next one on the last level, so the row disappears instead of
    // repeating the same numbers.
    readonly property string _nextLine:
        qsTr("Next: %1 / %2 · in %3 hands")
            .arg(_accent(_amount(_nextSmallBlind)))
            .arg(_accent(_amount(_nextSmallBlind * 2)))
            .arg(_value(_handsToLevel))

    readonly property string _levelLine:
        qsTr("Level: %1").arg(_level > 0 ? _accent(_level) : "—")

    // The place by stack against the people still in and the seats of the
    // event: rank is 0 while the handler does not know it yet (old binary).
    readonly property string _placeLine:
        qsTr("Place: %1 of %2 left · %3 total")
            .arg(_value(_rank))
            .arg(_value(_alive))
            .arg(_value(_seats))

    readonly property string _paidLine:
        qsTr("Paid: %1 of %2")
            .arg(_value(_paidPlaces))
            .arg(_value(_seats))

    readonly property string _handLine:
        qsTr("Hand: %1").arg(_value(_hand))

    // ── Content ───────────────────────────────────────────────────────────────
    Column {
        id: content
        anchors.fill: parent
        anchors.margins: panel._padding
        spacing: panel._rowGap

        // The blinds of the running level: current small and big blind.
        AppText {
            width: content.width
            textFormat: Text.StyledText
            color: panel._labelColor
            font.pixelSize: Config.Theme.tableFont(12)
            elide: Text.ElideRight
            text: panel._blindsLine
        }

        // The next level: it is only interesting while it is still ahead.
        AppText {
            width: content.width
            textFormat: Text.StyledText
            color: panel._labelColor
            font.pixelSize: Config.Theme.tableFont(12)
            elide: Text.ElideRight
            visible: panel._tournament && panel._nextSmallBlind > panel._smallBlind
                     && panel._handsToLevel > 0
            text: panel._nextLine
        }

        // The running level of the tournament (a dash until the handler knows it).
        AppText {
            width: content.width
            textFormat: Text.StyledText
            color: panel._labelColor
            font.pixelSize: Config.Theme.tableFont(12)
            elide: Text.ElideRight
            visible: panel._tournament
            text: panel._levelLine
        }

        // Own place by stack, how many are left and how many seats the event has.
        AppText {
            width: content.width
            textFormat: Text.StyledText
            color: panel._labelColor
            font.pixelSize: Config.Theme.tableFont(12)
            elide: Text.ElideRight
            visible: panel._tournament && panel._rank > 0
            text: panel._placeLine
        }

        // How many places pay out (of the seats of the event).
        AppText {
            width: content.width
            textFormat: Text.StyledText
            color: panel._labelColor
            font.pixelSize: Config.Theme.tableFont(12)
            elide: Text.ElideRight
            visible: panel._tournament && panel._paidPlaces > 0
            text: panel._paidLine
        }

        // The hand number of the running game (cash and tournament alike).
        AppText {
            width: content.width
            textFormat: Text.StyledText
            color: panel._labelColor
            font.pixelSize: Config.Theme.tableFont(12)
            elide: Text.ElideRight
            text: panel._handLine
        }
    }
}
