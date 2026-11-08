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

#ifndef AIENGINE4_H
#define AIENGINE4_H

#include "game_defs.h"

/* AI engine 4 -- the decision logic of the computer players.
 *
 * Deliberately free of game state: the engine gets a snapshot and
 * returns an intention. Booking the chips, the full bet rule and
 * the minimum raise stay in LocalPlayer::evaluation(), where they have always
 * been handled correctly. That makes the engine testable without a table, a
 * board and a player list.
 *
 * What it does differently from the old engines:
 *  - It computes the winning chance against ALL remaining opponents, not against
 *    a single random opponent.
 *  - It knows the pot and compares the chance with the pot odds instead of
 *    querying fixed thresholds.
 *  - It knows its position and plays by position charts before the flop -- as a
 *    percentage of the strongest starting hands, so that a style can express
 *    any range from a nit to a maniac without a hand-written chart.
 *  - It bets in shares of the pot instead of in multiples of the small blind.
 *  - It knows push-or-fold with a short stack.
 */

struct AiSituation {
	// --- cards ---
	int holeCards[2];
	int boardCards[5];
	int boardSize;              // 0 (preflop), 3, 4 or 5
	int round;                  // GAME_STATE_PREFLOP .. GAME_STATE_RIVER

	// --- money, everything in chips ---
	int myCash;                 // chips still in front of me
	int mySet;                  // already bet in this betting round
	int highestSet;             // highest bet of this round
	int minimumRaise;           // smallest permitted raise
	int smallBlind;
	int potBefore;              // collected pot of the earlier rounds
	int setsThisRound;          // sum of the bets of this round

	// --- table ---
	int opponents;              // opponents still in the hand
	int opponentsBehind;        // of those not yet to act -- our position
	int effectiveOpponentCash;  // largest stack among the opponents
	int raisesThisRound;        // number of raises beyond the base bet
	bool amBigBlind;            // we are the big blind (important preflop)

	/* --- opponent model ---
	 * Observation values of the opponents still in the hand. Each -1 as
	 * long as too few hands are available to derive anything from them; then
	 * the engine falls back to its default assumptions.
	 */
	int opponentVpipPercent;        // how often they enter voluntarily before the flop
	int opponentAggressionPercent;  // share of bets/raises among their actions after the flop
};

/* Player natures. The numbers come from a bot profile (botprofile.h) -- the
 * engine itself knows neither names nor strength levels. All percentages are
 * shares of the starting hands (see PreflopStrength), all multipliers are
 * around 1.0 so that a style shifts the behaviour without undermining the EV
 * basis.
 */
struct AiPersonality {
	// --- preflop ranges ---
	double openPercent[4];       // per position: five or more behind, three or four, one or two, button
	double openRaiseChance;      // share of the opens that raises, the rest limps
	double threeBetPercent;      // value re-raises
	double lightThreeBet;        // chance of a re-raise outside the value range
	double callRaisePercent;     // cold call of a raise, in position
	double callRaisePercentOop;  // cold call of a raise, out of position
	double defendBigBlindPercent;
	double pushPercent;          // short stack: push or fold

	// --- after the flop ---
	double callMargin;           // added to the equity a call needs (> 0 folds more)
	double cbetPercent;          // chance to bet a checked pot, by hand only in part
	double raiseFrequency;       // how often a made hand raises instead of calling
	double raiseBluffPercent;    // chance to raise a hand that would only be called
	double aggression;           // > 1 bets and raises more often and bigger
	double bluffRate;            // > 1 bluffs more often
	double looseness;            // > 1 pays more, holds on longer

	// --- judgement ---
	double thresholdNoise;       // +/- percentage points the bot misjudges its hand by

	AiPersonality()
		: openRaiseChance(0.90), threeBetPercent(4.0), lightThreeBet(0.0),
		  callRaisePercent(16.0), callRaisePercentOop(9.0), defendBigBlindPercent(30.0),
		  pushPercent(18.0), callMargin(0.0), cbetPercent(55.0), raiseFrequency(1.0),
		  raiseBluffPercent(5.0), aggression(1.0), bluffRate(1.0), looseness(1.0),
		  thresholdNoise(0.0)
	{
		openPercent[0] = 12.0;
		openPercent[1] = 18.0;
		openPercent[2] = 28.0;
		openPercent[3] = 45.0;
	}
};

struct AiDecision {
	PlayerAction action;
	// For PLAYER_ACTION_BET the intended bet, for PLAYER_ACTION_RAISE the
	// amount above the highest bet -- exactly what
	// LocalPlayer::evaluation(bet, raise) expects.
	int bet;
	int raise;

	AiDecision() : action(PLAYER_ACTION_NONE), bet(0), raise(0) {}
};

class AiEngine4
{
public:
	static AiDecision decide(const AiSituation& situation, const AiPersonality& personality);

	// Winning chance in the current situation (0..1). Public so that the
	// behaviour of the engine can be followed and logged.
	static double winningChance(const AiSituation& situation);
};

#endif
