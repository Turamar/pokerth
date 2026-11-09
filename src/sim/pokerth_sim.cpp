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
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/

/* pokerth_sim - headless bot/game simulator.
 *
 * Plays games with nothing but computer players and prints what they did:
 * VPIP, PFR, aggression, wins, chips. No window, no server, no network - the
 * same engine and the same LocalEngineFactory the local game uses, only with a
 * GUI that does not show anything (see simguiinterface.h).
 *
 * Why: the strength levels and playing styles (PLAN.md section 4) are meant to
 * be calibrated, not guessed, and a 180 seat tournament (section 3) cannot be
 * tuned by playing by hand. The same runner is the prototype of the headless
 * tables the tournament needs later on.
 *
 * Usage: pokerth_sim [--players N] [--stack N] [--sb N] [--hands N]
 *                    [--raise-every N] [--seed N] [--runs N] [--json FILE]
 *                    [--profile P | --strength S | --archetype A | --mixed]
 *                    [--seat-profiles L] [--field F]
 *                    [--scale-open F] [--scale-3bet F] [--scale-call F]
 *                    [--scale-raise F] [--scale-cbet F] [--scale-adapt F]
 *                    [--margin F] [--quiet]
 *        pokerth_sim --state-selftest DIR
 */

#include "simdriver.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <botprofile.h>
#include <configfile.h>
#include <statestore.h>
#include <wallet.h>

using namespace std;

namespace
{

void printUsage()
{
	printf(
		"pokerth_sim - headless simulation of a PokerTH game\n"
		"\n"
		"  --players N       seats at the table (2-10, default 6)\n"
		"  --stack N         starting chips per seat (default 1000)\n"
		"  --sb N            first small blind (default 10)\n"
		"  --hands N         stop after N hands at the latest (default 200)\n"
		"  --raise-every N   double the blinds every N hands (default 8, 0 = never)\n"
		"  --seed N          seed for cards and AI decisions (default: random, reported)\n"
		"  --runs N          play N independent games and aggregate them (default 1);\n"
		"                    game number i uses seed+i\n"
		"  --json FILE       write the report as JSON to FILE\n"
		"  --profile P       strength and style of every seat, e.g. elite/lag;\n"
		"                    either half may be left out (--profile tag, --profile pro)\n"
		"  --strength S      novice, amateur, pro or elite (all seats)\n"
		"  --archetype A     maniac, lag, tag, nit, caller or balanced (all seats)\n"
		"  --seat-profiles L one profile per seat, comma separated, e.g.\n"
		"                    'novice/balanced,novice/tag,pro/tag,elite/tag';\n"
		"                    the last entry repeats for the remaining seats\n"
		"  --field F         a reference field: rookie (between micro and low),\n"
		"                    micro, low or mid; the first seat then plays the\n"
		"                    profile given above (the hero), the rest the field\n"
		"  --tier T          a tier of the ladder: micro, rookie, low, mid or high;\n"
		"                    every game deals a new table from the tier's weights,\n"
		"                    the first seat plays the hero\n"
		"  --mixed           every seat draws its own profile - the mixture the\n"
		"                    client uses (default)\n"
		"  --scale-open F    multiply the opening ranges by F (calibration)\n"
		"  --scale-3bet F    multiply the three-bet ranges by F (calibration)\n"
		"  --scale-call F    multiply the calling ranges by F (calibration)\n"
		"  --scale-raise F   multiply how often a made hand raises instead of calling\n"
		"  --scale-cbet F    multiply how often a checked pot is bet at\n"
		"  --scale-adapt F   multiply how much the bot acts on the opponents'\n"
		"                    tendencies (0 = blind, 1 = normal)\n"
		"  --margin F        postflop calling margin in equity points, e.g. -0.05\n"
		"                    (calibration; overrides the value of the profile)\n"
		"  --quiet           only the summary line, no table\n"
		"  --state-selftest DIR\n"
		"                    check the state store in DIR instead of simulating\n"
		"\n"
		"One game stops earlier than --hands as soon as a seat holds all the chips.\n");
}

bool parseNumber(const char *value, long long &out)
{
	if(value == NULL || value[0] == 0) return false;
	char *end = NULL;
	errno = 0;
	const long long parsed = strtoll(value, &end, 10);
	if(errno != 0 || end == value || (end && *end != 0)) return false;
	out = parsed;
	return true;
}

bool parseDouble(const char *value, double &out)
{
	if(value == NULL || value[0] == 0) return false;
	char *end = NULL;
	errno = 0;
	const double parsed = strtod(value, &end);
	if(errno != 0 || end == value || (end && *end != 0)) return false;
	out = parsed;
	return true;
}

// "novice/tag,pro/lag,..." -- one profile per seat.
bool parseProfileList(const char *value, vector<BotProfile> &out)
{
	out.clear();
	const string text(value == NULL ? "" : value);

	string::size_type pos = 0;
	while(pos <= text.size()) {
		const string::size_type comma = text.find(',', pos);
		string token = text.substr(pos, comma == string::npos ? string::npos : comma - pos);
		pos = (comma == string::npos) ? text.size() + 1 : comma + 1;

		while(!token.empty() && (token[0] == ' ' || token[0] == '\t')) token.erase(0, 1);
		while(!token.empty() && (token[token.size() - 1] == ' ' || token[token.size() - 1] == '\t')) {
			token.erase(token.size() - 1);
		}
		if(token.empty()) continue;

		BotProfile profile;
		if(!BotProfile::parse(token, profile)) return false;
		out.push_back(profile);
	}
	return !out.empty();
}

double percentage(long long value, long long total)
{
	return total > 0 ? 100.0 * static_cast<double>(value) / static_cast<double>(total) : 0.0;
}

// All games of one invocation, summed per seat. VPIP and PFR are added up as
// counts, not averaged as percentages: a seat that busted after ten hands must
// not weigh as much as one that played three hundred.
struct SimTotals
{
	SimTotals() : runs(0), finishedRuns(0), hands(0), steps(0), elapsedMs(0) {}

