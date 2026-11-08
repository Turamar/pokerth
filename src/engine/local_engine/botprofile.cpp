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
 * GNU Affero General Public License for more details.                        *
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

#include "botprofile.h"

#include "tools.h"

#include <cctype>

using namespace std;

namespace
{

/* The playing styles. All percentages refer to the starting hands, measured as
 * a percentile of hand strength (see PreflopStrength): "we play the best 22 %".
 * The opening percentage is given per position, from the earliest seat to the
 * button; the engine maps "how many players sit behind me" onto these four
 * values.
 *
 * These are starting values that were then calibrated against the simulator
 * (see CHANGELOG.md): the numbers that matter for a style are VPIP, PFR and the
 * share of three bets, and they were tuned until every style sat in its
 * corridor.
 */
struct ArchetypeNumbers {
	const char *name;
	double open[4];
	double openRaiseChance;  // share of the opens that raises, the rest limps
	double threeBetPercent;
	double lightThreeBet;    // chance of a re-raise outside the value range
	double callInPosition;
	double callOutOfPosition;
	double defendBigBlind;
	double pushPercent;      // short stack, push or fold
	double callMargin;       // added to the equity a call needs after the flop
	double cbetPercent;      // chance to bet a checked pot
	double raiseFrequency;   // how often a made hand raises instead of calling
	double raiseBluff;       // chance to raise a hand that would only be called
	double aggression;
	double bluffRate;
	double looseness;
	bool diagnostic;         // never dealt by the random mixture
};

const ArchetypeNumbers ARCHETYPES[BOT_ARCHETYPE_COUNT] = {
	// maniac: raises nearly everything, keeps betting with nothing
	{ "maniac", { 40.0, 48.0, 56.0, 64.0 }, 0.96, 16.0, 7.0, 42.0, 34.0, 62.0, 50.0, -0.09, 85.0, 1.35, 45.0, 1.55, 2.3, 1.25, false },
	// LAG: wide and aggressive, but with a plan
	{ "lag",    { 23.0, 29.0, 36.0, 45.0 }, 0.88, 7.5, 3.5, 22.0, 14.0, 50.0, 30.0, -0.05, 70.0, 1.15, 18.0, 1.30, 1.5, 1.10, false },
	// TAG: tight and aggressive -- the solid default
	{ "tag",    { 13.0, 18.0, 25.0, 34.0 }, 0.95, 4.0, 1.5, 13.0, 8.0, 28.0, 18.0, -0.005, 60.0, 1.00, 6.0, 1.05, 0.9, 1.0, false },
	// nit: waits for premium hands and gives up early
	{ "nit",    { 7.0, 10.0, 14.0, 20.0 }, 0.92, 1.5, 0.3, 6.0, 4.0, 12.0, 9.0, 0.03, 45.0, 0.85, 2.0, 0.95, 0.35, 0.9, false },
	// caller: loose and passive, calls far too much, raises almost never
	{ "caller", { 22.0, 30.0, 40.0, 52.0 }, 0.15, 0.7, 0.3, 34.0, 30.0, 55.0, 12.0, -0.10, 20.0, 0.35, 1.0, 0.45, 0.25, 1.15, false },
	// balanced: no clear leak, the reference point
	{ "balanced", { 13.0, 19.0, 26.0, 34.0 }, 0.88, 4.0, 1.8, 15.0, 10.0, 32.0, 18.0, -0.01, 58.0, 1.05, 7.0, 1.15, 1.1, 1.0, false },

	/* Diagnostic styles, never dealt (see ARCHETYPE_WEIGHTS). Each one holds
	 * everything else at a solid level and does the one thing that punishes its
	 * target: the excess is the price of the target's leak, measured as a win
	 * rate against five of them.
	 *
	 * exploit-nit: they fold too much, so open wide and bet every flop; their
	 * rare aggression means a real hand, so pay them off only with a strong one.
	 * exploit-caller: they call too much, so bet for value every street, never
	 * bluff and fold when they finally raise.
	 * exploit-maniac: they bet air, so call down far below the pot odds and let
	 * them bloat the pot; raise only a made hand.
	 */
	{ "exploit-nit",    { 40.0, 50.0, 60.0, 70.0 }, 0.95, 12.0, 8.0, 22.0, 15.0, 40.0, 25.0, 0.06, 95.0, 1.20, 35.0, 1.55, 2.2, 1.05, true },
	{ "exploit-caller", { 20.0, 26.0, 32.0, 40.0 }, 0.95, 6.0, 0.5, 12.0, 7.0, 28.0, 16.0, 0.08, 80.0, 1.30, 2.0, 1.30, 0.35, 0.95, true },
	{ "exploit-maniac", { 12.0, 16.0, 22.0, 30.0 }, 0.95, 9.0, 3.0, 20.0, 16.0, 32.0, 14.0, -0.16, 50.0, 0.70, 6.0, 0.90, 0.60, 1.0, true }
};

/* What a strength level changes. The weak levels are not given worse cards,
 * they are given worse judgement: they pay too much for their hands, they
 * three-bet too rarely, they bluff at the wrong moment. The numbers are
 * multipliers on the style, plus additive shifts for the calling margin and the
 * hand judgement.
 *
 * noise: how many percentage points the bot misjudges its own hand by, in both
 * directions. That is what makes a novice call with trash and fold a premium
 * hand now and then -- expensive in both directions, and exactly the kind of
 * mistake a beginner makes.
 */
struct StrengthNumbers {
	const char *name;
	double openScale;
	double threeBetScale;
	double callScale;
	double aggressionScale;
	double bluffScale;
	double loosenessScale;
	double callMarginShift;
	double noise;
	double cbetScale;
};

const StrengthNumbers STRENGTHS[BOT_STRENGTH_COUNT] = {
	{ "novice",  1.15, 0.40, 1.55, 0.75, 1.40, 1.10, -0.080, 18.0, 0.75 },
	{ "amateur", 1.05, 0.70, 1.25, 0.90, 1.15, 1.05, -0.040, 9.0, 0.88 },
	{ "pro",     1.00, 1.00, 1.00, 1.00, 1.00, 1.00, -0.010, 3.0, 1.00 },
	{ "elite",   0.95, 1.20, 0.90, 1.10, 0.90, 0.97, 0.010, 0.0, 1.05 }
};

const int STRENGTH_WEIGHTS[BOT_STRENGTH_COUNT] = { 30, 35, 23, 12 };
const int ARCHETYPE_WEIGHTS[BOT_ARCHETYPE_COUNT] = { 6, 14, 22, 14, 16, 28, 0, 0, 0 };

/* Reference fields: what a real pool of one stake looks like, as five seats (a
 * six-max table minus the hero). The targets are the population averages a HUD
 * reports for that stake -- loose and passive at the bottom, tight and
 * aggressive at the top. A population is a distribution and these tables are
 * its typical member, so the numbers are approximate by nature; the simulator
 * prints the measured field next to its target, which is the point: the field
 * has to look like the pool, otherwise a win rate against it means nothing.
 */
struct PopulationNumbers {
	const char *name;
	const char *description;
	double targetVpip;
	double targetPfr;
	double targetThreeBet;
	struct Seat {
		BotStrength strength;
		BotArchetype archetype;
	} seats[5];
};

const PopulationNumbers POPULATIONS[BOT_POPULATION_COUNT] = {
	{ "micro", "NL2-NL5: recreational, loose and passive, two aggressors",
	  40.0, 10.0, 2.0, {
		  { BOT_STRENGTH_NOVICE, BOT_ARCHETYPE_CALLER },
		  { BOT_STRENGTH_NOVICE, BOT_ARCHETYPE_CALLER },
		  { BOT_STRENGTH_AMATEUR, BOT_ARCHETYPE_MANIAC },
		  { BOT_STRENGTH_NOVICE, BOT_ARCHETYPE_BALANCED },
		  { BOT_STRENGTH_AMATEUR, BOT_ARCHETYPE_TAG } } },
	{ "low", "NL10-NL25: some idea of position, still calls too much",
	  29.0, 16.0, 4.0, {
		  { BOT_STRENGTH_AMATEUR, BOT_ARCHETYPE_CALLER },
		  { BOT_STRENGTH_AMATEUR, BOT_ARCHETYPE_LAG },
		  { BOT_STRENGTH_AMATEUR, BOT_ARCHETYPE_TAG },
		  { BOT_STRENGTH_PRO, BOT_ARCHETYPE_TAG },
		  { BOT_STRENGTH_AMATEUR, BOT_ARCHETYPE_BALANCED } } },
	{ "mid", "NL50-NL100: regulars, no obvious leak",
	  23.0, 19.0, 6.0, {
		  { BOT_STRENGTH_PRO, BOT_ARCHETYPE_LAG },
		  { BOT_STRENGTH_PRO, BOT_ARCHETYPE_TAG },
		  { BOT_STRENGTH_PRO, BOT_ARCHETYPE_BALANCED },
		  { BOT_STRENGTH_ELITE, BOT_ARCHETYPE_TAG },
		  { BOT_STRENGTH_PRO, BOT_ARCHETYPE_NIT } } }
};

string lower(const string &text)
{
	string result = text;
	for(string::size_type i = 0; i < result.size(); ++i) {
		result[i] = static_cast<char>(tolower(static_cast<unsigned char>(result[i])));
	}
	return result;
}

// Rolls a weighted list and returns the chosen index.
int weightedIndex(const int *weights, int count)
{
	int total = 0;
	for(int i = 0; i < count; ++i) total += weights[i];

	int roll = 0;
	Tools::GetRand(1, total, 1, &roll);
	for(int i = 0; i < count; ++i) {
		roll -= weights[i];
		if(roll <= 0) return i;
	}
	return count - 1;
}

}

