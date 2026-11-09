/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#ifndef TOURNAMENTRUN_H
#define TOURNAMENTRUN_H

#include <roster.h>
#include <tournament.h>

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>

class ConfigFile;
class Game;
class Log;
class SilentGui;

/* The table the player sits at.
 *
 * A tournament of several tables has exactly one table somebody watches: the
 * one with the player. The controller does not build that table and does not
 * play its hands - the client owns the Game, the player acts and the bots
 * answer, and the clock of the table is the player's own - but the controller
 * tells it when a hand starts and at which blind, and reads the table back when
 * the hand is over. Everything else (the other tables, the level, the
 * balancing, the places) stays the controller's, which is what keeps the one
 * watched table and the silent ones in lockstep.
 */
class LocalTableHost
{
public:
	virtual ~LocalTableHost() {}

	// The table as the engine left it once the pot of the hand is distributed.
	struct Snapshot
	{
		// Chips in front of every seat, in the order seat() was given the
		// people.
		std::vector<int> stacks;
		// Roster id of the button for the next hand and of the player who paid
		// the big blind in the hand that just ended (-1 if unknown).
		int button = -1;
		int lastBigBlind = -1;
	};

	// Seats these people, in this order, with the button on this roster id.
	// No hand starts here: the controller says when (startHand).
	virtual bool seat(const std::vector<int> &ids, int button, std::string &error) = 0;
	// Starts one hand at this small blind.
	virtual bool startHand(int blind, std::string &error) = 0;
	// The table after that hand. Only asked while the hand is over.
	virtual Snapshot snapshot() const = 0;
};

/* A tournament played out on several tables (PLAN.md section 3, "Технические
 * решения").
 *
 * One step = one hand at every table that still has players. The tables are in
 * lockstep, so the level, the blinds and the bubble are the same everywhere
 * without a clock to synchronise: the level is counted from the steps, and the
 * controller sets the blind of every table before the hand.
 *
 * A table is a Game of its own with a silent GUI (SilentGui): the engine is
 * stepped directly and nothing is drawn. That is what lets the other tables of
 * a tournament play while the player sits at one of them.
 *
 * The people are the roster's, not the seats'. A table is rebuilt whenever its
 * seating changed - a bust, a break, a balancing move - and the rebuild seats
 * the same participants with the same strength, archetype, name and stack. The
 * engine only plays the hands: the controller owns the level, the button and
 * the balancing.
 *
 * Two ways in: run() plays every table itself (the headless simulator), and
 * begin()/startLocalHand()/step() hand one table to a LocalTableHost (the
 * client), which is the same tournament with the player's table played by the
 * UI instead of the controller.
 */
class TournamentRun
{
public:
	// What a finished (or unfinished) run produced.
	struct Result
	{
		bool finished = false;      // one player left
		int handsPlayed = 0;        // steps, i.e. hands at every table
		int level = 1;              // 1-based
		int moves = 0;              // balancing moves
		int winner = -1;            // roster id, -1 while undecided
		// 1-based place per roster id, 0 for a place the run never decided.
		std::vector<int> places;
		// How often each participant changed table.
		std::vector<int> movesById;
		// Chips in play are the same before and after: no pot is lost.
		bool chipsConserved = false;
		// Every table had the same blind at every step.
		bool blindsAligned = true;
		// The largest difference in table size seen after a rebalance.
		int worstImbalance = 0;
		// The largest table of the whole run, in people: nine is the tournament
		// limit, so a larger number means a seating bug.
		int biggestTable = 0;
		// How many players were left when the field came together at the final
		// table, -1 if it never did. The merge is due at nine, but two busts in
		// one step can bring it down harder, so this is a range in the tests.
		int finalTableAt = -1;
	};

	TournamentRun(ConfigFile *config, const PlayerState::TournamentEvent &event, Log *log);
	~TournamentRun();

