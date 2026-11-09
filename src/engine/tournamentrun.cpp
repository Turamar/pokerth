/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "tournamentrun.h"

#include <botprofile.h>
#include <enginefactory.h>
#include <game.h>
#include <game_defs.h>
#include <gamedata.h>
#include <handinterface.h>
#include <localenginefactory.h>
#include <localplayer.h>
#include <playerdata.h>
#include <playerinterface.h>
#include <silentgui.h>

#include <algorithm>
#include <sstream>

using namespace std;

namespace
{
// A hand that needs more steps than this is not a poker hand any more: the
// engine queued something that never ends. The simulator uses the same kind of
// limit for the same reason.
const long long MaxStepsPerHand = 20000;

bool contains(const vector<int> &ids, int id)
{
	return find(ids.begin(), ids.end(), id) != ids.end();
}
}

TournamentRun::TournamentRun(ConfigFile *config, const PlayerState::TournamentEvent &event, Log *log)
	: m_config(config), m_event(event), m_log(log)
{
}

TournamentRun::~TournamentRun()
{
}

int TournamentRun::level() const
{
	if(m_event.levelHands <= 0) return 0;
	return m_handsPlayed / m_event.levelHands;
}

int TournamentRun::blind() const
{
	// The blind doubles with the level, as it does in a single-table tournament.
	int small = m_event.smallBlind;
	// The ceiling is half of the chips in play: past that a hand is a coin flip
	// whatever the blind is, and a blind larger than every stack would only make
	// the last hands meaningless.
	const long long allChips = static_cast<long long>(m_event.startStack) * m_event.seats;
	int ceiling = static_cast<int>(allChips / 2);
	if(ceiling < small) ceiling = small;
	for(int i = 0; i < level() && small < ceiling; ++i) {
		small *= 2;
	}
	if(small > ceiling) small = ceiling;
	return small;
}

int TournamentRun::tableCapacity() const
{
	// Nine-handed, the size of the smaller events: the engine has ten seats but
	// a tournament table never uses the tenth. A smaller event is its own size.
	return m_event.seats < PlayerState::MaxTournamentTableSeats
		   ? m_event.seats : PlayerState::MaxTournamentTableSeats;
}

int TournamentRun::playersLeft() const
{
	int left = 0;
	for(int id = 0; id < m_roster.count(); ++id) {
		const PlayerState::RosterEntry *who = m_roster.entry(id);
		if(who && who->seated() && who->stack > 0) ++left;
	}
	return left;
}

bool TournamentRun::hostStillIn() const
{
	const PlayerState::RosterEntry *who = m_roster.entry(m_hostId);
	return who && who->seated() && who->stack > 0;
}

int TournamentRun::placeOf(int id) const
{
	if(id < 0 || id >= static_cast<int>(m_result.places.size())) return 0;
	return m_result.places[static_cast<size_t>(id)];
}

std::vector<int> TournamentRun::localIds() const
{
	const Table *table = localTable();
	return table ? table->ids : std::vector<int>();
}

std::vector<int> TournamentRun::idsAt(int table) const
{
	if(table < 0 || table >= static_cast<int>(m_tables.size())) return std::vector<int>();
	return m_tables[static_cast<size_t>(table)].ids;
}

TournamentRun::Table *TournamentRun::localTable()
{
	for(size_t i = 0; i < m_tables.size(); ++i) {
		if(m_tables[i].local) return &m_tables[i];
	}
	return nullptr;
}

const TournamentRun::Table *TournamentRun::localTable() const
{
	for(size_t i = 0; i < m_tables.size(); ++i) {
		if(m_tables[i].local) return &m_tables[i];
	}
	return nullptr;
}

void TournamentRun::reset(const PlayerState::Roster &roster)
{
	m_roster = roster;
	m_tables.clear();
	m_finishOrder.clear();
	m_handsPlayed = 0;
	m_moves = 0;
	m_chipsInPlay = 0;
	m_result = Result();
	m_result.places.assign(m_roster.count(), 0);
	m_result.movesById.assign(m_roster.count(), 0);

	for(int id = 0; id < m_roster.count(); ++id) {
		m_chipsInPlay += m_roster.stack(id);
	}
}

