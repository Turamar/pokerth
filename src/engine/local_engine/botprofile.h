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
	BOT_ARCHETYPE_COUNT
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
	static const char *archetypeList();

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
