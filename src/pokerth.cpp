/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/
#ifdef QML_CLIENT
//START THE QML SWITCH HERE
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <QtCore>
#include <QtQml>
#include <QApplication>
#include <QSettings>
#include <QQuickWindow>
#include <QIcon>
#include <QFileInfo>
#include <boost/shared_ptr.hpp>
#include "configfile.h"
#include "session.h"
#include "log.h"
#include <QTranslator>
#include <QQmlContext>
#include <QDebug>
#include <retranslate.h>
#include <settingsmanager.h>
#include "gui/qt6-qml/cpp/styleprovider.h"
#include "gui/qt6-qml/cpp/gamehandler.h"
#include "gui/qt6-qml/cpp/qmlguiinterface.h"
#include "gui/qt6-qml/cpp/screenhelper.h"
#include "gui/qt6-qml/cpp/wallethandler.h"
#include <soundevents.h>
#include <memory>
#include <core/loghelper.h>
#include <cstdio>

// Routes every Qt/QML diagnostic message (qDebug, qWarning, and crucially the
// QML console.log "[NAV]" navigation traces) into the persistent client debug
// log file (~/.pokerth/pokerth-debug.log) opened by loghelper_init(), while
// still echoing to stderr so a terminal run looks unchanged. Combined with the
// engine/net LOG_* messages (which share the same file) this gives a single,
// thread-id-tagged timeline – essential for the rare end-of-round freeze where
// the GUI thread blocks inside the SQLite log flush.
static void pokerthQmlMessageHandler(QtMsgType type, const QMessageLogContext & /*ctx*/, const QString &msg)
{
	// Debug messages are NOT processed any more: the per-action
	// instrumentation (e.g. [ACTDBG] in the GameHandler) stems from the freeze
	// investigation at the time (solved) and wrote synchronously to stderr
	// AND to disk per action (an fflush per line) → unnecessary I/O in the game. Only
	// info/warning/critical/fatal (rare, real diagnostics) are logged.
	//
	// The exception: a diagnostic build (-DPOKERTH_DIAG_LOG=ON, see qt6-qml/CMakeLists.txt).
	// There the per-action instrumentation is exactly what is wanted, so the
	// debug lines have to be let through here.
#ifndef POKERTH_DIAG_LOG
	if (type == QtDebugMsg)
		return;
#endif

	const char *tag = "[QML] ";
	switch (type) {
	case QtDebugMsg:
		tag = "[QML-D] ";
		break;
	case QtInfoMsg:
		tag = "[QML-I] ";
		break;
	case QtWarningMsg:
		tag = "[QML-W] ";
		break;
	case QtCriticalMsg:
		tag = "[QML-C] ";
		break;
	case QtFatalMsg:
		tag = "[QML-F] ";
		break;
	}
	const QByteArray local = msg.toLocal8Bit();
	fprintf(stderr, "%s%s\n", tag, local.constData());
	fflush(stderr);
	loghelper_write_raw(std::string(tag) + msg.toStdString());
	if (type == QtFatalMsg)
		abort();
}

// Forces a complete rebuild of the window content after every
// resize.
//
// The background (the forum bug "QML - Button 'hit zone' is sometimes wrong",
// Windows 10, 2.1.5): if you pull the window open at the BOTTOM edge,
// the D3D11 RHI backend keeps presenting the old frame. The scene is laid out
// correctly for the new size – only the image lags behind: the upper app bar
// is missing in it, and at the bottom an equally high strip stays unpainted (you see
// other windows through it). Since all click zones follow the VALID geometry,
// it looks as if the buttons were ~15–40 px too low – in the worst case
// a fold click aimed too high hits the all-in button above it.
// If you move the window by its title bar, everything is correct again:
// ONE additional render pass is enough to resolve the state.
// The reporter confirmed that with a backend test: with QSG_RHI_BACKEND=opengl
// the bug does not occur, whereas QSG_RENDER_LOOP=basic changes nothing.
//
// Exactly this additional pass is what the filter triggers. The trailing
// update is necessary because Windows blocks the event loop during the interactive
// resize loop – the timer only fires once the user has released the
// mouse and the final size really is settled.
class ResizeRepaintGuard : public QObject
{
public:
	explicit ResizeRepaintGuard(QQuickWindow *window)
		: QObject(window), m_window(window)
	{
		m_settle.setSingleShot(true);
		m_settle.setInterval(80);
		QObject::connect(&m_settle, &QTimer::timeout, this, [this]() {
			m_window->requestUpdate();
		});
		m_window->installEventFilter(this);
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (watched == m_window
				&& (event->type() == QEvent::Resize || event->type() == QEvent::Expose)) {
			m_window->requestUpdate();
			m_settle.start();
		}
		// Only read along, never swallow.
		return QObject::eventFilter(watched, event);
	}

private:
	QQuickWindow *m_window;
	QTimer m_settle;
};