bool TournamentRun::run(const PlayerState::Roster &roster, int maxHands, string &error)
{
	if(!begin(roster, nullptr, -1, error)) return false;

	while(m_handsPlayed < maxHands && playersLeft() > 1) {
		if(!step(error)) return false;
	}

	fillResult();
	return true;
}

bool TournamentRun::begin(const PlayerState::Roster &roster, LocalTableHost *host, int hostId,
						  string &error)
{
	error.clear();
	reset(roster);
	m_host = host;
	m_hostId = host ? hostId : -1;

	if(!seatTables(error)) return false;
	updateLocalFlags();
	// The host's table is seated right away: its Game is the client's, and the
	// client needs it before the first hand - there is nothing the controller
	// could play on it.
	if(!seatLocalTable(error)) return false;

	fillResult();
	return true;
}

bool TournamentRun::seatTables(string &error)
{
	// A full field: nine seats at a table (the tournament maximum, one less
	// than the engine's), spread as evenly as the seats allow. 18 players are
	// two tables of nine, 45 are five of nine, 180 is twenty of nine.
	vector<int> ids;
	for(int id = 0; id < m_roster.count(); ++id) {
		const PlayerState::RosterEntry *who = m_roster.entry(id);
		if(who && who->seated() && who->stack > 0) ids.push_back(id);
	}
	if(ids.size() < 2) {
		error = "a tournament needs at least two players";
		return false;
	}

	const int perTable = tableCapacity();
	const int tables = (static_cast<int>(ids.size()) + perTable - 1) / perTable;
	const int base = static_cast<int>(ids.size()) / tables;
	const int extra = static_cast<int>(ids.size()) % tables;

	m_tables.clear();
	size_t next = 0;
	for(int number = 0; number < tables; ++number) {
		Table table;
		table.number = number;
		const int size = base + (number < extra ? 1 : 0);
		for(int seat = 0; seat < size; ++seat) {
			table.ids.push_back(ids[next++]);
		}
		table.button = static_cast<unsigned>(table.ids.front());
		m_tables.push_back(table);
	}
	return true;
}

void TournamentRun::updateLocalFlags()
{
	// The host's table is not a fixed table number: the balancing may move him,
	// and then his table is the one he was moved to.
	for(size_t i = 0; i < m_tables.size(); ++i) {
		m_tables[i].local = m_hostId >= 0 && contains(m_tables[i].ids, m_hostId);
	}
}

unsigned TournamentRun::buttonOf(const Table &table) const
{
	for(size_t i = 0; i < table.ids.size(); ++i) {
		if(static_cast<unsigned>(table.ids[i]) == table.button) return table.button;
	}
	// The button left the table (a bust or a move): it goes to the first seat.
	return table.ids.empty() ? 0u : static_cast<unsigned>(table.ids.front());
}

