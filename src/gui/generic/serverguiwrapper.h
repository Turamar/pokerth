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
#ifndef SERVERGUIWRAPPER_H
#define SERVERGUIWRAPPER_H

#include <guiinterface.h>

class ConfigFile;

/* A GUI-less GuiInterface.
 *
 * Every refresh/animation/log callback is a no-op - nothing is shown and
 * nothing is forwarded anywhere. SilentGui derives from it and replaces only
 * the steps of the local game loop, so that a table nobody watches still plays
 * its hands (tournament runner, headless simulator).
 *
 * It used to be the GUI of the dedicated server and therefore carried the
 * network client/server callback surface; the server is gone and this
 * class is local-only now.
 */
class ServerGuiWrapper : public GuiInterface
{
public:
	explicit ServerGuiWrapper(ConfigFile *config);
	~ServerGuiWrapper();

	void initGui(int speed);

	void refreshSet() const;
	void refreshCash() const;
	void refreshAction(int =-1, int =-1) const;
	void refreshChangePlayer() const;
	void refreshPot() const;
	void refreshGroupbox(int =-1, int =-1) const;
	void refreshAll() const;
	void refreshPlayerName() const;
	void refreshButton() const;
	void refreshGameLabels(GameState state) const;

	void setPlayerAvatar(int myUniqueID, const std::string &myAvatar) const;

	void waitForGuiUpdateDone() const;

	void dealBeRoCards(int);
	void dealHoleCards();
	void dealFlopCards();
	void dealTurnCard();
	void dealRiverCard();

	void nextPlayerAnimation();

	void beRoAnimation2(int);

	void preflopAnimation1();
	void preflopAnimation2();

	void flopAnimation1();
	void flopAnimation2();

	void turnAnimation1();
	void turnAnimation2();

	void riverAnimation1();
	void riverAnimation2();

	void postRiverAnimation1();
	void postRiverRunAnimation1();

	void flipHolecardsAllIn();

	void nextRoundCleanGui();
	void prepareForNewHand();

	void meInAction();
	void updateMyButtonsState();
	void disableMyButtons();
	void startTimeoutAnimation(int playerNum, int timeoutSec);
	void stopTimeoutAnimation(int playerNum);

	void startVoteOnKick(unsigned playerId, unsigned voteStarterPlayerId, int timeoutSec, int numVotesNeededToKick);
	void changeVoteOnKickButtonsState(bool showHide);
	void refreshVotesMonitor(int currentVotes, int numVotesNeededToKick);
	void endVoteOnKick();

	void logPlayerActionMsg(std::string playerName, int action, int setValue) ;
	void logNewGameHandMsg(int gameID, int handID) ;
	void logPlayerWinsMsg(std::string playerName, int pot, bool main);
	void logPlayerSitsOut(std::string playerName);
	void logNewBlindsSetsMsg(int sbSet, int bbSet, std::string sbName, std::string bbName);
	void logDealBoardCardsMsg(int roundID, int card1, int card2, int card3, int card4 = -1, int card5 = -1) ;
	void logFlipHoleCardsMsg(std::string playerName, int card1, int card2, int cardsValueInt = -1, std::string showHas = "shows") ;
	void logPlayerWinGame(std::string playerName, int gameID);
	void flushLogAtGame(int gameID);
	void flushLogAtHand();

private:

	ConfigFile *myConfig;
};

#endif