// The directory config.xml lives in - and with it the place where the
// persistent game state (state.json) belongs. ConfigFile builds its paths in the
// local 8 bit encoding (see pathToQString in configfile.cpp), so decoding has to
// use the same one: fromUtf8 would corrupt a non-ASCII user name in the path.
static QString stateDirectoryFor(const std::string &configFileName)
{
	const QString path = QString::fromLocal8Bit(configFileName.c_str());
	if (path.endsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('\\')))
		return path;
	return QFileInfo(path).absolutePath();
}

// The QML translations are named after locales (:/i18n/pokerth_ru_RU.qm), while
// ConfigFile keeps the language in the form the widgets client writes it: a
// short code ("en", "ptbr", "zhcn") or a locale name ("ru_RU" - that is the
// default, the system locale). This table mirrors
// config/StaticData.qml::languageConfigCodes for the short codes; a value that
// already looks like a locale is passed through unchanged.
static QString qmlTranslationFile(const QString &language)
{
	static const QHash<QString, QString> shortCodes = {
		{QStringLiteral("de"), QStringLiteral("de_DE")},
		{QStringLiteral("en"), QStringLiteral("en_US")},
		{QStringLiteral("es"), QStringLiteral("es_ES")},
		{QStringLiteral("fr"), QStringLiteral("fr_FR")},
		{QStringLiteral("it"), QStringLiteral("it_IT")},
		{QStringLiteral("ptbr"), QStringLiteral("pt_BR")},
		{QStringLiteral("ptpt"), QStringLiteral("pt_PT")},
		{QStringLiteral("ru"), QStringLiteral("ru_RU")},
		{QStringLiteral("cz"), QStringLiteral("cs_CZ")},
		{QStringLiteral("dk"), QStringLiteral("da_DK")},
		{QStringLiteral("gr"), QStringLiteral("el_GR")},
		{QStringLiteral("hu"), QStringLiteral("hu_HU")},
		{QStringLiteral("nl"), QStringLiteral("nl_NL")},
		{QStringLiteral("pl"), QStringLiteral("pl_PL")},
		{QStringLiteral("sv"), QStringLiteral("sv_SE")},
		{QStringLiteral("tr"), QStringLiteral("tr_TR")},
		{QStringLiteral("bg"), QStringLiteral("bg_BG")},
		{QStringLiteral("ca"), QStringLiteral("ca_ES")},
		{QStringLiteral("fi"), QStringLiteral("fi_FI")},
		{QStringLiteral("no"), QStringLiteral("nb_NO")},
		{QStringLiteral("af"), QStringLiteral("af_ZA")},
		{QStringLiteral("gd"), QStringLiteral("gd_GB")},
		{QStringLiteral("gl"), QStringLiteral("gl_ES")},
		{QStringLiteral("jp"), QStringLiteral("ja_JP")},
		{QStringLiteral("lt"), QStringLiteral("lt_LT")},
		{QStringLiteral("sk"), QStringLiteral("sk_SK")},
		{QStringLiteral("ta"), QStringLiteral("ta_IN")},
		{QStringLiteral("vi"), QStringLiteral("vi_VN")},
		{QStringLiteral("zhcn"), QStringLiteral("zh_CN")},
		{QStringLiteral("uk"), QStringLiteral("uk_UA")}
	};
	const QString localeCode = shortCodes.value(language, language);
	const QString file = QStringLiteral(":/i18n/pokerth_%1.qm").arg(localeCode);
	return QFileInfo::exists(file) ? file : QString();
}

