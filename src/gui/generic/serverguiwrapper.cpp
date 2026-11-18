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
#include "serverguiwrapper.h"


using namespace std;


ServerGuiWrapper::ServerGuiWrapper(ConfigFile *config)
	: myConfig(config)
{
}

ServerGuiWrapper::~ServerGuiWrapper()
{
}

void ServerGuiWrapper::initGui(int /*speed*/) {}

void ServerGuiWrapper::refreshSet() const {}
void ServerGuiWrapper::refreshCash() const {}
void ServerGuiWrapper::refreshAction(int /*playerID*/, int /*playerAction*/) const {}
void ServerGuiWrapper::refreshChangePlayer() const {}
void ServerGuiWrapper::refreshAll() const {}
void ServerGuiWrapper::refreshPot() const {}
void ServerGuiWrapper::refreshGroupbox(int /*playerID*/, int /*status*/) const {}
void ServerGuiWrapper::refreshPlayerName() const {}
void ServerGuiWrapper::refreshButton() const {}
void ServerGuiWrapper::refreshGameLabels(GameState /*state*/) const {}
void ServerGuiWrapper::setPlayerAvatar(int /*myUniqueID*/, const std::string &/*myAvatar*/) const {};
void ServerGuiWrapper::waitForGuiUpdateDone() const {}
void ServerGuiWrapper::dealBeRoCards(int /*myBeRoID*/) {}
void ServerGuiWrapper::dealHoleCards() {}
void ServerGuiWrapper::dealFlopCards() {}
void ServerGuiWrapper::dealTurnCard() {}
void ServerGuiWrapper::dealRiverCard() {}
void ServerGuiWrapper::nextPlayerAnimation() {}
void ServerGuiWrapper::beRoAnimation2(int /*myBeRoID*/) {}
void ServerGuiWrapper::preflopAnimation1() {}
void ServerGuiWrapper::preflopAnimation2() {}
void ServerGuiWrapper::flopAnimation1() {}
void ServerGuiWrapper::flopAnimation2() {}
void ServerGuiWrapper::turnAnimation1() {}
void ServerGuiWrapper::turnAnimation2() {}
void ServerGuiWrapper::riverAnimation1() {}
void ServerGuiWrapper::riverAnimation2() {}
void ServerGuiWrapper::postRiverAnimation1() {}
void ServerGuiWrapper::postRiverRunAnimation1() {}
void ServerGuiWrapper::flipHolecardsAllIn() {}
void ServerGuiWrapper::nextRoundCleanGui() {}
void ServerGuiWrapper::prepareForNewHand() {}
void ServerGuiWrapper::meInAction() {}
void ServerGuiWrapper::disableMyButtons() {}
void ServerGuiWrapper::updateMyButtonsState() {}
void ServerGuiWrapper::startTimeoutAnimation(int /*playerNum*/, int /*timeoutSec*/) {}
void ServerGuiWrapper::stopTimeoutAnimation(int /*playerNum*/) {}
void ServerGuiWrapper::startVoteOnKick(unsigned /*playerId*/, unsigned /*voteStarterPlayerId*/, int /*timeoutSec*/, int /*numVotesNeededToKick*/) {}
void ServerGuiWrapper::changeVoteOnKickButtonsState(bool /*showHide*/) {}
void ServerGuiWrapper::refreshVotesMonitor(int /*currentVotes*/, int /*numVotesNeededToKick*/) {}
void ServerGuiWrapper::endVoteOnKick() {}
void ServerGuiWrapper::logPlayerActionMsg(string /*playerName*/, int /*action*/, int /*setValue*/) {}
void ServerGuiWrapper::logNewGameHandMsg(int /*gameID*/, int /*handID*/) {}
void ServerGuiWrapper::logPlayerWinsMsg(std::string /*playerName*/, int /*pot*/, bool /*main*/) {}
void ServerGuiWrapper::logPlayerSitsOut(std::string /*playerName*/) {}
void ServerGuiWrapper::logNewBlindsSetsMsg(int /*sbSet*/, int /*bbSet*/, std::string /*sbName*/, std::string /*bbName*/) {}
void ServerGuiWrapper::logDealBoardCardsMsg(int /*roundID*/, int /*card1*/, int /*card2*/, int /*card3*/, int /*card4*/, int /*card5*/) {}
void ServerGuiWrapper::logFlipHoleCardsMsg(std::string /*playerName*/, int /*card1*/, int /*card2*/, int /*cardsValueInt*/, std::string /*showHas*/) {}
void ServerGuiWrapper::logPlayerWinGame(std::string /*playerName*/, int /*gameID*/) {}
void ServerGuiWrapper::flushLogAtGame(int /*gameID*/) {}
void ServerGuiWrapper::flushLogAtHand() {}
