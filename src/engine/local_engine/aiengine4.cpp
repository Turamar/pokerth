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

#include "aiengine4.h"

#include "equitycalculator.h"
#include "holecardsrange.h"
#include "preflopstrength.h"
#include "tools.h"

#include <algorithm>
#include <cmath>

namespace
{

// ---------------------------------------------------------------------------
// Opponent ranges
// ---------------------------------------------------------------------------


/* The assumption about an opponent's hands, graded by the observed entering
 * behaviour. Whoever plays only every eighth hand holds something different when
 * calling than somebody who plays every second one -- the old engine assumes
 * the same random hand for both.
 */
const char* const OPPONENT_RANGE_VERY_TIGHT = "77+, ATs+, KJs+, QJs, AJo+, KQo";
const char* const OPPONENT_RANGE_MEDIUM     = "22+, A2s+, K8s+, Q9s+, J9s+, T9s, 98s, "
		"A9o+, KTo+, QTo+, JTo";

// The assumption about the hands an opponent can still hold after the flop.
const char* const OPPONENT_RANGE_RAISED_POT = "44+, A8s+, KTs+, QTs+, JTs, ATo+, KJo+, QJo";
const char* const OPPONENT_RANGE_LIMPED_POT = "22+, A2s+, K5s+, Q7s+, J7s+, T7s+, 96s+, 86s+, 75s+, 64s+, 54s, "
		"A2o+, K8o+, Q9o+, J9o+, T9o, 98o";

// Build the ranges once and reuse them -- parsing them per decision would be
// unnecessary work. Thread-safe via magic statics.
const HoleCardsRange& cachedRange(const char* notation)
{
	// A small cache is enough, there are only a handful of fixed charts.
	static thread_local const char* keys[16] = { 0 };
	static thread_local HoleCardsRange values[16];
	for(int i = 0; i < 16; ++i) {
		if(keys[i] == notation) return values[i];
		if(keys[i] == 0) {
			values[i] = HoleCardsRange(notation);
			keys[i] = notation;
			return values[i];
		}
	}
	// Cache full (does not happen with fixed charts): build it without the cache.
	static thread_local HoleCardsRange fallback;
	fallback = HoleCardsRange(notation);
	return fallback;
}

int randomPercent()
{
	int value;
	Tools::GetRand(1, 100, 1, &value);
	return value;
}

// ---------------------------------------------------------------------------
// Key figures of the situation
// ---------------------------------------------------------------------------

struct Metrics {
	int bigBlind;
	int toCall;         // what a call costs (already limited to the stack)
	int pot;            // everything that currently lies in the pot
	double potOdds;     // the share needed for a call to pay off
	double stackInBB;   // our own stack in big blinds
	bool canCheck;
};

Metrics measure(const AiSituation& s)
{
	Metrics m;
	m.bigBlind = std::max(1, 2 * s.smallBlind);
	m.toCall = std::max(0, std::min(s.highestSet - s.mySet, s.myCash));
	m.pot = std::max(0, s.potBefore + s.setsThisRound);
	m.potOdds = (m.pot + m.toCall) > 0 ? static_cast<double>(m.toCall) / (m.pot + m.toCall) : 0.0;
	m.stackInBB = static_cast<double>(s.myCash + s.mySet) / m.bigBlind;
	m.canCheck = (m.toCall == 0);
	return m;
}

// Position: we are late if few players sit behind us.
bool inPosition(const AiSituation& s)
{
	return s.opponentsBehind == 0;
}

/* Position as one of four buckets: the more players still sit behind us, the
 * tighter we have to be. The profile holds one opening percentage per bucket.
 */
int positionBucket(const AiSituation& s)
{
	if(s.opponentsBehind >= 5) return 0;
	if(s.opponentsBehind >= 3) return 1;
	if(s.opponentsBehind >= 1) return 2;
	return 3;
}

}

// ---------------------------------------------------------------------------
// Gewinnchance
// ---------------------------------------------------------------------------

