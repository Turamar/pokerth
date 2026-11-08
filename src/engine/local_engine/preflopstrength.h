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

#ifndef PREFLOPSTRENGTH_H
#define PREFLOPSTRENGTH_H

/* How strong is a starting hand, as a number?
 *
 * The bots describe their ranges as a percentage -- "we play the best 22 % of
 * hands" -- instead of using hand-written charts. Charts cannot express the
 * extremes a maniac or a nit needs, and a chart change means editing a long
 * string and hoping that the result is the intended one.
 *
 * The ordering is measured, not guessed: the value of a starting hand is its
 * equity against one random opponent, estimated by the same simulation the
 * engine uses for its decisions (EquityCalculator). The table is built once per
 * process (169 types * a few thousand runs, a few dozen milliseconds) and then
 * only read.
 */
class PreflopStrength
{
public:
	/* 0 .. 1, weighted by how often the combination occurs: 0 is aces (the
	 * strongest hand), 1 is seven-two offsuit (the weakest). The value is the
	 * share of the 1326 starting combinations that is at least as strong, with
	 * half of the own share added, so it behaves like a continuous percentile.
	 * Invalid cards give 0.5.
	 */
	static double percentile(int cardA, int cardB);

	// The same for a starting hand type 0..168 (HoleCardsRange::typeIndex).
	static double percentileOfType(int typeIndex);

private:
	static void buildTable();
};

#endif
