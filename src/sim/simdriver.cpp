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

#include "simdriver.h"
#include "simguiinterface.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <sstream>

#include <botprofile.h>
#include <engine_defs.h>
#include <enginefactory.h>
#include <game.h>
#include <game_defs.h>
#include <gamedata.h>
#include <handinterface.h>
#include <localenginefactory.h>
#include <localplayer.h>
#include <playerdata.h>
#include <playerinterface.h>
#include <tools.h>

using namespace std;

namespace
{

/* A step limit per hand: the engine cannot run forever, and a hand that does
 * not terminate is a bug in the loop mapping, not a slow simulation. A normal
 * hand needs a few dozen steps even with ten seats and several all-ins. */
const long long MaxStepsPerHand = 200000;

string seatName(int seat)
{
	ostringstream oss;
	oss << "P" << (seat + 1);
	return oss.str();
}

int playersWithCash(const boost::shared_ptr<Game> &game)
{
	int count = 0;
	PlayerList players = game->getActivePlayerList();
	for(PlayerListConstIterator it = players->begin(); it != players->end(); ++it) {
		if((*it)->getMyCash() > 0) count++;
	}
	return count;
}

/* Which strength and style a seat plays. A list of profiles wins over the single
 * profile; seats beyond the list repeat the last entry.
 */
BotProfile profileForSeat(const SimOptions &options, int seat)
{
	if(options.mixedProfiles) return BotProfile::randomForSeat();
	if(!options.seatProfiles.empty()) {
		const size_t index = std::min(static_cast<size_t>(seat), options.seatProfiles.size() - 1);
		return options.seatProfiles[index];
	}
	return options.profile;
}

// Stack, wins and bust-out bookkeeping for one finished hand.
void collectHandResult(const boost::shared_ptr<Game> &game, SimReport &report)
{
	PlayerList seats = game->getSeatsList();
	for(PlayerListConstIterator it = seats->begin(); it != seats->end(); ++it) {
		const int seat = (*it)->getMyID();
		if(seat < 0 || seat >= static_cast<int>(report.seats.size())) continue;

		SimSeatStats &stats = report.seats[seat];
		stats.chips = (*it)->getMyCash();
		// Busted seats were already taken out of the active list in initHand();
		// their last money won belongs to an older hand and must not be counted
		// twice.
		if((*it)->getMyActiveStatus()) {
			const int won = (*it)->getLastMoneyWon();
			if(won > 0) {
				stats.handsWon++;
				stats.chipsWon += won;
			}
		}
		if(stats.chips <= 0 && stats.bustedAtHand < 0) {
			stats.bustedAtHand = report.handsPlayed;
			stats.busts++;
		}
	}
}

// The opponent statistics the engine maintains for AI engine 4 - exactly the
// numbers that are interesting for calibrating levels and archetypes.
void collectStatistics(const boost::shared_ptr<Game> &game, SimReport &report)
{
	PlayerList seats = game->getSeatsList();
	for(PlayerListConstIterator it = seats->begin(); it != seats->end(); ++it) {
		const int seat = (*it)->getMyID();
		if(seat < 0 || seat >= static_cast<int>(report.seats.size())) continue;

		const PlayerStatistics &statistics = (*it)->getMyStatistics();
		SimSeatStats &stats = report.seats[seat];
		stats.hands = statistics.hands;
		stats.vpip = statistics.voluntary;
		stats.pfr = statistics.preflopRaises;
		stats.threeBets = statistics.threeBets;
		stats.threeBetOpportunities = statistics.threeBetOpportunities;
		stats.aggressive = statistics.aggressive;
		stats.passive = statistics.passive;
		stats.chips = (*it)->getMyCash();
	}
}

}