double AiEngine4::winningChance(const AiSituation& s)
{
	if(s.opponents < 1) return 1.0;

	const Metrics m = measure(s);

	/* Which hands do we credit the opponents with? A large pot before the flop
	 * means that somebody has raised -- then the selection is tighter. If, on the
	 * other hand, many players are in the hand, not everybody can possibly hold a
	 * premium hand, so we compute wider again.
	 */
	const char* rangeNotation;
	if(s.opponentVpipPercent >= 0) {
		// Beobachtung schlaegt Annahme.
		if(s.opponentVpipPercent < 18) rangeNotation = OPPONENT_RANGE_VERY_TIGHT;
		else if(s.opponentVpipPercent < 30) rangeNotation = OPPONENT_RANGE_RAISED_POT;
		else if(s.opponentVpipPercent < 45) rangeNotation = OPPONENT_RANGE_MEDIUM;
		else rangeNotation = OPPONENT_RANGE_LIMPED_POT;
	} else {
		// Without data the pot size stays the best indication.
		const double potInBigBlinds = static_cast<double>(m.pot) / m.bigBlind;
		const bool raisedPot = potInBigBlinds > 6.0 && s.opponents <= 3;
		rangeNotation = raisedPot ? OPPONENT_RANGE_RAISED_POT : OPPONENT_RANGE_LIMPED_POT;
	}
	const HoleCardsRange& opponentRange = cachedRange(rangeNotation);

	/* Simulation effort by round. Before the flop five board cards are still
	 * to come, which scatters the most; at the river a few runs are enough.
	 */
	int samples;
	switch(s.round) {
	case GAME_STATE_PREFLOP:
		samples = 6000;
		break;
	case GAME_STATE_FLOP:
		samples = 8000;
		break;
	case GAME_STATE_TURN:
		samples = 6000;
		break;
	default:
		samples = 4000;
		break;
	}

	const EquityCalculator::Result result =
		EquityCalculator::equity(s.holeCards, s.boardCards, s.boardSize,
								 s.opponents, &opponentRange, samples);

	return result.samples > 0 ? result.equity : 0.0;
}

namespace
{

// ---------------------------------------------------------------------------
// Before the flop
// ---------------------------------------------------------------------------

AiDecision fold(const Metrics& m)
{
	AiDecision d;
	// If calling costs nothing, we never fold.
	d.action = m.canCheck ? PLAYER_ACTION_CHECK : PLAYER_ACTION_FOLD;
	return d;
}

AiDecision call(const Metrics& m)
{
	AiDecision d;
	d.action = m.canCheck ? PLAYER_ACTION_CHECK : PLAYER_ACTION_CALL;
	return d;
}

// A raise to a target bet. evaluation() truncates to the stack itself and
// enforces the minimum raise.
AiDecision raiseTo(const AiSituation& s, const Metrics& m, int targetSet)
{
	AiDecision d;
	targetSet = std::min(targetSet, s.myCash + s.mySet);

	if(s.highestSet == 0) {
		d.action = PLAYER_ACTION_BET;
		d.bet = std::max(m.bigBlind, targetSet);
		return d;
	}

	d.action = PLAYER_ACTION_RAISE;
	d.raise = std::max(s.minimumRaise, targetSet - s.highestSet);
	return d;
}

AiDecision allIn(const AiSituation& s, const Metrics& m)
{
	return raiseTo(s, m, s.myCash + s.mySet);
}

/* Short stack: raising means the whole stack anyway, so the only decision left
 * is between "everything" and "nothing". The old engine lacks this
 * mode entirely -- it blinds itself to death in the tournament endgame.
 * The push range widens with position and with every blind we lose.
 */
AiDecision decideShortStack(const AiSituation& s, const AiPersonality& p, const Metrics& m, double judgedHand)
{
	const bool late = s.opponentsBehind <= 2;
	double push = p.pushPercent * (late ? 1.7 : 1.0);
	if(m.stackInBB < 8.0) push *= 1.4;

	if(judgedHand <= push) return allIn(s, m);

	// Outside the push range: only play on if it costs nothing.
	if(m.canCheck) return call(m);

	/* In the big blind against a small raise we often get such good pot
	 * odds that folding would be more expensive than calling.
	 */
	if(s.amBigBlind && m.potOdds < 0.25 * p.looseness) return call(m);

	return fold(m);
}

AiDecision decidePreflop(const AiSituation& s, const AiPersonality& p, const Metrics& m)
{
	// 0 % is the best hand, 100 % the worst (see PreflopStrength).
	const double hand = PreflopStrength::percentile(s.holeCards[0], s.holeCards[1]) * 100.0;

	/* A weak bot misjudges its own hand: sometimes it plays trash, sometimes it
	 * throws a premium hand away. Both cost money, and that is the point.
	 */
	double judgedHand = hand;
	if(p.thresholdNoise > 0.0) {
		judgedHand += (randomPercent() - 50.5) / 50.0 * p.thresholdNoise;
	}

	if(m.stackInBB <= 12.0) return decideShortStack(s, p, m, judgedHand);

	const bool facingRaise = s.highestSet > m.bigBlind;

	if(!facingRaise) {
		const int limpers = std::max(0, (s.setsThisRound - m.bigBlind - s.smallBlind) / m.bigBlind);
		const double openPercent = p.openPercent[positionBucket(s)];

		if(judgedHand <= openPercent) {
			/* Whether the range is played as a raise or as a limp is a matter of
			 * style: a caller limps with nearly everything and almost never
			 * raises, a maniac raises nearly everything.
			 */
			if(randomPercent() <= static_cast<int>(p.openRaiseChance * 100)) {
				// A base size of three big blinds, one more per limper.
				int target = static_cast<int>((3 + limpers) * m.bigBlind * p.aggression);
				target = std::max(target, s.highestSet + m.bigBlind);
				return raiseTo(s, m, target);
			}
			if(m.toCall <= m.bigBlind) return call(m);
			return fold(m);
		}

		// Checking is free, so it always happens.
		if(m.canCheck) return call(m);

		/* Limping behind: with a cheap price and somebody already in the pot,
		 * speculative hands are worth seeing a flop.
		 */
		if(m.toCall <= m.bigBlind && limpers >= 1 && judgedHand <= openPercent * 1.6) return call(m);

		return fold(m);
	}

	// Somebody raised.
	if(judgedHand <= p.threeBetPercent) {
		const int factor = inPosition(s) ? 3 : 4;
		return raiseTo(s, m, static_cast<int>(factor * s.highestSet * p.aggression));
	}

	/* A light re-raise, so that the value range does not become readable. Only
	 * in position and only against few players -- otherwise it is a bluff into
	 * a crowd.
	 */
	if(p.lightThreeBet > 0.0 && inPosition(s) && s.opponents <= 2
			&& randomPercent() <= static_cast<int>(p.lightThreeBet)) {
		return raiseTo(s, m, 3 * s.highestSet);
	}

	const double callPercent = s.amBigBlind ? p.defendBigBlindPercent
							 : (inPosition(s) ? p.callRaisePercent : p.callRaisePercentOop);

	if(judgedHand <= callPercent) {
		// Not at any price: a very large raise blows the odds.
		const double priceInBB = static_cast<double>(m.toCall) / m.bigBlind;
		if(priceInBB <= 12.0 * p.looseness) return call(m);
	}

	return fold(m);
}

}

