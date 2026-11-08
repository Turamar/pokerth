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
 * A cash table plays 1:1, i.e. the chips at the table are dollars. The rules
 * below are the ones documented in PLAN.md; the thresholds that are still open
 * there (rebuy threshold) have concrete numbers here and are re-checked with
 * the simulator.
 */
namespace PlayerState
{

// The numbers of the single player economy.
struct BankRules
{
	BankRules()
		: bigBlind(2), minBuyInBB(20), maxBuyInBB(100), shortStackMinBB(5),
		  rebuyAmount(20), rebuyBelow(40), rebuyCooldownSec(60 * 60) {}

	// The cash table plays "money" as chips, so a dollar is one chip. The
	// engine needs integral blinds, and the smallest sane big blind is 2 - that
	// is what sets the scale: 20-100 BB is a buy-in of 40-200 dollars.
	qint64 bigBlind;
	qint64 minBuyInBB;
	qint64 maxBuyInBB;
	// Below the regular minimum buy-in the rest of the balance is allowed, but
	// never less than this - otherwise the rebuy (20 dollars) could not buy a
	// seat and the player would be locked out of the game for good.
	qint64 shortStackMinBB;

	qint64 rebuyAmount;
	// A rebuy is offered while the balance is below this.
	qint64 rebuyBelow;
	qint64 rebuyCooldownSec;

	qint64 smallBlind() const
	{
		return bigBlind / 2;
	}
	qint64 minBuyIn() const
	{
		return minBuyInBB * bigBlind;
	}
	qint64 maxBuyIn() const
	{
		return maxBuyInBB * bigBlind;
	}
	qint64 shortBuyIn() const
	{
		return shortStackMinBB * bigBlind;
	}
};

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

	// Checks a buy-in against the balance and the table limits. On refusal the
	// reason is a short diagnostic text - the UI phrases its own message; this
	// one goes into the log and lastError.
	bool canSitDown(qint64 buyIn, const BankRules &rules, QString *reason = nullptr) const;
	// The buy-in a fresh page should offer: as deep as the balance allows.
	qint64 suggestedBuyIn(const BankRules &rules) const;

	// Takes the money out of the balance and opens the session.
	bool sitDown(qint64 buyIn, const BankRules &rules, const QDateTime &now);
	// Keeps the stack of the last finished hand up to date (crash safety).
	void updateStack(qint64 stack);
	// Ends the session: the stack goes back into the balance. Returns the stack.
	qint64 standUp(const QDateTime &now);
	// After loading a document: a session that is still there was interrupted
	// (crash, power loss, killed process). Its last known stack is returned.
	bool recoverInterruptedSession(const QDateTime &now);

	void recordHand(bool won);

	// ── rebuy ────────────────────────────────────────────────────────────────
	// Only while the balance is low and the cooldown has passed; not at a
	// table. secondsLeft (if given) is how long the cooldown still runs.
	bool rebuyAvailable(const BankRules &rules, const QDateTime &now,
						qint64 *secondsLeft = nullptr, QString *reason = nullptr) const;
	bool takeRebuy(const BankRules &rules, const QDateTime &now);

private:
	StateDocument m_document;
};

}

#endif // WALLET_H
