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
/* Game data. */

#ifndef _GAMEDATA_H_
#define _GAMEDATA_H_

#include <list>
#include <string>
#include <vector>
typedef std::list<unsigned> PlayerIdList;

enum GameType {
	GAME_TYPE_NORMAL = 1,
	GAME_TYPE_REGISTERED_ONLY,
	GAME_TYPE_INVITE_ONLY,
	GAME_TYPE_RANKING
};

enum RaiseIntervalMode {
	RAISE_ON_HANDNUMBER = 1,
	RAISE_ON_MINUTES
};

enum RaiseMode {
	DOUBLE_BLINDS = 1,
	MANUAL_BLINDS_ORDER
};

enum AfterManualBlindsMode {
	AFTERMB_DOUBLE_BLINDS = 1,
	AFTERMB_RAISE_ABOUT,
	AFTERMB_STAY_AT_LAST_BLIND
};

// For the sake of simplicity, this is a struct.
struct GameData {
	GameData() : gameType(GAME_TYPE_NORMAL), allowSpectators(true),
		maxNumberOfPlayers(0), startMoney(0), firstSmallBlind(0),
		raiseIntervalMode(RAISE_ON_HANDNUMBER), raiseSmallBlindEveryHandsValue(8),
		raiseSmallBlindEveryMinutesValue(1), raiseMode(DOUBLE_BLINDS),
		afterManualBlindsMode(AFTERMB_DOUBLE_BLINDS), afterMBAlwaysRaiseValue(0),
		guiSpeed(4), delayBetweenHandsSec(6), playerActionTimeoutSec(20) {}
	GameType gameType;
	bool allowSpectators;
	int maxNumberOfPlayers;
	int startMoney;
	int firstSmallBlind;
	RaiseIntervalMode raiseIntervalMode;
	int raiseSmallBlindEveryHandsValue;
	int raiseSmallBlindEveryMinutesValue;
	RaiseMode raiseMode;
	std::list<int> manualBlindsList;
	AfterManualBlindsMode afterManualBlindsMode;
	int afterMBAlwaysRaiseValue;
	int guiSpeed;
	int delayBetweenHandsSec;
	int playerActionTimeoutSec;
	/* Local games only: one profile label per bot seat ("pro/tag", see
	 * botprofile.h), the table a tier was dealt into. The labels keep this
	 * header free of the engine; empty means the default mixture. */
	std::vector<std::string> botProfiles;
	/* Local games only: the name and the starting stack of every seat, in seat
	 * order. A tournament seats a table from its roster (state/roster.h), where
	 * a moved player keeps the character and the stack they already had, so the
	 * engine must take both from the caller instead of handing everybody the
	 * same startMoney. Empty means "the configured name" and startMoney. */
	std::vector<std::string> seatNames;
	std::vector<int> seatStacks;
};

struct StartData {
	StartData() : startDealerPlayerId(0), numberOfPlayers(0) {}
	unsigned startDealerPlayerId;
	int numberOfPlayers;
};

#endif

