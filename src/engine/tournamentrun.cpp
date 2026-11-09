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

int TournamentRun::playersLeft() const
{
	int left = 0;
	for(int id = 0; id < m_roster.count(); ++id) {
		const PlayerState::RosterEntry *who = m_roster.entry(id);
		if(who && who->seated() && who->stack > 0) ++left;
	}
	return left;
}

bool TournamentRun::run(const PlayerState::Roster &roster, int maxHands, string &error)
{
	error.clear();
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

	if(!seatTables(error)) return false;

	while(m_handsPlayed < maxHands && playersLeft() > 1) {
		if(!playStep(error)) return false;
	}

	m_result.handsPlayed = m_handsPlayed;
	m_result.level = level() + 1;
	m_result.moves = m_moves;
	m_result.finished = playersLeft() == 1;
	assignPlaces();

	int chips = 0;
	for(int id = 0; id < m_roster.count(); ++id) {
		chips += m_roster.stack(id);
	}
	m_result.chipsConserved = chips == m_chipsInPlay;
	return true;
}

bool TournamentRun::seatTables(string &error)
{
	// The tables of a full field: 10 seats (the engine maximum), the field
	// spread as evenly as the seats allow. 18 players are two tables of nine.
	vector<int> ids;
	for(int id = 0; id < m_roster.count(); ++id) {
		const PlayerState::RosterEntry *who = m_roster.entry(id);
		if(who && who->seated() && who->stack > 0) ids.push_back(id);
	}
	if(ids.size() < 2) {
		error = "a tournament needs at least two players";
		return false;
	}

	const int perTable = MAX_NUMBER_OF_PLAYERS;
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
	if(table.ids.size() < 2) return true;
	if(table.ids.size() > static_cast<size_t>(MAX_NUMBER_OF_PLAYERS)) {
		ostringstream oss;
		oss << "table " << table.number << ": " << table.ids.size()
			<< " players do not fit into " << MAX_NUMBER_OF_PLAYERS << " seats";
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

bool TournamentRun::playStep(string &error)
{
	const int stepBlind = blind();

	for(size_t i = 0; i < m_tables.size(); ++i) {
		Table &table = m_tables[i];
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

	++m_handsPlayed;
	collectBusts();
	rebalance();
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


void TournamentRun::rebalance()
{
	for(;;) {
		closeEmptyTables();
		const int tables = static_cast<int>(m_tables.size());
		if(playersLeft() < 2 || tables < 2) break;

		// A player alone at a table cannot play. His table is broken and he is
		// seated at the fullest table that still has room - the one that gives
		// the most company and the fewest moves.
		int lone = -1;
		for(int i = 0; i < tables; ++i) {
			if(m_tables[i].ids.size() == 1) {
				lone = i;
				break;
			}
		}
		if(lone >= 0) {
			int to = -1;
			for(int i = 0; i < tables; ++i) {
				if(i == lone || m_tables[i].ids.size() >= static_cast<size_t>(MAX_NUMBER_OF_PLAYERS)) {
					continue;
				}
				if(to < 0 || m_tables[i].ids.size() > m_tables[to].ids.size()) to = i;
			}
			if(to >= 0) {
				moveOne(m_tables[lone], m_tables[to]);
				continue;
			}
		}

		// Otherwise one move per two seats of difference: one move fixes two
		// seats, so after it the sizes differ by one at most.
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
	int mover = -1;
	if(from.ids.size() == 1) {
		mover = from.ids.front();
	} else if(from.lastBigBlind >= 0
			  && find(from.ids.begin(), from.ids.end(), from.lastBigBlind) != from.ids.end()) {
		mover = from.lastBigBlind;
	} else {
		mover = from.ids.back();
	}
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
