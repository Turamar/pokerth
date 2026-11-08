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

#include "statestore.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>

using namespace std;

namespace PlayerState
{

namespace
{

// 1: profile only.
// 2: the cash table session was added (see CashSession). Files of version 1 are
//    still read - they simply have no session - and are written back as 2.
const int SchemaVersion = 2;
const char *StateFileName = "state.json";

const char *KeySchemaVersion = "schemaVersion";
const char *KeyProfile = "profile";
const char *KeyCash = "cashTable";
const char *KeyCashBuyIn = "buyIn";
const char *KeyCashStack = "stack";
const char *KeyCashStartedAt = "startedAt";
const char *KeyName = "name";
const char *KeyBalance = "balance";
const char *KeyHandsPlayed = "handsPlayed";
const char *KeyHandsWon = "handsWon";
const char *KeyTournamentsPlayed = "tournamentsPlayed";
const char *KeyTournamentsWon = "tournamentsWon";
const char *KeyCreatedAt = "createdAt";
const char *KeyUpdatedAt = "updatedAt";
const char *KeyLastRebuyAt = "lastRebuyAt";

QString toIso(const QDateTime &dateTime)
{
	return dateTime.isValid() ? dateTime.toUTC().toString(Qt::ISODate) : QString();
}

QDateTime fromIso(const QString &text)
{
	if(text.isEmpty()) return QDateTime();
	return QDateTime::fromString(text, Qt::ISODate).toUTC();
}

QJsonObject profileToJson(const PlayerProfile &profile)
{
	QJsonObject object;
	object[QLatin1String(KeyName)] = profile.name;
	object[QLatin1String(KeyBalance)] = static_cast<double>(profile.balance);
	object[QLatin1String(KeyHandsPlayed)] = static_cast<double>(profile.handsPlayed);
	object[QLatin1String(KeyHandsWon)] = static_cast<double>(profile.handsWon);
	object[QLatin1String(KeyTournamentsPlayed)] = static_cast<double>(profile.tournamentsPlayed);
	object[QLatin1String(KeyTournamentsWon)] = static_cast<double>(profile.tournamentsWon);
	object[QLatin1String(KeyCreatedAt)] = toIso(profile.createdAt);
	object[QLatin1String(KeyUpdatedAt)] = toIso(profile.updatedAt);
	if(profile.lastRebuyAt.isValid()) {
		object[QLatin1String(KeyLastRebuyAt)] = toIso(profile.lastRebuyAt);
	}
	return object;
}

PlayerProfile profileFromJson(const QJsonObject &object)
{
	PlayerProfile profile;
	profile.name = object.value(QLatin1String(KeyName)).toString();
	// Counters are stored as JSON numbers (doubles); qint64 keeps the arithmetic
	// in the game integral.
	profile.balance = static_cast<qint64>(object.value(QLatin1String(KeyBalance)).toDouble());
	profile.handsPlayed = static_cast<qint64>(object.value(QLatin1String(KeyHandsPlayed)).toDouble());
	profile.handsWon = static_cast<qint64>(object.value(QLatin1String(KeyHandsWon)).toDouble());
	profile.tournamentsPlayed = static_cast<qint64>(object.value(QLatin1String(KeyTournamentsPlayed)).toDouble());
	profile.tournamentsWon = static_cast<qint64>(object.value(QLatin1String(KeyTournamentsWon)).toDouble());
	profile.createdAt = fromIso(object.value(QLatin1String(KeyCreatedAt)).toString());
	profile.updatedAt = fromIso(object.value(QLatin1String(KeyUpdatedAt)).toString());
	profile.lastRebuyAt = fromIso(object.value(QLatin1String(KeyLastRebuyAt)).toString());
	return profile;
}

QJsonObject cashToJson(const CashSession &cash)
{
	if(!cash.active()) {
		return QJsonObject();
	}
	QJsonObject object;
	object[QLatin1String(KeyCashBuyIn)] = static_cast<double>(cash.buyIn);
	object[QLatin1String(KeyCashStack)] = static_cast<double>(cash.stack);
	object[QLatin1String(KeyCashStartedAt)] = toIso(cash.startedAt);
	return object;
}

CashSession cashFromJson(const QJsonObject &object)
{
	CashSession cash;
	cash.buyIn = static_cast<qint64>(object.value(QLatin1String(KeyCashBuyIn)).toDouble());
	cash.stack = static_cast<qint64>(object.value(QLatin1String(KeyCashStack)).toDouble());
	cash.startedAt = fromIso(object.value(QLatin1String(KeyCashStartedAt)).toString());
	if(cash.stack < 0) {
		cash.stack = 0;
	}
	return cash;
}

}

StateStore::StateStore(const QString &directory)
	: m_directory(directory)
{
}

QString StateStore::filePath() const
{
	return m_directory + QLatin1Char('/') + QLatin1String(StateFileName);
}

bool StateStore::exists() const
{
	return QFile::exists(filePath());
}

int StateStore::currentSchemaVersion()
{
	return SchemaVersion;
}

PlayerProfile StateStore::newProfile(const QString &name)
{
	PlayerProfile profile;
	profile.name = name;
	profile.balance = StartingBalance;
	const QDateTime now = QDateTime::currentDateTimeUtc();
	profile.createdAt = now;
	profile.updatedAt = now;
	return profile;
}

StateDocument StateStore::newDocument(const QString &name)
{
	StateDocument document;
	document.profile = newProfile(name);
	return document;
}

bool StateStore::load(StateDocument &document, QString &error) const
{
	error.clear();

	QFile file(filePath());
	if(!file.exists()) {
		error = QStringLiteral("no stored state at %1").arg(filePath());
		return false;
	}
	if(!file.open(QIODevice::ReadOnly)) {
		error = QStringLiteral("cannot read %1: %2").arg(filePath(), file.errorString());
		return false;
	}
	const QByteArray content = file.readAll();
	file.close();

	QJsonParseError parseError;
	const QJsonDocument json = QJsonDocument::fromJson(content, &parseError);
	if(parseError.error != QJsonParseError::NoError || !json.isObject()) {
		// Not something this version can read: keep the file for inspection and
		// let the caller start from a fresh profile.
		moveCorruptFileAside();
		error = QStringLiteral("%1 is not valid: %2")
				.arg(filePath(), parseError.errorString());
		return false;
	}

	const QJsonObject root = json.object();
	const int version = root.value(QLatin1String(KeySchemaVersion)).toInt(-1);
	if(version < 0) {
		moveCorruptFileAside();
		error = QStringLiteral("%1 has no schema version").arg(filePath());
		return false;
	}
	if(version > SchemaVersion) {
		// A newer game wrote this. Reading it would silently drop whatever the
		// newer version added, so leave the file alone and report it.
		error = QStringLiteral("%1 was written by a newer version (schema %2, this build knows %3)")
				.arg(filePath())
				.arg(version)
				.arg(SchemaVersion);
		return false;
	}

	// Version 1 has no cash session; it reads as "not at a table" and is written
	// back as version 2 on the next save.
	document.profile = profileFromJson(root.value(QLatin1String(KeyProfile)).toObject());
	document.cash = cashFromJson(root.value(QLatin1String(KeyCash)).toObject());
	return true;
}

bool StateStore::save(StateDocument &document, QString &error) const
{
	error.clear();

	PlayerProfile &profile = document.profile;
	QDir directory(m_directory);
	if(!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
		error = QStringLiteral("cannot create %1").arg(m_directory);
		return false;
	}

	profile.updatedAt = QDateTime::currentDateTimeUtc();
	if(!profile.createdAt.isValid()) {
		profile.createdAt = profile.updatedAt;
	}

	QJsonObject root;
	root[QLatin1String(KeySchemaVersion)] = SchemaVersion;
	root[QLatin1String(KeyProfile)] = profileToJson(profile);
	if(document.cash.active()) {
		root[QLatin1String(KeyCash)] = cashToJson(document.cash);
	}

	// QSaveFile writes a temporary file and only replaces the target on commit:
	// a crash, a full disk or a killed process can leave the temporary file
	// behind but never a truncated state.json.
	QSaveFile file(filePath());
	if(!file.open(QIODevice::WriteOnly)) {
		error = QStringLiteral("cannot write %1: %2").arg(filePath(), file.errorString());
		return false;
	}
	const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
	if(file.write(json) != json.size()) {
		error = QStringLiteral("short write to %1: %2").arg(filePath(), file.errorString());
		file.cancelWriting();
		return false;
	}
	if(!file.commit()) {
		error = QStringLiteral("cannot store %1: %2").arg(filePath(), file.errorString());
		return false;
	}
	return true;
}

bool StateStore::moveCorruptFileAside() const
{
	const QString source = filePath();
	const QString target = source + QStringLiteral(".corrupt");
	QFile::remove(target);
	return QFile::rename(source, target);
}

}
