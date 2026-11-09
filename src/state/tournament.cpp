/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "tournament.h"

#include <cstring>

namespace PlayerState
{

/* The starting pair (PLAN.md, "Каталог: стартовая пятёрка").
 *
 * Both are single tables, both 75 BB deep and both raise the blinds every 20
 * hands; what differs is the size and the payout, i.e. the question they ask.
 * The field moves with the price, not with the size: 0.40 is the softest tier,
 * 1.00 is one step above it. That is the rule the plan set after the cash
 * ladder - an event may be harder because it is bigger, not because the same
 * event got a stronger table.
 */
static const TournamentEvent TOURNAMENTS[] = {
	{
		"sng6", "Шестёрка", "6 Plätze, zwei werden bezahlt: das Geld ist nah, die Blinds warten nicht",
		40, 6, "micro", 20, 1500, 10,
		{ { 1, 650 }, { 2, 350 } }, 2
	},
	{
		"sng9", "Девятка", "9 Plätze, drei werden bezahlt: voller Tisch und eine echte Bubble",
		100, 9, "rookie", 20, 1500, 10,
		{ { 1, 500 }, { 2, 300 }, { 3, 200 } }, 3
	},
};

static const int TOURNAMENT_COUNT =
	static_cast<int>(sizeof(TOURNAMENTS) / sizeof(TOURNAMENTS[0]));

qint64 TournamentEvent::prizeForPlace(int place) const
{
	for(int i = 0; i < payoutCount && i < MaxTournamentPayouts; ++i) {
		if(payouts[i].place == place) {
			return pool() * payouts[i].permille / 1000;
		}
	}
	return 0;
}

int tournamentCount()
{
	return TOURNAMENT_COUNT;
}

const TournamentEvent &tournamentEvent(int index)
{
	if(index < 0 || index >= TOURNAMENT_COUNT) {
		index = 0;
	}
	return TOURNAMENTS[index];
}

int tournamentIndex(const QString &key)
{
	for(int i = 0; i < TOURNAMENT_COUNT; ++i) {
		if(key == QLatin1String(TOURNAMENTS[i].key)) {
			return i;
		}
	}
	return -1;
}

const TournamentEvent &tournamentEventByKey(const QString &key)
{
	const int index = tournamentIndex(key);
	return tournamentEvent(index < 0 ? 0 : index);
}

const char *tournamentList()
{
	return "sng6, sng9";
}

}