namespace
{

// ---------------------------------------------------------------------------
// After the flop
// ---------------------------------------------------------------------------

AiDecision decidePostflop(const AiSituation& s, const AiPersonality& p, const Metrics& m)
{
	double equity = AiEngine4::winningChance(s);

	/* A weak bot misjudges its hand here as well, only less than before the
	 * flop: the board already says a lot about what the hand is worth.
	 */
	if(p.thresholdNoise > 0.0) {
		equity += (randomPercent() - 50.5) / 50.0 * p.thresholdNoise * 0.003;
		equity = std::max(0.0, std::min(1.0, equity));
	}

	/* The reference size is the share that would be due to us if everybody were
	 * equally strong. At a full table that is little, heads-up it is half -- which is why
	 * the same hand must not be played like a monster multiway.
	 */
	const double fairShare = 1.0 / (s.opponents + 1);

	/* How likely a bet wins the pot without a showdown. There is no EV of a bet
	 * in this engine and no model of a bet size, but the opponents' fold rate
	 * says which way to lean: against opponents who give up, pressure pays;
	 * against opponents who call, bluffs burn money and value bets can be
	 * thinner. The pivot is where a balanced style ends up (28 %, measured);
	 * the shift stays small on purpose -- it is a bias on top of the equity
	 * basis, nothing more (see PLAN.md, the fold step).
	 */
	double pressure = 0.0;
	if(s.opponentFoldToBetPercent >= 0) {
		pressure = (s.opponentFoldToBetPercent - 28) / 22.0;
		pressure = std::max(-0.8, std::min(0.8, pressure * p.adaptation));
	}

	if(m.canCheck) {
		/* How readily we bet into a checked pot is a matter of aggression, not
		 * of looseness: a caller with a decent hand still just checks. Against
		 * a table that calls everything the value bets go thinner and the
		 * bluffs stop; against a table that gives up the pressure goes up while
		 * the value threshold barely moves -- they do not pay off thin value,
		 * but there is no reason to stop betting for it either.
		 */
		const double valueShift = (pressure < 0.0) ? 0.30 * pressure : 0.10 * pressure;
		if(equity > fairShare * (1.5 + valueShift) / p.aggression) {
			// The clearer the lead, the larger the bet.
			double fraction = (equity > fairShare * 2.2) ? 0.75 : 0.55;
			fraction *= p.aggression;
			const int amount = std::max(m.bigBlind, static_cast<int>(m.pot * fraction));
			return raiseTo(s, m, s.mySet + amount);
		}

		/* A continuation bet: betting by range instead of by hand. That is where
		 * a large part of the aggression of the styles comes from -- a maniac
		 * fires at nearly every checked pot, a caller almost never. Turn and
		 * river are fired at less often than the flop.
		 */
		double stab = p.cbetPercent * p.raiseFrequency * (1.0 + 0.8 * pressure);
		if(s.round == GAME_STATE_TURN) stab *= 0.7;
		else if(s.round == GAME_STATE_RIVER) stab *= 0.5;
		if(randomPercent() <= static_cast<int>(stab)) {
			const int amount = std::max(m.bigBlind, static_cast<int>(m.pot * 0.55 * p.aggression));
			return raiseTo(s, m, s.mySet + amount);
		}

		// Bluff only against few opponents and preferably as the last one --
		// and not at all against opponents who call everything.
		const double bluffChance = 18.0 * p.bluffRate * (1.0 + 1.0 * pressure);
		if(bluffChance > 0.0 && s.opponents <= 2 && inPosition(s)
				&& randomPercent() <= static_cast<int>(bluffChance)) {
			const int amount = std::max(m.bigBlind, static_cast<int>(m.pot * 0.5));
			return raiseTo(s, m, s.mySet + amount);
		}

		return call(m);
	}

	/* A call pays off if the winning chance exceeds the pot odds. Before
	 * the river a surcharge is added, because later rounds can still cost
	 * money.
	 */
	double margin = (s.round == GAME_STATE_RIVER) ? 0.0 : 0.03;

	/* Whoever bets and raises frequently inevitably does so with weak
	 * hands as well -- against such opponents one may call more cheaply. Against notably
	 * passive opponents the opposite applies: their bets mean more.
	 */
	if(s.opponentAggressionPercent >= 0) {
		double shift = (s.opponentAggressionPercent - 40) / 1000.0;
		shift = std::max(-0.03, std::min(0.03, shift));
		margin -= shift;
		margin = std::max(-0.02, margin);
	}

	// The style decides how much worse than the pot odds a call may be.
	margin += p.callMargin;

	if(equity >= m.potOdds + margin) {
		const bool clearlyAhead = equity > fairShare * 1.9;
		const bool strong = equity > fairShare * 1.25;

		/* A weak-tight player folds hands that were worth a call: the board
		 * looks scary, the bet is big, so the hand goes into the muck although
		 * the price is fine. That leak has to exist before anybody can aim a
		 * bet at it, and it is what the fold statistic of the opponent model
		 * measures (PLAN.md, the fold step).
		 */
		if(p.giveUpChance > 0.0 && randomPercent() <= static_cast<int>(p.giveUpChance)) {
			return fold(m);
		}

		/* Putting the money in now instead of just calling. Without this a table
		 * of bots is one long series of calls and every style ends up with the
		 * same aggression factor.
		 */
		double raiseChance = 0.0;
		if(clearlyAhead && s.raisesThisRound < 3) raiseChance = 60.0 + 40.0 * p.aggression;
		else if(strong && s.raisesThisRound < 2) raiseChance = 25.0 * p.aggression;
		else if(s.raisesThisRound < 2) raiseChance = p.raiseBluffPercent;
		raiseChance *= p.raiseFrequency;

		if(raiseChance > 0.0 && randomPercent() <= static_cast<int>(std::min(100.0, raiseChance))) {
			const int amount = std::max(s.minimumRaise, static_cast<int>(m.pot * 0.7 * p.aggression));
			return raiseTo(s, m, s.highestSet + amount);
		}
		return call(m);
	}

	/* Semi-bluff: too little for a call, but enough substance to win with a
	 * raise if the opponent gives up.
	 */
	if(s.opponents == 1 && inPosition(s) && equity > 0.25 &&
			randomPercent() <= static_cast<int>(12 * p.bluffRate)) {
		const int amount = std::max(s.minimumRaise, static_cast<int>(m.pot * 0.6));
		return raiseTo(s, m, s.highestSet + amount);
	}

	/* And the opposite leak: a station pays even when the price is against
	 * him. Bluffing into him is pointless, value betting is not.
	 */
	if(p.stubbornChance > 0.0 && randomPercent() <= static_cast<int>(p.stubbornChance)) {
		return call(m);
	}

	return fold(m);
}

}

AiDecision AiEngine4::decide(const AiSituation& situation, const AiPersonality& personality)
{
	const Metrics m = measure(situation);

	// Without opponents there is nothing to decide.
	if(situation.opponents < 1) return call(m);

	if(situation.round == GAME_STATE_PREFLOP) return decidePreflop(situation, personality, m);

	return decidePostflop(situation, personality, m);
}
