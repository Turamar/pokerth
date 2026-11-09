/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2025 Felix Hammer, Florian Thauer, Lothar May          *
 *****************************************************************************/

#include "roster.h"

namespace PlayerState
{

void Roster::clear()
{
	m_entries.clear();
}

int Roster::add(const QString &name, const QString &profile, int stack)
{
	RosterEntry entry;
	entry.id = m_entries.size() == 0 ? 0 : m_entries.last().id + 1;
	entry.name = name;
	entry.profile = profile;
	entry.stack = stack;
	m_entries.push_back(entry);
	return entry.id;
}

bool Roster::seat(int id, int table, int seat)
{
	if (table < 0 || seat < 0)
		return false;
	RosterEntry *who = nullptr;
	for (RosterEntry &entry : m_entries) {
		if (entry.id == id)
			who = &entry;
	}
	if (!who)
		return false;
	// The target seat can only belong to one person: whoever is there makes
	// room. A caller that seats two people on one seat is a bug on its side,
	// not something the roster can honour.
	for (RosterEntry &entry : m_entries) {
		if (entry.id != id && entry.table == table && entry.seat == seat) {
			entry.table = -1;
			entry.seat = -1;
		}
	}
	who->table = table;
	who->seat = seat;
	return true;
}

void Roster::unseat(int id)
{
	for (RosterEntry &entry : m_entries) {
		if (entry.id == id) {
			entry.table = -1;
			entry.seat = -1;
		}
	}
}

const RosterEntry *Roster::entry(int id) const
{
	for (const RosterEntry &entry : m_entries) {
		if (entry.id == id)
			return &entry;
	}
	return nullptr;
}

const RosterEntry *Roster::entryAt(int table, int seat) const
{
	for (const RosterEntry &entry : m_entries) {
		if (entry.table == table && entry.seat == seat)
			return &entry;
	}
	return nullptr;
}

int Roster::idAt(int table, int seat) const
{
	const RosterEntry *who = entryAt(table, seat);
	return who ? who->id : -1;
}

int Roster::tableCount() const
{
	int used = 0;
	for (const RosterEntry &entry : m_entries) {
		if (entry.seated())
			used = qMax(used, entry.table + 1);
	}
	return used;
}

int Roster::stack(int id) const
{
	const RosterEntry *who = entry(id);
	return who ? who->stack : 0;
}

void Roster::setStack(int id, int stack)
{
	for (RosterEntry &entry : m_entries) {
		if (entry.id == id) {
			entry.stack = stack;
		}
	}
}

QVector<int> Roster::seatsAt(int table) const
{
	int seats = 0;
	for (const RosterEntry &entry : m_entries) {
		if (entry.seated() && entry.table == table)
			seats = qMax(seats, entry.seat + 1);
	}
	QVector<int> ids(seats, -1);
	for (const RosterEntry &entry : m_entries) {
		if (entry.seated() && entry.table == table && entry.seat < seats)
			ids[entry.seat] = entry.id;
	}
	return ids;
}

QStringList Roster::seatNames(int table) const
{
	QStringList names;
	const QVector<int> ids = seatsAt(table);
	for (int id : ids) {
		const RosterEntry *who = id >= 0 ? entry(id) : nullptr;
		names << (who ? who->name : QString());
	}
	return names;
}

QStringList Roster::seatProfiles(int table) const
{
	QStringList profiles;
	const QVector<int> ids = seatsAt(table);
	for (int id : ids) {
		const RosterEntry *who = id >= 0 ? entry(id) : nullptr;
		profiles << (who ? who->profile : QString());
	}
	return profiles;
}

QVector<int> Roster::seatStacks(int table) const
{
	QVector<int> stacks;
	const QVector<int> ids = seatsAt(table);
	for (int id : ids) {
		const RosterEntry *who = id >= 0 ? entry(id) : nullptr;
		stacks << (who ? who->stack : 0);
	}
	return stacks;
}

QStringList Roster::botProfiles(int table) const
{
	QStringList profiles = seatProfiles(table);
	if (!profiles.isEmpty())
		profiles.removeFirst();   // seat 0 is the human
	return profiles;
}

}
