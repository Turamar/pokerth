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
 *                                                                           *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/

#ifndef WALLET_H
#define WALLET_H

#include <QDateTime>
#include <QString>

#include "statestore.h"
#include "tournament.h"

/* The player's bank (PLAN.md section 2).
 *
 * Two kinds of value, and only one of them is permanent:
 *
 *   money  the balance of the profile. It changes by exactly four things: a
 *          cash table buy-in, the stack coming back when the table is left, a
 *          rebuy, and later a tournament buy-in / payout.
 *   chips  a stack. It exists only inside a table or a tournament.
 *
 * This class is the arithmetic of that - no Qt widgets, no I/O, so the
 * simulator and the self test can exercise it cheaply. The client wraps it in
 * WalletHandler (a QObject for QML), the state on disk is a StateDocument.
 *
 * A cash table plays 1:1, i.e. the chips at the table are money - one cent per
 * chip. The ladder, its gates and the rebuy are the numbers documented in
 * PLAN.md section 2.
 */
namespace PlayerState
{

/* The cash ladder (PLAN.md section 2).
 *
 * A cash table of a lower limit is not "the same game with less money": it is
 * the step the player stands on, and the balance decides which steps are open.
 * The gate of a limit is a bankroll of ten maximal buy-ins, i.e. 1000 big
 * blinds; below that the limit is shown but not offered.
 *
 * The engine derives the big blind from the small one (2*SB, see
 * localberopreflop), so a limit is one number in cents.
 */
const qint64 MinBuyInBB = 20;
const qint64 MaxBuyInBB = 100;
const qint64 GateBB = 1000;

struct CashLimit
{
	const char *key;     // "nl2" - stored in the session, never shown
	const char *label;   // "$0.01/$0.02" - what the interface shows
	/* The field the table deals, i.e. the tier of the ladder (botprofile.h):
	 * "micro", "rookie", "low", "mid", "high". A step is harder not only
	 * because its blinds are bigger - the opponents are the other half of it.
	 */
	const char *field;
	qint64 smallBlind;   // cents
	// The bankroll the step needs, in big blinds of this limit.
	//
	// Every step above the bottom asks for GateBB (ten maximal buy-ins). The
	// bottom step asks only for a seat (MinBuyInBB): closing it would mean "you
	// cannot play at all", and losing one buy-in at the lowest limit must not
	// do that - the player then sits down shorter instead of waiting.
	qint64 gateBB;

	qint64 bigBlind() const
	{
		return smallBlind * 2;
	}
	qint64 minBuyIn() const
	{
		return bigBlind() * MinBuyInBB;
	}
	qint64 maxBuyIn() const
	{
		return bigBlind() * MaxBuyInBB;
	}
	qint64 gate() const
	{
		return bigBlind() * gateBB;
	}
	bool unlocked(qint64 balance) const
	{
		return balance >= gate();
	}
	bool bottom() const
	{
		return gateBB < GateBB;
	}
};

int cashLimitCount();
const CashLimit &cashLimit(int index);
// The limit of a stored key. An unknown or empty key - a session written before
// the ladder existed - reads as the top limit, which is the table it was played
// on: there was only one.
const CashLimit &cashLimitByKey(const QString &key);
int cashLimitIndex(const QString &key);
// How many limits the balance opens; 0 means the balance is below the bottom.
int unlockedCashLimits(qint64 balance);
const char *cashLimitList();

// The rebuy tops the balance up to the gate of the bottom limit once per
// cooldown, and never while sitting at a table.
const qint64 RebuyCooldownSec = 60 * 60;

class Wallet
{
public:
	Wallet();
	explicit Wallet(const StateDocument &document);

	const StateDocument &document() const
	{
		return m_document;
	}
	StateDocument &document()
	{
		return m_document;
	}

	qint64 balance() const;
	QString name() const;
	void setName(const QString &name);

	qint64 handsPlayed() const;
	qint64 handsWon() const;

	// ── cash table ───────────────────────────────────────────────────────────
	bool atTable() const;
	const CashSession &cashSession() const;

	// Checks a buy-in against the balance and the limits of the chosen limit.
	// On refusal the reason is a short diagnostic text - the UI phrases its own
	// message; this one goes into the log and lastError.
	bool canSitDown(qint64 buyIn, const CashLimit &limit, QString *reason = nullptr) const;
	// The buy-in a fresh page should offer: as deep as the limit and the
	// balance allow.
	qint64 suggestedBuyIn(const CashLimit &limit) const;

	// Takes the money out of the balance and opens the session.
	bool sitDown(qint64 buyIn, const CashLimit &limit, const QDateTime &now);
	// Keeps the stack of the last finished hand up to date (crash safety).
	void updateStack(qint64 stack);
	// Ends the session: the stack goes back into the balance. Returns the stack.
	qint64 standUp(const QDateTime &now);
	// After loading a document: a session that is still there was interrupted
	// (crash, power loss, killed process). Its last known stack is returned.
	bool recoverInterruptedSession(const QDateTime &now);

	void recordHand(bool won);

	// ── tournament ───────────────────────────────────────────────────────────
	// A tournament has no session on disk: the buy-in leaves the balance when
	// the table starts, and a tournament that is left or lost is gone
	// (PLAN.md, "Экономика" - no save, no resume).
	bool canEnterTournament(const TournamentEvent &event, QString *reason = nullptr) const;
	bool enterTournament(const TournamentEvent &event);
	// The buy-in goes back when the table could not be started after all.
	void refundTournament(qint64 amount);
	// Pays the prize of a place; returns what was paid (0 for an unpaid place).
	qint64 payTournamentPrize(const TournamentEvent &event, int place);

	// ── rebuy ────────────────────────────────────────────────────────────────
	// Only while the balance is below the bottom gate and the cooldown has
	// passed; not at a table. secondsLeft (if given) is how long the cooldown
	// still runs.
	bool rebuyAvailable(const QDateTime &now,
						qint64 *secondsLeft = nullptr, QString *reason = nullptr) const;
	// Tops the balance up to the bottom gate. amount (if given) is what was
	// added.
	bool takeRebuy(const QDateTime &now, qint64 *amount = nullptr);

private:
	StateDocument m_document;
};

}

#endif // WALLET_H
