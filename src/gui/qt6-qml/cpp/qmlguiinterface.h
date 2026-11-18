/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef QMLGUIINTERFACE_H
#define QMLGUIINTERFACE_H

#include <guiinterface.h>

class ConfigFile;
class GameHandler;

/* The GuiInterface of the QML client.
 *
 * It is local-only: every callback of the local engine is forwarded to the
 * GameHandler through the Qt event loop (queued) or as a single-shot timer, and
 * the GameHandler does the actual work on the QML side. The network client
 * surface it used to implement (the network client/server callbacks, session access,
 * lobby/connection handlers) is gone with the network layer.
 */
class QmlGuiInterface : public GuiInterface
{
public:
	explicit QmlGuiInterface(ConfigFile *config);
	virtual ~QmlGuiInterface();

	void setGameHandler(GameHandler *gameHandler)
	{
		m_gameHandler = gameHandler;
	}

	// GuiInterface implementation - required methods
	virtual void initGui(int speed) override {}
	virtual void refreshSet() const override;
	virtual void refreshCash() const override;
	virtual void refreshAction(int = -1, int = -1) const override;
	virtual void refreshChangePlayer() const override;
	virtual void refreshPot() const override;
	virtual void refreshGroupbox(int = -1, int = -1) const override;
	virtual void refreshAll() const override;
	virtual void refreshPlayerName() const override;
	virtual void refreshButton() const override {}
	virtual void refreshGameLabels(GameState state) const override;

	virtual void setPlayerAvatar(int myUniqueID, const std::string &myAvatar) const override {}
	virtual void waitForGuiUpdateDone() const override {}

	// Card functions
	virtual void dealBeRoCards(int beRoID) override;
	virtual void dealHoleCards() override {}
	virtual void dealFlopCards() override;
	virtual void dealTurnCard() override;
	virtual void dealRiverCard() override;
	virtual void nextPlayerAnimation() override;
	virtual void beRoAnimation2(int myBeRoID) override;
	virtual void preflopAnimation1() override;
	virtual void preflopAnimation2() override {}
	virtual void flopAnimation1() override;
	virtual void flopAnimation2() override {}
	virtual void turnAnimation1() override;
	virtual void turnAnimation2() override {}
	virtual void riverAnimation1() override;
	virtual void riverAnimation2() override {}
	virtual void postRiverAnimation1() override;
	virtual void postRiverRunAnimation1() override;
	virtual void flipHolecardsAllIn() override;
	virtual void nextRoundCleanGui() override;
	virtual void meInAction() override;
	virtual void updateMyButtonsState() override {}
	virtual void disableMyButtons() override;
	virtual void startTimeoutAnimation(int playerNum, int timeoutSec) override;
	virtual void stopTimeoutAnimation(int playerNum) override;

	virtual void startVoteOnKick(unsigned playerId, unsigned voteStarterPlayerId, int timeoutSec, int numVotesNeededToKick) override {}
	virtual void changeVoteOnKickButtonsState(bool showHide) override {}
	virtual void refreshVotesMonitor(int currentVotes, int numVotesNeededToKick) override {}
	virtual void endVoteOnKick() override {}

	// Log functions
	virtual void logPlayerActionMsg(std::string playName, int action, int setValue) override;
	virtual void logNewGameHandMsg(int gameID, int HandID) override;
	virtual void logPlayerWinsMsg(std::string playerName, int pot, bool main) override;
	virtual void logPlayerSitsOut(std::string playerName) override;
	virtual void logNewBlindsSetsMsg(int sbSet, int bbSet, std::string sbName, std::string bbName) override;
	virtual void logDealBoardCardsMsg(int roundID, int card1, int card2, int card3, int card4 = -1, int card5 = -1) override;
	virtual void logFlipHoleCardsMsg(std::string playerName, int card1, int card2, int cardsValueInt = -1, std::string showHas = "shows") override;
	virtual void logPlayerWinGame(std::string playerName, int gameID) override;
	virtual void flushLogAtGame(int gameID) override {}
	virtual void flushLogAtHand() override {}

private:
	ConfigFile *m_config;
	GameHandler *m_gameHandler = nullptr;
};

#endif // QMLGUIINTERFACE_H
