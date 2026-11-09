/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef ROSTER_H
#define ROSTER_H

#include <QString>
#include <QStringList>
#include <QVector>

#include <qglobal.h>

/* The roster of a tournament (PLAN.md section 3, "Ростер").
 *
 * A table in this engine is rebuilt from scratch whenever it has to be reseated
 * - a busted player, a table break, a balancing move. A rebuilt table deals its
 * opponents from the seat number (LocalEngineFactory::createPlayer takes
 * myBots[(id-1) % size]), so without a roster "the player was moved" would also
 * mean "the player got another character".
 *
 * The roster is the list of people, not of seats: one entry per participant,
 * carrying the strength and the archetype chosen when the seat was taken, the
 * current stack and the place at a table. Seating, moving and unseating change
 * the place only; the entry keeps who it is. Seat order, names, profiles and
 * stacks for the engine come from the roster.
 *
 * The profiles are dealt by the caller (BotProfile::dealTier), which keeps this
 * file free of the engine, the same way gameData.h does.
 */
namespace PlayerState
{

struct RosterEntry
{
	int id = 0;        // stable inside the tournament, never reused
	QString name;      // what the player sees
	QString profile;   // "pro/tag" (botprofile.h); empty for the human
	int table = -1;    // -1 while not seated
	int seat = -1;
	int stack = 0;     // chips

	bool seated() const
	{
		return table >= 0 && seat >= 0;
	}
};

class Roster
{
public:
	void clear();
	bool isEmpty() const
	{
		return m_entries.isEmpty();
	}
	int count() const
	{
		return m_entries.size();
	}

	// Adds a participant and returns the id. From here on the profile is fixed:
	// nothing in this class can change it, which is the whole point.
	int add(const QString &name, const QString &profile, int stack);

	// Seats the participant. A previous seat of the same participant is freed;
	// whoever sat in the target seat is unseated (a table is rebuilt anyway).
	bool seat(int id, int table, int seat);
	// Takes the participant off any table; the record and the stack stay.
	void unseat(int id);

	const RosterEntry *entry(int id) const;
	const RosterEntry *entryAt(int table, int seat) const;
	// -1 for an empty seat or a missing table.
	int idAt(int table, int seat) const;
	// Number of tables in use, i.e. the highest table index plus one.
	int tableCount() const;

	int stack(int id) const;
	void setStack(int id, int stack);

	// Participants of a table in seat order, -1 for a free seat. The length is
	// the highest occupied seat of that table plus one.
	QVector<int> seatsAt(int table) const;

	// What the engine needs to build that table, in seat order: names, profiles
	// (empty for the human) and stacks.
	QStringList seatNames(int table) const;
	QStringList seatProfiles(int table) const;
	QVector<int> seatStacks(int table) const;
	// The profiles without the human seat, for gameData.botProfiles.
	QStringList botProfiles(int table) const;

private:
	QVector<RosterEntry> m_entries;
};

}

#endif // ROSTER_H
