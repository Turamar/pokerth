/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "qmlguiinterface.h"
#include "gamehandler.h"
#include "configfile.h"
#include <game.h>
#include <gamedata.h>
#include <game_defs.h>
#include <cardsvalue.h>
#include <QString>
#include <QChar>
#include <QMetaObject>
#include <QTimer>

namespace
{
// Card code (0-51) → a short form with a Unicode suit symbol, e.g. "K♥".
//   0-12 diamonds(♦), 13-25 hearts(♥), 26-38 spades(♠), 39-51 clubs(♣); rank 2..A.
QString fmtCard(int code)
{
	if (code < 0 || code > 51)
		return QStringLiteral("?");
	static const char *ranks[] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
	static const QChar suits[] = { QChar(0x2666), QChar(0x2665), QChar(0x2660), QChar(0x2663) };
	return QString::fromLatin1(ranks[code % 13]) + QString(suits[code / 13]);
}
} // namespace

QmlGuiInterface::QmlGuiInterface(ConfigFile *config)
	: m_config(config)
{
}

QmlGuiInterface::~QmlGuiInterface()
{
}

void QmlGuiInterface::refreshSet() const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshSet", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshCash() const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshCash", Qt::QueuedConnection);
	}
}

// The active player / the player action has changed → update the player data,
// so that the yellow "to act" frame and the action display (fold/call/…) follow live.
void QmlGuiInterface::refreshGroupbox(int /*playerId*/, int /*state*/) const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshSet", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshAction(int playerId, int action) const
{
	// IMPORTANT: only refresh the GUI here, do NOT play an action sound.
	// The action sound comes exclusively via logPlayerActionMsg() (which fires
	// exactly once per action, in local AND network games). In a network game
	// the engine calls both refreshAction(id, action) and
	// logPlayerActionMsg() per action – if both played the sound, you would hear it
	// twice (slightly offset). Locally refreshAction plays no sound anyway
	// (the action = PLAYER_ACTION_NONE). onRefreshSet = the same refresh as
	// onRefreshAction, only without the sound.
	Q_UNUSED(playerId);
	Q_UNUSED(action);
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshSet", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshChangePlayer() const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshSet", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshPlayerName() const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshPlayerName", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshPot() const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshPot", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshGameLabels(GameState state) const
{
	if (m_gameHandler) {
		int stateInt = static_cast<int>(state);
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshGameLabels", Qt::QueuedConnection,
								  Q_ARG(int, stateInt));
	}
}

void QmlGuiInterface::meInAction()
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onMeInAction", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::disableMyButtons()
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onDisableMyButtons", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::startTimeoutAnimation(int playerNum, int timeoutSec)
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onStartTimeoutAnimation", Qt::QueuedConnection,
								  Q_ARG(int, playerNum), Q_ARG(int, timeoutSec));
	}
}

void QmlGuiInterface::stopTimeoutAnimation(int playerNum)
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onStopTimeoutAnimation", Qt::QueuedConnection,
								  Q_ARG(int, playerNum));
	}
}

void QmlGuiInterface::logPlayerActionMsg(std::string playName, int action, int setValue)
{
	if (m_gameHandler && action > 0) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshAction", Qt::QueuedConnection,
								  Q_ARG(int, 0), Q_ARG(int, action));

		// Spielverlauf-Text (analog guiLog::logPlayerActionMsg).
		const QString name = QString::fromStdString(playName);
		QString msg = name;
		switch (action) {
		case 1:
			msg += QStringLiteral(" folds.");
			break;
		case 2:
			msg += QStringLiteral(" checks.");
			break;
		case 3:
			msg += QStringLiteral(" calls $") + QString::number(setValue) + ".";
			break;
		case 4:
			msg += QStringLiteral(" bets $") + QString::number(setValue) + ".";
			break;
		case 5:
			msg += QStringLiteral(" bets $") + QString::number(setValue) + ".";
			break;
		case 6:
			msg += QStringLiteral(" is all in with $") + QString::number(setValue) + ".";
			break;
		default:
			msg.clear();
		}
		if (!msg.isEmpty())
			QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
									  Q_ARG(QString, msg), Q_ARG(int, GameHandler::LogNormal));
	}
}

void QmlGuiInterface::logNewBlindsSetsMsg(int sbSet, int bbSet, std::string sbName, std::string bbName)
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onBlindsSet", Qt::QueuedConnection,
								  Q_ARG(int, sbSet));

		const QString sb = QString::fromStdString(sbName) + " posts small blind ($" + QString::number(sbSet) + ")";
		const QString bb = QString::fromStdString(bbName) + " posts big blind ($" + QString::number(bbSet) + ")";
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, sb), Q_ARG(int, GameHandler::LogNormal));
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, bb), Q_ARG(int, GameHandler::LogNormal));
	}
}

