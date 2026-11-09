#pragma once

#include <QString>
#include <qglobal.h>

/* The two units of the table, in the one place that writes them.
 *
 * Money is cents (PLAN.md, "Экономика"): 4000 -> "$40", 40 -> "$0.40".
 * Chips are chips: a tournament stack has no currency, and printing 1500 chips
 * as "$15.00" makes a stack look like a prize. The table therefore asks for the
 * unit it plays (GameHandler::amount), the wallet and the result popup always
 * ask for money.
 */
namespace AmountFormat
{

inline QString money(qint64 cents)
{
	const bool negative = cents < 0;
	const qint64 value = negative ? -cents : cents;
	const qint64 dollars = value / 100;
	const qint64 rest = value % 100;

	QString text = QString::number(dollars);
	if (rest != 0) {
		text += QLatin1Char('.');
		text += QStringLiteral("%1").arg(rest, 2, 10, QLatin1Char('0'));
	}
	return (negative ? QStringLiteral("-$") : QStringLiteral("$")) + text;
}

// 1500 -> "1 500", 30 -> "30". Thousands are grouped by a space, never by a
// locale rule: the separator must not turn into a decimal comma on the way.
inline QString chips(qint64 value)
{
	const bool negative = value < 0;
	QString digits = QString::number(negative ? -value : value);
	for (int pos = digits.size() - 3; pos > 0; pos -= 3) {
		digits.insert(pos, QLatin1Char(' '));
	}
	return negative ? QStringLiteral("-") + digits : digits;
}

}