AiPersonality BotProfile::personality() const
{
	const ArchetypeNumbers &style = ARCHETYPES[archetype];
	const StrengthNumbers &level = STRENGTHS[strength];

	AiPersonality p;
	for(int i = 0; i < 4; ++i) p.openPercent[i] = style.open[i] * level.openScale;
	p.openRaiseChance = style.openRaiseChance;
	p.threeBetPercent = style.threeBetPercent * level.threeBetScale;
	p.lightThreeBet = style.lightThreeBet * level.threeBetScale;
	p.callRaisePercent = style.callInPosition * level.callScale;
	p.callRaisePercentOop = style.callOutOfPosition * level.callScale;
	p.defendBigBlindPercent = style.defendBigBlind * level.callScale;
	p.pushPercent = style.pushPercent;
	p.callMargin = style.callMargin + level.callMarginShift;
	p.cbetPercent = style.cbetPercent * level.cbetScale;
	p.raiseFrequency = style.raiseFrequency;
	p.raiseBluffPercent = style.raiseBluff;
	p.aggression = style.aggression * level.aggressionScale;
	p.bluffRate = style.bluffRate * level.bluffScale;
	p.looseness = style.looseness * level.loosenessScale;
	p.thresholdNoise = level.noise;
	return p;
}

string BotProfile::label() const
{
	return string(strengthName(strength)) + "/" + archetypeName(archetype);
}