bool runSimulation(ConfigFile *config, const SimOptions &options, SimReport &report, string &error)
{
	error.clear();
	report = SimReport();

	if(options.players < 2 || options.players > MAX_NUMBER_OF_PLAYERS) {
		ostringstream oss;
		oss << "players must be between 2 and " << MAX_NUMBER_OF_PLAYERS;
		error = oss.str();
		return false;
	}
	if(options.startCash <= 0) {
		error = "start cash must be positive";
		return false;
	}
	if(options.smallBlind <= 0) {
		error = "small blind must be positive";
		return false;
	}
	if(options.maxHands <= 0) {
		error = "hands must be positive";
		return false;
	}

	report.seed = options.seed;
	if(options.seed != 0) {
		Tools::SetRandomSeed(options.seed);
	}

	GameData gameData;
	gameData.gameType = GAME_TYPE_NORMAL;
	gameData.allowSpectators = false;
	gameData.maxNumberOfPlayers = options.players;
	gameData.startMoney = options.startCash;
	gameData.firstSmallBlind = options.smallBlind;
	gameData.raiseIntervalMode = RAISE_ON_HANDNUMBER;
	// A value of 0 is not "raise every hand" but "never raise": the run then
	// behaves like a cash table with fixed blinds.
	gameData.raiseSmallBlindEveryHandsValue = options.raiseEveryHands > 0
			? options.raiseEveryHands
			: options.maxHands + 1;
	gameData.raiseMode = DOUBLE_BLINDS;
	gameData.afterManualBlindsMode = AFTERMB_DOUBLE_BLINDS;
	gameData.afterMBAlwaysRaiseValue = 0;
	gameData.guiSpeed = 4;
	gameData.delayBetweenHandsSec = 0;
	gameData.playerActionTimeoutSec = 0;

	StartData startData;
	startData.numberOfPlayers = options.players;
	startData.startDealerPlayerId = 0;

	PlayerDataList playerDataList;
	for(int i = 0; i < options.players; i++) {
		boost::shared_ptr<PlayerData> playerData(new PlayerData(
					i, i, PLAYER_TYPE_COMPUTER, PLAYER_RIGHTS_NORMAL, i == 0));
		playerData->SetName(seatName(i));
		playerDataList.push_back(playerData);
	}

	SimGui gui(config);
	boost::shared_ptr<EngineFactory> factory(new LocalEngineFactory(config));

	boost::shared_ptr<Game> game;
	try {
		game.reset(new Game(&gui, factory, playerDataList, gameData, startData, 1, NULL));
	} catch(const exception &e) {
		error = string("creating the game failed: ") + e.what();
		return false;
	}
	gui.setGame(game.get());

	report.seats.resize(options.players);
	for(int i = 0; i < options.players; i++) {
		report.seats[i].seat = i;
		report.seats[i].name = seatName(i);
		report.seats[i].chips = options.startCash;
	}

	/* Hand out strength and style. The simulator can play one profile at every
	 * seat -- that is how a corridor is measured -- or leave the choice to the
	 * mixture the client uses.
	 */
	{
		PlayerList seats = game->getSeatsList();
		for(PlayerListConstIterator it = seats->begin(); it != seats->end(); ++it) {
			const int seat = (*it)->getMyID();
			if(seat < 0 || seat >= static_cast<int>(report.seats.size())) continue;

			LocalPlayer *player = dynamic_cast<LocalPlayer*>((*it).get());
			if(!player) continue;

			const BotProfile profile = profileForSeat(options, seat);

			AiPersonality knobs = profile.personality();
			for(int i = 0; i < 4; ++i) knobs.openPercent[i] *= options.openScale;
			knobs.threeBetPercent *= options.threeBetScale;
			knobs.lightThreeBet *= options.threeBetScale;
			knobs.callRaisePercent *= options.callScale;
			knobs.callRaisePercentOop *= options.callScale;
			knobs.defendBigBlindPercent *= options.callScale;
			knobs.raiseFrequency *= options.raiseScale;
			knobs.cbetPercent *= options.cbetScale;
			if(options.overrideCallMargin) knobs.callMargin = options.callMargin;
			player->setBotPersonality(knobs);

			report.seats[seat].profile = profile.label();
		}
	}

	bool handFinished = false;
	gui.onHandFinished = [&handFinished]() {
		handFinished = true;
	};

	const chrono::steady_clock::time_point started = chrono::steady_clock::now();

	for(int hand = 1; hand <= options.maxHands; hand++) {
		if(playersWithCash(game) < 2) break;

		try {
			game->initHand();
			game->startHand();
		} catch(const exception &e) {
			ostringstream oss;
			oss << "hand " << hand << ": starting the hand failed: " << e.what();
			error = oss.str();
			return false;
		}

		handFinished = false;
		long long steps = 0;
		// Every pump() runs one engine step, which may queue further steps.
		// A LocalException from the engine ends the run with a readable message
		// instead of an abort: in a headless run there is no GUI to show it.
		try {
			while(gui.pump()) {
				if(++steps > MaxStepsPerHand) {
					ostringstream oss;
					oss << "hand " << hand << ": more than " << MaxStepsPerHand
						<< " engine steps without the pot being distributed";
					error = oss.str();
					return false;
				}
				report.steps++;
			}
		} catch(const exception &e) {
			ostringstream oss;
			oss << "hand " << hand << ": engine error: " << e.what();
			error = oss.str();
			return false;
		}
		if(!handFinished) {
			ostringstream oss;
			oss << "hand " << hand << ": the engine went idle before the pot was distributed";
			error = oss.str();
			return false;
		}

		report.handsPlayed = hand;
		collectHandResult(game, report);
	}

	const chrono::steady_clock::time_point finished = chrono::steady_clock::now();
	report.elapsedMs = chrono::duration_cast<chrono::milliseconds>(finished - started).count();

	collectStatistics(game, report);

	// Net chips of the game: the stack at the end minus the buy-in. Summed over
	// several games it is the number that says whether a style wins.
	for(size_t i = 0; i < report.seats.size(); ++i) {
		report.seats[i].netChips = report.seats[i].chips - options.startCash;
	}

	report.playersLeft = playersWithCash(game);
	report.finished = report.playersLeft == 1;
	if(report.finished) {
		PlayerList players = game->getActivePlayerList();
		for(PlayerListConstIterator it = players->begin(); it != players->end(); ++it) {
			if((*it)->getMyCash() > 0) {
				report.winner = (*it)->getMyName();
				break;
			}
		}
	}

	if(options.seed != 0) {
		Tools::ClearRandomSeed();
	}

	return true;
}