bool TournamentRun::buildTable(Table &table, string &error)
{
	table.game.reset();
	table.gui.reset();
	table.dirty = false;
	if(table.local) return true;
	if(table.ids.size() < 2) return true;
	if(table.ids.size() > static_cast<size_t>(tableCapacity())) {
		ostringstream oss;
		oss << "table " << table.number << ": " << table.ids.size()
			<< " players do not fit into " << tableCapacity() << " seats";
		error = oss.str();
		return false;
	}

	GameData gameData;
	gameData.gameType = GAME_TYPE_NORMAL;
	gameData.allowSpectators = false;
	gameData.maxNumberOfPlayers = static_cast<int>(table.ids.size());
	gameData.startMoney = m_event.startStack;
	gameData.firstSmallBlind = blind();
	gameData.raiseIntervalMode = RAISE_ON_HANDNUMBER;
	// The controller raises the blind, not every table on its own: the tables
	// are rebuilt now and then and each rebuild would restart the count.
	gameData.raiseSmallBlindEveryHandsValue = 1000000000;
	gameData.raiseMode = DOUBLE_BLINDS;
	gameData.afterManualBlindsMode = AFTERMB_DOUBLE_BLINDS;
	gameData.afterMBAlwaysRaiseValue = 0;
	gameData.guiSpeed = 4;
	gameData.delayBetweenHandsSec = 0;
	gameData.playerActionTimeoutSec = 0;

	PlayerDataList playerDataList;
	for(size_t seat = 0; seat < table.ids.size(); ++seat) {
		const int id = table.ids[seat];
		const PlayerState::RosterEntry *who = m_roster.entry(id);
		boost::shared_ptr<PlayerData> playerData(new PlayerData(
				static_cast<unsigned>(id), id, PLAYER_TYPE_COMPUTER, PLAYER_RIGHTS_NORMAL,
				seat == 0));
		if(who) playerData->SetName(who->name.toStdString());
		playerData->SetStartCash(m_roster.stack(id));
		playerDataList.push_back(playerData);
	}

	StartData startData;
	startData.numberOfPlayers = static_cast<int>(table.ids.size());
	startData.startDealerPlayerId = buttonOf(table);

	table.gui.reset(new SilentGui(m_config));
	boost::shared_ptr<EngineFactory> factory(new LocalEngineFactory(m_config));
	try {
		table.game.reset(new Game(table.gui.get(), factory, playerDataList, gameData,
								  startData, table.number + 1, m_log));
	} catch(const exception &e) {
		ostringstream oss;
		oss << "table " << table.number << ": cannot build the table: " << e.what();
		error = oss.str();
		return false;
	}
	table.gui->setGame(table.game.get());

	// The people, not the seats: every seat takes the profile of its roster
	// entry, so the same player comes back with the same character after a
	// rebuild.
	for(size_t seat = 0; seat < table.ids.size(); ++seat) {
		const PlayerState::RosterEntry *who = m_roster.entry(table.ids[seat]);
		if(!who || who->profile.isEmpty()) continue;
		LocalPlayer *player = dynamic_cast<LocalPlayer*>(table.game->getPlayerByNumber(
								 static_cast<int>(seat)).get());
		if(!player) continue;
		BotProfile profile;
		if(BotProfile::parse(who->profile.toStdString(), profile)) {
			player->setBotProfile(profile);
		}
	}
	table.game->setCurrentSmallBlind(blind());
	return true;
}

bool TournamentRun::seatLocalTable(string &error)
{
	Table *table = localTable();
	// The player is out, or alone and about to be moved: nothing to seat.
	if(!table || !table->dirty) return true;
	if(table->ids.size() < 2) return true;

	// The player's seat is the first one. His table is an ordinary local game,
	// and a local game takes the type of every seat from its number: seat 0 is
	// the human, the rest are bots.
	vector<int> ordered;
	ordered.reserve(table->ids.size());
	if(m_hostId >= 0) ordered.push_back(m_hostId);
	for(size_t i = 0; i < table->ids.size(); ++i) {
		if(table->ids[i] != m_hostId) ordered.push_back(table->ids[i]);
	}
	table->ids = ordered;

	if(!m_host->seat(table->ids, static_cast<int>(buttonOf(*table)), error)) return false;
	table->dirty = false;
	return true;
}

bool TournamentRun::startLocalHand(string &error)
{
	error.clear();
	if(!m_host) {
		error = "this tournament has no table of its own";
		return false;
	}
	if(!hostStillIn()) {
		error = "the player is out of the tournament";
		return false;
	}
	Table *table = localTable();
	if(!table || table->ids.size() < 2) {
		error = "the player's table has nobody to play against";
		return false;
	}
	// The rebuild happens here, right before the hand: a table without a hand
	// cannot be drawn by the client (its player list needs the hand's data).
	if(table->dirty && !seatLocalTable(error)) return false;
	return m_host->startHand(blind(), error);
}

