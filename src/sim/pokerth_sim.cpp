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
 * Plays a game with nothing but computer players and prints what they did:
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
 *                    [--raise-every N] [--seed N] [--json FILE] [--quiet]
 */

#include "simdriver.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <string>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <configfile.h>
#include <statestore.h>

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
		"  --json FILE       write the report as JSON to FILE\n"
		"  --quiet           only the summary line, no table\n"
		"  --state-selftest DIR\n"
		"                    check the state store in DIR instead of simulating\n"
		"\n"
		"The run stops earlier than --hands as soon as one seat holds all the chips.\n");
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

double percentage(long long value, long long total)
{
	return total > 0 ? 100.0 * static_cast<double>(value) / static_cast<double>(total) : 0.0;
}

void printReport(const SimReport &report)
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

	printf("\n%-6s %7s %7s %7s %7s %7s %8s %10s\n",
		   "seat", "hands", "VPIP%", "PFR%", "AF", "won", "chips", "busted");
	for(size_t i = 0; i < report.seats.size(); i++) {
		const SimSeatStats &seat = report.seats[i];
		char aggression[16];
		if(seat.passive > 0) {
			snprintf(aggression, sizeof(aggression), "%.2f",
					 static_cast<double>(seat.aggressive) / static_cast<double>(seat.passive));
		} else if(seat.aggressive > 0) {
			snprintf(aggression, sizeof(aggression), "inf");
		} else {
			snprintf(aggression, sizeof(aggression), "0.00");
		}
		char busted[16];
		if(seat.bustedAtHand >= 0) {
			snprintf(busted, sizeof(busted), "%d", seat.bustedAtHand);
		} else {
			snprintf(busted, sizeof(busted), "-");
		}
		printf("%-6s %7lld %7.1f %7.1f %7s %7lld %8lld %10s\n",
			   seat.name.c_str(), seat.hands,
			   percentage(seat.vpip, seat.hands),
			   percentage(seat.pfr, seat.hands),
			   aggression, seat.handsWon, seat.chips, busted);
	}
}

bool writeJson(const string &fileName, const SimReport &report, const SimOptions &options)
{
	QJsonObject root;
	root[QStringLiteral("seed")] = static_cast<qint64>(report.seed);
	root[QStringLiteral("handsPlayed")] = report.handsPlayed;
	root[QStringLiteral("engineSteps")] = static_cast<qint64>(report.steps);
	root[QStringLiteral("elapsedMs")] = static_cast<qint64>(report.elapsedMs);
	root[QStringLiteral("playersLeft")] = report.playersLeft;
	root[QStringLiteral("finished")] = report.finished;
	root[QStringLiteral("winner")] = QString::fromStdString(report.winner);

	QJsonObject setup;
	setup[QStringLiteral("players")] = options.players;
	setup[QStringLiteral("startCash")] = options.startCash;
	setup[QStringLiteral("smallBlind")] = options.smallBlind;
	setup[QStringLiteral("raiseEveryHands")] = options.raiseEveryHands;
	setup[QStringLiteral("maxHands")] = options.maxHands;
	root[QStringLiteral("setup")] = setup;

	QJsonArray seats;
	for(size_t i = 0; i < report.seats.size(); i++) {
		const SimSeatStats &seat = report.seats[i];
		QJsonObject entry;
		entry[QStringLiteral("seat")] = seat.seat;
		entry[QStringLiteral("name")] = QString::fromStdString(seat.name);
		entry[QStringLiteral("hands")] = static_cast<qint64>(seat.hands);
		entry[QStringLiteral("vpipPercent")] = percentage(seat.vpip, seat.hands);
		entry[QStringLiteral("pfrPercent")] = percentage(seat.pfr, seat.hands);
		entry[QStringLiteral("aggressive")] = static_cast<qint64>(seat.aggressive);
		entry[QStringLiteral("passive")] = static_cast<qint64>(seat.passive);
		entry[QStringLiteral("handsWon")] = static_cast<qint64>(seat.handsWon);
		entry[QStringLiteral("chipsWon")] = static_cast<qint64>(seat.chipsWon);
		entry[QStringLiteral("chips")] = static_cast<qint64>(seat.chips);
		entry[QStringLiteral("bustedAtHand")] = seat.bustedAtHand;
		seats.append(entry);
	}
	root[QStringLiteral("seats")] = seats;

	QFile file(QString::fromStdString(fileName));
	const QFileInfo info(QString::fromStdString(fileName));
	if(!info.absolutePath().isEmpty()) {
		QDir().mkpath(info.absolutePath());
	}
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		fprintf(stderr, "pokerth_sim: cannot write '%s'\n", fileName.c_str());
		return false;
	}
	const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
	file.write(json);
	file.close();
	return true;
}

