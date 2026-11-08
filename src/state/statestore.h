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
 * GNU Affero General Public License for more details.                       *
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

#ifndef STATESTORE_H
#define STATESTORE_H

#include <QDateTime>
#include <QString>

/* Persistent game state.
 *
 * The widgets client keeps everything it remembers in config.xml (settings)
 * and in the .pdb log files (history). Both are the wrong place for the state
 * the single player variant needs: money, wins and - later - the running
 * tournament have to be written frequently, must never be corrupted by a crash
 * and need a schema of their own so that old files keep loading after an
 * update.
 *
 * That is what this store is for: one small JSON document per player profile,
 * written atomically (QSaveFile -> temp file + rename, a crash in the middle
 * leaves the previous version intact) and versioned at the top level.
 *
 * Money is the only value that survives a session (PLAN.md section 2). Chips
 * live inside a table or a tournament and are never stored here - a stack at a
 * cash table goes back into the balance when the table is left.
 */
namespace PlayerState
{

struct PlayerProfile
{
	PlayerProfile()
		: balance(0), handsPlayed(0), handsWon(0), tournamentsPlayed(0), tournamentsWon(0) {}

	QString name;

	// Money, i.e. the permanent currency - not a chip stack.
	qint64 balance;

	// Lifetime counters. Deliberately coarse: the detailed statistics the
	// opponent model needs are per game and stay in the engine.
	qint64 handsPlayed;
	qint64 handsWon;
	qint64 tournamentsPlayed;
	qint64 tournamentsWon;

	QDateTime createdAt;
	QDateTime updatedAt;
	// Invalid as long as no rebuy happened - see PLAN.md for the cooldown.
	QDateTime lastRebuyAt;
};

// A seat at a cash table. The money is already gone from the balance when the
// session exists - it is lying in front of the player as chips. That is also
// the crash safety net: the stack of the last finished hand is written on every
// hand end, so a crash or a power loss costs at most one hand, and the money is
// never lost entirely.
struct CashSession
{
	CashSession() : buyIn(0), stack(0) {}

	// Money that was taken from the balance when sitting down (0 = no session).
	qint64 buyIn;
	// Chips in front of the player as of the last finished hand.
	qint64 stack;
	// Which limit of the cash ladder the table is played at (see wallet.h).
	// Empty for a session written before the ladder existed - it then reads as
	// the top limit, which was the only table there was.
	QString limit;

	QDateTime startedAt;

	bool active() const
	{
		return buyIn > 0;
	}
};

// One stored game state: the permanent profile plus - if the player is sitting
// at a table - the interrupted cash session.
struct StateDocument
{
	PlayerProfile profile;
	CashSession cash;
};

/* Money is stored in cents (PLAN.md section 2). The reason is the bottom of
 * the cash ladder: a micro limit plays a cent as its small blind, the engine
 * derives the big blind from it and needs integral blinds, so a cent is the
 * finest unit that can be dealt. Everything that is money - the balance, a
 * buy-in, a stack - is cents; only the display is in dollars.
 *
 * 2000 is $20.00: exactly the gate of the bottom limit (1000 BB of 0.01/0.02),
 * so a new profile starts where the ladder starts and has to play its way up.
 */
const qint64 StartingBalance = 2000;

class StateStore
{
public:
	// The directory is created on the first save. The caller decides where it
	// is: the client passes its config directory, the simulator a scratch
	// directory.
	explicit StateStore(const QString &directory);

	QString directory() const
	{
		return m_directory;
	}
	QString filePath() const;

	bool exists() const;

	// Reads the stored document.
	//
	// Returns false and sets error when the file is unreadable, not valid JSON
	// or was written by a newer version of the game. A corrupt file is moved
	// aside (state.json.corrupt) so that the next save does not silently
	// overwrite what a later version might still have been able to read. A file
	// from a newer version is left untouched.
	bool load(StateDocument &document, QString &error) const;

	// Writes the document atomically and refreshes its updatedAt.
	bool save(StateDocument &document, QString &error) const;

	// The document of a first time player: the starting balance, no cash session.
	static StateDocument newDocument(const QString &name);

	// The profile of a first time player (without the cash session).
	static PlayerProfile newProfile(const QString &name);

	// Version of the on-disk format; bump it whenever the layout changes and
	// teach load() how to migrate the older files.
	static int currentSchemaVersion();

private:
	bool moveCorruptFileAside() const;

	QString m_directory;
};

}

#endif