const char *BotProfile::strengthName(BotStrength strength)
{
	if(strength < 0 || strength >= BOT_STRENGTH_COUNT) return "unknown";
	return STRENGTHS[strength].name;
}

const char *BotProfile::archetypeName(BotArchetype archetype)
{
	if(archetype < 0 || archetype >= BOT_ARCHETYPE_COUNT) return "unknown";
	return ARCHETYPES[archetype].name;
}

const char *BotProfile::strengthList()
{
	return "novice, amateur, pro, elite";
}

const char *BotProfile::archetypeList()
{
	return "maniac, lag, tag, nit, caller, balanced, "
		   "exploit-nit, exploit-caller, exploit-maniac";
}

bool BotProfile::isDiagnostic(BotArchetype archetype)
{
	if(archetype < 0 || archetype >= BOT_ARCHETYPE_COUNT) return false;
	return ARCHETYPES[archetype].diagnostic;
}

// Index of a reference field by name, -1 if there is no such field.
static int populationIndex(const string &text)
{
	const string key = lower(text);
	for(int i = 0; i < BOT_POPULATION_COUNT; ++i) {
		if(key == POPULATIONS[i].name) return i;
	}
	return -1;
}

const char *BotProfile::populationList()
{
	return "micro, low, mid";
}

const char *BotProfile::populationName(BotPopulation population)
{
	if(population < 0 || population >= BOT_POPULATION_COUNT) return "unknown";
	return POPULATIONS[population].name;
}

bool BotProfile::populationTarget(const string &text, BotPopulationTarget &out)
{
	const int index = populationIndex(text);
	if(index < 0) return false;
	out.name = POPULATIONS[index].name;
	out.description = POPULATIONS[index].description;
	out.vpip = POPULATIONS[index].targetVpip;
	out.pfr = POPULATIONS[index].targetPfr;
	out.threeBet = POPULATIONS[index].targetThreeBet;
	return true;
}

bool BotProfile::parsePopulation(const string &text, vector<BotProfile> &seats)
{
	seats.clear();
	const int index = populationIndex(text);
	if(index < 0) return false;

	for(int i = 0; i < 5; ++i) {
		seats.push_back(BotProfile(POPULATIONS[index].seats[i].strength,
								   POPULATIONS[index].seats[i].archetype));
	}
	return true;
}

bool BotProfile::parseStrength(const string &text, BotStrength &out)
{
	const string key = lower(text);
	for(int i = 0; i < BOT_STRENGTH_COUNT; ++i) {
		if(key == STRENGTHS[i].name) {
			out = static_cast<BotStrength>(i);
			return true;
		}
	}
	return false;
}

bool BotProfile::parseArchetype(const string &text, BotArchetype &out)
{
	const string key = lower(text);
	for(int i = 0; i < BOT_ARCHETYPE_COUNT; ++i) {
		if(key == ARCHETYPES[i].name) {
			out = static_cast<BotArchetype>(i);
			return true;
		}
	}
	return false;
}

bool BotProfile::parse(const string &text, BotProfile &out)
{
	const string key = lower(text);
	const string::size_type slash = key.find('/');
	if(slash == string::npos) {
		BotStrength strength;
		if(parseStrength(key, strength)) {
			out.strength = strength;
			return true;
		}
		BotArchetype archetype;
		if(parseArchetype(key, archetype)) {
			out.archetype = archetype;
			return true;
		}
		return false;
	}

	BotStrength strength;
	BotArchetype archetype;
	if(!parseStrength(key.substr(0, slash), strength)) return false;
	if(!parseArchetype(key.substr(slash + 1), archetype)) return false;
	out.strength = strength;
	out.archetype = archetype;
	return true;
}

BotProfile BotProfile::randomForSeat()
{
	return BotProfile(static_cast<BotStrength>(weightedIndex(STRENGTH_WEIGHTS, BOT_STRENGTH_COUNT)),
					  static_cast<BotArchetype>(weightedIndex(ARCHETYPE_WEIGHTS, BOT_ARCHETYPE_COUNT)));
}