/* Self test of the state store: money and counters have to survive a restart,
 * and a damaged or too new file must never be overwritten silently. Runs
 * without a GUI and without Qt application state, so it can be part of a build
 * check. */
bool runStateSelfTest(const QString &directory)
{
	int failures = 0;
	auto check = [&failures](bool condition, const QString &message) {
		printf("  %s %s\n", condition ? "ok  " : "FAIL", message.toUtf8().constData());
		if(!condition) failures++;
	};

	GameState::StateStore store(directory);
	QString error;
	printf("state store: %s\n", store.filePath().toUtf8().constData());

	// A leftover of an earlier run must not confuse the test.
	QFile::remove(store.filePath());
	QFile::remove(store.filePath() + QStringLiteral(".corrupt"));
	check(!store.exists(), QStringLiteral("a fresh directory has no state file"));

	GameState::PlayerProfile profile = GameState::StateStore::newProfile(QStringLiteral("SelfTest"));
	check(profile.balance == GameState::StartingBalance,
		  QStringLiteral("a new profile starts with the starting balance"));
	check(store.save(profile, error), QStringLiteral("first save (%1)").arg(error));
	check(store.exists(), QStringLiteral("the state file exists after the first save"));

	GameState::PlayerProfile loaded;
	check(store.load(loaded, error), QStringLiteral("load after the first save (%1)").arg(error));
	check(loaded.name == profile.name, QStringLiteral("the name survives"));
	check(loaded.balance == profile.balance, QStringLiteral("the balance survives"));
	check(loaded.createdAt.isValid(), QStringLiteral("the creation time is set"));

	// Change everything a session changes, then read it back.
	loaded.balance = 75;
	loaded.handsPlayed = 3;
	loaded.handsWon = 1;
	loaded.lastRebuyAt = QDateTime::currentDateTimeUtc();
	const QDateTime createdBefore = loaded.createdAt;
	check(store.save(loaded, error), QStringLiteral("second save (%1)").arg(error));

	GameState::PlayerProfile reloaded;
	check(store.load(reloaded, error), QStringLiteral("load after the second save (%1)").arg(error));
	check(reloaded.balance == 75, QStringLiteral("the balance survives a save/load round trip"));
	check(reloaded.handsPlayed == 3 && reloaded.handsWon == 1,
		  QStringLiteral("the counters survive a save/load round trip"));
	check(reloaded.lastRebuyAt.isValid(), QStringLiteral("the rebuy time survives"));
	check(reloaded.createdAt == createdBefore,
		  QStringLiteral("the creation time is not touched by saving"));

	// A corrupt file has to be set aside, not overwritten.
	{
		QFile broken(store.filePath());
		if(broken.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			broken.write("{ this is not json");
			broken.close();
		}
	}
	GameState::PlayerProfile ignored;
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
		} else {
			fprintf(stderr, "pokerth_sim: unknown option '%s' (see --help)\n", arg);
			return 2;
		}
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

	SimReport report;
	string error;
	if(!runSimulation(&config, options, report, error)) {
		fprintf(stderr, "pokerth_sim: %s\n", error.c_str());
		return 1;
	}

	if(!quiet) {
		printReport(report);
	} else {
		printf("%d hands, %d seats left, %lld ms, seed %llu\n",
			   report.handsPlayed, report.playersLeft, report.elapsedMs, report.seed);
	}

	if(!jsonFile.empty() && !writeJson(jsonFile, report, options)) {
		return 1;
	}

	return 0;
}