	int runs;
	int finishedRuns;
	long long hands;
	long long steps;
	long long elapsedMs;
	vector<SimSeatStats> seats;
};

void accumulate(SimTotals &totals, const SimReport &report)
{
	totals.runs++;
	if(report.finished) totals.finishedRuns++;
	totals.hands += report.handsPlayed;
	totals.steps += report.steps;
	totals.elapsedMs += report.elapsedMs;

	if(totals.seats.size() < report.seats.size()) {
		totals.seats.resize(report.seats.size());
	}
	for(size_t i = 0; i < report.seats.size(); i++) {
		const SimSeatStats &source = report.seats[i];
		SimSeatStats &target = totals.seats[i];
		target.seat = source.seat;
		target.name = source.name;
		target.hands += source.hands;
		target.vpip += source.vpip;
		target.pfr += source.pfr;
		target.threeBets += source.threeBets;
		target.threeBetOpportunities += source.threeBetOpportunities;
		target.aggressive += source.aggressive;
		target.passive += source.passive;
		target.facedBets += source.facedBets;
		target.foldedToBets += source.foldedToBets;
		target.postflopBets += source.postflopBets;
		target.postflopChecks += source.postflopChecks;
		target.postflopRaises += source.postflopRaises;
		target.handsWon += source.handsWon;
		target.chipsWon += source.chipsWon;
		target.netChips += source.netChips;
		target.busts += source.busts;
		// The seat keeps its profile name unless the runs disagree.
		if(totals.runs == 1) target.profile = source.profile;
		else if(target.profile != source.profile) target.profile = "mixed";
		// chips and bustedAtHand describe a single game, not a series.
	}
}

void formatAggression(const SimSeatStats &seat, char *buffer, size_t size)
{
	if(seat.passive > 0) {
		snprintf(buffer, size, "%.2f",
				 static_cast<double>(seat.aggressive) / static_cast<double>(seat.passive));
	} else if(seat.aggressive > 0) {
		snprintf(buffer, size, "inf");
	} else {
		snprintf(buffer, size, "0.00");
	}
}

void formatThreeBet(const SimSeatStats &seat, char *buffer, size_t size)
{
	if(seat.threeBetOpportunities > 0) {
		snprintf(buffer, size, "%.1f", percentage(seat.threeBets, seat.threeBetOpportunities));
	} else {
		snprintf(buffer, size, "-");
	}
}

// Share of the times the seat gave up when a bet was in front of it after the flop.
void formatFoldToBet(const SimSeatStats &seat, char *buffer, size_t size)
{
	if(seat.facedBets > 0) {
		snprintf(buffer, size, "%.1f", percentage(seat.foldedToBets, seat.facedBets));
	} else {
		snprintf(buffer, size, "-");
	}
}

/* Net chips per 100 hands, measured in big blinds so that runs with different
 * stakes can be compared. This is the number that decides whether a style wins.
 */
double bigBlindsPer100(const SimSeatStats &seat, int smallBlind)
{
	const int bigBlind = 2 * smallBlind;
	if(seat.hands <= 0 || bigBlind <= 0) return 0.0;
	return 100.0 * seat.netChips / (static_cast<double>(seat.hands) * bigBlind);
}

// Has to be printed with every run of a field: the targets are what makes the
// measured field meaningful, not decoration.
void printFieldHeader(const SimOptions &options)
{
	if(!options.tierName.empty()) {
		BotPopulationTarget target;
		if(BotProfile::tierTarget(options.tierName, target)) {
			printf("\ntier %s (%s): target VPIP %.0f%%, PFR %.0f%%, 3B %.0f%%\n",
				   target.name, target.description, target.vpip, target.pfr, target.threeBet);
		}
		printf("seat 1 plays the hero (%s), seats 2..%d draw from the tier; "
			   "every game deals a new table\n",
			   options.profile.label().c_str(), options.players);
		return;
	}
	if(options.fieldProfiles.empty()) return;

	BotPopulationTarget target;
	if(BotProfile::populationTarget(options.fieldName, target)) {
		printf("\nfield %s (%s): target pool VPIP %.0f%%, PFR %.0f%%, 3B %.0f%%\n",
			   target.name, target.description, target.vpip, target.pfr, target.threeBet);
	}
	printf("seat 1 plays the hero (%s), seats 2..%d play the field\n",
		   options.profile.label().c_str(), options.players);
}

// Share of the free postflop decisions (bet or check) that were bets. This is
// the number that tells a station from a passive-but-alive style: a caller that
// never bets sits at 0 %, a caller that bets a made hand once in a while does
// not, while both show the same low aggression factor.
void formatBetShare(const SimSeatStats &seat, char *buffer, size_t size)
{
	const long long free = seat.postflopBets + seat.postflopChecks;
	if(free > 0) {
		snprintf(buffer, size, "%.0f", percentage(seat.postflopBets, free));
	} else {
		snprintf(buffer, size, "-");
	}
}

void formatRaiseShare(const SimSeatStats &seat, char *buffer, size_t size)
{
	// facesBets already counts every spot with a bet in front, and a raise is
	// one of them -- so the share is raises over faced bets, not over their sum.
	if(seat.facedBets > 0) {
		snprintf(buffer, size, "%.0f", percentage(seat.postflopRaises, seat.facedBets));
	} else {
		snprintf(buffer, size, "-");
	}
}

void printSeatTable(const vector<SimSeatStats> &seats, bool detailed, int smallBlind)
{
	printf("\n%-6s %-14s %8s %7s %7s %6s %6s %6s %6s %5s %8s %6s %7s\n",
		   "seat", "profile", "hands", "VPIP%", "PFR%", "3B%", "F2B%", "AF",
		   "bet%", "rais%", detailed ? "net" : "bb/100", "won",
		   detailed ? "busted" : "busts");

	for(size_t i = 0; i < seats.size(); i++) {
		const SimSeatStats &seat = seats[i];
		char aggression[16];
		formatAggression(seat, aggression, sizeof(aggression));
		char threeBet[8];
		formatThreeBet(seat, threeBet, sizeof(threeBet));
		char foldToBet[8];
		formatFoldToBet(seat, foldToBet, sizeof(foldToBet));
		char betShare[8];
		formatBetShare(seat, betShare, sizeof(betShare));
		char raiseShare[8];
		formatRaiseShare(seat, raiseShare, sizeof(raiseShare));

		char net[16];
		if(detailed) {
			snprintf(net, sizeof(net), "%lld", seat.netChips);
		} else {
			snprintf(net, sizeof(net), "%.1f", bigBlindsPer100(seat, smallBlind));
		}

		char busted[16];
		if(detailed) {
			if(seat.bustedAtHand >= 0) snprintf(busted, sizeof(busted), "%d", seat.bustedAtHand);
			else snprintf(busted, sizeof(busted), "-");
		} else {
			snprintf(busted, sizeof(busted), "%d", seat.busts);
		}

		printf("%-6s %-14s %8lld %7.1f %7.1f %6s %6s %6s %6s %5s %8s %6lld %7s\n",
			   seat.name.c_str(), seat.profile.c_str(), seat.hands,
			   percentage(seat.vpip, seat.hands),
			   percentage(seat.pfr, seat.hands),
			   threeBet, foldToBet, aggression, betShare, raiseShare,
			   net, seat.handsWon, busted);
	}
}

void printSingleRun(const SimReport &report, int smallBlind)
{
	printf("\n");
	printf("seed %llu, %d hands, %lld engine steps, %lld ms (%.1f ms/hand), %d seats left\n",
		   report.seed, report.handsPlayed, report.steps, report.elapsedMs,
		   report.handsPlayed > 0 ? static_cast<double>(report.elapsedMs) / report.handsPlayed : 0.0,
		   report.playersLeft);
	if(report.finished) {
		printf("finished: %s holds all the chips\n", report.winner.c_str());
	} else {
		printf("stopped after the hand limit\n");
	}
	printSeatTable(report.seats, true, smallBlind);
}

void printTotals(const SimTotals &totals, const SimOptions &options)
{
	printf("\n");
	printf("%d game(s) from seed %llu, %d seat(s), %d decided, %lld hands, %lld engine steps\n",
		   totals.runs, options.seed, options.players, totals.finishedRuns, totals.hands, totals.steps);
	printf("%lld ms total (%.1f ms per game, %.2f ms per hand)\n",
		   totals.elapsedMs,
		   totals.runs > 0 ? static_cast<double>(totals.elapsedMs) / totals.runs : 0.0,
		   totals.hands > 0 ? static_cast<double>(totals.elapsedMs) / static_cast<double>(totals.hands) : 0.0);
	printSeatTable(totals.seats, false, options.smallBlind);
}

bool writeJson(const string &fileName, const SimTotals &totals, const SimOptions &options)
{
	QJsonObject root;
	root[QStringLiteral("seed")] = static_cast<qint64>(options.seed);
	root[QStringLiteral("runs")] = totals.runs;
	root[QStringLiteral("finishedRuns")] = totals.finishedRuns;
	root[QStringLiteral("hands")] = static_cast<qint64>(totals.hands);
	root[QStringLiteral("engineSteps")] = static_cast<qint64>(totals.steps);
	root[QStringLiteral("elapsedMs")] = static_cast<qint64>(totals.elapsedMs);

	QJsonObject setup;
	setup[QStringLiteral("players")] = options.players;
	setup[QStringLiteral("startCash")] = options.startCash;
	setup[QStringLiteral("smallBlind")] = options.smallBlind;
	setup[QStringLiteral("raiseEveryHands")] = options.raiseEveryHands;
	setup[QStringLiteral("maxHands")] = options.maxHands;
	setup[QStringLiteral("field")] = QString::fromStdString(options.fieldName);
	setup[QStringLiteral("tier")] = QString::fromStdString(options.tierName);
	setup[QStringLiteral("hero")] =
			(options.fieldProfiles.empty() && options.tierName.empty())
			? QString() : QString::fromStdString(options.profile.label());
	root[QStringLiteral("setup")] = setup;

	QJsonArray seats;
	for(size_t i = 0; i < totals.seats.size(); i++) {
		const SimSeatStats &seat = totals.seats[i];
		QJsonObject entry;
		entry[QStringLiteral("seat")] = seat.seat;
		entry[QStringLiteral("name")] = QString::fromStdString(seat.name);
		entry[QStringLiteral("profile")] = QString::fromStdString(seat.profile);
		entry[QStringLiteral("hands")] = static_cast<qint64>(seat.hands);
		entry[QStringLiteral("vpipPercent")] = percentage(seat.vpip, seat.hands);
		entry[QStringLiteral("pfrPercent")] = percentage(seat.pfr, seat.hands);
		entry[QStringLiteral("threeBetPercent")] = percentage(seat.threeBets, seat.threeBetOpportunities);
		entry[QStringLiteral("netChips")] = static_cast<qint64>(seat.netChips);
		entry[QStringLiteral("bigBlindsPer100")] = bigBlindsPer100(seat, options.smallBlind);
		entry[QStringLiteral("aggressive")] = static_cast<qint64>(seat.aggressive);
		entry[QStringLiteral("passive")] = static_cast<qint64>(seat.passive);
		entry[QStringLiteral("facedBets")] = static_cast<qint64>(seat.facedBets);
		entry[QStringLiteral("foldedToBets")] = static_cast<qint64>(seat.foldedToBets);
		entry[QStringLiteral("foldToBetPercent")] = percentage(seat.foldedToBets, seat.facedBets);
		entry[QStringLiteral("postflopBets")] = static_cast<qint64>(seat.postflopBets);
		entry[QStringLiteral("postflopChecks")] = static_cast<qint64>(seat.postflopChecks);
		entry[QStringLiteral("postflopRaises")] = static_cast<qint64>(seat.postflopRaises);
		entry[QStringLiteral("handsWon")] = static_cast<qint64>(seat.handsWon);
		entry[QStringLiteral("chipsWon")] = static_cast<qint64>(seat.chipsWon);
		entry[QStringLiteral("busts")] = seat.busts;
		seats.append(entry);
	}
	root[QStringLiteral("seats")] = seats;

	const QString path = QString::fromStdString(fileName);
	const QFileInfo info(path);
	if(!info.absolutePath().isEmpty()) {
		QDir().mkpath(info.absolutePath());
	}

	QFile file(path);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		fprintf(stderr, "pokerth_sim: cannot write '%s'\n", fileName.c_str());
		return false;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	file.close();
	return true;
}

/* Self test of the state store and the bank: money and counters have to survive
 * a restart, a session that was interrupted by a crash has to come back, the
 * cash table limits and the rebuy cooldown have to hold, and a damaged or too
 * new file must never be overwritten silently. Runs without a GUI and without
 * Qt application state, so it can be part of a build check. */
bool runStateSelfTest(const QString &directory)
{
	int failures = 0;
	auto check = [&failures](bool condition, const QString &message) {
		printf("  %s %s\n", condition ? "ok  " : "FAIL", message.toUtf8().constData());
		if(!condition) failures++;
	};

	PlayerState::StateStore store(directory);
	QString error;
	printf("state store: %s\n", store.filePath().toUtf8().constData());

	// A leftover of an earlier run must not confuse the test.
	QFile::remove(store.filePath());
	QFile::remove(store.filePath() + QStringLiteral(".corrupt"));
	check(!store.exists(), QStringLiteral("a fresh directory has no state file"));

	PlayerState::StateDocument document = PlayerState::StateStore::newDocument(QStringLiteral("SelfTest"));
	check(document.profile.balance == PlayerState::StartingBalance,
		  QStringLiteral("a new profile starts with the starting balance"));
	check(!document.cash.active(), QStringLiteral("a new profile is not sitting at a table"));
	// The action first, the message afterwards: in one argument list the order
	// is unspecified, and the message quotes what the action produced.
	bool ok = store.save(document, error);
	check(ok, QStringLiteral("first save (%1)").arg(error));
	check(store.exists(), QStringLiteral("the state file exists after the first save"));

	PlayerState::StateDocument loaded;
	ok = store.load(loaded, error);
	check(ok, QStringLiteral("load after the first save (%1)").arg(error));
	check(loaded.profile.name == document.profile.name, QStringLiteral("the name survives"));
	check(loaded.profile.balance == document.profile.balance, QStringLiteral("the balance survives"));
	check(loaded.profile.createdAt.isValid(), QStringLiteral("the creation time is set"));

	// Change everything a session changes, then read it back.
	loaded.profile.balance = 75;
	loaded.profile.handsPlayed = 3;
	loaded.profile.handsWon = 1;
	loaded.profile.lastRebuyAt = QDateTime::currentDateTimeUtc();
	const QDateTime createdBefore = loaded.profile.createdAt;
	ok = store.save(loaded, error);
	check(ok, QStringLiteral("second save (%1)").arg(error));

	// Show what actually ends up on disk - the format is part of the contract
	// with the next version of this file.
	auto printStored = [&store]() {
		QFile stored(store.filePath());
		if(stored.open(QIODevice::ReadOnly)) {
			printf("  ---- stored state ----\n");
			const QList<QByteArray> lines = stored.readAll().split('\n');
			for(int i = 0; i < lines.size(); i++) {
				if(!lines.at(i).trimmed().isEmpty()) {
					printf("  %s\n", lines.at(i).constData());
				}
			}
			printf("  ----------------------\n");
		}
	};
	printStored();

	PlayerState::StateDocument reloaded;
	ok = store.load(reloaded, error);
	check(ok, QStringLiteral("load after the second save (%1)").arg(error));
	check(reloaded.profile.balance == 75, QStringLiteral("the balance survives a save/load round trip"));
	check(reloaded.profile.handsPlayed == 3 && reloaded.profile.handsWon == 1,
		  QStringLiteral("the counters survive a save/load round trip"));
	check(reloaded.profile.lastRebuyAt.isValid(), QStringLiteral("the rebuy time survives"));
	check(reloaded.profile.createdAt == createdBefore,
		  QStringLiteral("the creation time is not touched by saving"));

	// ── The bank: the ladder, buy-in, stack, leaving, rebuy ─────────────────
	{
		// Money is cents (see StartingBalance); the test reasons in dollars.
		const qint64 D = 100;
		const PlayerState::CashLimit &limit = PlayerState::cashLimit(0);
		const PlayerState::CashLimit &higher = PlayerState::cashLimit(1);
		const PlayerState::CashLimit &top =
				PlayerState::cashLimit(PlayerState::cashLimitCount() - 1);
		printf("  ladder: %d limits, bottom %s blinds %lld/%lld cents, buy-in %lld-%lld, gate %lld\n",
			   PlayerState::cashLimitCount(), limit.label,
			   static_cast<long long>(limit.smallBlind), static_cast<long long>(limit.bigBlind()),
			   static_cast<long long>(limit.minBuyIn()), static_cast<long long>(limit.maxBuyIn()),
			   static_cast<long long>(limit.gate()));

		// A step is blinds plus composition: every limit has to name a field
		// that botprofile knows and that deals a whole table.
		for(int i = 0; i < PlayerState::cashLimitCount(); ++i) {
			const PlayerState::CashLimit &step = PlayerState::cashLimit(i);
			check(BotProfile::hasTier(step.field),
				  QStringLiteral("limit %1 names a known field (%2)")
				  .arg(QString::fromLatin1(step.key))
				  .arg(QString::fromLatin1(step.field)));
			std::vector<BotProfile> dealt;
			check(BotProfile::dealTier(step.field, 5, dealt) && dealt.size() == 5,
				  QStringLiteral("limit %1 deals five opponents")
				  .arg(QString::fromLatin1(step.key)));
		}

		// ── The tournament catalogue (PLAN.md section 3) ────────────────────
		check(PlayerState::tournamentCount() >= 2,
			  QStringLiteral("the catalogue has at least two events"));
		for(int i = 0; i < PlayerState::tournamentCount(); ++i) {
			const PlayerState::TournamentEvent &event = PlayerState::tournamentEvent(i);
			const QString key = QString::fromLatin1(event.key);
			check(BotProfile::hasTier(event.field),
				  QStringLiteral("tournament %1 names a known field (%2)").arg(key)
				  .arg(QString::fromLatin1(event.field)));
			std::vector<BotProfile> dealt;
			check(BotProfile::dealTier(event.field, event.seats - 1, dealt)
				  && dealt.size() == static_cast<size_t>(event.seats - 1),
				  QStringLiteral("tournament %1 deals a whole table").arg(key));
			qint64 paidOut = 0;
			for(int p = 1; p <= event.seats; ++p) {
				paidOut += event.prizeForPlace(p);
			}
			check(paidOut == event.pool(),
				  QStringLiteral("tournament %1 pays out exactly its pool").arg(key));
		}
		{
			const PlayerState::TournamentEvent &event = PlayerState::tournamentEvent(0);
			PlayerState::Wallet entering(
				PlayerState::StateStore::newDocument(QStringLiteral("SelfTest")));
			const qint64 before = entering.balance();
			check(entering.enterTournament(event),
				  QStringLiteral("a tournament takes the buy-in"));
			check(entering.balance() == before - event.buyIn,
				  QStringLiteral("the balance drops by the tournament buy-in (%1)")
				  .arg(entering.balance()));
			check(entering.payTournamentPrize(event, 1) == event.prizeForPlace(1),
				  QStringLiteral("the first prize is paid"));
			check(entering.balance() == before - event.buyIn + event.prizeForPlace(1),
				  QStringLiteral("the balance grows by the prize (%1)").arg(entering.balance()));
			check(entering.payTournamentPrize(event, event.seats) == 0,
				  QStringLiteral("an unpaid place pays nothing"));
		}

		PlayerState::Wallet wallet;
		check(wallet.balance() == PlayerState::StartingBalance,
			  QStringLiteral("an empty wallet starts with the starting balance"));

		const qint64 start = wallet.balance();
		const qint64 minBuyIn = limit.minBuyIn();
		// The balance is above the gate, so the full buy-in of the limit is offered.
		check(wallet.suggestedBuyIn(limit) == limit.maxBuyIn(),
			  QStringLiteral("the suggested buy-in is the maximum of the limit (%1)")
			  .arg(wallet.suggestedBuyIn(limit)));

		QString reason;
		bool allowed = wallet.canSitDown(minBuyIn - limit.bigBlind(), limit, &reason);
		check(!allowed, QStringLiteral("below the minimum buy-in is refused (%1)").arg(reason));
		allowed = wallet.canSitDown(wallet.balance() + 1, limit, &reason);
		check(!allowed, QStringLiteral("more than the balance is refused (%1)").arg(reason));
		allowed = wallet.canSitDown(minBuyIn, limit, &reason);
		check(allowed, QStringLiteral("the minimum buy-in is accepted"));
		allowed = wallet.canSitDown(top.minBuyIn(), top, &reason);
		check(!allowed, QStringLiteral("a limit above the bankroll is closed (%1)").arg(reason));

		// Sit down with the minimum: the money leaves the balance and becomes a
		// stack.
		const QDateTime now = QDateTime::currentDateTimeUtc();
		const bool satDown = wallet.sitDown(minBuyIn, limit, now);
		check(satDown, QStringLiteral("sitting down takes the buy-in"));
		check(wallet.balance() == start - minBuyIn,
			  QStringLiteral("the balance drops by the buy-in (%1)").arg(wallet.balance()));
		check(wallet.atTable() && wallet.cashSession().stack == minBuyIn,
			  QStringLiteral("the stack at the table equals the buy-in"));
		check(wallet.cashSession().limit == QLatin1String(limit.key),
			  QStringLiteral("the session remembers the limit (%1)").arg(wallet.cashSession().limit));
		allowed = wallet.canSitDown(10, limit, &reason);
		check(!allowed, QStringLiteral("a second table is refused (%1)").arg(reason));

		// A hand is played: counters grow, the stack changes.
		wallet.recordHand(true);
		wallet.updateStack(55);
		check(wallet.handsPlayed() == 1 && wallet.handsWon() == 1,
			  QStringLiteral("the hand counters grow"));

		// Stored and read back: the session survives a restart.
		PlayerState::StateDocument withTable;
		withTable.profile = wallet.document().profile;
		withTable.cash = wallet.cashSession();
		ok = store.save(withTable, error);
		check(ok, QStringLiteral("save with a cash session (%1)").arg(error));
		printStored();

		PlayerState::StateDocument reopened;
		ok = store.load(reopened, error);
		check(ok, QStringLiteral("load with a cash session (%1)").arg(error));
		check(reopened.cash.active() && reopened.cash.stack == 55,
			  QStringLiteral("the stack at the table survives a save/load round trip"));
		check(reopened.cash.buyIn == minBuyIn, QStringLiteral("the buy-in survives as well"));
		check(reopened.cash.limit == QLatin1String(limit.key),
			  QStringLiteral("the limit survives a save/load round trip (%1)").arg(reopened.cash.limit));

		// A crash leaves the session on disk: the next start returns the stack.
		PlayerState::Wallet recovered(reopened);
		check(recovered.recoverInterruptedSession(now),
			  QStringLiteral("an interrupted session is recovered"));
		check(!recovered.atTable(), QStringLiteral("the recovered session is closed"));
		check(recovered.balance() == start - minBuyIn + 55,
			  QStringLiteral("the last known stack went back into the balance (%1)")
			  .arg(recovered.balance()));

		// Leaving the table normally does the same.
		PlayerState::Wallet leaving;
		check(leaving.sitDown(minBuyIn, limit, now), QStringLiteral("sit down again"));
		leaving.updateStack(30);
		const qint64 returned = leaving.standUp(now);
		check(returned == 30, QStringLiteral("standing up returns the stack (%1)").arg(returned));
		check(leaving.balance() == start - minBuyIn + 30,
			  QStringLiteral("the balance is what was left plus the stack (%1)").arg(leaving.balance()));
		check(!leaving.atTable(), QStringLiteral("no session is left after standing up"));

		// The rebuy: only below the floor, once per cooldown, and it tops up.
		PlayerState::Wallet rebuying;
		rebuying.document().profile.balance = 10 * D;
		qint64 secondsLeft = 0;
		check(rebuying.rebuyAvailable(now, &secondsLeft),
			  QStringLiteral("a balance below the floor may rebuy"));
		qint64 added = 0;
		check(rebuying.takeRebuy(now, &added), QStringLiteral("the rebuy is paid out"));
		check(rebuying.balance() == PlayerState::StartingBalance,
			  QStringLiteral("the rebuy tops the balance up to the floor (%1)")
			  .arg(rebuying.balance()));
		check(added == PlayerState::StartingBalance - 10 * D,
			  QStringLiteral("the rebuy reports what it added (%1)").arg(added));
		rebuying.document().profile.balance = 5 * D;
		check(!rebuying.rebuyAvailable(now, &secondsLeft),
			  QStringLiteral("the cooldown blocks a second rebuy"));
		check(secondsLeft > 0 && secondsLeft <= PlayerState::RebuyCooldownSec,
			  QStringLiteral("the cooldown reports the remaining time (%1 s)").arg(secondsLeft));
		check(rebuying.rebuyAvailable(now.addSecs(PlayerState::RebuyCooldownSec + 1)),
			  QStringLiteral("after the cooldown the rebuy is available again"));
		rebuying.document().profile.balance = PlayerState::StartingBalance;
		check(!rebuying.rebuyAvailable(now.addSecs(PlayerState::RebuyCooldownSec + 1)),
			  QStringLiteral("a balance at the floor gets no rebuy"));

		// With no seat open anywhere the cooldown does not apply - there would be
		// nothing to play while waiting.
		rebuying.document().profile.balance = limit.minBuyIn() - 1;
		check(rebuying.rebuyAvailable(now, &secondsLeft),
			  QStringLiteral("a balance that cannot buy a seat rebuys without waiting"));

		// The ladder: the bankroll opens the steps, not the buy-in.
		check(PlayerState::unlockedCashLimits(limit.minBuyIn()) == 1,
			  QStringLiteral("a seat's worth opens the bottom limit (%1)")
			  .arg(PlayerState::unlockedCashLimits(limit.minBuyIn())));
		check(PlayerState::unlockedCashLimits(limit.minBuyIn() - 1) == 0,
			  QStringLiteral("below the minimum buy-in nothing is open (%1)")
			  .arg(PlayerState::unlockedCashLimits(limit.minBuyIn() - 1)));
		check(PlayerState::unlockedCashLimits(20 * D) == 1,
			  QStringLiteral("$20 opens exactly the bottom limit (%1)")
			  .arg(PlayerState::unlockedCashLimits(20 * D)));
		check(PlayerState::unlockedCashLimits(40 * D) == 2,
			  QStringLiteral("$40 opens the second limit as well (%1)")
			  .arg(PlayerState::unlockedCashLimits(40 * D)));
		check(PlayerState::unlockedCashLimits(PlayerState::StartingBalance) >= 1,
			  QStringLiteral("the starting balance opens at least one limit (%1)")
			  .arg(PlayerState::unlockedCashLimits(PlayerState::StartingBalance)));

		PlayerState::Wallet small;
		small.document().profile.balance = 20 * D;
		allowed = small.canSitDown(limit.minBuyIn(), limit, &reason);
		check(allowed, QStringLiteral("$20 is enough for the bottom limit (%1)").arg(reason));
		allowed = small.canSitDown(higher.minBuyIn(), higher, &reason);
		check(!allowed, QStringLiteral("$20 does not open the second limit (%1)").arg(reason));
		// Losing a buy-in at the bottom must not lock the player out: the step
		// stays open down to the minimum buy-in.
		small.document().profile.balance = 20 * D - 1;
		allowed = small.canSitDown(limit.minBuyIn(), limit, &reason);
		check(allowed, QStringLiteral("a dollar short of the gate still plays the bottom step (%1)")
			  .arg(reason));
		small.document().profile.balance = limit.minBuyIn() - 1;
		allowed = small.canSitDown(limit.minBuyIn(), limit, &reason);
		check(!allowed, QStringLiteral("below the minimum buy-in no seat is sold (%1)").arg(reason));
	}

	// A file from version 1 (no cash table) still loads.
	{
		QFile older(store.filePath());
		if(older.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			older.write("{\"schemaVersion\": 1, \"profile\": {\"name\": \"Old\", \"balance\": 42}}");
			older.close();
		}
		PlayerState::StateDocument migrated;
		ok = store.load(migrated, error);
		check(ok, QStringLiteral("a version 1 file is still read (%1)").arg(error));
		check(migrated.profile.balance == 42 * 100,
			  QStringLiteral("its balance survives, scaled from dollars to cents (%1)")
			  .arg(migrated.profile.balance));
		check(!migrated.cash.active(), QStringLiteral("it has no cash session"));
	}

	// A file from version 2 (dollars) still loads: the money is scaled.
	{
		QFile older(store.filePath());
		if(older.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			older.write("{\"schemaVersion\": 2, \"profile\": {\"name\": \"Old\", \"balance\": 60},"
						" \"cashTable\": {\"buyIn\": 40, \"stack\": 55}}");
			older.close();
		}
		PlayerState::StateDocument migrated;
		ok = store.load(migrated, error);
		check(ok, QStringLiteral("a version 2 file is still read (%1)").arg(error));
		check(migrated.profile.balance == 6000,
			  QStringLiteral("its balance is scaled from dollars to cents (%1)")
			  .arg(migrated.profile.balance));
		check(migrated.cash.active() && migrated.cash.buyIn == 4000 && migrated.cash.stack == 5500,
			  QStringLiteral("its stack at the table is scaled too (%1 / %2)")
			  .arg(migrated.cash.buyIn).arg(migrated.cash.stack));
		// A session written before the ladder existed has no limit key; it was
		// played on the only table there was, the top one.
		check(PlayerState::cashLimitByKey(migrated.cash.limit).smallBlind
			  == PlayerState::cashLimit(PlayerState::cashLimitCount() - 1).smallBlind,
			  QStringLiteral("a session without a limit reads as the top limit (%1)")
			  .arg(QString::fromLatin1(PlayerState::cashLimitByKey(migrated.cash.limit).label)));
	}

	// A corrupt file has to be set aside, not overwritten.
	{
		QFile broken(store.filePath());
		if(broken.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			broken.write("{ this is not json");
			broken.close();
		}
	}
	PlayerState::StateDocument ignored;
	check(!store.load(ignored, error), QStringLiteral("a corrupt file is rejected"));
	check(QFile::exists(store.filePath() + QStringLiteral(".corrupt")),
		  QStringLiteral("the corrupt file is kept as state.json.corrupt"));
	check(!store.exists(), QStringLiteral("no state file is left after the corrupt one"));
	check(!store.load(ignored, error), QStringLiteral("loading without a state file fails"));

	// A file of a newer version is not ours to touch.
	{
		QFile newer(store.filePath());
		if(newer.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			newer.write("{\"schemaVersion\": 99, \"profile\": {}}");
			newer.close();
		}
	}
	const qint64 sizeBefore = QFileInfo(store.filePath()).size();
	check(!store.load(ignored, error), QStringLiteral("a file from a newer version is rejected"));
	check(QFileInfo(store.filePath()).size() == sizeBefore,
		  QStringLiteral("the file from a newer version is left untouched"));

	// Do not leave a damaged test directory behind.
	QFile::remove(store.filePath());
	QFile::remove(store.filePath() + QStringLiteral(".corrupt"));

	printf("  %d failure(s)\n", failures);
	return failures == 0;
}

}

