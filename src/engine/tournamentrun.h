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
	};

	TournamentRun(ConfigFile *config, const PlayerState::TournamentEvent &event, Log *log);
	~TournamentRun();

	// Seats the roster and plays until one player is left, the hand limit is
	// reached or the engine reports an error.
	bool run(const PlayerState::Roster &roster, int maxHands, std::string &error);

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
	};

	bool seatTables(std::string &error);
	bool buildTable(Table &table, std::string &error);
	unsigned buttonOf(const Table &table) const;
	bool playStep(std::string &error);
	void collectBusts();
	void rebalance();
	int moveOne(Table &from, Table &to);
	void closeEmptyTables();
	int largestTable() const;
	// Difference in size between the fullest and the emptiest table.
	int imbalance() const;
	void assignPlaces();

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
	Result m_result;
};

#endif // TOURNAMENTRUN_H
