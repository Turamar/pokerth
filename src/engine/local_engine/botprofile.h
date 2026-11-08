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

#ifndef BOTPROFILE_H
#define BOTPROFILE_H

#include <string>
#include <vector>

#include "aiengine4.h"

/* Strength and playing style of the computer players (PLAN.md section 4).
 *
 * Two independent axes:
 *  - strength: how well a bot plays. The weak levels do not get a different
 *    hand chart, they misjudge their own hand and pay too much for it; the
 *    strong levels keep the discipline of the books.
 *  - style: which hands it plays and how it plays them -- a nit folds what a
 *    maniac re-raises.
 *
 * A profile is pure data: it is translated into the knobs of AI engine 4
 * (AiPersonality) and nothing else knows about it. That keeps the engine free
 * of names and the table of numbers in one place, where it can be calibrated
 * with the simulator (see CHANGELOG.md for the measured values).
 *
 * Two more things live here because they are made of the same numbers:
 *  - a reference field of human-like opponents (the "population" tables), so
 *    that a win rate can be compared with a real pool instead of only with
 *    other bots;
 *  - the diagnostic styles ("exploit-*"), which price one leak of one style.
 *    They are never dealt at a table by the random mixture.
 */
enum BotStrength {
	BOT_STRENGTH_NOVICE = 0,
	BOT_STRENGTH_AMATEUR,
	BOT_STRENGTH_PRO,
	BOT_STRENGTH_ELITE,
	BOT_STRENGTH_COUNT
};

enum BotArchetype {
	BOT_ARCHETYPE_MANIAC = 0,
	BOT_ARCHETYPE_LAG,
	BOT_ARCHETYPE_TAG,
	BOT_ARCHETYPE_NIT,
	BOT_ARCHETYPE_CALLER,
	BOT_ARCHETYPE_BALANCED,
	/* Diagnostic styles. They answer one question each -- "how much does this
	 * leak cost?" -- and are never dealt by the random mixture (weight 0).
	 */
	BOT_ARCHETYPE_EXPLOIT_NIT,
	BOT_ARCHETYPE_EXPLOIT_CALLER,
	BOT_ARCHETYPE_EXPLOIT_MANIAC,
	BOT_ARCHETYPE_COUNT
};

/* A field of human-like opponents. The simulator puts the hero in one seat and
 * these opponents in the others; the win rate against such a field is the only
 * figure in this project that has an outside reference (published HUD
 * statistics of a real pool), so the tables below carry those targets.
 */
enum BotPopulation {
	BOT_POPULATION_MICRO = 0,
	BOT_POPULATION_LOW,
	BOT_POPULATION_MID,
	BOT_POPULATION_COUNT
};

/* What a field is supposed to look like, in the terms a HUD uses. The numbers
 * are published population averages for the stake, not our own measurements:
 * they exist so that a measured field can be compared with them.
 */
struct BotPopulationTarget {
	const char *name;
	const char *description;
	double vpip;
	double pfr;
	double threeBet;

	BotPopulationTarget() : name(""), description(""), vpip(0.0), pfr(0.0), threeBet(0.0) {}
};

struct BotProfile {
	BotStrength strength;
	BotArchetype archetype;

	BotProfile() : strength(BOT_STRENGTH_AMATEUR), archetype(BOT_ARCHETYPE_BALANCED) {}
	BotProfile(BotStrength s, BotArchetype a) : strength(s), archetype(a) {}

	// The numbers AI engine 4 works with.
	AiPersonality personality() const;

	// "pro/tag" -- stable identifier, used by the simulator and the logs.
	std::string label() const;

	static const char *strengthName(BotStrength strength);
	static const char *archetypeName(BotArchetype archetype);
	static const char *strengthList();
	// Every style the parser accepts, diagnostic ones included.
	static const char *archetypeList();
	static bool isDiagnostic(BotArchetype archetype);

	// "micro", "low", "mid": the opponents of a reference field, five of them.
	static const char *populationList();
	static const char *populationName(BotPopulation population);
	static bool populationTarget(const std::string &text, BotPopulationTarget &out);
	static bool parsePopulation(const std::string &text, std::vector<BotProfile> &seats);

	static bool parseStrength(const std::string &text, BotStrength &out);
	static bool parseArchetype(const std::string &text, BotArchetype &out);
	// "elite/lag", "elite" or "lag"; missing parts keep their current value.
	static bool parse(const std::string &text, BotProfile &out);

	/* The mixture a local game uses when nobody chose anything: mostly ordinary
	 * players, a few extremes, strength leaning towards the weaker half -- a
	 * table of elites is a different game and belongs to a later difficulty
	 * setting, not to the default.
	 */
	static BotProfile randomForSeat();
};

#endif
