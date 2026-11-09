/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "tournament.h"

#include <QCoreApplication>

#include <cstring>

namespace PlayerState
{

// The words of an event travel through QT_TRANSLATE_NOOP so that the UI can ask
// for them in the running language (WalletHandler::tournaments does).
#define TOURNAMENT_TEXT(text) QT_TRANSLATE_NOOP("TournamentEvent", text)

/* The events of the starting catalogue that exist (PLAN.md, "Каталог:
 * стартовая пятёрка" - three of the five are here; the 45- and 180-seat events
 * wait for the larger fields).
 *
 * All of them are 75 BB deep and raise the blinds every 20 hands; what differs
 * is the size, the payout and the shape of the field, i.e. the question they
 * ask. The field moves with the price, not with the size: 0.40 is the softest
 * tier, 1.00 is one step above it. That is the rule the plan set after the cash
 * ladder - an event may be harder because it is bigger, not because the same
 * event got a stronger table. The payouts are in permille and every event pays
 * out its whole pool; with an even pool the shares have to keep the cents
 * whole (0.40 x 18 = 7.20, so its shares are multiples of 25).
 */
static const TournamentEvent TOURNAMENTS[] = {
	/* The first event on more than one table (PLAN.md section 3, "Школа").
	 * Eighteen seats are two tables of nine, so the field is split, the tables
	 * are balanced and merged as the people drop out, and the level runs on one
	 * clock for both. What it asks is different from the single tables in three
	 * ways at once: a field that is not all in one room, five paid places
	 * instead of two or three (the bubble is far away, so folding into the money
	 * takes much longer), and a soft field - it is the first tournament of the
	 * ladder, so it costs the least and pays almost a third of the field. */
	{
		"school",
		TOURNAMENT_TEXT("Schule"),
		TOURNAMENT_TEXT("18 Plätze an zwei Tischen, fünf werden bezahlt: der erste Schritt hinauf"),
		40, 18, "micro", 20, 1500, 10,
		{ { 1, 375 }, { 2, 225 }, { 3, 175 }, { 4, 125 }, { 5, 100 } }, 5
	},
	{
		"sng6",
		TOURNAMENT_TEXT("Sechser"),
		TOURNAMENT_TEXT("6 Plätze, zwei werden bezahlt: das Geld ist nah, die Blinds warten nicht"),
		40, 6, "micro", 20, 1500, 10,
		{ { 1, 650 }, { 2, 350 } }, 2
	},
	{
		"sng9",
		TOURNAMENT_TEXT("Neuner"),
		TOURNAMENT_TEXT("9 Plätze, drei werden bezahlt: voller Tisch und eine echte Bubble"),
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
	return "school, sng6, sng9";
}

}