void QmlGuiInterface::logNewGameHandMsg(int gameID, int handID)
{
	if (m_gameHandler) {
		// The wording 1:1 as in guiLog::logNewGameHandMsg in the Qt widgets client.
		const QString msg = QStringLiteral("## Game: ") + QString::number(gameID)
							+ QStringLiteral(" | Hand: ") + QString::number(handID) + QStringLiteral(" ##");
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, msg), Q_ARG(int, GameHandler::LogHeader));
	}
}

void QmlGuiInterface::logPlayerWinsMsg(std::string playerName, int pot, bool main)
{
	if (m_gameHandler) {
		QString msg = QString::fromStdString(playerName) + " wins $" + QString::number(pot);
		if (!main)
			msg += QStringLiteral(" (side pot)");
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, msg),
								  Q_ARG(int, main ? GameHandler::LogWinnerMain : GameHandler::LogWinnerSide));
	}
}

void QmlGuiInterface::logPlayerSitsOut(std::string playerName)
{
	if (m_gameHandler) {
		const QString msg = QString::fromStdString(playerName) + " sits out";
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, msg), Q_ARG(int, GameHandler::LogSitOut));
	}
}

void QmlGuiInterface::logDealBoardCardsMsg(int roundID, int card1, int card2, int card3, int card4, int card5)
{
	if (!m_gameHandler)
		return;
	// The number of cards is – as in the widgets client (guiLog::logDealBoardCardsMsg) –
	// determined exclusively by the round ID (flop=3, turn=4, river=5).
	// Cards that have not been dealt come in as 0 (not -1), a value check
	// would wrongly insert them as 2♦.
	QString round;
	QStringList cards;
	switch (roundID) {
	case 1:
		round = QStringLiteral("Flop");
		cards << fmtCard(card1) << fmtCard(card2) << fmtCard(card3);
		break;
	case 2:
		round = QStringLiteral("Turn");
		cards << fmtCard(card1) << fmtCard(card2) << fmtCard(card3) << fmtCard(card4);
		break;
	case 3:
		round = QStringLiteral("River");
		cards << fmtCard(card1) << fmtCard(card2) << fmtCard(card3) << fmtCard(card4) << fmtCard(card5);
		break;
	default:
		// Other round IDs (post-river during an all-in runout) log the
		// full board again – redundant with the river line → do not show it.
		return;
	}
	const QString msg = "--- " + round + " --- [" + cards.join(", ") + "]";
	QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
							  Q_ARG(QString, msg), Q_ARG(int, GameHandler::LogBoard));
}

void QmlGuiInterface::logFlipHoleCardsMsg(std::string playerName, int card1, int card2, int cardsValueInt, std::string showHas)
{
	if (m_gameHandler) {
		QString msg = QString::fromStdString(playerName) + " " + QString::fromStdString(showHas)
					  + " [" + fmtCard(card1) + ", " + fmtCard(card2) + "]";
		// Append the hand name (as in guiLog::logFlipHoleCardsMsg), e.g. - "Straight, six high".
		const boost::shared_ptr<Game> game = m_gameHandler->game();
		if (cardsValueInt != -1 && game) {
			const std::string handName =
				CardsValue::determineHandName(cardsValueInt, game->getActivePlayerList());
			if (!handName.empty())
				msg += " - \"" + QString::fromStdString(handName) + "\"";
		}
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, msg), Q_ARG(int, GameHandler::LogNormal));
	}
}

void QmlGuiInterface::logPlayerWinGame(std::string playerName, int gameID)
{
	if (m_gameHandler) {
		const QString msg = QString::fromStdString(playerName) + " wins game " + QString::number(gameID) + "!";
		QMetaObject::invokeMethod(m_gameHandler, "appendGameLog", Qt::QueuedConnection,
								  Q_ARG(QString, msg), Q_ARG(int, GameHandler::LogGameWin));
	}
}

void QmlGuiInterface::nextRoundCleanGui()
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onNextRoundCleanGui", Qt::QueuedConnection);
	}
}

void QmlGuiInterface::refreshAll() const
{
	if (m_gameHandler) {
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshPlayerName", Qt::QueuedConnection);
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshCash",       Qt::QueuedConnection);
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshSet",        Qt::QueuedConnection);
		QMetaObject::invokeMethod(m_gameHandler, "onRefreshPot",        Qt::QueuedConnection);
	}
}

void QmlGuiInterface::dealFlopCards()
{
	if (m_gameHandler) QMetaObject::invokeMethod(m_gameHandler, "onDealFlopCards", Qt::QueuedConnection);
}

void QmlGuiInterface::dealTurnCard()
{
	if (m_gameHandler) QMetaObject::invokeMethod(m_gameHandler, "onDealTurnCard", Qt::QueuedConnection);
}

