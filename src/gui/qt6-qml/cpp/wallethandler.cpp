/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "wallethandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QVariantMap>

WalletHandler::WalletHandler(const QString &directory, QObject *parent)
	: QObject(parent)
{
	m_store.reset(new PlayerState::StateStore(directory));
	m_clock = new QTimer(this);
	m_clock->setInterval(1000);
	connect(m_clock, &QTimer::timeout, this, &WalletHandler::refresh);
	open();
}

int WalletHandler::balance() const
{
	return static_cast<int>(m_wallet->balance());
}

// Money is cents (see statestore.h) and this is the only place that turns it
// into what the player reads. The cents are left off when they are zero, so a
// dollar amount does not grow a needless tail, and they are always two digits
// otherwise - the bottom of the cash ladder plays cents.
QString WalletHandler::money(int cents) const
{
	const bool negative = cents < 0;
	const qint64 value = negative ? -static_cast<qint64>(cents) : cents;
	const qint64 dollars = value / 100;
	const qint64 rest = value % 100;

	QString text = QString::number(dollars);
	if(rest != 0) {
		text += QLatin1Char('.');
		text += QStringLiteral("%1").arg(rest, 2, 10, QLatin1Char('0'));
	}
	return (negative ? QStringLiteral("-$") : QStringLiteral("$")) + text;
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

QString WalletHandler::tableLimit() const
{
	if(!m_wallet->atTable()) {
		return QString();
	}
	return QString::fromLatin1(PlayerState::cashLimitByKey(m_wallet->cashSession().limit).label);
}

int WalletHandler::tableLimitIndex() const
{
	if(!m_wallet->atTable()) {
		return -1;
	}
	const int index = PlayerState::cashLimitIndex(m_wallet->cashSession().limit);
	// A session stored before the ladder existed has no key: it was played on
	// the only table there was, the top one.
	return index >= 0 ? index : PlayerState::cashLimitCount() - 1;
}

// What the page needs to draw the ladder: one map per limit, cheapest first.
QVariantList WalletHandler::limits() const
{
	QVariantList list;
	const qint64 balanceNow = m_wallet->balance();
	for(int i = 0; i < PlayerState::cashLimitCount(); ++i) {
		const PlayerState::CashLimit &limit = PlayerState::cashLimit(i);
		QVariantMap entry;
		entry[QStringLiteral("key")] = QString::fromLatin1(limit.key);
		entry[QStringLiteral("label")] = QString::fromLatin1(limit.label);
		entry[QStringLiteral("smallBlind")] = static_cast<int>(limit.smallBlind);
		entry[QStringLiteral("bigBlind")] = static_cast<int>(limit.bigBlind());
		entry[QStringLiteral("minBuyIn")] = static_cast<int>(limit.minBuyIn());
		entry[QStringLiteral("maxBuyIn")] = static_cast<int>(limit.maxBuyIn());
		entry[QStringLiteral("gate")] = static_cast<int>(limit.gate());
		entry[QStringLiteral("unlocked")] = limit.unlocked(balanceNow);
		list.append(entry);
	}
	return list;
}

int WalletHandler::unlockedLimits() const
{
	return PlayerState::unlockedCashLimits(m_wallet->balance());
}

// The tournament catalogue for the page: one map per event. The prize of every
// paid place is computed here (from the pool, no rake), so that QML never does
// money arithmetic. Label and note are QT_TRANSLATE_NOOP sources in the
// catalogue (state/tournament.cpp) and get their words here.
QVariantList WalletHandler::tournaments() const
{
	QVariantList list;
	const qint64 balanceNow = m_wallet->balance();
	for(int i = 0; i < PlayerState::tournamentCount(); ++i) {
		const PlayerState::TournamentEvent &event = PlayerState::tournamentEvent(i);
		QVariantMap entry;
		entry[QStringLiteral("key")] = QString::fromLatin1(event.key);
		entry[QStringLiteral("label")] =
			QCoreApplication::translate("TournamentEvent", event.label);
		entry[QStringLiteral("note")] =
			QCoreApplication::translate("TournamentEvent", event.note);
		entry[QStringLiteral("seats")] = event.seats;
		entry[QStringLiteral("buyIn")] = static_cast<int>(event.buyIn);
		entry[QStringLiteral("field")] = QString::fromLatin1(event.field);
		entry[QStringLiteral("levelHands")] = event.levelHands;
		entry[QStringLiteral("stackInBB")] = event.stackInBB();
		entry[QStringLiteral("firstPrize")] = static_cast<int>(event.prizeForPlace(1));
		entry[QStringLiteral("unlocked")] = balanceNow >= event.buyIn;
		QVariantList prizes;
		for(int p = 0; p < event.payoutCount; ++p) {
			QVariantMap prize;
			prize[QStringLiteral("place")] = event.payouts[p].place;
			prize[QStringLiteral("amount")] =
				static_cast<int>(event.prizeForPlace(event.payouts[p].place));
			prizes.append(prize);
		}
		entry[QStringLiteral("prizes")] = prizes;
		list.append(entry);
	}
	return list;
}

int WalletHandler::rebuyTarget() const
{
	return static_cast<int>(PlayerState::StartingBalance);
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

bool WalletHandler::sitDown(qint64 buyIn, int limitIndex)
{
	if(limitIndex < 0 || limitIndex >= PlayerState::cashLimitCount()) {
		fail(QStringLiteral("there is no such limit"));
		return false;
	}
	const PlayerState::CashLimit &limit = PlayerState::cashLimit(limitIndex);

	QString reason;
	if(!m_wallet->canSitDown(buyIn, limit, &reason)) {
		fail(reason);
		return false;
	}
	if(!m_wallet->sitDown(buyIn, limit, QDateTime::currentDateTimeUtc())) {
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

bool WalletHandler::enterTournament(const QString &key)
{
	const int index = PlayerState::tournamentIndex(key);
	if(index < 0) {
		fail(QStringLiteral("there is no such tournament"));
		return false;
	}
	const PlayerState::TournamentEvent &event = PlayerState::tournamentEvent(index);
	QString reason;
	if(!m_wallet->canEnterTournament(event, &reason)) {
		fail(reason);
		return false;
	}
	if(!m_wallet->enterTournament(event)) {
		fail(QStringLiteral("buy-in refused"));
		return false;
	}
	m_lastError.clear();
	save();
	emit balanceChanged();
	refresh();
	return true;
}

void WalletHandler::refundTournament(const QString &key)
{
	const int index = PlayerState::tournamentIndex(key);
	if(index < 0) {
		return;
	}
	m_wallet->refundTournament(PlayerState::tournamentEvent(index).buyIn);
	save();
	emit balanceChanged();
	refresh();
}

qint64 WalletHandler::payTournamentPrize(const QString &key, int place)
{
	const int index = PlayerState::tournamentIndex(key);
	if(index < 0) {
		return 0;
	}
	const qint64 prize =
		m_wallet->payTournamentPrize(PlayerState::tournamentEvent(index), place);
	save();
	emit balanceChanged();
	refresh();
	return prize;
}

void WalletHandler::noteTournamentHand(bool won)
{
	// A tournament moves no money per hand - only the profile counters.
	m_wallet->recordHand(won);
	save();
	emit profileChanged();
}

bool WalletHandler::rebuy()
{
	const QDateTime now = QDateTime::currentDateTimeUtc();
	QString reason;
	if(!m_wallet->rebuyAvailable(now, nullptr, &reason)) {
		fail(reason);
		return false;
	}
	if(!m_wallet->takeRebuy(now)) {
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
	const bool available = m_wallet->rebuyAvailable(QDateTime::currentDateTimeUtc(), &left);
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
		m_notice = tr("Interrupted cash table session: %1 of the %2 buy-in went back "
					  "into your balance.")
				   .arg(money(static_cast<int>(interruptedStack)))
				   .arg(money(static_cast<int>(interruptedBuyIn)));
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