int main(int argc, char *argv[])
{
	// QT_QUICK_CONTROLS_STYLE is evaluated during the early style resolve.
	// Under KDE/Plasma a native style is often already set in the environment
	// ("Desktop" / "org.kde.desktop"), which demands the missing
	// module in the deploy. So normalize problematic values early to Universal,
	// before Q(Gui)Application and before the first
	// QtQuick Controls access.
	const QByteArray quickControlsStyle = qgetenv("QT_QUICK_CONTROLS_STYLE");
	const QByteArray quickControlsStyleLower = quickControlsStyle.toLower();
	if (quickControlsStyle.isEmpty()
			|| quickControlsStyleLower == "desktop"
			|| quickControlsStyleLower.startsWith("org.kde")) {
		qputenv("QT_QUICK_CONTROLS_STYLE", "Universal");
	}

	// Switch off the QML disk cache (.qmlc) centrally – before any use of the engine, i.e.
	// effective on EVERY start path (the wrapper, the bundled .desktop, the direct
	// binary, the AppImage) and on every platform. The reason: Qt validates the cache via
	// file mtimes; after an update it sometimes keeps the old .qmlc of an
	// *existing* QML file, so that properties/functions that were NEWLY added are read as
	// `undefined` (e.g. Config.Parameters.showCommunitySuggest →
	// the suggest button disappears). So far only the pokerth-qml wrapper
	// or the Flatpak manifest set this – here it applies everywhere for sure. The minimal
	// recompile effort at the start is negligible.
	qputenv("QML_DISABLE_DISK_CACHE", "1");

	// On Wayland Qt6 uses the THREADED scene graph render loop by default.
	// Its teardown at the end of the program can collide with the GUI thread destruction of the
	// QML items → a crash in QQuickWindowPrivate::cleanup(QSGNode*).
	// The BASIC render loop renders on the GUI thread and tears down synchronously/without
	// a race. Only force it if the user has set nothing AND we run on
	// Wayland (X11 is not affected by the problem; the feature set/
	// effects stay identical, only the render thread split changes).
	if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP")) {
		const bool onWayland = !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
							   || qgetenv("XDG_SESSION_TYPE") == QByteArrayLiteral("wayland");
		if (onWayland)
			qputenv("QSG_RENDER_LOOP", "basic");
	}

	QGuiApplication::setApplicationName("PokerTH");
	QGuiApplication::setOrganizationName("PokerTH");
	QGuiApplication::setOrganizationDomain("pokerth.net");

	QApplication app(argc, argv);

	// single instance check using QLockFile
	QString lockPath = QDir::temp().absoluteFilePath("pokerth_qml-client.lock");
	QLockFile lockFile(lockPath);
	lockFile.setStaleLockTime(0);
	if (!lockFile.tryLock()) {
		return 0;
	}


	QIcon::setThemeName("pokerth");
	// Set the application/window icon explicitly. On X11 (and on Wayland with
	// Qt >= 6.9 via the xdg-toplevel-icon protocol) this makes the window
	// manager / task-bar show the PokerTH logo instead of the generic "W".
	// IMPORTANT: build the icon from concrete rasterized pixmaps, NOT directly
	// from the scalable SVG. A QIcon backed only by an SVG reports no
	// availableSizes(); Qt's Wayland xdg-toplevel-icon path (Qt >= 6.9, used by
	// the AppImage) sends the icon as sized buffers, so with no sizes it sends
	// nothing and the compositor keeps its generic fallback. The widget client
	// works because it uses a raster PNG; here we render the SVG to fixed sizes.
	{
		const QIcon svgIcon(QStringLiteral(":/resources/pokerth.svg"));
		QIcon appIcon;
		for (int sz : {
					16, 24, 32, 48, 64, 128, 256
				})
			appIcon.addPixmap(svgIcon.pixmap(sz, sz));
		QGuiApplication::setWindowIcon(appIcon);
	}
	// On Wayland with the Qt 6.8 deploy the compositor (e.g. KWin) can NOT get
	// the icon from setWindowIcon; it resolves it from the matching .desktop
	// file via the surface app_id. Pin the app_id to the installed desktop-file
	// basename so the portable launcher's ~/.local/share entry is matched.
	QGuiApplication::setDesktopFileName(QStringLiteral("pokerth_qml"));

	boost::shared_ptr<ConfigFile> myConfig;
	myConfig.reset(new ConfigFile(argv[0], false));

	// Open the persistent client debug log (~/.pokerth/pokerth-debug.log) and
	// funnel all Qt/QML diagnostics through it. Done as early as possible so the
	// whole session – including the navigation traces around end-of-game – is
	// captured for after-the-fact analysis of the rare leave-game freeze.
	loghelper_init(myConfig->readConfigString("LogDir"), 1);
	qInstallMessageHandler(pokerthQmlMessageHandler);
	// Install the crash handler right after the log – it writes into its
	// file. From here on a SIGSEGV/abort() leaves a [CRASH] line including a
	// backtrace, instead of letting the log simply break off mid-sentence; the
	// core dump is still produced unchanged (coredumpctl).
	loghelper_install_crash_handler();
	qInfo().noquote() << "[BOOT] PokerTH QML client starting; debug log in"
					  << QString::fromStdString(myConfig->readConfigString("LogDir"));

	// The QtQuick Controls style is already fully determined by the
	// QT_QUICK_CONTROLS_STYLE normalization above (before QApplication);
	// QtQuick reads the env variable itself. NO more QSettings on config.xml:
	// the earlier XML handler (settingsxmlhandler) only read the value
	// attributes and discarded all list entries when writing back
	// (PlayerTooltips = player notes/ratings, ...). Only ConfigFile writes to
	// config.xml.

	// Startup translator: the C++ side needs the configured language before the
	// QML engine runs - the wallet composes its "interrupted cash table session"
	// notice right at construction, and that string has to be in the player's
	// language. Deliberately the CONFIGURED language, not the system locale: on a
	// Russian Windows an English client must not show a Russian notice. The QML
	// side takes the language over once it is running
	// (LanguageManager::switchLanguage removes this translator), so exactly one
	// language is in effect - two stacked translators would fall back from the
	// chosen one to the system locale of the other, i.e. leak Russian into an
	// English UI for every string the English side does not have.
	QTranslator *startupTranslator = new QTranslator(&app);
	const QString startupQm = qmlTranslationFile(
								  QString::fromStdString(myConfig->readConfigString("Language")));
	if (!startupQm.isEmpty() && startupTranslator->load(startupQm)) {
		app.installTranslator(startupTranslator);
		qInfo().noquote() << "[BOOT] startup translation:" << startupQm;
	} else {
		qWarning() << "[BOOT] no QML translation for the configured language,"
				   << "showing the source strings";
	}

	// Initialize the log. main() owns it and closes it after the Session is
	// gone (see the teardown order at the end of main).
	std::unique_ptr<Log> log(new Log(myConfig.get()));

	// Exactly one sound handler for the whole app: every SoundEvents instance
	// opens an audio stream of its own – on Android an OpenSL ES player of
	// its own each time, whose initialization additionally resets the global
	// AudioManager routing.  The lobby and game handlers therefore share
	// this instance (they do not own it).
	std::unique_ptr<SoundEvents> soundEvents(new SoundEvents(myConfig.get()));

	GameHandler *gameHandler = new GameHandler(&app);
	gameHandler->setSoundEvents(soundEvents.get());

	// The GUI interface of the local engine: every callback it gets is
	// forwarded to the GameHandler (or, for the silent tables, not shown).
	QmlGuiInterface *guiInterface = new QmlGuiInterface(myConfig.get());
	guiInterface->setGameHandler(gameHandler);

	// Create and initialize Session
	boost::shared_ptr<Session> session;
	try {
		session.reset(new Session(guiInterface, myConfig.get(), log.get()));
		bool initResult = session->init();
		if (!initResult) {
			qWarning() << "Session initialization failed";
		}
		log->init();

		gameHandler->setSession(session);
		gameHandler->setConfig(myConfig.get());

		// qDebug() << "Session initialized successfully";
	} catch (const std::exception &e) {
		qWarning() << "Exception during session init:" << e.what();
	}

	QQmlApplicationEngine engine;

	SettingsManager settingsMgr(myConfig);
	StyleProvider styleProvider(myConfig);
	LanguageManager langMgr(&engine, startupTranslator);
	ScreenHelper screenHelper;
	// The player's bank: the permanent balance, the profile counters and the
	// cash table session. It lives in state.json next to config.xml (PLAN.md
	// sections 2 and "Сквозные темы").
	WalletHandler walletHandler(stateDirectoryFor(myConfig->configFileName));
	{
		QString playerName = QString::fromStdString(myConfig->readConfigString("MyName")).trimmed();
		if (playerName.isEmpty())
			playerName = QStringLiteral("Human Player");
		walletHandler.setPlayerName(playerName);
	}
	gameHandler->setWallet(&walletHandler);
	engine.rootContext()->setContextProperty("SettingsManager", &settingsMgr);
	engine.rootContext()->setContextProperty("StyleProvider", &styleProvider);
	// The chat and the game history at the table take their text colours from the
	// table theme (light themes need dark text) and recolour themselves on
	// every style change.
	gameHandler->setStyleProvider(&styleProvider);
	engine.rootContext()->setContextProperty("LanguageManager", &langMgr);
	engine.rootContext()->setContextProperty("ScreenHelper", &screenHelper);
	engine.rootContext()->setContextProperty("GameTable", gameHandler);
	engine.rootContext()->setContextProperty("Wallet", &walletHandler);
	// A development switch for the table look: with POKERTH_QML_DIR pointing at
	// the source tree of the QML client (src/gui/qt6-qml) the interface is
	// loaded from DISK instead of the compiled-in qrc. Then a layout change is
	// "save the file, restart the client" instead of a ten minute CI build -
	// the qrc stays the release path, the switch is never set in a release.
	const QString qmlDir = qEnvironmentVariable("POKERTH_QML_DIR");
	QUrl qmlRoot(QStringLiteral("qrc:/pokerth.qml"));
	if (!qmlDir.isEmpty()) {
		const QString fromDisk = QDir(qmlDir).filePath(QStringLiteral("pokerth.qml"));
		if (QFileInfo::exists(fromDisk)) {
			qmlRoot = QUrl::fromLocalFile(fromDisk);
			qInfo() << "QML loaded from disk:" << fromDisk;
		} else {
			qWarning() << "POKERTH_QML_DIR is set but" << fromDisk
			           << "does not exist - loading the built-in QML";
		}
	}
	engine.load(qmlRoot);

	if (engine.rootObjects().isEmpty()) {
		session.reset();
		delete guiInterface;
		log->flushLog();
		return -1;
	}

	// A repaint safeguard for resizes (see ResizeRepaintGuard). The
	// guard hangs off the window and is destroyed with it.
	for (QObject *root : engine.rootObjects()) {
		if (auto *w = qobject_cast<QQuickWindow *>(root))
			new ResizeRepaintGuard(w);
	}

	int result = app.exec();

	// Shut the scene graph down cleanly BEFORE the engine (and with it the
	// QML items including the window) is destroyed during the stack unwind. On Wayland with
	// a threaded render loop QQuickWindowPrivate::cleanup() otherwise crashes when
	// destroying the items (derefWindow → the cleanup of an already half
	// torn down SG). hide() stops the render thread and frees the SG resources
	// while the GUI thread and the app are still fully alive.
	for (QObject *root : engine.rootObjects()) {
		if (auto *w = qobject_cast<QQuickWindow *>(root))
			w->hide();
	}

	// Cleanup order matters:
	// 1. The GameHandler is parented to &app, so its destructor runs inside
	//    ~QApplication - at that point QCoreApplication::instance() is already
	//    nullptr. Drop the session reference it holds here, while the
	//    application is still fully alive.
	// 2. The bank is a plain stack object of this function and dies before the
	//    app-parented handlers do - drop the pointer.
	// 3. Delete the GUI interface; the Session holds a raw pointer to it.
	// 4. Destroy the Session, then flush and close the log explicitly: the
	//    Session does not own the Log, so the engine can write until it is
	//    torn down, and the SQLite connection is closed only afterwards.
	//    This matters because the Wayland path below ends the process with
	//    std::_Exit(), which skips all destructors.
	gameHandler->setSession(boost::shared_ptr<Session>());
	gameHandler->setWallet(nullptr);
	delete guiInterface; // releases guiInterface's raw pointer into the session
	session.reset();     // ~Session() runs here (the Log stays alive)
	log->flushLog();
	log.reset();         // ~Log(): the SQLite connection is closed here

	// A Wayland workaround against the reproducible shutdown crash: the
	// QQmlApplicationEngine destructor (the stack unwind at the end of main) tears the
	// QtQuickLayouts items down; on Wayland an item dereferences an
	// already half destroyed window in the process → a SIGSEGV in QQuickWindowPrivate::cleanup
	// (not reliably avoidable with a basic render loop and hide() either).
	// Since all persistence is already done – the config XML eagerly via
	// writeBuffer(), the log SQL flushed and closed above, and the QtQuick settings
	// (Parameters.qml) we save explicitly to disk here – we end
	// the process hard, without running the faulty QML/Qt teardown.
	// A closing marker: if it is missing at the end of a log section, the session
	// was NOT ended cleanly (a crash, a kill, a power failure) – without it, it is
	// impossible to tell afterwards whether the client crashed or the
	// player closed it. It deliberately stands here, after the whole
	// teardown; if something does crash after that, the crash handler writes
	// its [CRASH] line below this line.
	loghelper_write_raw("[SHUTDOWN] clean exit (code " + std::to_string(result) + ")");

	if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
		QSettings().sync();   // QtQuick-Settings (gemeinsamer QConfFile-Store) flushen
		std::cout.flush();
		std::cerr.flush();
		std::_Exit(result);
	}

	return result;
}


#endif