bool TournamentRun::step(string &error)
{
	error.clear();

	// 1. What the finished hand did to the host's table: the stacks, the button
	// and the big blind come back before anything is rebuilt from the roster.
	Table *local = localTable();
	if(local && m_host) {
		const LocalTableHost::Snapshot snap = m_host->snapshot();
		for(size_t seat = 0; seat < local->ids.size() && seat < snap.stacks.size(); ++seat) {
			m_roster.setStack(local->ids[seat], snap.stacks[seat]);
		}
		if(snap.button >= 0) local->button = static_cast<unsigned>(snap.button);
		local->lastBigBlind = snap.lastBigBlind;
	}

	// 2. Every other table plays the same hand: same blind, same step.
	if(!playSilentTables(error)) return false;

	++m_handsPlayed;
	collectBusts();
	rebalance();
	assignPlaces();

	// The seating of the host's table may have changed (a bust, a move). It is
	// NOT rebuilt here: the host rebuilds it when the next hand starts, so that
	// the table is never left without a hand (the client draws it, and a table
	// without a hand cannot be drawn). startLocalHand() does the rebuild.

	fillResult();
	return true;
}

bool TournamentRun::playSilentTables(string &error)
{
	const int stepBlind = blind();

	for(size_t i = 0; i < m_tables.size(); ++i) {
		Table &table = m_tables[i];
		if(table.local) continue;
		if(table.ids.size() < 2) continue;
		if(table.dirty && !buildTable(table, error)) return false;

		table.game->setCurrentSmallBlind(stepBlind);
		if(table.game->getCurrentSmallBlind() != stepBlind) {
			m_result.blindsAligned = false;
		}
		try {
			table.game->initHand();
			table.game->startHand();
		} catch(const exception &e) {
			ostringstream oss;
			oss << "table " << table.number << ", hand " << (m_handsPlayed + 1)
				<< ": starting the hand failed: " << e.what();
			error = oss.str();
			return false;
		}

		bool handFinished = false;
		table.gui->onHandFinished = [&handFinished]() {
			handFinished = true;
		};
		long long steps = 0;
		try {
			while(table.gui->pump()) {
				if(++steps > MaxStepsPerHand) {
					ostringstream oss;
					oss << "table " << table.number << ", hand " << (m_handsPlayed + 1)
						<< ": more than " << MaxStepsPerHand << " engine steps";
					error = oss.str();
					return false;
				}
			}
		} catch(const exception &e) {
			ostringstream oss;
			oss << "table " << table.number << ", hand " << (m_handsPlayed + 1)
				<< ": engine error: " << e.what();
			error = oss.str();
			return false;
		}
		if(!handFinished) {
			ostringstream oss;
			oss << "table " << table.number << ", hand " << (m_handsPlayed + 1)
				<< ": the engine went idle before the pot was distributed";
			error = oss.str();
			return false;
		}

		// The engine moved the button on at the start of the hand: what it holds
		// now is the button of the next hand, and a rebuild keeps it.
		table.button = table.game->getDealerPosition();
		boost::shared_ptr<HandInterface> hand = table.game->getCurrentHand();
		if(hand) table.lastBigBlind = static_cast<int>(hand->getBigBlindPositionId());

		// The stacks go back into the roster: it is what a rebuild seats from.
		for(size_t seat = 0; seat < table.ids.size(); ++seat) {
			boost::shared_ptr<PlayerInterface> player =
					table.game->getPlayerByUniqueId(static_cast<unsigned>(table.ids[seat]));
			if(player) m_roster.setStack(table.ids[seat], player->getMyCash());
		}
	}

	return true;
}

void TournamentRun::collectBusts()
{
	for(size_t i = 0; i < m_tables.size(); ++i) {
		Table &table = m_tables[i];
		vector<int> left;
		for(size_t seat = 0; seat < table.ids.size(); ++seat) {
			const int id = table.ids[seat];
			if(m_roster.stack(id) > 0) {
				left.push_back(id);
				continue;
			}
			// Out of chips: his place is decided by how many are still standing.
			m_finishOrder.push_back(id);
			m_roster.unseat(id);
			table.dirty = true;
		}
		table.ids = left;
	}
}

void TournamentRun::closeEmptyTables()
{
	vector<Table> kept;
	for(size_t i = 0; i < m_tables.size(); ++i) {
		if(!m_tables[i].ids.empty()) kept.push_back(m_tables[i]);
	}
	if(kept.size() != m_tables.size()) m_tables = kept;
}

