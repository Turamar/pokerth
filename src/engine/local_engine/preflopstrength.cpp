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

#include "preflopstrength.h"

#include "equitycalculator.h"
#include "holecardsrange.h"

#include <algorithm>
#include <mutex>

using namespace std;

namespace
{

const int TypeCount = 169;
/* Runs per starting hand type. The table is built once per process; 3000 runs
 * keep the standard error around one percent of equity, which is far below the
 * distance between two neighbouring percentiles.
 */
const int TableSamples = 3000;

double sEquity[TypeCount];
double sPercentile[TypeCount];
once_flag sBuilt;

// How many of the 1326 card combinations a type stands for.
int combinationsOfType(int type)
{
	if(type < 13) return 6;       // pair
	if(type < 91) return 4;       // suited
	return 12;                    // offsuit
}

/* Two cards that represent the whole type. The equity against a random hand
 * depends only on the ranks and on whether the cards are suited, so one
 * representative combination per type is enough.
 * Card encoding: card = suit * 13 + rank, rank 0 = two, 12 = ace.
 */
void representativeCards(int type, int &cardA, int &cardB)
{
	if(type < 13) {
		cardA = type;           // pair, two different suits
		cardB = 13 + type;
		return;
	}

	const bool suited = (type < 91);
	const int offset = suited ? type - 13 : type - 91;

	// Invert offset = high * (high - 1) / 2 + low, low < high.
	int high = 1;
	while(offset >= high * (high + 1) / 2) ++high;
	const int low = offset - high * (high - 1) / 2;

	cardA = high;                         // suit 0
	cardB = suited ? low : 13 + low;      // suit 0 or suit 1
}

}

void PreflopStrength::buildTable()
{
	for(int type = 0; type < TypeCount; ++type) {
		int cardA, cardB;
		representativeCards(type, cardA, cardB);
		const int cards[2] = { cardA, cardB };

		const EquityCalculator::Result result =
			EquityCalculator::equity(cards, NULL, 0, 1, NULL, TableSamples);
		sEquity[type] = result.samples > 0 ? result.equity : 0.5;
	}

	int order[TypeCount];
	for(int type = 0; type < TypeCount; ++type) order[type] = type;
	stable_sort(order, order + TypeCount, [](int a, int b) {
		return sEquity[a] > sEquity[b];
	});

	int seen = 0;
	for(int i = 0; i < TypeCount; ++i) {
		const int type = order[i];
		const int combinations = combinationsOfType(type);
		sPercentile[type] = (seen + 0.5 * combinations) / 1326.0;
		seen += combinations;
	}
}

double PreflopStrength::percentileOfType(int typeIndex)
{
	if(typeIndex < 0 || typeIndex >= TypeCount) return 0.5;
	call_once(sBuilt, buildTable);
	return sPercentile[typeIndex];
}

double PreflopStrength::percentile(int cardA, int cardB)
{
	return percentileOfType(HoleCardsRange::typeIndex(cardA, cardB));
}
