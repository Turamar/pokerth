/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef WALLETHANDLER_H
#define WALLETHANDLER_H

#include <QDateTime>
#include <QObject>
#include <QString>

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

	Q_PROPERTY(int bigBlind READ bigBlind CONSTANT)
	Q_PROPERTY(int smallBlind READ smallBlind CONSTANT)
	Q_PROPERTY(int minBuyIn READ minBuyIn CONSTANT)
	Q_PROPERTY(int maxBuyIn READ maxBuyIn CONSTANT)
	Q_PROPERTY(int shortBuyIn READ shortBuyIn CONSTANT)
	Q_PROPERTY(int suggestedBuyIn READ suggestedBuyIn NOTIFY balanceChanged)

	Q_PROPERTY(int rebuyAmount READ rebuyAmount CONSTANT)
	Q_PROPERTY(int rebuyBelow READ rebuyBelow CONSTANT)
	Q_PROPERTY(bool canRebuy READ canRebuy NOTIFY rebuyChanged)
	Q_PROPERTY(int rebuySecondsLeft READ rebuySecondsLeft NOTIFY rebuyChanged)
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
	// The state file lives in directory (normally the config directory).
	explicit WalletHandler(const QString &directory, QObject *parent = nullptr);

	// ── what the GameHandler uses ────────────────────────────────────────────
	// Takes the buy-in out of the balance. False if it is not allowed right now.
	bool sitDown(qint64 buyIn);
	// Keeps the stack current; called at the end of every hand.
	void noteHandFinished(qint64 stack, bool won);
	// Back to the lobby: the stack returns into the balance.
	void leaveTable(qint64 stack);

	// ── QML ──────────────────────────────────────────────────────────────────
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
	int bigBlind() const;
	int smallBlind() const;
	int minBuyIn() const;
	int maxBuyIn() const;
	int shortBuyIn() const;
	int suggestedBuyIn() const;
	int rebuyAmount() const;
	int rebuyBelow() const;
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
	PlayerState::BankRules rules() const;

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