int TournamentRun::largestTable() const
{
	int best = -1;
	for(size_t i = 0; i < m_tables.size(); ++i) {
		if(m_tables[i].ids.empty()) continue;
		if(best < 0 || m_tables[i].ids.size() > m_tables[best].ids.size()) {
			best = static_cast<int>(i);
		}
	}
	return best;
}


/* The final table: while the field fits into one table fewer than there are,
 * the smallest table is broken and its people are seated at the others.
 *
 * This is the standard MTT rule - 18 seats play 9+9 and the field comes
 * together when nine are left - and it is what the balancing alone cannot do:
 * two balanced tables shrink together and would play 2+2 to the end. The rule
 * only ever breaks a table that is not the player's: he stays where he sits and
 * the others come to him. Breaking his table would rebuild the felt in front of
 * him for no reason he could see, which is exactly what the player noticed.
 *
 * Who waits where: the one who paid the big blind at the broken table arrives
 * last, so of all the arrivals the blinds reach him last - he does not take the
 * big blind in the first hand at the new table if somebody else could. A proper
 * "wait for the big blind" (sit out one orbit) is still a gap of its own.
 */
bool TournamentRun::consolidateOne()
{
	const int tables = static_cast<int>(m_tables.size());
	if(tables < 2) return false;

	const int capacity = tableCapacity();
	if(playersLeft() > capacity * (tables - 1)) return false;

	int victim = -1;
	for(int i = 0; i < tables; ++i) {
		if(m_tables[i].local || m_tables[i].ids.empty()) continue;
		if(victim < 0 || m_tables[i].ids.size() < m_tables[victim].ids.size()) victim = i;
	}
	if(victim < 0) return false;

	vector<int> people = m_tables[victim].ids;
	const int lastBigBlind = m_tables[victim].lastBigBlind;
	if(lastBigBlind >= 0) {
		people.erase(remove(people.begin(), people.end(), lastBigBlind), people.end());
		people.push_back(lastBigBlind);
	}

	for(size_t k = 0; k < people.size(); ++k) {
		// The fullest table with room: the people fill the tables up instead of
		// spreading thin again.
		int to = -1;
		for(int i = 0; i < tables; ++i) {
			if(i == victim) continue;
			if(static_cast<int>(m_tables[i].ids.size()) >= capacity) continue;
			if(to < 0 || m_tables[i].ids.size() > m_tables[to].ids.size()) to = i;
		}
		if(to < 0) return false;   // the numbers said there is room
		// The button of the table stays with the person who had it: a rebuild
		// keeps it by roster id, so the newcomers fall after it in seat order.
		m_tables[to].ids.push_back(people[k]);
		m_tables[to].dirty = true;
		++m_moves;
		if(people[k] >= 0 && people[k] < static_cast<int>(m_result.movesById.size())) {
			m_result.movesById[people[k]] += 1;
		}
	}
	m_tables[victim].ids.clear();
	m_tables[victim].dirty = true;
	closeEmptyTables();
	if(m_tables.size() == 1 && m_result.finalTableAt < 0) {
		// The field is in one room now: from here on the tournament is a single
		// table, which is the shape the players know as the final table.
		m_result.finalTableAt = playersLeft();
	}
	return true;
}

void TournamentRun::rebalance()
{
	// The flag says which table is the player's: the merge must not break it,
	// and the player may have just busted (then no table is his).
	updateLocalFlags();

	for(;;) {
		closeEmptyTables();
		const int tables = static_cast<int>(m_tables.size());
		if(playersLeft() < 2 || tables < 2) break;

		// The final table comes first: while the field fits into fewer tables,
		// one table is broken and the rest is balancing among the others.
		if(consolidateOne()) continue;

		// Otherwise one move per two seats of difference: one move fixes two
		// seats, so after it the sizes differ by one at most. A player who is
		// left alone at his table is not moved himself - the balancing sends
		// somebody over, so his table fills up.
		const int full = largestTable();
		int thin = -1;
		for(int i = 0; i < tables; ++i) {
			if(i == full) continue;
			if(thin < 0 || m_tables[i].ids.size() < m_tables[thin].ids.size()) thin = i;
		}
		if(thin < 0) break;
		const int difference = static_cast<int>(m_tables[full].ids.size())
							   - static_cast<int>(m_tables[thin].ids.size());
		if(difference < 2) break;
		moveOne(m_tables[full], m_tables[thin]);
	}

	// The host may have been moved to another table; his table goes with him.
	updateLocalFlags();

	// What the tables look like once the moves are done: one apart at most, and
	// that is the number the report quotes.
	const int now = imbalance();
	if(now > m_result.worstImbalance) m_result.worstImbalance = now;
}

