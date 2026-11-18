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

/* The events of the starting catalogue that exist (PLAN.md section 3): the
 * three single tables and two multi-table fields ("Schule" on two tables of
 * nine, "Daily" on five). The 180-seat events wait for the larger fields.
 *
 * All of them are 75 BB deep and raise the blinds every 20 hands; what differs
 * is the size, the payout and the shape of the field, i.e. the question they
 * ask. The field moves with the price, not with the size: 0.40 is the softest
 * tier, 1.00 is one step above it, "low" starts at 2.00. That is the rule the
 * plan set after the cash ladder - an event may be harder because it is
 * bigger, not because the same event got a stronger table; the "Daily" is the
 * one deliberate exception (a big soft field, D-048). The payouts are in
 * permille and every event pays out its whole pool; with an even pool the
 * shares have to keep the cents whole (0.40 x 18 = 7.20, so its shares are
 * multiples of 25).
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
		40, 0, 18, "micro", 20, 1500, 10,
		{ { 1, 375 }, { 2, 225 }, { 3, 175 }, { 4, 125 }, { 5, 100 } }, 5
	},
	{
		"sng6",
		TOURNAMENT_TEXT("Sechser"),
		TOURNAMENT_TEXT("6 Plätze, zwei werden bezahlt: das Geld ist nah, die Blinds warten nicht"),
		40, 0, 6, "micro", 20, 1500, 10,
		{ { 1, 650 }, { 2, 350 } }, 2
	},
	{
		"sng9",
		TOURNAMENT_TEXT("Neuner"),
		TOURNAMENT_TEXT("9 Plätze, drei werden bezahlt: voller Tisch und eine echte Bubble"),
		100, 0, 9, "rookie", 20, 1500, 10,
		{ { 1, 500 }, { 2, 300 }, { 3, 200 } }, 3
	},
	/* The first five-table event (PLAN.md section 3). Forty-five seats are five
	 * tables of nine: "Schule" was the first on more than one table, this is
	 * the first with a field that is not all in one room and a bubble far
	 * away - and it still costs the bottom buy-in, so a new profile can see a
	 * real MTT. Seven places are paid out of forty-five (D-048); the shares in
	 * permille keep the 1800-cent pool whole, and the field is "rookie" on
	 * purpose: it is the size that makes the event, not a stronger table. */
	{
		"daily",
		TOURNAMENT_TEXT("Tagesturnier"),
		TOURNAMENT_TEXT("45 Plätze an fünf Tischen, sieben werden bezahlt: das erste große Feld"),
		40, 0, 45, "rookie", 20, 1500, 10,
		{ { 1, 320 }, { 2, 210 }, { 3, 150 }, { 4, 120 }, { 5, 95 }, { 6, 65 }, { 7, 40 } }, 7
	},
	/* The double-or-nothing (PLAN.md section 9). The plan's table called it SNG
	 * 10, but a table seats nine (MAX_TABLE_SEATS), so it is eight at one table
	 * (D-049): four of the eight double their buy-in, four get nothing, and
	 * the payout is flat - the question is survival, not the final table. It
	 * is the first event with an entry gate: 40 dollars of bankroll before the
	 * seat is offered, the same idea as a cash limit's gate. The field is
	 * "low" because the price is 2.00 and the field follows the price. */
	{
		"don",
		TOURNAMENT_TEXT("Doppelt oder nichts"),
		TOURNAMENT_TEXT("8 Plätze, vier verdoppeln: überleben statt Finaltisch"),
		200, 4000, 8, "low", 20, 1500, 10,
		{ { 1, 250 }, { 2, 250 }, { 3, 250 }, { 4, 250 } }, 4
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

}
