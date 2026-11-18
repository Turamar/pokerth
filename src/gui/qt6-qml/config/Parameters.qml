pragma Singleton
import QtQuick 6.5
import QtCore

// Values that live in the Qt Settings singleton (the platform settings store:
// the Windows registry under HKCU\Software\PokerTH), NOT in config.xml. They are
// reset together with the config file by the "Standardeinstellung" page (see
// resetToDefaults below, settings audit 2026-11-18, PLAN.md §7).
//
// The dead duplicates of config.xml keys that used to sit here (displayLeftToolboxCheckbox,
// fadeOutLosingCardsAnimationCheckbox, showNetworkStatusColorOnAvatarCheckbox, …) were
// removed with the same audit: nothing read them, they only left stale values in the
// registry.
Settings {
    // Runtime mirror of the config key "Language": the ApplicationWindow copies it
    // there from the ConfigFile at startup. It is deliberately not reset here -
    // the next start takes it from config.xml again.
    property string language: StaticData.findSupportedLocale(Qt.locale().name)

    // Table zoom on touch/mobile: swiping and zooming the felt (compact layout only).
    property bool tableZoomEnabled: true

    // Keep seats of players who have left the table (disconnect, kick, leave,
    // knocked out) as invisible placeholders in the ring → the remaining player
    // boxes keep their position (default). false = the ellipse is redistributed
    // without the free seat (the boxes move up).
    property bool keepEmptySeats: true

    // Community/ranking and forum news are network features; this fork is a
    // single-player build (PLAN.md, section 1), so both are off. The UI entries
    // are gone as well (start page, top bar, settings).
    property bool showCommunityContent: false

    // Optional admin feature: creators of a BBC step/WEC invite game can
    // suggest matching idle players in the chat of the waiting room
    // (Config.BotSuggest). Only effective with community content enabled.
    property bool showCommunitySuggest: false

    // Preselected ranking source ("pokerth" | "bbc" | "wec") for table info and
    // player stats when community content is active. For the selection see
    // Config.Community.entries.
    property string defaultCommunity: "pokerth"

    // Tooltips on icon buttons (desktop only – on touch there is no hover).
    property bool showTooltips: true

    // Back to the defaults of this store. The "Standardeinstellung" page calls
    // it together with SettingsManager.resetToDefaults(), so that the reset
    // really covers both storages (settings audit, PLAN.md §7). Money and
    // state.json are not touched by either.
    function resetToDefaults() {
        tableZoomEnabled = true
        keepEmptySeats = true
        showCommunityContent = false
        showCommunitySuggest = false
        defaultCommunity = "pokerth"
        showTooltips = true
    }
}