int TournamentRun::imbalance() const
{
	int smallest = -1;
	int largest = -1;
	for(size_t i = 0; i < m_tables.size(); ++i) {
		if(m_tables[i].ids.empty()) continue;
		const int size = static_cast<int>(m_tables[i].ids.size());
		if(smallest < 0 || size < smallest) smallest = size;
		if(largest < 0 || size > largest) largest = size;
	}
	return smallest < 0 ? 0 : largest - smallest;
}

int TournamentRun::moveOne(Table &from, Table &to)
{
	// Who goes: the player who just paid the big blind. He is the one who has
	// the longest wait for the button anyway, so moving costs him least - and it
	// cannot make him pay the big blind twice in a row.
	//
	// Never the host's player: his table is never the one the merge breaks, and
	// the balancing never takes from it while somebody else is seated there.
	// The felt he sees is rebuilt only when people arrive, never because he
	// left: he plays the whole tournament in the same chair.
	int mover = -1;
	if(from.lastBigBlind >= 0 && from.lastBigBlind != m_hostId
	   && contains(from.ids, from.lastBigBlind)) {
		mover = from.lastBigBlind;
	} else {
		for(size_t i = from.ids.size(); i > 0; --i) {
			if(from.ids[i - 1] != m_hostId) {
				mover = from.ids[i - 1];
				break;
			}
		}
	}
	if(mover < 0) mover = from.ids.back();
	from.ids.erase(remove(from.ids.begin(), from.ids.end(), mover), from.ids.end());

	// Where: the newcomer becomes the button, so he posts no blind in his first
	// hand at the new table. Heads-up the button is the small blind, so there he
	// takes the big blind instead - the fair place for a player who arrives.
	if(to.ids.empty()) {
		to.button = static_cast<unsigned>(mover);
	} else if(to.ids.size() == 1) {
		to.button = static_cast<unsigned>(to.ids.front());
	} else {
		to.button = static_cast<unsigned>(mover);
	}
	to.ids.push_back(mover);

	from.dirty = true;
	to.dirty = true;
	++m_moves;
	if(mover >= 0 && mover < static_cast<int>(m_result.movesById.size())) {
		m_result.movesById[mover] += 1;
	}
	return mover;
}

void TournamentRun::assignPlaces()
{
	const int seats = m_roster.count();
	// The order of busting is the finishing order from the bottom: the first one
	// out is the last place.
	for(size_t i = 0; i < m_finishOrder.size(); ++i) {
		const int id = m_finishOrder[i];
		if(id < 0 || id >= seats) continue;
		m_result.places[id] = seats - static_cast<int>(i);
	}
	// Whoever is still standing when the run stops.
	for(int id = 0; id < seats; ++id) {
		const PlayerState::RosterEntry *who = m_roster.entry(id);
		if(!who || !who->seated() || who->stack <= 0) continue;
		if(playersLeft() == 1) {
			m_result.places[id] = 1;
			m_result.winner = id;
		}
	}
}

void TournamentRun::fillResult()
{
	m_result.handsPlayed = m_handsPlayed;
	m_result.level = level() + 1;
	m_result.moves = m_moves;
	m_result.finished = playersLeft() <= 1;
	assignPlaces();

	int chips = 0;
	for(int id = 0; id < m_roster.count(); ++id) {
		chips += m_roster.stack(id);
	}
	m_result.chipsConserved = chips == m_chipsInPlay;

	for(size_t i = 0; i < m_tables.size(); ++i) {
		const int size = static_cast<int>(m_tables[i].ids.size());
		if(size > m_result.biggestTable) m_result.biggestTable = size;
	}
}
