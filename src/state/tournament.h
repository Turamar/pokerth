/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include <QString>

#include <qglobal.h>

#include <game_defs.h>

/* The tournament catalogue (PLAN.md section 3).
 *
 * A tournament is not "a cash table with bigger blinds": the blinds rise, the
 * payout structure decides how the last hands are played, and the field is a
 * tier of its own. This file holds the numbers of the events - what it costs,
 * how many seats, which field, how fast the levels rise and who is paid - so
 * that the client, the wallet and the self test all read the same table.
 *
 * Money is cents, chips are chips. A tournament pays out of the pool
 * (buy-in × seats), no rake; a seat is worth exactly its share.
 */
namespace PlayerState
{

/* The largest payout grid in the catalogue. The "Daily" pays seven places
 * (D-048), so the array holds seven; the 180-seat events would need 27-36 and
 * will raise it when they arrive (PLAN.md section 9). */
const int MaxTournamentPayouts = 7;

/* Nine seats at a tournament table - the common table limit (MAX_TABLE_SEATS in
 * game_defs.h), not the engine's ten (MAX_NUMBER_OF_PLAYERS). Decision 2026-11-10:
 * MTT never seats ten, so a table is nine-handed, 18 seats are two tables of nine,
 * and the final table is formed when nine players are left. The engine still
 * creates its ten seats; the tenth is the empty one. Since 2026-11-12 the same
 * limit governs every table (cash and local games too), so there is one number. */
const int MaxTournamentTableSeats = MAX_TABLE_SEATS;

struct TournamentPayout
{
	int place;      // 1-based
	int permille;   // share of the pool in tenths of a percent
};

struct TournamentEvent
{
	const char *key;      // "sng6" - stored and passed around, never shown
	const char *label;    // what the player sees
	const char *note;     // one line: the question this event asks
	qint64 buyIn;         // cents
	/* The bankroll the event asks for before the seat is offered, in cents;
	 * 0 = the buy-in is the only condition. The same idea as the gate of a
	 * cash limit (PLAN.md section 2): the event is not "too expensive", it is
	 * "too expensive for the bankroll". The first event with a gate is the
	 * double-or-nothing (D-049). */
	qint64 gate;
	int seats;
	/* The field (botprofile.h): "micro", "rookie", "low", "mid", "high".
	 * For the bottom events it follows the price, not the size: the field is
	 * the second dial and moves one step at a time; "low" starts at 2.00. */
	const char *field;
	int levelHands;       // hands per blind level
	int startStack;       // chips every seat starts with
	int smallBlind;       // chips, the first level
	TournamentPayout payouts[MaxTournamentPayouts];
	int payoutCount;

	qint64 pool() const
	{
		return buyIn * seats;
	}
	// What the balance has to reach before the seat can be bought: the gate if
	// there is one, the buy-in otherwise.
	qint64 requiredBalance() const
	{
		return gate > buyIn ? gate : buyIn;
	}
	// What a place pays, 0 for an unpaid one.
	qint64 prizeForPlace(int place) const;
	// The big blind of the first level, for the "how many BB is the stack" line.
	int bigBlind() const
	{
		return smallBlind * 2;
	}
	int stackInBB() const
	{
		return bigBlind() > 0 ? startStack / bigBlind() : 0;
	}
};

int tournamentCount();
const TournamentEvent &tournamentEvent(int index);
// -1 for an unknown key.
int tournamentIndex(const QString &key);
// The event of a stored key; the first event for anything unknown (a state
// written by a newer version cannot be guessed, but it must not crash).
const TournamentEvent &tournamentEventByKey(const QString &key);

}

#endif // TOURNAMENT_H
