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

bool Wallet::canSitDown(qint64 buyIn, const BankRules &rules, QString *reason) const
{
	QString why;
	bool ok = false;

	if(m_document.cash.active()) {
		why = QStringLiteral("already sitting at a table");
	} else if(buyIn <= 0) {
		why = QStringLiteral("the buy-in has to be positive");
	} else if(buyIn > m_document.profile.balance) {
		why = QStringLiteral("the balance does not cover this buy-in");
	} else if(buyIn > rules.maxBuyIn()) {
		why = QStringLiteral("above the table maximum");
	} else if(m_document.profile.balance >= rules.minBuyIn() && buyIn < rules.minBuyIn()) {
		// Regular rules: no deliberately short stacks while the money lasts.
		why = QStringLiteral("below the table minimum");
	} else if(m_document.profile.balance < rules.minBuyIn() && buyIn < rules.shortBuyIn()) {
		why = QStringLiteral("below the minimum for a short stack");
	} else {
		ok = true;
	}

	if(reason) {
		*reason = why;
	}
	return ok;
}

qint64 Wallet::suggestedBuyIn(const BankRules &rules) const
{
	const qint64 balanceNow = m_document.profile.balance;
	qint64 buyIn = balanceNow >= rules.minBuyIn() ? rules.minBuyIn() : balanceNow;
	if(balanceNow >= rules.minBuyIn()) {
		buyIn = balanceNow < rules.maxBuyIn() ? balanceNow : rules.maxBuyIn();
	}
	// Whole big blinds only - the table's stack should match its blinds.
	if(rules.bigBlind > 0) {
		buyIn -= buyIn % rules.bigBlind;
	}
	return buyIn;
}

bool Wallet::sitDown(qint64 buyIn, const BankRules &rules, const QDateTime &now)
{
	if(!canSitDown(buyIn, rules)) {
		return false;
	}
	m_document.profile.balance -= buyIn;
	m_document.cash = CashSession();
	m_document.cash.buyIn = buyIn;
	m_document.cash.stack = buyIn;
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

bool Wallet::rebuyAvailable(const BankRules &rules, const QDateTime &now,
							qint64 *secondsLeft, QString *reason) const
{
	QString why;
	bool available = false;
	qint64 left = 0;

	if(m_document.cash.active()) {
		why = QStringLiteral("not while sitting at a table");
	} else if(m_document.profile.balance >= rules.rebuyBelow) {
		why = QStringLiteral("the balance is above the rebuy threshold");
	} else if(m_document.profile.lastRebuyAt.isValid()) {
		const qint64 elapsed = m_document.profile.lastRebuyAt.secsTo(now);
		if(elapsed < rules.rebuyCooldownSec) {
			left = rules.rebuyCooldownSec - elapsed;
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

bool Wallet::takeRebuy(const BankRules &rules, const QDateTime &now)
{
	if(!rebuyAvailable(rules, now)) {
		return false;
	}
	m_document.profile.balance += rules.rebuyAmount;
	m_document.profile.lastRebuyAt = now;
	return true;
}

}