int main(int argc, char *argv[])
{
	SimOptions options;
	string jsonFile;
	string stateSelfTestDir;
	bool quiet = false;

	// Parse before QCoreApplication exists so that nothing can strip our
	// arguments - and so that --help works without Qt being set up.
	for(int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;

		if(strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			printUsage();
			return 0;
		} else if(strcmp(arg, "--quiet") == 0) {
			quiet = true;
			continue;
		} else if(strcmp(arg, "--mixed") == 0) {
			options.mixedProfiles = true;
			continue;
		}

		if(value == NULL) {
			fprintf(stderr, "pokerth_sim: option '%s' needs a value (see --help)\n", arg);
			return 2;
		}

		if(strcmp(arg, "--json") == 0) {
			jsonFile = value;
			i++;
			continue;
		}

		if(strcmp(arg, "--state-selftest") == 0) {
			stateSelfTestDir = value;
			i++;
			continue;
		}

		if(strcmp(arg, "--profile") == 0) {
			BotProfile profile = options.profile;
			if(!BotProfile::parse(value, profile)) {
				fprintf(stderr, "pokerth_sim: unknown profile '%s'\n"
						"  strength: %s\n  style:    %s\n",
						value, BotProfile::strengthList(), BotProfile::archetypeList());
				return 2;
			}
			options.profile = profile;
			options.mixedProfiles = false;
			i++;
			continue;
		}

		if(strcmp(arg, "--strength") == 0) {
			BotStrength strength;
			if(!BotProfile::parseStrength(value, strength)) {
				fprintf(stderr, "pokerth_sim: unknown strength '%s' (%s)\n",
						value, BotProfile::strengthList());
				return 2;
			}
			options.profile.strength = strength;
			options.mixedProfiles = false;
			i++;
			continue;
		}

		if(strcmp(arg, "--archetype") == 0) {
			BotArchetype archetype;
			if(!BotProfile::parseArchetype(value, archetype)) {
				fprintf(stderr, "pokerth_sim: unknown style '%s' (%s)\n",
						value, BotProfile::archetypeList());
				return 2;
			}
			options.profile.archetype = archetype;
			options.mixedProfiles = false;
			i++;
			continue;
		}

		if(strcmp(arg, "--seat-profiles") == 0) {
			if(!parseProfileList(value, options.seatProfiles)) {
				fprintf(stderr, "pokerth_sim: cannot read the profile list '%s'\n"
						"  strength: %s\n  style:    %s\n",
						value, BotProfile::strengthList(), BotProfile::archetypeList());
				return 2;
			}
			options.mixedProfiles = false;
			i++;
			continue;
		}

		if(strcmp(arg, "--field") == 0) {
			if(!BotProfile::parsePopulation(value, options.fieldProfiles)) {
				fprintf(stderr, "pokerth_sim: unknown field '%s' (%s)\n",
						value, BotProfile::populationList());
				return 2;
			}
			options.fieldName = value;
			options.mixedProfiles = false;
			i++;
			continue;
		}

		if(strcmp(arg, "--tier") == 0) {
			if(!BotProfile::hasTier(value)) {
				fprintf(stderr, "pokerth_sim: unknown tier '%s' (%s)\n",
						value, BotProfile::tierList());
				return 2;
			}
			options.tierName = value;
			options.mixedProfiles = false;
			i++;
			continue;
		}

		if(strcmp(arg, "--scale-open") == 0 || strcmp(arg, "--scale-3bet") == 0
				|| strcmp(arg, "--scale-call") == 0 || strcmp(arg, "--scale-raise") == 0
				|| strcmp(arg, "--scale-cbet") == 0 || strcmp(arg, "--scale-adapt") == 0
				|| strcmp(arg, "--margin") == 0) {
			double number = 0.0;
			if(!parseDouble(value, number)) {
				fprintf(stderr, "pokerth_sim: option '%s' expects a number, got '%s'\n", arg, value);
				return 2;
			}
			i++;
			if(strcmp(arg, "--scale-open") == 0) options.openScale = number;
			else if(strcmp(arg, "--scale-3bet") == 0) options.threeBetScale = number;
			else if(strcmp(arg, "--scale-call") == 0) options.callScale = number;
			else if(strcmp(arg, "--scale-raise") == 0) options.raiseScale = number;
			else if(strcmp(arg, "--scale-cbet") == 0) options.cbetScale = number;
			else if(strcmp(arg, "--scale-adapt") == 0) options.adaptScale = number;
			else {
				options.callMargin = number;
				options.overrideCallMargin = true;
			}
			continue;
		}

		long long number = 0;
		if(!parseNumber(value, number)) {
			fprintf(stderr, "pokerth_sim: option '%s' expects a number, got '%s'\n", arg, value);
			return 2;
		}
		i++;

		if(strcmp(arg, "--players") == 0) {
			options.players = static_cast<int>(number);
		} else if(strcmp(arg, "--stack") == 0) {
			options.startCash = static_cast<int>(number);
		} else if(strcmp(arg, "--sb") == 0) {
			options.smallBlind = static_cast<int>(number);
		} else if(strcmp(arg, "--hands") == 0) {
			options.maxHands = static_cast<int>(number);
		} else if(strcmp(arg, "--raise-every") == 0) {
			options.raiseEveryHands = static_cast<int>(number);
		} else if(strcmp(arg, "--seed") == 0) {
			options.seed = static_cast<unsigned long long>(number);
		} else if(strcmp(arg, "--runs") == 0) {
			options.runs = static_cast<int>(number);
		} else {
			fprintf(stderr, "pokerth_sim: unknown option '%s' (see --help)\n", arg);
			return 2;
		}
	}

	if(options.runs < 1) {
		fprintf(stderr, "pokerth_sim: --runs must be at least 1\n");
		return 2;
	}

	// A field and a seat list describe the same seats in different ways; playing
	// both would silently ignore one of them, so refuse instead.
	if(!options.fieldProfiles.empty() && !options.seatProfiles.empty()) {
		fprintf(stderr, "pokerth_sim: --field and --seat-profiles are exclusive\n");
		return 2;
	}

	// A tier deals the opponents itself, so a fixed list of them has no place
	// next to it.
	if(!options.tierName.empty()
			&& (!options.fieldProfiles.empty() || !options.seatProfiles.empty())) {
		fprintf(stderr, "pokerth_sim: --tier, --field and --seat-profiles are exclusive\n");
		return 2;
	}

	// A diagnostic style is an instrument: one seat of it next to the opponents
	// it punishes. A table of five of them measures nothing.
	if(options.fieldProfiles.empty() && options.seatProfiles.empty() && options.tierName.empty()
			&& !options.mixedProfiles && BotProfile::isDiagnostic(options.profile.archetype)) {
		fprintf(stderr, "pokerth_sim: '%s' is a diagnostic style, "
				"give it one seat with --seat-profiles\n",
				BotProfile::archetypeName(options.profile.archetype));
		return 2;
	}

	if(options.seed == 0) {
		// Report the seed of a non-reproducible run as well, so that anything
		// interesting can be repeated exactly.
		random_device device;
		options.seed = (static_cast<unsigned long long>(device()) << 32) | device();
		if(options.seed == 0) options.seed = 1;
	}

	// ConfigFile resolves its directories through QtHelper, which reads
	// QCoreApplication::applicationDirPath() - and the engine reads
	// EngineVersion and friends from the config.
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("pokerth_sim"));

	if(!stateSelfTestDir.empty()) {
		const bool passed = runStateSelfTest(QString::fromStdString(stateSelfTestDir));
		printf("\nstate self test: %s\n", passed ? "PASS" : "FAILED");
		return passed ? 0 : 1;
	}

	ConfigFile config(argv[0], true);

	SimTotals totals;
	SimReport lastReport;
	string error;
	for(int run = 0; run < options.runs; run++) {
		SimOptions runOptions = options;
		runOptions.seed = options.seed + static_cast<unsigned long long>(run);

		SimReport report;
		if(!runSimulation(&config, runOptions, report, error)) {
			fprintf(stderr, "pokerth_sim: game %d: %s\n", run + 1, error.c_str());
			return 1;
		}
		accumulate(totals, report);
		lastReport = report;
	}

	if(!quiet) {
		if(options.runs == 1) {
			printSingleRun(lastReport, options.smallBlind);
		} else {
			printTotals(totals, options);
		}
		printFieldHeader(options);
	} else {
		printf("%d game(s), %lld hands, %lld ms, seed %llu\n",
			   totals.runs, totals.hands, totals.elapsedMs, options.seed);
	}

	if(!jsonFile.empty() && !writeJson(jsonFile, totals, options)) {
		return 1;
	}

	return 0;
}
