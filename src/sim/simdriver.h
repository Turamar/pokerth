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

#ifndef SIMDRIVER_H
#define SIMDRIVER_H

#include <string>
#include <vector>

class ConfigFile;

/* Parameters of a headless run. The defaults describe a small tournament: six
 * seats, 10 BB deep at the start, blinds doubling every eight hands. */
struct SimOptions
{
	SimOptions()
		: players(6), startCash(1000), smallBlind(10), raiseEveryHands(8), maxHands(200), seed(0) {}

	int players;
	int startCash;
	int smallBlind;
	// 0 switches the blind increase off: the run then plays a cash table.
	int raiseEveryHands;
	// Upper bound for the run: it stops earlier as soon as one seat holds all
	// the chips.
	int maxHands;
	// 0 means "pick a random seed and report it", anything else is reproducible.
	unsigned long long seed;
};

/* What one seat did over the run. The engine collects the opponent statistics
 * itself (PlayerStatistics), wins and chips are counted by the driver. */
struct SimSeatStats
{
	SimSeatStats()
		: seat(0), hands(0), vpip(0), pfr(0), aggressive(0), passive(0), handsWon(0),
		  chipsWon(0), chips(0), bustedAtHand(-1) {}

	int seat;
	std::string name;
	long long hands;      // hands in which this seat had a decision
	long long vpip;       // of those: voluntarily put money in (preflop)
	long long pfr;        // of those: raised preflop
	long long aggressive; // bets and raises after the flop
	long long passive;    // calls after the flop
	long long handsWon;
	long long chipsWon;
	long long chips;      // stack at the end of the run
	int bustedAtHand;     // hand in which the stack went to zero, -1 if never
};

struct SimReport
{
	SimReport()
		: handsPlayed(0), playersLeft(0), steps(0), elapsedMs(0), seed(0), finished(false) {}

	std::vector<SimSeatStats> seats;
	int handsPlayed;
	int playersLeft;
	long long steps;       // engine steps executed (one per game loop callback)
	long long elapsedMs;
	unsigned long long seed;
	bool finished;         // stopped because a single seat held all chips
	std::string winner;
};

/* Play a complete game headlessly. Returns false and fills error on a failed
 * run; the caller decides what to do with the report. */
bool runSimulation(ConfigFile *config, const SimOptions &options, SimReport &report, std::string &error);

#endif
