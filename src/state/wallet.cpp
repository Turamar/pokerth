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

#include "wallet.h"

namespace PlayerState
{

namespace
{

/* The ladder (PLAN.md section 2), cheapest first. Every limit has the shape the
 * engine needs - the big blind is twice the small one - so a limit is one
 * number plus the bankroll it asks for. The bottom step needs only a seat:
 * "$20 opens the micro limit" is where the ladder becomes comfortable, but
 * losing one buy-in there must not lock the player out of the game.
 *
 * The third column is the field the table deals (botprofile.h): the ladder is
 * blinds plus composition, and the composition is what makes a step harder.
 */
const CashLimit LADDER[] = {
	{ "nl2",   "$0.01/$0.02", "micro",  1,   MinBuyInBB },
	{ "nl4",   "$0.02/$0.04", "rookie", 2,   GateBB },
	{ "nl10",  "$0.05/$0.10", "low",    5,   GateBB },
	{ "nl50",  "$0.25/$0.50", "mid",    25,  GateBB },
	{ "nl200", "$1/$2",       "high",   100, GateBB }
};
const int LadderSize = static_cast<int>(sizeof(LADDER) / sizeof(LADDER[0]));

}

int cashLimitCount()
{
	return LadderSize;
}

const CashLimit &cashLimit(int index)
{
	if(index < 0 || index >= LadderSize) {
		return LADDER[0];
	}
	return LADDER[index];
}

int cashLimitIndex(const QString &key)
{
	for(int i = 0; i < LadderSize; ++i) {
		if(key == QLatin1String(LADDER[i].key)) {
			return i;
		}
	}
	return -1;
}

const CashLimit &cashLimitByKey(const QString &key)
{
	const int index = cashLimitIndex(key);
	return index >= 0 ? LADDER[index] : LADDER[LadderSize - 1];
}

int unlockedCashLimits(qint64 balance)
{
	int count = 0;
	for(int i = 0; i < LadderSize; ++i) {
		if(LADDER[i].unlocked(balance)) {
			count = i + 1;
		}
	}
	return count;
}

const char *cashLimitList()
{
	return "nl2, nl4, nl10, nl50, nl200";
}

Wallet::Wallet()
	: m_document(StateStore::newDocument(QString()))
{
}

Wallet::Wallet(const StateDocument &document)
	: m_document(document)
{
}

qint64 Wallet::balance() const
{
	return m_document.profile.balance;
}

QString Wallet::name() const
{
	return m_document.profile.name;
}

void Wallet::setName(const QString &name)
{
	m_document.profile.name = name;
}

qint64 Wallet::handsPlayed() const
{
	return m_document.profile.handsPlayed;
}

qint64 Wallet::handsWon() const
{
	return m_document.profile.handsWon;
}

bool Wallet::atTable() const
{
	return m_document.cash.active();
}

const CashSession &Wallet::cashSession() const
{
	return m_document.cash;
}

bool Wallet::canSitDown(qint64 buyIn, const CashLimit &limit, QString *reason) const
{
	QString why;
	bool ok = false;

	if(m_document.cash.active()) {
		why = QStringLiteral("already sitting at a table");
	} else if(buyIn <= 0) {
		why = QStringLiteral("the buy-in has to be positive");
	} else if(!limit.unlocked(m_document.profile.balance)) {
		// The gate comes first: with too small a bankroll the buy-in bounds are
		// not the interesting part of the refusal.
		why = QStringLiteral("the bankroll is too small for this limit");
	} else if(buyIn > m_document.profile.balance) {
		why = QStringLiteral("the balance does not cover this buy-in");
	} else if(buyIn > limit.maxBuyIn()) {
		why = QStringLiteral("above the table maximum");
	} else if(buyIn < limit.minBuyIn()) {
		why = QStringLiteral("below the table minimum");
	} else {
		ok = true;
	}

	if(reason) {
		*reason = why;
	}
	return ok;
}

qint64 Wallet::suggestedBuyIn(const CashLimit &limit) const
{
	const qint64 balanceNow = m_document.profile.balance;
	qint64 buyIn = balanceNow < limit.maxBuyIn() ? balanceNow : limit.maxBuyIn();
	// Whole big blinds only - the table's stack should match its blinds.
	buyIn -= buyIn % limit.bigBlind();
	if(buyIn < limit.minBuyIn()) {
		// A balance that cannot afford the minimum is still offered as a whole,
		// so that the page can say how much is missing.
		buyIn = balanceNow - (balanceNow % limit.bigBlind());
	}
	return buyIn > 0 ? buyIn : 0;
}

bool Wallet::sitDown(qint64 buyIn, const CashLimit &limit, const QDateTime &now)
{
	if(!canSitDown(buyIn, limit)) {
		return false;
	}
	m_document.profile.balance -= buyIn;
	m_document.cash = CashSession();
	m_document.cash.buyIn = buyIn;
	m_document.cash.stack = buyIn;
	m_document.cash.limit = QLatin1String(limit.key);
	m_document.cash.startedAt = now;
	return true;
}

void Wallet::updateStack(qint64 stack)
{
	if(!m_document.cash.active()) {
		return;
	}
	m_document.cash.stack = stack > 0 ? stack : 0;
}

qint64 Wallet::standUp(const QDateTime &now)
{
	if(!m_document.cash.active()) {
		return 0;
	}
	const qint64 stack = m_document.cash.stack;
	m_document.profile.balance += stack;
	m_document.cash = CashSession();
	m_document.profile.updatedAt = now;
	return stack;
}

bool Wallet::recoverInterruptedSession(const QDateTime &now)
{
	if(!m_document.cash.active()) {
		return false;
	}
	standUp(now);
	return true;
}

void Wallet::recordHand(bool won)
{
	m_document.profile.handsPlayed++;
	if(won) {
		m_document.profile.handsWon++;
	}
}

bool Wallet::canEnterTournament(const TournamentEvent &event, QString *reason) const
{
	QString why;
	bool ok = false;

	if(m_document.cash.active()) {
		why = QStringLiteral("already sitting at a table");
	} else if(event.buyIn <= 0) {
		why = QStringLiteral("the buy-in has to be positive");
	} else if(event.buyIn > m_document.profile.balance) {
		why = QStringLiteral("the balance does not cover this buy-in");
	} else {
		ok = true;
	}

	if(reason) {
		*reason = why;
	}
	return ok;
}

bool Wallet::enterTournament(const TournamentEvent &event)
{
	if(!canEnterTournament(event)) {
		return false;
	}
	m_document.profile.balance -= event.buyIn;
	m_document.profile.tournamentsPlayed++;
	m_document.profile.updatedAt = QDateTime::currentDateTimeUtc();
	return true;
}

void Wallet::refundTournament(qint64 amount)
{
	if(amount > 0) {
		m_document.profile.balance += amount;
	}
}

qint64 Wallet::payTournamentPrize(const TournamentEvent &event, int place)
{
	const qint64 prize = event.prizeForPlace(place);
	if(prize > 0) {
		m_document.profile.balance += prize;
	}
	if(place == 1) {
		m_document.profile.tournamentsWon++;
	}
	m_document.profile.updatedAt = QDateTime::currentDateTimeUtc();
	return prize;
}

bool Wallet::rebuyAvailable(const QDateTime &now,
							qint64 *secondsLeft, QString *reason) const
{
	QString why;
	bool available = false;
	qint64 left = 0;

	if(m_document.cash.active()) {
		why = QStringLiteral("not while sitting at a table");
	} else if(m_document.profile.balance >= StartingBalance) {
		why = QStringLiteral("the balance is at the floor already");
	} else if(unlockedCashLimits(m_document.profile.balance) == 0) {
		// No seat is open anywhere: waiting out a cooldown with no game to play
		// would leave the player with nothing to do.
		available = true;
	} else if(m_document.profile.lastRebuyAt.isValid()) {
		const qint64 elapsed = m_document.profile.lastRebuyAt.secsTo(now);
		if(elapsed < RebuyCooldownSec) {
			left = RebuyCooldownSec - elapsed;
			why = QStringLiteral("the cooldown is still running");
		} else {
			available = true;
		}
	} else {
		available = true;
	}

	if(secondsLeft) {
		*secondsLeft = left;
	}
	if(reason) {
		*reason = why;
	}
	return available;
}

bool Wallet::takeRebuy(const QDateTime &now, qint64 *amount)
{
	if(!rebuyAvailable(now)) {
		return false;
	}
	const qint64 added = StartingBalance - m_document.profile.balance;
	m_document.profile.balance = StartingBalance;
	m_document.profile.lastRebuyAt = now;
	if(amount) {
		*amount = added;
	}
	return true;
}

}