	// Seats the roster and plays until one player is left, the hand limit is
	// reached or the engine reports an error. Every table is silent.
	bool run(const PlayerState::Roster &roster, int maxHands, std::string &error);

	// The client's way in: the table with hostId is played by the host, every
	// other table by the controller. begin() seats the field and seats the
	// host's table; startLocalHand() starts the first hand there; step() is
	// called every time that hand is over and does the rest of the step
	// (the silent tables, the busts, the moves) - it leaves the host's table
	// ready but does not start a hand, so the caller can pause first.
	bool begin(const PlayerState::Roster &roster, LocalTableHost *host, int hostId,
			   std::string &error);
	bool startLocalHand(std::string &error);
	bool step(std::string &error);

	// The roster after the run: the stacks are the ones the tables ended with.
	const PlayerState::Roster &roster() const
	{
		return m_roster;
	}
	const Result &result() const
	{
		return m_result;
	}
	// The level of the running step, 0-based; the blind is the event's small
	// blind doubled that many times.
	int level() const;
	int blind() const;
	int playersLeft() const;

	// Is the host's player still in chips?
	bool hostStillIn() const;
	// 1-based place of a roster id, 0 while he is still playing.
	int placeOf(int id) const;
	int tableCount() const
	{
		return static_cast<int>(m_tables.size());
	}
	// How many seats a table of this event has: the field is spread evenly, so
	// 18 seats are two tables of nine, 45 are five of nine, 180 is eighteen of
	// ten. The final table is formed when the field fits into fewer tables.
	int tableCapacity() const;
	// The people at the host's table, in seat order (the host's player first).
	std::vector<int> localIds() const;
	// The people at any table, in seat order; empty for a table number the
	// tournament does not have.
	std::vector<int> idsAt(int table) const;

private:
	struct Table
	{
		int number = 0;
		// Roster ids in seat order. The engine packs seats, so there are no
		// holes: a rebuild writes this list as seats 0..n-1.
		std::vector<int> ids;
		// Roster id of the button for the next hand, not the seat: a rebuild
		// keeps the button on the same person.
		unsigned button = 0;
		// Roster id that paid the big blind in the last hand; that is the player
		// a balancing move takes, so he does not pay it twice in a row.
		int lastBigBlind = -1;
		boost::shared_ptr<SilentGui> gui;
		boost::shared_ptr<Game> game;
		// The seating changed since the table was built.
		bool dirty = true;
		// This is the host's table: the controller does not build it and does
		// not play its hands.
		bool local = false;
	};

	void reset(const PlayerState::Roster &roster);
	bool seatTables(std::string &error);
	bool buildTable(Table &table, std::string &error);
	unsigned buttonOf(const Table &table) const;
	Table *localTable();
	const Table *localTable() const;
	void updateLocalFlags();
	bool seatLocalTable(std::string &error);
	bool playSilentTables(std::string &error);
	void collectBusts();
	void rebalance();
	// Breaks one table if the field fits into fewer tables than there are now.
	// True when a table was broken and its people were seated elsewhere.
	bool consolidateOne();
	int moveOne(Table &from, Table &to);
	void closeEmptyTables();
	int largestTable() const;
	// Difference in size between the fullest and the emptiest table.
	int imbalance() const;
	void assignPlaces();
	// The numbers of the result that are read from the tables now.
	void fillResult();

	ConfigFile *m_config;
	const PlayerState::TournamentEvent m_event;
	Log *m_log;
	PlayerState::Roster m_roster;
	std::vector<Table> m_tables;
	// Roster ids in the order they busted: the first one out is the last place.
	std::vector<int> m_finishOrder;
	int m_handsPlayed = 0;
	int m_moves = 0;
	int m_chipsInPlay = 0;
	// The client's table (see LocalTableHost), NULL in the headless run.
	LocalTableHost *m_host = nullptr;
	int m_hostId = -1;
	Result m_result;
};

#endif // TOURNAMENTRUN_H