void QmlGuiInterface::dealRiverCard()
{
	if (m_gameHandler) QMetaObject::invokeMethod(m_gameHandler, "onDealRiverCard", Qt::QueuedConnection);
}

// ─── Local game-loop animation callbacks ─────────────────────────────────────
// These replicate the timer-driven animation chain in the Qt5 gametableimpl.
// Each "Animation1" starts a new betting round (calls BeRo::run()).
// "beRoAnimation2" advances to the next CPU player (calls BeRo::nextPlayer()).
// "nextPlayerAnimation" processes the end of an action (calls switchRounds()).
// "postRiverAnimation1" distributes the pot; "postRiverRunAnimation1" starts the next hand.

void QmlGuiInterface::nextPlayerAnimation()
{
	// After a player acts: trigger switchRounds() with a short delay
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(300, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onSwitchRounds", Qt::DirectConnection);
	});
}

void QmlGuiInterface::beRoAnimation2(int /*myBeRoID*/)
{
	// CPU player's turn: advance to nextPlayer() after the configured pause.
	// "BotThinkMs" (settings audit, PLAN.md §7) is the only pacing knob of the
	// local table: 150..900 ms, the default is the 300 ms that used to be fixed.
	// Everything else (streets, showdown, the 5.5 s before the next hand) stays
	// hard-coded - the payout animation needs that window.
	int delayMs = 300;
	if (m_config) {
		const int configured = m_config->readConfigInt("BotThinkMs");
		if (configured >= 150 && configured <= 900)
			delayMs = configured;
	}
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(delayMs, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onNextPlayerBeRo", Qt::DirectConnection);
	});
}

void QmlGuiInterface::preflopAnimation1()
{
	// Start of preflop betting: call BeRo::run()
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(300, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onRunBeRo", Qt::DirectConnection);
	});
}

void QmlGuiInterface::flopAnimation1()
{
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(300, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onRunBeRo", Qt::DirectConnection);
	});
}

void QmlGuiInterface::turnAnimation1()
{
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(300, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onRunBeRo", Qt::DirectConnection);
	});
}

void QmlGuiInterface::riverAnimation1()
{
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(300, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onRunBeRo", Qt::DirectConnection);
	});
}

void QmlGuiInterface::flipHolecardsAllIn()
{
	if (!m_gameHandler) return;
	QMetaObject::invokeMethod(m_gameHandler, "onFlipHolecardsAllIn", Qt::QueuedConnection);
}

void QmlGuiInterface::postRiverAnimation1()
{
	// Show-down: call BeRo::postRiverRun() which distributes the pot
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;
	QTimer::singleShot(500, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onPostRiverRunBeRo", Qt::DirectConnection);
	});
}

void QmlGuiInterface::postRiverRunAnimation1()
{
	// Pot already distributed. Show showdown: reveal cards + mark winner.
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;

	// Freeze the fold/reveal state IMMEDIATELY, while the engine data of the
	// hand that has just finished is still valid. onShowdown() below runs
	// QUEUED and the next hand may already have reset the actions by then;
	// without this snapshot, folded players would appear with a rated
	// hand in the showdown/game history.
	QMetaObject::invokeMethod(gh, "captureShowdownSnapshot", Qt::DirectConnection);

	QMetaObject::invokeMethod(gh, "onShowdown", Qt::QueuedConnection);

	// Start the next hand after a pause so the user can see the result
	// (show the winner + the revealed cards + the winning hand a bit longer).
	QTimer::singleShot(5500, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onLocalHandFinished", Qt::DirectConnection);
	});
}

void QmlGuiInterface::dealBeRoCards(int beRoID)
{
	// Called by BeRo::run() on its first invocation to "show" dealing.
	// Reveal board cards for Flop/Turn/River, then trigger the second BeRo::run().
	if (!m_gameHandler) return;
	GameHandler *gh = m_gameHandler;

	// Reveal the appropriate board cards
	if (beRoID == GAME_STATE_FLOP) {
		QMetaObject::invokeMethod(gh, "onDealFlopCards", Qt::QueuedConnection);
	} else if (beRoID == GAME_STATE_TURN) {
		QMetaObject::invokeMethod(gh, "onDealTurnCard", Qt::QueuedConnection);
	} else if (beRoID == GAME_STATE_RIVER) {
		QMetaObject::invokeMethod(gh, "onDealRiverCard", Qt::QueuedConnection);
	}

	// After the reveal, continue the round. In an all-in condition this advances
	// to the next street/showdown (no betting); otherwise it starts the betting.
	QTimer::singleShot(300, gh, [gh]() {
		QMetaObject::invokeMethod(gh, "onAfterDealCards", Qt::DirectConnection);
	});
}
