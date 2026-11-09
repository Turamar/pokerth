/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef WALLETHANDLER_H
#define WALLETHANDLER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

#include <statestore.h>
#include <wallet.h>

class QTimer;

/* The player's bank as the QML side sees it: the balance, the profile
 * counters, the cash table session and the rebuy.
 *
 * It owns the persistent state document (state.json next to config.xml) and is
 * the only place that writes it. The GameHandler calls into it on the three
 * moments that change money:
 *
 *   startCashGame()  -> sitDown(), the money leaves the balance
 *   hand finished    -> recordHand() + updateStack(), the stack is kept current
 *   endLocalGame()   -> leaveTable(), the stack goes back into the balance
 *
 * See PLAN.md section 2 and wallet.h for the rules.
 */
class WalletHandler : public QObject
{
	Q_OBJECT

	Q_PROPERTY(int balance READ balance NOTIFY balanceChanged)
	Q_PROPERTY(QString playerName READ playerName NOTIFY profileChanged)
	Q_PROPERTY(int handsPlayed READ handsPlayed NOTIFY profileChanged)
	Q_PROPERTY(int handsWon READ handsWon NOTIFY profileChanged)
	// Where the state lives - shown on the wallet page, also for support.
	Q_PROPERTY(QString stateFilePath READ stateFilePath CONSTANT)
	// Empty while everything is fine. Otherwise the reason why nothing is being
	// stored (a file of a newer version, a directory that cannot be written).
	Q_PROPERTY(QString stateProblem READ stateProblem NOTIFY stateProblemChanged)
	Q_PROPERTY(bool persistent READ persistent NOTIFY stateProblemChanged)
	// A note for the player, e.g. that an interrupted cash session was returned.
	Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)

	Q_PROPERTY(bool atTable READ atTable NOTIFY tableChanged)
	Q_PROPERTY(int tableStack READ tableStack NOTIFY tableChanged)
	Q_PROPERTY(int tableBuyIn READ tableBuyIn NOTIFY tableChanged)
	Q_PROPERTY(QString tableLimit READ tableLimit NOTIFY tableChanged)
	Q_PROPERTY(int tableLimitIndex READ tableLimitIndex NOTIFY tableChanged)

	// The cash ladder, cheapest first. Every entry is a map: key, label (the
	// blinds as the player reads them), smallBlind, bigBlind, minBuyIn,
	// maxBuyIn, gate and unlocked - the last one says whether the balance is
	// high enough to sit down there (PLAN.md section 2).
	Q_PROPERTY(QVariantList limits READ limits NOTIFY balanceChanged)
	// The tournament catalogue (PLAN.md section 3), one map per event: key,
	// label, note, seats, buyIn, field, levelHands, stackInBB, firstPrize,
	// prizes (place/amount pairs) and unlocked (the balance covers the buy-in).
	Q_PROPERTY(QVariantList tournaments READ tournaments NOTIFY balanceChanged)
	// How many limits the balance opens; 0 means "no seat at all".
	Q_PROPERTY(int unlockedLimits READ unlockedLimits NOTIFY balanceChanged)

	// The rebuy tops the balance up to this - the gate of the bottom limit.
	Q_PROPERTY(int rebuyTarget READ rebuyTarget CONSTANT)
	Q_PROPERTY(bool canRebuy READ canRebuy NOTIFY rebuyChanged)
	Q_PROPERTY(int rebuySecondsLeft READ rebuySecondsLeft NOTIFY rebuyChanged)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
	// The state file lives in directory (normally the config directory).
	explicit WalletHandler(const QString &directory, QObject *parent = nullptr);

	// ── what the GameHandler uses ────────────────────────────────────────────
	// Takes the buy-in out of the balance and remembers the limit of the table.
	// False if it is not allowed right now.
	bool sitDown(qint64 buyIn, int limitIndex);
	// Keeps the stack current; called at the end of every hand.
	void noteHandFinished(qint64 stack, bool won);
	// Back to the lobby: the stack returns into the balance.
	void leaveTable(qint64 stack);

	// ── tournament (PLAN.md section 3) ───────────────────────────────────────
	// The buy-in leaves the balance; there is no session to keep, so a
	// tournament that is left, lost or crashed is simply gone.
	bool enterTournament(const QString &key);
	// The buy-in returns if the table could not be started after all.
	void refundTournament(const QString &key);
	// Pays the prize of a place; returns what was paid (0 for an unpaid one).
	qint64 payTournamentPrize(const QString &key, int place);
	// One hand of a tournament: only the profile counters move.
	void noteTournamentHand(bool won);

	// ── QML ──────────────────────────────────────────────────────────────────
	// Money as the player reads it: cents in, "$0.40" out, cents left off when
	// they are zero. Every amount shown in the interface goes through here -
	// money is stored in cents (see statestore.h) and must never leak as a
	// number into a label.
	Q_INVOKABLE QString money(int cents) const;
	Q_INVOKABLE bool rebuy();
	// Re-reads clock dependent state (the rebuy cooldown).
	Q_INVOKABLE void refresh();
	Q_INVOKABLE void clearNotice();
	// The profile name follows the configured player name.
	Q_INVOKABLE void setPlayerName(const QString &name);

	int balance() const;
	QString playerName() const;
	int handsPlayed() const;
	int handsWon() const;
	QString stateFilePath() const;
	QString stateProblem() const;
	bool persistent() const;
	QString notice() const;
	bool atTable() const;
	int tableStack() const;
	int tableBuyIn() const;
	QString tableLimit() const;
	int tableLimitIndex() const;
	QVariantList limits() const;
	QVariantList tournaments() const;
	int unlockedLimits() const;
	int rebuyTarget() const;
	bool canRebuy() const;
	int rebuySecondsLeft() const;
	QString lastError() const;

signals:
	void balanceChanged();
	void profileChanged();
	void tableChanged();
	void rebuyChanged();
	void stateProblemChanged();
	void noticeChanged();
	void lastErrorChanged();

private:
	void open();
	void save();
	void emitAll();
	void fail(const QString &message);

	std::unique_ptr<PlayerState::StateStore> m_store;
	std::unique_ptr<PlayerState::Wallet> m_wallet;
	QTimer *m_clock = nullptr;
	bool m_persistent = true;
	// The cached rebuy state (the cooldown ticks, so it cannot be a pure getter).
	bool m_canRebuy = false;
	int m_rebuySecondsLeft = 0;
	QString m_stateProblem;
	QString m_notice;
	QString m_lastError;
};

#endif // WALLETHANDLER_H
