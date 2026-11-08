/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "wallethandler.h"

#include <QDebug>
#include <QTimer>

using PlayerState::BankRules;

WalletHandler::WalletHandler(const QString &directory, QObject *parent)
	: QObject(parent)
{
	m_store.reset(new PlayerState::StateStore(directory));
	m_clock = new QTimer(this);
	m_clock->setInterval(1000);
	connect(m_clock, &QTimer::timeout, this, &WalletHandler::refresh);
	open();
}

BankRules WalletHandler::rules() const
{
	return BankRules();
}

int WalletHandler::balance() const
{
	return static_cast<int>(m_wallet->balance());
}

QString WalletHandler::playerName() const
{
	return m_wallet->name();
}

int WalletHandler::handsPlayed() const
{
	return static_cast<int>(m_wallet->handsPlayed());
}

int WalletHandler::handsWon() const
{
	return static_cast<int>(m_wallet->handsWon());
}

QString WalletHandler::stateFilePath() const
{
	return m_store->filePath();
}

QString WalletHandler::stateProblem() const
{
	return m_stateProblem;
}

bool WalletHandler::persistent() const
{
	return m_persistent;
}

QString WalletHandler::notice() const
{
	return m_notice;
}

bool WalletHandler::atTable() const
{
	return m_wallet->atTable();
}

int WalletHandler::tableStack() const
{
	return static_cast<int>(m_wallet->cashSession().stack);
}

int WalletHandler::tableBuyIn() const
{
	return static_cast<int>(m_wallet->cashSession().buyIn);
}

int WalletHandler::bigBlind() const
{
	return static_cast<int>(rules().bigBlind);
}

int WalletHandler::smallBlind() const
{
	return static_cast<int>(rules().smallBlind());
}

int WalletHandler::minBuyIn() const
{
	return static_cast<int>(rules().minBuyIn());
}

int WalletHandler::maxBuyIn() const
{
	return static_cast<int>(rules().maxBuyIn());
}

int WalletHandler::shortBuyIn() const
{
	return static_cast<int>(rules().shortBuyIn());
}

int WalletHandler::suggestedBuyIn() const
{
	return static_cast<int>(m_wallet->suggestedBuyIn(rules()));
}

int WalletHandler::rebuyAmount() const
{
	return static_cast<int>(rules().rebuyAmount);
}

int WalletHandler::rebuyBelow() const
{
	return static_cast<int>(rules().rebuyBelow);
}

bool WalletHandler::canRebuy() const
{
	return m_canRebuy;
}

int WalletHandler::rebuySecondsLeft() const
{
	return m_rebuySecondsLeft;
}

QString WalletHandler::lastError() const
{
	return m_lastError;
}

void WalletHandler::setPlayerName(const QString &name)
{
	const QString trimmed = name.trimmed();
	if(trimmed.isEmpty() || trimmed == m_wallet->name()) {
		return;
	}
	m_wallet->setName(trimmed);
	save();
	emit profileChanged();
}

bool WalletHandler::sitDown(qint64 buyIn)
{
	QString reason;
	if(!m_wallet->canSitDown(buyIn, rules(), &reason)) {
		fail(reason);
		return false;
	}
	if(!m_wallet->sitDown(buyIn, rules(), QDateTime::currentDateTimeUtc())) {
		fail(QStringLiteral("buy-in refused"));
		return false;
	}
	m_lastError.clear();
	save();
	emit balanceChanged();
	emit tableChanged();
	refresh();
	return true;
}

void WalletHandler::noteHandFinished(qint64 stack, bool won)
{
	if(!m_wallet->atTable()) {
		return;
	}
	m_wallet->recordHand(won);
	m_wallet->updateStack(stack);
	save();
	emit profileChanged();
	emit tableChanged();
}

void WalletHandler::leaveTable(qint64 stack)
{
	if(!m_wallet->atTable()) {
		return;
	}
	m_wallet->updateStack(stack);
	m_wallet->standUp(QDateTime::currentDateTimeUtc());
	save();
	emit balanceChanged();
	emit tableChanged();
	refresh();
}

bool WalletHandler::rebuy()
{
	QString reason;
	if(!m_wallet->rebuyAvailable(rules(), QDateTime::currentDateTimeUtc(), nullptr, &reason)) {
		fail(reason);
		return false;
	}
	if(!m_wallet->takeRebuy(rules(), QDateTime::currentDateTimeUtc())) {
		fail(QStringLiteral("rebuy refused"));
		return false;
	}
	m_lastError.clear();
	save();
	emit balanceChanged();
	emit profileChanged();
	refresh();
	return true;
}

void WalletHandler::refresh()
{
	qint64 left = 0;
	const bool available = m_wallet->rebuyAvailable(rules(), QDateTime::currentDateTimeUtc(), &left);
	const int leftSeconds = static_cast<int>(left);
	if(leftSeconds != m_rebuySecondsLeft || available != m_canRebuy) {
		m_canRebuy = available;
		m_rebuySecondsLeft = leftSeconds;
		emit rebuyChanged();
	}
	// Tick only while a cooldown is actually running.
	if(m_rebuySecondsLeft > 0 && !m_clock->isActive()) {
		m_clock->start();
	} else if(m_rebuySecondsLeft == 0 && m_clock->isActive()) {
		m_clock->stop();
	}
}

void WalletHandler::clearNotice()
{
	if(m_notice.isEmpty()) {
		return;
	}
	m_notice.clear();
	emit noticeChanged();
}

void WalletHandler::open()
{
	m_stateProblem.clear();
	QString error;
	PlayerState::StateDocument document;
	const bool loaded = m_store->load(document, error);

	if(loaded) {
		m_persistent = true;
	} else if(m_store->exists()) {
		// The file is there but not readable for us (a newer version, or
		// something that is not JSON at all). Playing on is fine - silently
		// overwriting it is not: a later version may still be able to read it.
		m_persistent = false;
		m_stateProblem = error;
		document = PlayerState::StateStore::newDocument(QString());
	} else {
		m_persistent = true;
		document = PlayerState::StateStore::newDocument(QString());
	}

	m_wallet.reset(new PlayerState::Wallet(document));

	// A session that is still on disk was interrupted (crash, power loss, a
	// killed process). Its last known stack goes back into the balance.
	const qint64 interruptedStack = m_wallet->cashSession().stack;
	const qint64 interruptedBuyIn = m_wallet->cashSession().buyIn;
	if(m_wallet->recoverInterruptedSession(QDateTime::currentDateTimeUtc())) {
		m_notice = tr("Interrupted cash table session: $%1 of the $%2 buy-in went back "
					  "into your balance.")
				   .arg(interruptedStack)
				   .arg(interruptedBuyIn);
		qInfo() << "Wallet: replaced an interrupted cash session,"
				<< interruptedStack << "of" << interruptedBuyIn << "returned";
	}

	save();
	refresh();
	emitAll();
}

void WalletHandler::save()
{
	if(!m_persistent) {
		return;
	}
	QString error;
	if(!m_store->save(m_wallet->document(), error)) {
		m_persistent = false;
		m_stateProblem = error;
		qWarning() << "Wallet: cannot store the state:" << error;
		emit stateProblemChanged();
	}
}

void WalletHandler::emitAll()
{
	emit stateProblemChanged();
	emit balanceChanged();
	emit profileChanged();
	emit tableChanged();
	emit rebuyChanged();
	emit noticeChanged();
}

void WalletHandler::fail(const QString &message)
{
	m_lastError = message;
	qWarning() << "Wallet:" << message;
	emit lastErrorChanged();
}
